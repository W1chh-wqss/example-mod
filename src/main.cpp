#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>

using namespace geode::prelude;

// Структура для хранения одной метки
struct Waypoint {
    float x;
    float y;
    std::string text;
};

// Глобальный список меток для текущей сессии
static std::vector<Waypoint> g_waypoints;

// 1. Логика в самой игре (отрисовка меток и проверка расстояния)
class $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        CCLabelBMFont* m_hintLabel = nullptr;
        CCDrawNode* m_markerNode = nullptr;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        // Создаем текст подсказки сверху экрана
        m_fields->m_hintLabel = CCLabelBMFont::create("", "bigFont.fnt");
        m_fields->m_hintLabel->setPosition({ winSize.width / 2, winSize.height - 45.f });
        m_fields->m_hintLabel->setScale(0.45f);
        m_fields->m_hintLabel->setVisible(false);
        this->addChild(m_fields->m_hintLabel, 100);

        // Создаем слой для отрисовки кружков-меток в мире уровня
        m_fields->m_markerNode = CCDrawNode::create();
        if (m_objectLayer) {
            m_objectLayer->addChild(m_fields->m_markerNode, 999);
        }

        return true;
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        if (!m_player1 || !m_fields->m_markerNode || !m_fields->m_hintLabel) return;

        m_fields->m_markerNode->clear();
        auto playerPos = m_player1->getPosition();
        bool nearAny = false;

        for (const auto& wp : g_waypoints) {
            // Рисуем яркий бирюзовый кружок на координатах метки
            m_fields->m_markerNode->drawDot({ wp.x, wp.y }, 12.f, { 0.0f, 1.0f, 0.8f, 0.8f });

            // Проверяем, находится ли игрок рядом с меткой (радиус 75 пикселей)
            float dist = ccpDistance(playerPos, { wp.x, wp.y });
            if (dist < 75.f) {
                m_fields->m_hintLabel->setString(wp.text.c_str());
                m_fields->m_hintLabel->setVisible(true);
                nearAny = true;
            }
        }

        if (!nearAny) {
            m_fields->m_hintLabel->setVisible(false);
        }
    }
};

// 2. Кнопки в хотбаре (меню паузы) для добавления и удаления меток
class $modify(MyPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto menu = CCMenu::create();
        menu->setPosition({ 0, 0 });
        this->addChild(menu, 100);

        // Кнопка "Поставить метку"
        auto addSpr = ButtonSprite::create("+ Метка", "goldFont.fnt", "GJ_button_01.png", 0.7f);
        auto addBtn = CCMenuItemSpriteExtra::create(
            addSpr, this, menu_selector(MyPauseLayer::onAddWaypoint)
        );
        addBtn->setPosition({ winSize.width - 75.f, winSize.height / 2 + 25.f });
        menu->addChild(addBtn);

        // Кнопка "Убрать метки"
        auto clearSpr = ButtonSprite::create("Убрать", "goldFont.fnt", "GJ_button_06.png", 0.7f);
        auto clearBtn = CCMenuItemSpriteExtra::create(
            clearSpr, this, menu_selector(MyPauseLayer::onClearWaypoints)
        );
        clearBtn->setPosition({ winSize.width - 75.f, winSize.height / 2 - 25.f });
        menu->addChild(clearBtn);
    }

    void onAddWaypoint(CCObject*) {
        if (auto pl = PlayLayer::get()) {
            if (pl->m_player1) {
                auto pos = pl->m_player1->getPosition();
                g_waypoints.push_back({ pos.x, pos.y, "Действие: Прыгай здесь!" });
                FLAlertLayer::create("Метка", "Метка установлена на ваших координатах!", "ОК")->show();
            }
        }
    }

    void onClearWaypoints(CCObject*) {
        g_waypoints.clear();
        FLAlertLayer::create("Метка", "Все метки на уровне удалены.", "ОК")->show();
    }
};
