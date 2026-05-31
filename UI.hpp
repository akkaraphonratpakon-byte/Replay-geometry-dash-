#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include "ReplayBot.hpp"

using namespace geode::prelude;

// ─────────────────────────────────────────────────────────────
// Forward declarations
// ─────────────────────────────────────────────────────────────

class ReplayBotLayer;
class SeedPopup;
class RendererPopup;
class SpeedhackPopup;
class FrameStepperPopup;
class SaveMacroPopup;
class LoadMacroPopup;

// ─────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────

namespace RBUI {
    // Create a styled toggle button (ON/OFF)
    CCMenuItemToggler* makeToggle(
        CCObject* target,
        SEL_MenuHandler callback,
        bool initialState
    );

    // Create a small gear/settings button
    CCMenuItemSpriteExtra* makeGearBtn(
        CCObject* target,
        SEL_MenuHandler callback
    );

    // Status label helper
    CCLabelBMFont* makeLabel(
        const std::string& text,
        const std::string& font = "bigFont.fnt",
        float scale = 0.4f
    );

    // Create labelled row: [label] [toggle] [gear?]
    // Returns the row node; caller positions it
    CCNode* makeFeatureRow(
        const std::string& label,
        CCObject* target,
        SEL_MenuHandler toggleCb,
        bool state,
        CCObject* gearTarget   = nullptr,
        SEL_MenuHandler gearCb = nullptr
    );
}

// ─────────────────────────────────────────────────────────────
// ReplayBotLayer — main popup
// ─────────────────────────────────────────────────────────────

class ReplayBotLayer : public geode::Popup<> {
public:
    static ReplayBotLayer* create();

    bool setup() override;
    void onClose(CCObject*) override;

    // Refresh dynamic UI text (frame counter, state badge, macro name)
    void refresh();

private:
    // Refresh timer
    CCScheduler* m_scheduler = nullptr;

    // Dynamic labels
    CCLabelBMFont* m_stateLabel   = nullptr;
    CCLabelBMFont* m_frameLabel   = nullptr;
    CCLabelBMFont* m_macroLabel   = nullptr;
    CCLabelBMFont* m_speedLabel   = nullptr;

    // Speed slider
    Slider* m_speedSlider = nullptr;

    // Feature toggles (refs so we can update their state)
    CCMenuItemToggler* m_noclipToggle    = nullptr;
    CCMenuItemToggler* m_trajToggle      = nullptr;
    CCMenuItemToggler* m_layoutToggle    = nullptr;
    CCMenuItemToggler* m_fstepToggle     = nullptr;
    CCMenuItemToggler* m_safeModeToggle  = nullptr;
    CCMenuItemToggler* m_rendToggle      = nullptr;
    CCMenuItemToggler* m_instRespToggle  = nullptr;
    CCMenuItemToggler* m_loopToggle      = nullptr;

    void buildHeader(CCNode* parent, float w, float h);
    void buildMainButtons(CCNode* parent, float w, float h);
    void buildSpeedRow(CCNode* parent, float w, float h);
    void buildFeatureGrid(CCNode* parent, float w, float h);
    void buildStatusBar(CCNode* parent, float w, float h);
    void scheduleRefresh();

    // Button callbacks
    void onRecord(CCObject*);
    void onPlay(CCObject*);
    void onStop(CCObject*);
    void onLoadMacro(CCObject*);

    void onNoclip(CCObject*);
    void onTrajectory(CCObject*);
    void onLayout(CCObject*);
    void onFrameStep(CCObject*);
    void onSafeMode(CCObject*);
    void onRenderer(CCObject*);
    void onInstRespawn(CCObject*);
    void onLoop(CCObject*);

    void onSeedGear(CCObject*);
    void onRendererGear(CCObject*);
    void onSpeedhackGear(CCObject*);
    void onFrameStepGear(CCObject*);

    void onSpeedSlider(CCObject*);

    void refreshCb(float);
};

// ─────────────────────────────────────────────────────────────
// SeedPopup
// ─────────────────────────────────────────────────────────────

class SeedPopup : public geode::Popup<> {
public:
    static SeedPopup* create();
    bool setup() override;
private:
    InputNode* m_seedInput = nullptr;
    void onRandom(CCObject*);
    void onApply(CCObject*);
};

// ─────────────────────────────────────────────────────────────
// RendererPopup
// ─────────────────────────────────────────────────────────────

class RendererPopup : public geode::Popup<> {
public:
    static RendererPopup* create();
    bool setup() override;
private:
    InputNode* m_fpsInput     = nullptr;
    InputNode* m_widthInput   = nullptr;
    InputNode* m_heightInput  = nullptr;
    InputNode* m_bitrateInput = nullptr;
    InputNode* m_pathInput    = nullptr;
    void onApply(CCObject*);
};

// ─────────────────────────────────────────────────────────────
// SpeedhackPopup
// ─────────────────────────────────────────────────────────────

class SpeedhackPopup : public geode::Popup<> {
public:
    static SpeedhackPopup* create();
    bool setup() override;
private:
    InputNode* m_input = nullptr;
    void onApply(CCObject*);
};

// ─────────────────────────────────────────────────────────────
// FrameStepperPopup
// ─────────────────────────────────────────────────────────────

class FrameStepperPopup : public geode::Popup<> {
public:
    static FrameStepperPopup* create();
    bool setup() override;
private:
    InputNode* m_input = nullptr;
    void onApply(CCObject*);
};

// ─────────────────────────────────────────────────────────────
// SaveMacroPopup
// ─────────────────────────────────────────────────────────────

class SaveMacroPopup : public geode::Popup<> {
public:
    static SaveMacroPopup* create();
    bool setup() override;
private:
    InputNode* m_nameInput = nullptr;
    CCLabelBMFont* m_fileLabel = nullptr;
    void onNameChanged(CCObject*);
    void onSave(CCObject*);
    void updateFileLabel(const std::string& name);
};

// ─────────────────────────────────────────────────────────────
// LoadMacroPopup
// ─────────────────────────────────────────────────────────────

class LoadMacroPopup : public geode::Popup<> {
public:
    static LoadMacroPopup* create();
    bool setup() override;
private:
    CCScrollLayerExt* m_scroll = nullptr;
    std::vector<std::string> m_files;
    void populateList();
    void onSelectFile(CCObject*);
};
