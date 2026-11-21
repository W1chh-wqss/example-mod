#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCLayer.hpp>

using namespace geode::prelude;

struct Waypoint {
    int id;
    float x;
    float y;
    std::string text;
};

static std::vector<Waypoint> g_waypoints;
static bool g_cameraEditMode = false;
static CCPoint g_freeCamOffset = { 0.f, 0.f };

class CameraEditorOverlay : public CCLayer {
public:
    CCMenu* m_topMenu = nullptr;
    CCLabelBMFont* m_modeLabel = nullptr;
    CCSprite* m_crosshair = nullptr;
    CCPoint m_lastTouch;
    bool m_isPanning = false;

    static CameraEditorOverlay* create() {
        auto ret = new CameraEditorOverlay();
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
        this->setTouchPriority(-600);
        this->setVisible(false);

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        m_crosshair = CCSprite::createWithSpriteFrameName("GJ_plusBtn_001.png");
        m_crosshair->setPosition({ winSize.width / 2.f, winSize.height / 2.f });
        m_crosshair->setScale(0.65f);
        m_crosshair->setOpacity(180);
        this->addChild(m_crosshair);

        m_modeLabel = CCLabelBMFont::create("CAMERA MODE: DRAG TO MOVE", "goldFont.fnt");
        m_modeLabel->setPosition({ winSize.width / 2.f, winSize.height - 25.f });
        m_modeLabel->setScale(0.55f);
        this->addChild(m_modeLabel);

        m_topMenu = CCMenu::create();
        m_topMenu->setPosition({ 0, 0 });
        m_topMenu->setTouchPriority(-700);
        this->addChild(m_topMenu);

        auto addSpr = ButtonSprite::create("Add Mark", "goldFont.fnt", "GJ_button_01.png", 0.75f);
        auto addBtn = CCMenuItemSpriteExtra::create(addSpr, this, menu_selector(CameraEditorOverlay::onAddMark));
        addBtn->setPosition({ winSize.width / 2.f - 75.f, 45.f });
        m_topMenu->addChild(addBtn);

        auto delSpr = ButtonSprite::create("Remove", "goldFont.fnt", "GJ_button_06.png", 0.75f);
        auto delBtn = CCMenuItemSpriteExtra::create(delSpr, this, menu_selector(CameraEditorOverlay::onRemoveMark));
        delBtn->setPosition({ winSize.width / 2.f + 75.f, 45.f });
        m_topMenu->addChild(delBtn);

        return true;
    }

    bool ccTouchBegan(CCTouch* touch, CCEvent* event) override {
        if (!this->isVisible() || !g_cameraEditMode) return false;
        m_lastTouch = touch->getLocation();
        m_isPanning = true;
        return true;
    }

    void ccTouchMoved(CCTouch* touch, CCEvent* event) override {
        if (!m_isPanning || !g_cameraEditMode) return;
        auto pos = touch->getLocation();
        auto delta = ccpSub(pos, m_lastTouch);
        m_lastTouch = pos;

        g_freeCamOffset.x -= delta.x;
        g_freeCamOffset.y -= delta.y;
        if (g_freeCamOffset.x < 0.f) g_freeCamOffset.x = 0.f;
    }

    void ccTouchEnded(CCTouch* touch, CCEvent* event) override {
        m_isPanning = false;
    }

    void onAddMark(CCObject*) {
        auto pl = PlayLayer::get();
        if (!pl || !pl->m_objectLayer) return;

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        CCPoint screenCenter = { winSize.width / 2.f, winSize.height / 2.f };
        CCPoint worldPos = pl->m_objectLayer->convertToNodeSpace(screenCenter);

        int newId = static_cast<int>(g_waypoints.size()) + 1;
        std::string hintText = fmt::format("MARK #{}: JUMP / ACTION!", newId);
        g_waypoints.push_back({ newId, worldPos.x, worldPos.y, hintText });

        Notification::create(fmt::format("Mark #{} placed!", newId), NotificationIcon::Success)->show();
    }

