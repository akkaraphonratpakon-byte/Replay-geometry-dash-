#include "UI.hpp"
#include <Geode/Geode.hpp>
#include <filesystem>
#include <fmt/format.h>

using namespace geode::prelude;
namespace fs = std::filesystem;

// ═══════════════════════════════════════════════════════════
// RBUI helpers
// ═══════════════════════════════════════════════════════════

namespace RBUI {

CCMenuItemToggler* makeToggle(
    CCObject* target,
    SEL_MenuHandler callback,
    bool initialState)
{
    auto* off = CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png");
    auto* on  = CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");
    off->setScale(0.7f);
    on ->setScale(0.7f);

    auto* tog = CCMenuItemToggler::create(off, on, target, callback);
    tog->toggle(initialState);
    return tog;
}

CCMenuItemSpriteExtra* makeGearBtn(
    CCObject* target,
    SEL_MenuHandler callback)
{
    auto* spr = CCSprite::createWithSpriteFrameName("GJ_optionsBtn02_001.png");
    spr->setScale(0.55f);
    return CCMenuItemSpriteExtra::create(spr, target, callback);
}

CCLabelBMFont* makeLabel(
    const std::string& text,
    const std::string& font,
    float scale)
{
    auto* lbl = CCLabelBMFont::create(text.c_str(), font.c_str());
    lbl->setScale(scale);
    return lbl;
}

CCNode* makeFeatureRow(
    const std::string& label,
    CCObject* target,
    SEL_MenuHandler toggleCb,
    bool state,
    CCObject* gearTarget,
    SEL_MenuHandler gearCb)
{
    auto* row  = CCNode::create();
    auto* menu = CCMenu::create();
    menu->setPosition({ 0, 0 });

    auto* lbl  = makeLabel(label, "bigFont.fnt", 0.35f);
    lbl->setAnchorPoint({ 0, 0.5f });
    lbl->setPosition({ 0, 0 });
    row->addChild(lbl);

    auto* tog  = makeToggle(target, toggleCb, state);
    tog->setPositionX(100.0f);
    menu->addChild(tog);

    if (gearTarget && gearCb) {
        auto* gear = makeGearBtn(gearTarget, gearCb);
        gear->setPositionX(122.0f);
        menu->addChild(gear);
    }

    row->addChild(menu);
    return row;
}

} // namespace RBUI

// ═══════════════════════════════════════════════════════════
// ReplayBotLayer
// ═══════════════════════════════════════════════════════════

