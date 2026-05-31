#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>

#include "ReplayBot.hpp"
#include "UI.hpp"

using namespace geode::prelude;

// ═══════════════════════════════════════════════════════════
// Mod entry — $execute runs once when the mod is loaded
// ═══════════════════════════════════════════════════════════

$execute {
    ReplayBot::get().init();
    log::info("[ReplayBot] Mod loaded successfully.");
}

// ═══════════════════════════════════════════════════════════
// Inject button into the Pause Menu (in-level access)
// ═══════════════════════════════════════════════════════════

class $modify(RBPauseLayer, PauseLayer) {

    void customSetup() {
        PauseLayer::customSetup();

        auto* menu = this->getChildByID("left-button-menu");
        if (!menu) {
            // Fallback: find any CCMenu
            menu = dynamic_cast<CCMenu*>(
                this->getChildByType<CCMenu>(0)
            );
        }
        if (!menu) return;

        // Create bot button
        auto* iconSpr = CCSprite::createWithSpriteFrameName(
            "GJ_editBtn_001.png"
        );
        iconSpr->setScale(0.7f);

        auto* btn = CCMenuItemSpriteExtra::create(
            iconSpr,
            this,
            menu_selector(RBPauseLayer::onOpenBot)
        );
        btn->setID("replay-bot-button");

        menu->addChild(btn);
        menu->updateLayout();
    }

    void onOpenBot(CCObject*) {
        auto* popup = ReplayBotLayer::create();
        if (popup) popup->show();
    }
};

// ═══════════════════════════════════════════════════════════
// Inject button into the main MenuLayer (quick access)
// ═══════════════════════════════════════════════════════════

class $modify(RBMenuLayer, MenuLayer) {

    bool init() {
        if (!MenuLayer::init()) return false;

        auto* menu = this->getChildByID("bottom-menu");
        if (!menu) return true;

        auto* iconBg = CCScale9Sprite::create("GJ_button_04.png");
        iconBg->setContentSize({ 34.0f, 34.0f });
        auto* iconLbl = CCLabelBMFont::create("RB", "bigFont.fnt");
        iconLbl->setScale(0.5f);
        iconLbl->setPosition(iconBg->getContentSize() / 2);
        iconBg->addChild(iconLbl);

        auto* btn = CCMenuItemSpriteExtra::create(
            iconBg,
            this,
            menu_selector(RBMenuLayer::onOpenBot)
        );
        btn->setID("replay-bot-main-button");
        menu->addChild(btn);
        menu->updateLayout();

        return true;
    }

    void onOpenBot(CCObject*) {
        auto* popup = ReplayBotLayer::create();
        if (popup) popup->show();
    }
};

// ═══════════════════════════════════════════════════════════
// PHASE 1 — C++ FUNDAMENTALS ILLUSTRATED
// These snippets show each concept as it's used in this mod:
// ═══════════════════════════════════════════════════════════

/*
────────────────────────────────────────────────────────────
1. MEMORY MANAGEMENT
────────────────────────────────────────────────────────────
// Raw pointer (Cocos2d ref-counted, handled via autorelease):
CCDrawNode* trajectoryNode = CCDrawNode::create();
parent->addChild(trajectoryNode);   // parent retains it
trajectoryNode->release();          // we release our hold

// Smart pointer (for non-Cocos objects owned by us):
std::unique_ptr<ReplayInput[]> buffer(new ReplayInput[1024]);

// Shared across systems:
std::shared_ptr<std::vector<ReplayInput>> sharedInputs =
    std::make_shared<std::vector<ReplayInput>>();

────────────────────────────────────────────────────────────
2. CLASSES & INHERITANCE (GD objects)
────────────────────────────────────────────────────────────
// GD object hierarchy:
// CCObject → CCNode → CCLayer → GJBaseGameLayer → PlayLayer
// We hook with $modify which creates a derived class:

class $modify(MyPlayLayer, PlayLayer) {
    void update(float dt) {
        // This override intercepts PlayLayer::update
        PlayLayer::update(dt); // call original
    }
};

────────────────────────────────────────────────────────────
3. VECTORS & STRUCTS
────────────────────────────────────────────────────────────
struct ReplayInput { int frame; bool holding; int button; bool player2; };
std::vector<ReplayInput> inputs;
inputs.reserve(4096);                        // pre-allocate
inputs.push_back({ 42, true, 1, false });    // add

// Range-for iteration:
for (const auto& inp : inputs)
    if (inp.frame == currentFrame)
        executeInput(inp);

────────────────────────────────────────────────────────────
4. FILE I/O
────────────────────────────────────────────────────────────
// Binary write:
std::ofstream f("macro.replay", std::ios::binary);
f.write("RBOT", 4);                          // magic
uint32_t count = inputs.size();
f.write(reinterpret_cast<const char*>(&count), 4);
for (auto& inp : inputs)
    f.write(reinterpret_cast<const char*>(&inp), sizeof(inp));

// Binary read:
std::ifstream r("macro.replay", std::ios::binary);
char magic[4]; r.read(magic, 4);
uint32_t n;    r.read(reinterpret_cast<char*>(&n), 4);

────────────────────────────────────────────────────────────
5. LAMBDAS & CALLBACKS
────────────────────────────────────────────────────────────
// Geode event listener via lambda:
listenForAllEvents<SomeEvent>([](SomeEvent* e) {
    ReplayBot::get().handleEvent(e);
});

// CCSchedule callback stored as lambda-compatible SEL:
this->schedule(schedule_selector(MyLayer::onTick), 0.1f);

────────────────────────────────────────────────────────────
6. ENUMS — bot state machine
────────────────────────────────────────────────────────────
enum class BotState { IDLE, RECORDING, PLAYING };

// Transition:
// IDLE  --(startRecording)--> RECORDING
// RECORDING --(stop/quit)---> IDLE (auto-saves)
// IDLE  --(startPlaying)----> PLAYING
// PLAYING --(stop/end)------> IDLE
*/