    void onRemoveMark(CCObject*) {
        if (!g_waypoints.empty()) {
            int removedId = g_waypoints.back().id;
            g_waypoints.pop_back();
            Notification::create(fmt::format("Mark #{} removed!", removedId), NotificationIcon::Info)->show();
        } else {
            Notification::create("No marks to remove!", NotificationIcon::Warning)->show();
        }
    }
};

class FloatingModButton : public CCLayer {
public:
    CCSprite* m_icon = nullptr;
    bool m_isDragging = false;
    CCPoint m_touchStart;
    CCPoint m_nodeStart;

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
        this->setTouchPriority(-1000);

        auto winSize = CCDirector::sharedDirector()->getWinSize();

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
        box.origin.x -= 12.f;
        box.origin.y -= 12.f;
        box.size.width += 24.f;
        box.size.height += 24.f;

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
            g_cameraEditMode = !g_cameraEditMode;

            if (auto pl = PlayLayer::get()) {
                if (g_cameraEditMode) {
                    if (pl->m_player1) {
                        g_freeCamOffset = pl->m_player1->getPosition();
                    }
                    if (auto pauseLayer = CCDirector::sharedDirector()->getRunningScene()->getChildByType<PauseLayer>(0)) {
                        pauseLayer->removeFromParentAndCleanup(true);
                    }
                    Notification::create("Mark Editor ON", NotificationIcon::Success)->show();
                } else {
                    Notification::create("Mark Editor OFF", NotificationIcon::Info)->show();
                }
            }
        }
        m_isDragging = false;
    }
};