ReplayBotLayer* ReplayBotLayer::create() {
    auto* ret = new ReplayBotLayer();
    if (ret && ret->initAnchored(360.0f, 300.0f)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool ReplayBotLayer::setup() {
    auto* bg = m_mainLayer;
    float w  = m_mainLayer->getContentSize().width;
    float h  = m_mainLayer->getContentSize().height;

    this->setTitle("★ REPLAY BOT");

    buildHeader(bg, w, h);
    buildMainButtons(bg, w, h);
    buildSpeedRow(bg, w, h);
    buildFeatureGrid(bg, w, h);
    buildStatusBar(bg, w, h);
    scheduleRefresh();
    return true;
}

// ── Header ─────────────────────────────────────────────────

void ReplayBotLayer::buildHeader(CCNode* parent, float w, float h) {
    m_stateLabel = RBUI::makeLabel("● IDLE", "bigFont.fnt", 0.38f);
    m_stateLabel->setPosition({ w - 55.0f, h - 18.0f });
    m_stateLabel->setColor({ 100, 220, 100 });
    parent->addChild(m_stateLabel, 2);
}

// ── Main buttons ───────────────────────────────────────────

void ReplayBotLayer::buildMainButtons(CCNode* parent, float w, float h) {
    auto* menu = CCMenu::create();
    menu->setPosition({ 0, 0 });

    // Helper: colored button
    auto makeBtn = [](const std::string& text,
                      CCObject* target, SEL_MenuHandler cb,
                      ccColor3B col) -> CCMenuItemSpriteExtra* {
        auto* bg  = CCScale9Sprite::create("GJ_button_01.png");
        bg->setContentSize({ 80.0f, 28.0f });
        bg->setColor(col);
        auto* lbl = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
        lbl->setScale(0.45f);
        lbl->setPosition(bg->getContentSize() / 2);
        bg->addChild(lbl);
        return CCMenuItemSpriteExtra::create(bg, target, cb);
    };

    float row1Y = h - 55.0f;
    float row2Y = h - 88.0f;
    float col1  = w * 0.27f;
    float col2  = w * 0.73f;

    // Row 1
    auto* recBtn  = makeBtn("⏺ RECORD",   this,
        menu_selector(ReplayBotLayer::onRecord),  { 220, 80,  80  });
    auto* playBtn = makeBtn("▶ PLAY",      this,
        menu_selector(ReplayBotLayer::onPlay),    { 80,  200, 80  });
    recBtn ->setPosition({ col1, row1Y });
    playBtn->setPosition({ col2, row1Y });
    menu->addChild(recBtn);
    menu->addChild(playBtn);

    // Row 2
    auto* loadBtn = makeBtn("📂 LOAD",     this,
        menu_selector(ReplayBotLayer::onLoadMacro), { 100, 160, 220 });
    auto* stopBtn = makeBtn("■ STOP",      this,
        menu_selector(ReplayBotLayer::onStop),      { 200, 140, 60  });
    loadBtn->setPosition({ col1, row2Y });
    stopBtn->setPosition({ col2, row2Y });
    menu->addChild(loadBtn);
    menu->addChild(stopBtn);

    parent->addChild(menu, 2);
}

// ── Speed row ──────────────────────────────────────────────

void ReplayBotLayer::buildSpeedRow(CCNode* parent, float w, float h) {
    float y = h - 118.0f;

    auto* lbl = RBUI::makeLabel("Speed", "bigFont.fnt", 0.38f);
    lbl->setAnchorPoint({ 0, 0.5f });
    lbl->setPosition({ 20.0f, y });
    parent->addChild(lbl, 2);

    // Slider 0.1 → 4.0  mapped 0 → 1
    m_speedSlider = Slider::create(
        this,
        menu_selector(ReplayBotLayer::onSpeedSlider),
        0.8f
    );
    m_speedSlider->setPosition({ w / 2 + 10.0f, y });
    m_speedSlider->setValue(
        (ReplayBot::get().speedMultiplier - 0.1f) / 3.9f
    );
    parent->addChild(m_speedSlider, 2);

    m_speedLabel = RBUI::makeLabel("1.0x", "bigFont.fnt", 0.36f);
    m_speedLabel->setAnchorPoint({ 0, 0.5f });
    m_speedLabel->setPosition({ w - 44.0f, y });
    parent->addChild(m_speedLabel, 2);

    // Gear — exact value input
    auto* menu = CCMenu::create();
    menu->setPosition({ 0, 0 });
    auto* gear = RBUI::makeGearBtn(this, menu_selector(ReplayBotLayer::onSpeedhackGear));
    gear->setPosition({ w - 18.0f, y });
    menu->addChild(gear);
    parent->addChild(menu, 2);
}

// ── Feature grid ───────────────────────────────────────────

void ReplayBotLayer::buildFeatureGrid(CCNode* parent, float w, float h) {
    // Two columns of feature rows
    struct FeatureDef {
        std::string label;
        bool*       flag;
        SEL_MenuHandler toggleCb;
        CCObject*   gearTarget;
        SEL_MenuHandler gearCb;
    };

    auto& bot = ReplayBot::get();

    // We'll build menus per column
    auto* menu = CCMenu::create();
    menu->setPosition({ 0, 0 });

    float startY = h - 148.0f;
    float rowH   = 20.0f;
    float col1X  = 20.0f;
    float col2X  = w / 2 + 10.0f;

    // ── Left column ─────────────────────────────────────
    // Noclip
    {
        auto* row = RBUI::makeFeatureRow(
            "Noclip", this,
            menu_selector(ReplayBotLayer::onNoclip),
            bot.noclip);
        row->setPosition({ col1X, startY });
        parent->addChild(row, 2);
    }
    // Trajectory
    {
        auto* row = RBUI::makeFeatureRow(
            "Trajectory", this,
            menu_selector(ReplayBotLayer::onTrajectory),
            bot.showTrajectory);
        row->setPosition({ col1X, startY - rowH });
        parent->addChild(row, 2);
    }
    // Layout Mode
    {
        auto* row = RBUI::makeFeatureRow(
            "Layout Mode", this,
            menu_selector(ReplayBotLayer::onLayout),
            bot.layoutMode);
        row->setPosition({ col1X, startY - rowH * 2 });
        parent->addChild(row, 2);
    }
    // Frame Stepper (with gear)
    {
        auto* row = RBUI::makeFeatureRow(
            "Frame Step", this,
            menu_selector(ReplayBotLayer::onFrameStep),
            bot.frameStepMode,
            this,
            menu_selector(ReplayBotLayer::onFrameStepGear)
        );
        row->setPosition({ col1X, startY - rowH * 3 });
        parent->addChild(row, 2);
    }
    // Loop Playback
    {
        auto* row = RBUI::makeFeatureRow(
            "Loop Play", this,
            menu_selector(ReplayBotLayer::onLoop),
            bot.loopPlayback);
        row->setPosition({ col1X, startY - rowH * 4 });
        parent->addChild(row, 2);
    }

    // ── Right column ─────────────────────────────────────
    // Renderer (with gear)
    {
        auto* row = RBUI::makeFeatureRow(
            "Renderer", this,
            menu_selector(ReplayBotLayer::onRenderer),
            bot.renderer.enabled,
            this,
            menu_selector(ReplayBotLayer::onRendererGear)
        );
        row->setPosition({ col2X, startY });
        parent->addChild(row, 2);
    }
    // Safe Mode
    {
        auto* row = RBUI::makeFeatureRow(
            "Safe Mode", this,
            menu_selector(ReplayBotLayer::onSafeMode),
            bot.safeMode);
        row->setPosition({ col2X, startY - rowH });
        parent->addChild(row, 2);
    }
    // Instant Respawn
    {
        auto* row = RBUI::makeFeatureRow(
            "Inst. Resp.", this,
            menu_selector(ReplayBotLayer::onInstRespawn),
            bot.instantRespawn);
        row->setPosition({ col2X, startY - rowH * 2 });
        parent->addChild(row, 2);
    }
    // Seed (gear only, toggle on/off)
    {
        auto* row = RBUI::makeFeatureRow(
            "Seed Mod", this,
            // We use a lambda-style approach via a helper toggle
            menu_selector(ReplayBotLayer::onSafeMode), // placeholder, see note
            bot.seed.enabled,
            this,
            menu_selector(ReplayBotLayer::onSeedGear)
        );
        row->setPosition({ col2X, startY - rowH * 3 });
        parent->addChild(row, 2);
    }

    parent->addChild(menu, 2);
}

// ── Status bar ─────────────────────────────────────────────

void ReplayBotLayer::buildStatusBar(CCNode* parent, float w, float h) {
    float y = 22.0f;

    m_macroLabel = RBUI::makeLabel("Macro: none loaded", "chatFont.fnt", 0.55f);
    m_macroLabel->setAnchorPoint({ 0, 0.5f });
    m_macroLabel->setPosition({ 14.0f, y + 8.0f });
    parent->addChild(m_macroLabel, 2);

    m_frameLabel = RBUI::makeLabel("Frame: 0 / 0", "chatFont.fnt", 0.55f);
    m_frameLabel->setAnchorPoint({ 0, 0.5f });
    m_frameLabel->setPosition({ 14.0f, y - 4.0f });
    parent->addChild(m_frameLabel, 2);
}

// ── Refresh ────────────────────────────────────────────────

void ReplayBotLayer::scheduleRefresh() {
    this->schedule(
        schedule_selector(ReplayBotLayer::refreshCb),
        0.1f  // 10 Hz refresh
    );
}

void ReplayBotLayer::refreshCb(float) {
    refresh();
}

void ReplayBotLayer::refresh() {
    auto& bot = ReplayBot::get();

    // State badge
    std::string stateStr = "● " + bot.stateString();
    m_stateLabel->setString(stateStr.c_str());
    if (bot.state == BotState::RECORDING)
        m_stateLabel->setColor({ 220, 60, 60 });
    else if (bot.state == BotState::PLAYING)
        m_stateLabel->setColor({ 60, 200, 60 });
    else
        m_stateLabel->setColor({ 180, 180, 180 });

    // Frame counter
    std::string frameStr = fmt::format(
        "Frame: {} / {}",
        bot.getCurrentFrame(),
        bot.totalFrames
    );
    m_frameLabel->setString(frameStr.c_str());

    // Macro name
    std::string macroStr = "Macro: " +
        (bot.macroName.empty() ? "none loaded" : bot.macroName);
    m_macroLabel->setString(macroStr.c_str());

    // Speed label
    std::string speedStr = fmt::format("{:.1f}x", bot.speedMultiplier);
    m_speedLabel->setString(speedStr.c_str());
}

// ── Button callbacks ───────────────────────────────────────

void ReplayBotLayer::onRecord(CCObject*) {
    auto& bot = ReplayBot::get();
    if (bot.state == BotState::IDLE) bot.startRecording();
}

void ReplayBotLayer::onPlay(CCObject*) {
    auto& bot = ReplayBot::get();
    if (bot.state == BotState::IDLE) bot.startPlaying();
}

void ReplayBotLayer::onStop(CCObject*) {
    auto& bot = ReplayBot::get();
    if (bot.state == BotState::RECORDING) bot.stopRecording();
    if (bot.state == BotState::PLAYING)   bot.stopPlaying();
}

void ReplayBotLayer::onLoadMacro(CCObject*) {
    auto* popup = LoadMacroPopup::create();
    if (popup) popup->show();
}

void ReplayBotLayer::onNoclip(CCObject*) {
    ReplayBot::get().noclip = !ReplayBot::get().noclip;
}
void ReplayBotLayer::onTrajectory(CCObject*) {
    ReplayBot::get().showTrajectory = !ReplayBot::get().showTrajectory;
}
void ReplayBotLayer::onLayout(CCObject*) {
    auto& bot = ReplayBot::get();
    bot.layoutMode = !bot.layoutMode;
    bot.applyLayoutMode(bot.layoutMode);
}
void ReplayBotLayer::onFrameStep(CCObject*) {
    ReplayBot::get().frameStepMode = !ReplayBot::get().frameStepMode;
}
void ReplayBotLayer::onSafeMode(CCObject*) {
    ReplayBot::get().safeMode = !ReplayBot::get().safeMode;
}
void ReplayBotLayer::onRenderer(CCObject*) {
    ReplayBot::get().renderer.enabled = !ReplayBot::get().renderer.enabled;
}
void ReplayBotLayer::onInstRespawn(CCObject*) {
    ReplayBot::get().instantRespawn = !ReplayBot::get().instantRespawn;
}
void ReplayBotLayer::onLoop(CCObject*) {
    ReplayBot::get().loopPlayback = !ReplayBot::get().loopPlayback;
}

void ReplayBotLayer::onSeedGear(CCObject*) {
    auto* popup = SeedPopup::create();
    if (popup) popup->show();
}
void ReplayBotLayer::onRendererGear(CCObject*) {
    auto* popup = RendererPopup::create();
    if (popup) popup->show();
}
void ReplayBotLayer::onSpeedhackGear(CCObject*) {
    auto* popup = SpeedhackPopup::create();
    if (popup) popup->show();
}
void ReplayBotLayer::onFrameStepGear(CCObject*) {
    auto* popup = FrameStepperPopup::create();
    if (popup) popup->show();
}

void ReplayBotLayer::onSpeedSlider(CCObject* sender) {
    auto* slider = dynamic_cast<Slider*>(sender);
    if (!slider) return;
    // Map 0–1 → 0.1–4.0
    float val = slider->getValue();
    ReplayBot::get().speedMultiplier = 0.1f + val * 3.9f;
}

void ReplayBotLayer::onClose(CCObject* s) {
    this->unschedule(schedule_selector(ReplayBotLayer::refreshCb));
    Popup::onClose(s);
}

// ═══════════════════════════════════════════════════════════
// SeedPopup
// ═══════════════════════════════════════════════════════════

SeedPopup* SeedPopup::create() {
    auto* ret = new SeedPopup();
    if (ret && ret->initAnchored(220.0f, 130.0f)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool SeedPopup::setup() {
    this->setTitle("Seed Modifier");
    auto* bg = m_mainLayer;
    float w  = bg->getContentSize().width;
    float h  = bg->getContentSize().height;

    auto* lbl = RBUI::makeLabel("Seed:", "bigFont.fnt", 0.4f);
    lbl->setPosition({ w / 2 - 40.0f, h / 2 + 10.0f });
    bg->addChild(lbl);

    m_seedInput = InputNode::create(120.0f, "0");
    m_seedInput->setPosition({ w / 2 + 20.0f, h / 2 + 10.0f });
    m_seedInput->setString(
        std::to_string(ReplayBot::get().seed.seed).c_str()
    );
    bg->addChild(m_seedInput);

    auto* menu = CCMenu::create();
    menu->setPosition({ 0, 0 });

    auto makeSBtn = [&](const std::string& t, float x, float y,
                         SEL_MenuHandler cb) {
        auto* bg2 = CCScale9Sprite::create("GJ_button_01.png");
        bg2->setContentSize({ 70.0f, 24.0f });
        auto* lbl2 = CCLabelBMFont::create(t.c_str(), "bigFont.fnt");
        lbl2->setScale(0.4f);
        lbl2->setPosition(bg2->getContentSize() / 2);
        bg2->addChild(lbl2);
        auto* btn = CCMenuItemSpriteExtra::create(bg2, this, cb);
        btn->setPosition({ x, y });
        menu->addChild(btn);
    };

    makeSBtn("RANDOM", w / 2 - 40.0f, 28.0f,
             menu_selector(SeedPopup::onRandom));
    makeSBtn("APPLY",  w / 2 + 40.0f, 28.0f,
             menu_selector(SeedPopup::onApply));

    bg->addChild(menu);
    return true;
}

void SeedPopup::onRandom(CCObject*) {
    std::mt19937 rng{ std::random_device{}() };
    int s = static_cast<int>(rng());
    m_seedInput->setString(std::to_string(s).c_str());
}

void SeedPopup::onApply(CCObject*) {
    int s = std::atoi(m_seedInput->getString().c_str());
    ReplayBot::get().applySeed(s);
    ReplayBot::get().seed.enabled = true;
    this->onClose(nullptr);
}

// ═══════════════════════════════════════════════════════════
// RendererPopup
// ═══════════════════════════════════════════════════════════

RendererPopup* RendererPopup::create() {
    auto* ret = new RendererPopup();
    if (ret && ret->initAnchored(260.0f, 200.0f)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool RendererPopup::setup() {
    this->setTitle("Renderer Settings");
    auto& rs = ReplayBot::get().renderer;
    auto* bg = m_mainLayer;
    float w  = bg->getContentSize().width;
    float h  = bg->getContentSize().height;

    auto addRow = [&](const std::string& label,
                      InputNode*& outInput,
                      const std::string& defVal,
                      float y) {
        auto* lbl = RBUI::makeLabel(label, "bigFont.fnt", 0.35f);
        lbl->setAnchorPoint({ 0, 0.5f });
        lbl->setPosition({ 14.0f, y });
        bg->addChild(lbl);

        outInput = InputNode::create(80.0f, defVal.c_str());
        outInput->setPosition({ w - 60.0f, y });
        outInput->setString(defVal.c_str());
        bg->addChild(outInput);
    };

    float startY = h - 45.0f;
    float step   = 26.0f;

    addRow("FPS",     m_fpsInput,     std::to_string(rs.fps),     startY);
    addRow("Width",   m_widthInput,   std::to_string(rs.width),   startY - step);
    addRow("Height",  m_heightInput,  std::to_string(rs.height),  startY - step * 2);
    addRow("Bitrate", m_bitrateInput, std::to_string(rs.bitrate), startY - step * 3);

    auto* pathLbl = RBUI::makeLabel("Output", "bigFont.fnt", 0.35f);
    pathLbl->setAnchorPoint({ 0, 0.5f });
    pathLbl->setPosition({ 14.0f, startY - step * 4 });
    bg->addChild(pathLbl);

    m_pathInput = InputNode::create(120.0f, "replay_render.mp4");
    m_pathInput->setPosition({ w - 75.0f, startY - step * 4 });
    m_pathInput->setString(rs.outputPath.c_str());
    bg->addChild(m_pathInput);

    auto* menu = CCMenu::create();
    menu->setPosition({ 0, 0 });
    auto* applyBg = CCScale9Sprite::create("GJ_button_01.png");
    applyBg->setContentSize({ 80.0f, 24.0f });
    auto* applyLbl = CCLabelBMFont::create("APPLY", "bigFont.fnt");
    applyLbl->setScale(0.4f);
    applyLbl->setPosition(applyBg->getContentSize() / 2);
    applyBg->addChild(applyLbl);
    auto* applyBtn = CCMenuItemSpriteExtra::create(
        applyBg, this, menu_selector(RendererPopup::onApply));
    applyBtn->setPosition({ w / 2, 20.0f });
    menu->addChild(applyBtn);
    bg->addChild(menu);

    return true;
}

void RendererPopup::onApply(CCObject*) {
    auto& rs        = ReplayBot::get().renderer;
    rs.fps          = std::max(1, std::atoi(m_fpsInput->getString().c_str()));
    rs.width        = std::max(1, std::atoi(m_widthInput->getString().c_str()));
    rs.height       = std::max(1, std::atoi(m_heightInput->getString().c_str()));
    rs.bitrate      = std::max(100, std::atoi(m_bitrateInput->getString().c_str()));
    rs.outputPath   = m_pathInput->getString();
    log::info("[ReplayBot] Renderer settings updated: {}x{} @ {}fps, {}kbps -> {}",
              rs.width, rs.height, rs.fps, rs.bitrate, rs.outputPath);
    this->onClose(nullptr);
}

// ═══════════════════════════════════════════════════════════
// SpeedhackPopup
// ═══════════════════════════════════════════════════════════

SpeedhackPopup* SpeedhackPopup::create() {
    auto* ret = new SpeedhackPopup();
    if (ret && ret->initAnchored(200.0f, 100.0f)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool SpeedhackPopup::setup() {
    this->setTitle("Speedhack");
    auto* bg = m_mainLayer;
    float w  = bg->getContentSize().width;
    float h  = bg->getContentSize().height;

    auto* lbl = RBUI::makeLabel("Speed (0.1-4.0):", "bigFont.fnt", 0.38f);
    lbl->setPosition({ w / 2, h / 2 + 12.0f });
    bg->addChild(lbl);

    m_input = InputNode::create(100.0f, "1.0");
    m_input->setPosition({ w / 2, h / 2 - 8.0f });
    m_input->setString(
        fmt::format("{:.2f}", ReplayBot::get().speedMultiplier).c_str()
    );
    bg->addChild(m_input);

    auto* menu = CCMenu::create();
    menu->setPosition({ 0, 0 });
    auto* applyBg  = CCScale9Sprite::create("GJ_button_01.png");
    applyBg->setContentSize({ 70.0f, 22.0f });
    auto* applyLbl = CCLabelBMFont::create("APPLY", "bigFont.fnt");
    applyLbl->setScale(0.38f);
    applyLbl->setPosition(applyBg->getContentSize() / 2);
    applyBg->addChild(applyLbl);
    auto* btn = CCMenuItemSpriteExtra::create(
        applyBg, this, menu_selector(SpeedhackPopup::onApply));
    btn->setPosition({ w / 2, 16.0f });
    menu->addChild(btn);
    bg->addChild(menu);

    return true;
}

void SpeedhackPopup::onApply(CCObject*) {
    float val = std::atof(m_input->getString().c_str());
    val = std::clamp(val, 0.1f, 4.0f);
    ReplayBot::get().speedMultiplier = val;
    this->onClose(nullptr);
}

// ═══════════════════════════════════════════════════════════
// FrameStepperPopup
// ═══════════════════════════════════════════════════════════

FrameStepperPopup* FrameStepperPopup::create() {
    auto* ret = new FrameStepperPopup();
    if (ret && ret->initAnchored(220.0f, 110.0f)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool FrameStepperPopup::setup() {
    this->setTitle("Frame Stepper");
    auto* bg = m_mainLayer;
    float w  = bg->getContentSize().width;
    float h  = bg->getContentSize().height;

    auto* lbl = RBUI::makeLabel("Frames per [F] key:", "bigFont.fnt", 0.38f);
    lbl->setPosition({ w / 2, h / 2 + 14.0f });
    bg->addChild(lbl);

    auto* hint = RBUI::makeLabel("Press F to step while paused", "chatFont.fnt", 0.48f);
    hint->setPosition({ w / 2, h / 2 + 2.0f });
    hint->setColor({ 160, 160, 160 });
    bg->addChild(hint);

    m_input = InputNode::create(80.0f, "1");
    m_input->setPosition({ w / 2, h / 2 - 14.0f });
    m_input->setString(std::to_string(ReplayBot::get().stepTarget).c_str());
    bg->addChild(m_input);

    auto* menu = CCMenu::create();
    menu->setPosition({ 0, 0 });
    auto* applyBg  = CCScale9Sprite::create("GJ_button_01.png");
    applyBg->setContentSize({ 70.0f, 22.0f });
    auto* applyLbl = CCLabelBMFont::create("APPLY", "bigFont.fnt");
    applyLbl->setScale(0.38f);
    applyLbl->setPosition(applyBg->getContentSize() / 2);
    applyBg->addChild(applyLbl);
    auto* btn = CCMenuItemSpriteExtra::create(
        applyBg, this, menu_selector(FrameStepperPopup::onApply));
    btn->setPosition({ w / 2, 16.0f });
    menu->addChild(btn);
    bg->addChild(menu);

    return true;
}

void FrameStepperPopup::onApply(CCObject*) {
    int val = std::max(1, std::atoi(m_input->getString().c_str()));
    ReplayBot::get().stepTarget = val;
    this->onClose(nullptr);
}

// ═══════════════════════════════════════════════════════════
// SaveMacroPopup
// ═══════════════════════════════════════════════════════════

SaveMacroPopup* SaveMacroPopup::create() {
    auto* ret = new SaveMacroPopup();
    if (ret && ret->initAnchored(240.0f, 140.0f)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool SaveMacroPopup::setup() {
    this->setTitle("Save Macro");
    auto* bg = m_mainLayer;
    float w  = bg->getContentSize().width;
    float h  = bg->getContentSize().height;

    auto* nameLbl = RBUI::makeLabel("Name:", "bigFont.fnt", 0.4f);
    nameLbl->setAnchorPoint({ 0, 0.5f });
    nameLbl->setPosition({ 14.0f, h - 50.0f });
    bg->addChild(nameLbl);

    // Auto-fill with level name
    std::string autoName = ReplayBot::get().resolveMacroName("");
    m_nameInput = InputNode::create(150.0f, autoName.c_str());
    m_nameInput->setPosition({ w - 80.0f, h - 50.0f });
    m_nameInput->setString(autoName.c_str());
    bg->addChild(m_nameInput);

    m_fileLabel = RBUI::makeLabel(
        "File: " + RBUtil::sanitizeFilename(autoName) + ".replay",
        "chatFont.fnt", 0.5f
    );
    m_fileLabel->setPosition({ w / 2, h - 75.0f });
    m_fileLabel->setColor({ 160, 160, 255 });
    bg->addChild(m_fileLabel);

    auto* menu = CCMenu::create();
    menu->setPosition({ 0, 0 });

    auto makeBtn2 = [&](const std::string& t, float x, SEL_MenuHandler cb) {
        auto* s = CCScale9Sprite::create("GJ_button_01.png");
        s->setContentSize({ 75.0f, 24.0f });
        auto* l = CCLabelBMFont::create(t.c_str(), "bigFont.fnt");
        l->setScale(0.4f);
        l->setPosition(s->getContentSize() / 2);
        s->addChild(l);
        auto* b = CCMenuItemSpriteExtra::create(s, this, cb);
        b->setPosition({ x, 22.0f });
        menu->addChild(b);
    };

    makeBtn2("CANCEL", w / 2 - 45.0f,
             menu_selector(SaveMacroPopup::onClose));
    makeBtn2("SAVE",   w / 2 + 45.0f,
             menu_selector(SaveMacroPopup::onSave));

    bg->addChild(menu);
    return true;
}

void SaveMacroPopup::updateFileLabel(const std::string& name) {
    std::string fname = RBUtil::sanitizeFilename(name) + ".replay";
    m_fileLabel->setString(("File: " + fname).c_str());
}

void SaveMacroPopup::onNameChanged(CCObject*) {
    updateFileLabel(m_nameInput->getString());
}

void SaveMacroPopup::onSave(CCObject*) {
    std::string name = m_nameInput->getString();
    name = ReplayBot::get().resolveMacroName(name);
    std::string path = RBUtil::getSaveDir() + "/" +
                       RBUtil::sanitizeFilename(name) + ".replay";
    ReplayBot::get().saveMacro(path, name);
    this->onClose(nullptr);
}

// ═══════════════════════════════════════════════════════════
// LoadMacroPopup
// ═══════════════════════════════════════════════════════════

LoadMacroPopup* LoadMacroPopup::create() {
    auto* ret = new LoadMacroPopup();
    if (ret && ret->initAnchored(260.0f, 220.0f)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool LoadMacroPopup::setup() {
    this->setTitle("Load Macro");
    auto* bg = m_mainLayer;
    float w  = bg->getContentSize().width;
    float h  = bg->getContentSize().height;

    // Scan save dir for .replay files
    m_files.clear();
    std::string dir = RBUtil::getSaveDir();
    for (auto& entry : fs::directory_iterator(dir)) {
        if (entry.path().extension() == ".replay")
            m_files.push_back(entry.path().string());
    }

    if (m_files.empty()) {
        auto* noFiles = RBUI::makeLabel("No macros found.", "chatFont.fnt", 0.6f);
        noFiles->setPosition({ w / 2, h / 2 });
        bg->addChild(noFiles);
        return true;
    }

    // Scrollable list
    float listH = h - 50.0f;
    m_scroll    = CCScrollLayerExt::create({ 0, 0, w - 20.0f, listH });
    m_scroll->setPosition({ 10.0f, 30.0f });
    bg->addChild(m_scroll);

    auto* menu  = CCMenu::create();
    menu->setPosition({ 0, 0 });

    for (int i = 0; i < static_cast<int>(m_files.size()); ++i) {
        fs::path p(m_files[i]);
        std::string stem = p.stem().string();

        auto* rowBg = CCScale9Sprite::create("GJ_button_05.png");
        rowBg->setContentSize({ w - 30.0f, 28.0f });
        auto* rowLbl = CCLabelBMFont::create(stem.c_str(), "chatFont.fnt");
        rowLbl->setScale(0.55f);
        rowLbl->setAnchorPoint({ 0, 0.5f });
        rowLbl->setPosition({ 8.0f, 14.0f });
        rowBg->addChild(rowLbl);

        auto* btn = CCMenuItemSpriteExtra::create(
            rowBg, this,
            menu_selector(LoadMacroPopup::onSelectFile)
        );
        btn->setTag(i);
        btn->setPosition({
            (w - 20.0f) / 2,
            listH - 20.0f - i * 32.0f
        });
        menu->addChild(btn);
    }

    m_scroll->addChild(menu);
    m_scroll->setTouchEnabled(true);

    return true;
}

void LoadMacroPopup::onSelectFile(CCObject* sender) {
    auto* btn = dynamic_cast<CCNode*>(sender);
    if (!btn) return;
    int idx = btn->getTag();
    if (idx < 0 || idx >= static_cast<int>(m_files.size())) return;

    ReplayBot::get().loadMacro(m_files[idx]);
    this->onClose(nullptr);
}
