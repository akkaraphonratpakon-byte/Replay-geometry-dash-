#pragma once
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/GameObject.hpp>
#include <Geode/modify/CCDirector.hpp>

#include <vector>
#include <string>
#include <fstream>
#include <memory>
#include <functional>
#include <chrono>
#include <cmath>
#include <random>

using namespace geode::prelude;

// ─────────────────────────────────────────────
// PHASE 1 — C++ FUNDAMENTALS DEMONSTRATED
// ─────────────────────────────────────────────

// Enums — Bot state machine
enum class BotState {
    IDLE,
    RECORDING,
    PLAYING
};

// Struct — per-frame input data
struct ReplayInput {
    int   frame;    // Absolute frame number from level start
    bool  holding;  // true = press, false = release
    int   button;   // 1 = jump/primary, 2 = left, 3 = right
    bool  player2;  // true if this input belongs to player 2 (dual mode)
};

// Struct — renderer settings (popup config)
struct RendererSettings {
    int  fps        = 60;
    int  width      = 1920;
    int  height     = 1080;
    int  bitrate    = 8000;  // kbps
    bool enabled    = false;
    std::string outputPath = "replay_render.mp4";
};

// Struct — seed modifier settings
struct SeedSettings {
    bool enabled  = false;
    int  seed     = 0;
};

// ─────────────────────────────────────────────
// ReplayBot — singleton manager
// ─────────────────────────────────────────────

class ReplayBot {
public:
    // Singleton access
    static ReplayBot& get() {
        static ReplayBot instance;
        return instance;
    }
    ReplayBot(const ReplayBot&) = delete;
    ReplayBot& operator=(const ReplayBot&) = delete;

    // ── State ──────────────────────────────────
    BotState        state      = BotState::IDLE;
    int             frame      = 0;           // Current frame counter
    int             totalFrames= 0;           // Last recorded total

    // ── Replay data ────────────────────────────
    std::vector<ReplayInput> inputs;          // Recorded inputs
    std::string              macroName;       // Display name
    std::string              macroPath;       // Full file path

    // ── Feature toggles ────────────────────────
    bool noclip          = false;
    bool showTrajectory  = false;
    bool layoutMode      = false;
    bool frameStepMode   = false;
    bool safeMode        = true;
    bool instantRespawn  = false;
    bool noDeathEffect   = false;
    bool clickBotHuman   = false;  // ±1-2 frame offset
    bool loopPlayback    = false;

    // ── Speedhack ──────────────────────────────
    float speedMultiplier = 1.0f;

    // ── Renderer ───────────────────────────────
    RendererSettings renderer;

    // ── Seed ───────────────────────────────────
    SeedSettings seed;

    // ── Frame stepper ──────────────────────────
    bool  stepPending    = false;
    int   stepTarget     = 1;    // frames per keypress

    // ── Autosave ───────────────────────────────
    std::chrono::steady_clock::time_point lastAutosave;
    int  autosaveIntervalSec = 30;

    // ── Trajectory draw node ───────────────────
    // Raw pointer managed by Cocos; cleaned up when scene changes
    CCDrawNode* trajectoryNode = nullptr;

    // ── Public API ─────────────────────────────

    // Called once at mod load
    void init();

    // Called by PlayLayer::update hook each tick
    void onUpdate(float dt);

    // Called by handleButton hook
    void onInput(bool hold, int button, bool isPlayer2);

    // Called on level reset
    void onReset();

    // Called on quit
    void onQuit();

    // State transitions
    void startRecording();
    void stopRecording();
    void startPlaying();
    void stopPlaying();

    // File I/O
    bool saveMacro(const std::string& path, const std::string& name);
    bool loadMacro(const std::string& path);
    void autosaveTick();

    // Macro name resolution
    std::string resolveMacroName(const std::string& userInput);

    // Helpers
    int   getCurrentFrame() const;
    float getTotalSeconds()  const;
    std::string stateString() const;

    // Renderer
    void rendererCaptureFrame();
    void rendererFinalize();

    // Trajectory
    void drawTrajectory();

    // Layout mode — strip deco objects
    void applyLayoutMode(bool on);

    // RNG seed
    void applySeed(int s);

private:
    ReplayBot() = default;

    // FFmpeg pipe handle (nullptr when renderer off)
    FILE* ffmpegPipe = nullptr;

    // Random engine for clickbot humanization
    std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<int> humanOffset{ -2, 2 };

    // Internal helper: execute a recorded input at current frame
    void executeInput(const ReplayInput& inp);

    // Advance playhead, return inputs matching current frame
    std::vector<ReplayInput> inputsAtFrame(int f) const;
};

// ─────────────────────────────────────────────
// Utility helpers
// ─────────────────────────────────────────────

namespace RBUtil {
    // Get macro save directory (Geode saves dir / replaybot)
    std::string getSaveDir();

    // Sanitize a string for use as a filename
    std::string sanitizeFilename(const std::string& s);

    // Current Unix timestamp as string
    std::string timestampString();
}