class $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        CCLabelBMFont* m_hintLabel = nullptr;
        CCDrawNode* m_markerNode = nullptr;
        CCDrawNode* m_progressBarMarks = nullptr;
        FloatingModButton* m_floatBtn = nullptr;
        CameraEditorOverlay* m_camOverlay = nullptr;
        CCNode* m_labelsContainer = nullptr;
        float m_saveTimer = 0.f;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        g_cameraEditMode = false;
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        m_fields->m_hintLabel = CCLabelBMFont::create("", "bigFont.fnt");
        m_fields->m_hintLabel->setPosition({ winSize.width / 2, winSize.height - 55.f });
        m_fields->m_hintLabel->setScale(0.45f);
        m_fields->m_hintLabel->setVisible(false);
        this->addChild(m_fields->m_hintLabel, 100);

        m_fields->m_markerNode = CCDrawNode::create();
        m_fields->m_labelsContainer = CCNode::create();
        if (m_objectLayer) {
            m_objectLayer->addChild(m_fields->m_markerNode, 998);
            m_objectLayer->addChild(m_fields->m_labelsContainer, 999);
        }

        m_fields->m_progressBarMarks = CCDrawNode::create();
        if (m_progressBar) {
            m_progressBar->addChild(m_fields->m_progressBarMarks, 50);
        }

        m_fields->m_camOverlay = CameraEditorOverlay::create();
        this->addChild(m_fields->m_camOverlay, 110);

        m_fields->m_floatBtn = FloatingModButton::create();
        this->addChild(m_fields->m_floatBtn, 200);

        return true;
    }

    void resetLevel() {
        PlayLayer::resetLevel();

        if (m_player1 && m_level && m_level->isPlatformer()) {
            std::string keyX = fmt::format("plat_x_{}", m_level->m_levelID.value());
            std::string keyY = fmt::format("plat_y_{}", m_level->m_levelID.value());

            if (Mod::get()->hasSavedValue(keyX) && Mod::get()->hasSavedValue(keyY)) {
                float savedX = Mod::get()->getSavedValue<float>(keyX);
                float savedY = Mod::get()->getSavedValue<float>(keyY);
                if (savedX > 10.f) {
                    m_player1->setPosition({ savedX, savedY });
                    m_player1->m_position = { savedX, savedY };
                    m_player1->m_startPosition = { savedX, savedY };
                    Notification::create("Platformer Saved Pos Loaded!", NotificationIcon::Success)->show();
                }
            }
        }
    }

    void postUpdate(float dt) {
        if (g_cameraEditMode && m_objectLayer) {
            auto winSize = CCDirector::sharedDirector()->getWinSize();
            m_objectLayer->setPosition({
                winSize.width / 2.f - g_freeCamOffset.x,
                winSize.height / 2.f - g_freeCamOffset.y
            });
            if (m_fields->m_camOverlay) m_fields->m_camOverlay->setVisible(true);
        } else {
            if (m_fields->m_camOverlay) m_fields->m_camOverlay->setVisible(false);
            PlayLayer::postUpdate(dt);
        }

        if (!m_player1 || !m_level) return;

        if (m_level->isPlatformer() && !g_cameraEditMode) {
            m_fields->m_saveTimer += dt;
            if (m_fields->m_saveTimer >= 2.0f) {
                m_fields->m_saveTimer = 0.f;
                auto pos = m_player1->getPosition();
                if (pos.x > 10.f) {
                    Mod::get()->setSavedValue<float>(fmt::format("plat_x_{}", m_level->m_levelID.value()), pos.x);
                    Mod::get()->setSavedValue<float>(fmt::format("plat_y_{}", m_level->m_levelID.value()), pos.y);
                }
            }
        }

        if (m_fields->m_markerNode && m_fields->m_labelsContainer) {
            m_fields->m_markerNode->clear();
            m_fields->m_labelsContainer->removeAllChildren();

            auto playerPos = m_player1->getPosition();
            bool nearAny = false;

            for (const auto& wp : g_waypoints) {
                CCPoint pts[4] = {
                    { wp.x, wp.y + 16.f },
                    { wp.x + 16.f, wp.y },
                    { wp.x, wp.y - 16.f },
                    { wp.x - 16.f, wp.y }
                };
                m_fields->m_markerNode->drawPolygon(pts, 4, { 0.0f, 0.9f, 1.0f, 0.35f }, 1.5f, { 1.0f, 0.85f, 0.0f, 1.0f });
                m_fields->m_markerNode->drawDot({ wp.x, wp.y }, 4.f, { 1.0f, 1.0f, 1.0f, 1.0f });

                auto idLbl = CCLabelBMFont::create(fmt::format("#{}", wp.id).c_str(), "goldFont.fnt");
                idLbl->setPosition({ wp.x, wp.y + 24.f });
                idLbl->setScale(0.45f);
                m_fields->m_labelsContainer->addChild(idLbl);

                if (!g_cameraEditMode && ccpDistance(playerPos, { wp.x, wp.y }) < 85.f) {
                    m_fields->m_hintLabel->setString(wp.text.c_str());
                    m_fields->m_hintLabel->setVisible(true);
                    nearAny = true;
                }
            }

            if (!nearAny && m_fields->m_hintLabel) {
                m_fields->m_hintLabel->setVisible(false);
            }
        }

        if (m_fields->m_progressBarMarks && m_progressBar && m_levelLength > 0.f) {
            m_fields->m_progressBarMarks->clear();
            auto barSize = m_progressBar->getContentSize();
            for (const auto& wp : g_waypoints) {
                float pct = std::clamp(wp.x / m_levelLength, 0.f, 1.f);
                float barX = pct * barSize.width;
                m_fields->m_progressBarMarks->drawSegment(
                    { barX, 0.f }, { barX, barSize.height }, 2.f, { 1.0f, 0.85f, 0.0f, 1.0f }
                );
            }
        }
    }

    void onQuit() {
        g_cameraEditMode = false;
        if (m_player1 && m_level && m_level->isPlatformer()) {
            auto pos = m_player1->getPosition();
            if (pos.x > 10.f) {
                Mod::get()->setSavedValue<float>(fmt::format("plat_x_{}", m_level->m_levelID.value()), pos.x);
                Mod::get()->setSavedValue<float>(fmt::format("plat_y_{}", m_level->m_levelID.value()), pos.y);
                Mod::get()->saveData();
            }
        }
        PlayLayer::onQuit();
    }

    void levelComplete() {
        g_cameraEditMode = false;
        if (m_level && m_level->isPlatformer()) {
            Mod::get()->setSavedValue<float>(fmt::format("plat_x_{}", m_level->m_levelID.value()), 0.f);
            Mod::get()->setSavedValue<float>(fmt::format("plat_y_{}", m_level->m_levelID.value()), 0.f);
            Mod::get()->saveData();
        }
        PlayLayer::levelComplete();
    }
};

