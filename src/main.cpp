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

