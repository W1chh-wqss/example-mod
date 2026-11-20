#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCLayer.hpp>

using namespace geode::prelude;

struct Waypoint {
    float x;
    float y;
    std::string text;
};

static std::vector<Waypoint> g_waypoints;

// Плавающая перетаскиваемая кнопка (как в QOLMod / Eclipse)
class FloatingModButton : public CCLayer {
protected:
    CCSprite* m_icon = nullptr;
    bool m_isDragging = false;
    CCPoint m_touchStart;
    CCPoint m_nodeStart;

public:
    static FloatingModButton* create() {
        auto ret = new FloatingModButton();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    bool init() override {
        if (!CCLayer::init()) return false;

        this->setTouchEnabled(true);
        this->setTouchMode(kCCTouchesOneByOne);
        this->setTouchPriority(-500); // Высокий приоритет, чтобы всегда нажималась

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        // Создаем аккуратную круглую кнопку с иконкой
        m_icon = CircleButtonSprite::createWithSpriteFrameName(
            "GJ_plusBtn_001.png", 1.0f, CircleBaseColor::Green, CircleBaseSize::Small
        );
        m_icon->setPosition({ 40.f, winSize.height / 2.f });
        this->addChild(m_icon);

        return true;
    }

    bool ccTouchBegan(CCTouch* touch, CCEvent* event) override {
        if (!m_icon || !this->isVisible()) return false;
        auto pos = touch->getLocation();
        auto box = m_icon->boundingBox();
        // Увеличиваем зону нажатия пальцем
        box.origin.x -= 10.f;
        box.origin.y -= 10.f;
        box.size.width += 20.f;
        box.size.height += 20.f;

        if (box.containsPoint(pos)) {
            m_touchStart = pos;
            m_nodeStart = m_icon->getPosition();
            m_isDragging = false;
            return true;
        }
        return false;
    }

    void ccTouchMoved(CCTouch* touch, CCEvent* event) override {
        auto pos = touch->getLocation();
        if (ccpDistance(pos, m_touchStart) > 8.f) {
            m_isDragging = true;
            auto delta = ccpSub(pos, m_touchStart);
            m_icon->setPosition(ccpAdd(m_nodeStart, delta));
        }
    }

    void ccTouchEnded(CCTouch* touch, CCEvent* event) override {
        if (!m_isDragging) {
            // Если не тянули, а просто нажали — открываем меню мода
            this->openModMenu();
        }
        m_isDragging = false;
    }

    void openModMenu() {
        auto pl = PlayLayer::get();
        if (!pl || !pl->m_player1) return;

        geode::createQuickPopup(
            "Platformer Helper",
            "Choose an action:\n<cg>Add Mark</c> - set waypoint here\n<cr>Clear</c> - remove all marks",
            "Clear", "Add Mark",
            [pl](auto, bool btn2) {
                if (btn2) {
                    // Нажали "Add Mark"
                    auto pos = pl->m_player1->getPosition();
                    g_waypoints.push_back({ pos.x, pos.y, "ACTION: JUMP / USE ORB!" });
                    Notification::create("Mark added!", NotificationIcon::Success)->show();
                } else {
                    // Нажали "Clear"
                    g_waypoints.clear();
                    Notification::create("All marks cleared!", NotificationIcon::Info)->show();
                }
            }
        );
    }
};

// Основная логика уровня: метки, подсказки и сохранение позиции в платформере
class $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        CCLabelBMFont* m_hintLabel = nullptr;
        CCDrawNode* m_markerNode = nullptr;
        FloatingModButton* m_floatBtn = nullptr;
        bool m_restoredPos = false;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        // Текст подсказки сверху экрана (только английские символы для корректного шрифта!)
        m_fields->m_hintLabel = CCLabelBMFont::create("", "bigFont.fnt");
        m_fields->m_hintLabel->setPosition({ winSize.width / 2, winSize.height - 40.f });
        m_fields->m_hintLabel->setScale(0.45f);
        m_fields->m_hintLabel->setVisible(false);
        this->addChild(m_fields->m_hintLabel, 100);

        // Слой для отрисовки меток в мире уровня
        m_fields->m_markerNode = CCDrawNode::create();
        if (m_objectLayer) {
            m_objectLayer->addChild(m_fields->m_markerNode, 999);
        }

        // Добавляем плавающую кнопку поверх игры
        m_fields->m_floatBtn = FloatingModButton::create();
        this->addChild(m_fields->m_floatBtn, 105);

        return true;
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        if (!m_player1 || !m_level) return;

        // 1. Восстановление сохраненной позиции в платформере при входе в уровень
        if (!m_fields->m_restoredPos && m_level->isPlatformer()) {
            m_fields->m_restoredPos = true;
            std::string keyX = fmt::format("plat_x_{}", m_level->m_levelID.value());
            std::string keyY = fmt::format("plat_y_{}", m_level->m_levelID.value());

            if (Mod::get()->hasSavedValue(keyX) && Mod::get()->hasSavedValue(keyY)) {
                float savedX = Mod::get()->getSavedValue<float>(keyX);
                float savedY = Mod::get()->getSavedValue<float>(keyY);
                if (savedX > 0.f) {
                    m_player1->setPosition({ savedX, savedY });
                    Notification::create("Platformer position restored!", NotificationIcon::Success)->show();
                }
            }
        }

        // 2. Отрисовка меток и проверка приближения игрока
        if (m_fields->m_markerNode && m_fields->m_hintLabel) {
            m_fields->m_markerNode->clear();
            auto playerPos = m_player1->getPosition();
            bool nearAny = false;

            for (const auto& wp : g_waypoints) {
                // Яркий бирюзовый кружок в мире уровня
                m_fields->m_markerNode->drawDot({ wp.x, wp.y }, 10.f, { 0.0f, 1.0f, 0.8f, 0.9f });

                if (ccpDistance(playerPos, { wp.x, wp.y }) < 75.f) {
                    m_fields->m_hintLabel->setString(wp.text.c_str());
                    m_fields->m_hintLabel->setVisible(true);
                    nearAny = true;
                }
            }

            if (!nearAny) {
                m_fields->m_hintLabel->setVisible(false);
            }
        }
    }

    // Сохраняем координаты платформера при выходе из уровня
    void onQuit() {
        if (m_player1 && m_level && m_level->isPlatformer()) {
            auto pos = m_player1->getPosition();
            std::string keyX = fmt::format("plat_x_{}", m_level->m_levelID.value());
            std::string keyY = fmt::format("plat_y_{}", m_level->m_levelID.value());

            Mod::get()->setSavedValue<float>(keyX, pos.x);
            Mod::get()->setSavedValue<float>(keyY, pos.y);
            Mod::get()->saveData();
        }
        PlayLayer::onQuit();
    }

    // Сбрасываем сохранение, если игрок прошел платформер до конца
    void levelComplete() {
        if (m_level && m_level->isPlatformer()) {
            std::string keyX = fmt::format("plat_x_{}", m_level->m_levelID.value());
            std::string keyY = fmt::format("plat_y_{}", m_level->m_levelID.value());
            Mod::get()->setSavedValue<float>(keyX, 0.f);
            Mod::get()->setSavedValue<float>(keyY, 0.f);
            Mod::get()->saveData();
        }
        PlayLayer::levelComplete();
    }
};
