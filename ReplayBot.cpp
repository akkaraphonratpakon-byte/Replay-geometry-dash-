#include "ReplayBot.hpp"
#include <Geode/Geode.hpp>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <ctime>

using namespace geode::prelude;
namespace fs = std::filesystem;

// ═══════════════════════════════════════════════════════════
// RBUtil
// ═══════════════════════════════════════════════════════════

namespace RBUtil {

std::string getSaveDir() {
    auto dir = Mod::get()->getSaveDir() / "macros";
    if (!fs::exists(dir)) fs::create_directories(dir);
    return dir.string();
}

std::string sanitizeFilename(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (std::isalnum(static_cast<unsigned char>(c)) ||
            c == '_' || c == '-' || c == ' ') {
            out += (c == ' ') ? '_' : c;
        }
    }
    if (out.empty()) out = "macro";
    return out;
}

std::string timestampString() {
    auto now = std::time(nullptr);
    std::ostringstream oss;
    oss << now;
    return oss.str();
}

} // namespace RBUtil

// ═══════════════════════════════════════════════════════════
// ReplayBot — init
// ═══════════════════════════════════════════════════════════

void ReplayBot::init() {
    safeMode             = Mod::get()->getSettingValue<bool>("safe-mode-on-load");
    autosaveIntervalSec  = static_cast<int>(Mod::get()->getSettingValue<int64_t>("autosave-interval"));
    lastAutosave         = std::chrono::steady_clock::now();
    log::info("[ReplayBot] Initialized. SafeMode={}", safeMode);
}

// ═══════════════════════════════════════════════════════════
// State string
// ═══════════════════════════════════════════════════════════

std::string ReplayBot::stateString() const {
    switch (state) {
        case BotState::RECORDING: return "RECORDING";
        case BotState::PLAYING:   return "PLAYING";
        default:                  return "IDLE";
    }
}

// ═══════════════════════════════════════════════════════════
// Frame helpers
// ═══════════════════════════════════════════════════════════

int ReplayBot::getCurrentFrame() const {
    return frame;
}

float ReplayBot::getTotalSeconds() const {
    // GD default physics = 240 fps
    return static_cast<float>(totalFrames) / 240.0f;
}

// ═══════════════════════════════════════════════════════════
// State transitions
// ═══════════════════════════════════════════════════════════

void ReplayBot::startRecording() {
    if (state != BotState::IDLE) return;
    inputs.clear();
    frame = 0;
    state = BotState::RECORDING;
    log::info("[ReplayBot] Recording started.");
}

void ReplayBot::stopRecording() {
    if (state != BotState::RECORDING) return;
    totalFrames = frame;
    state = BotState::IDLE;
    log::info("[ReplayBot] Recording stopped. {} inputs, {} frames.",
              inputs.size(), totalFrames);
    // Auto-save
    std::string name = resolveMacroName("");
    std::string path = RBUtil::getSaveDir() + "/" +
                       RBUtil::sanitizeFilename(name) + ".replay";
    saveMacro(path, name);
}

void ReplayBot::startPlaying() {
    if (inputs.empty()) {
        log::warn("[ReplayBot] No macro loaded.");
        return;
    }
    if (state != BotState::IDLE) return;
    frame = 0;
    state = BotState::PLAYING;
    log::info("[ReplayBot] Playback started. {} inputs.", inputs.size());

    // Open renderer pipe if enabled
    if (renderer.enabled) {
        std::string cmd = fmt::format(
            "ffmpeg -y -f rawvideo -pixel_format rgba"
            " -video_size {}x{} -framerate {} -i pipe:0"
            " -b:v {}k -vf vflip \"{}\" 2>/dev/null",
            renderer.width, renderer.height,
            renderer.fps, renderer.bitrate,
            renderer.outputPath
        );
#ifdef _WIN32
        ffmpegPipe = _popen(cmd.c_str(), "wb");
#else
        ffmpegPipe = popen(cmd.c_str(), "w");
#endif
        if (!ffmpegPipe)
            log::warn("[ReplayBot] Failed to open FFmpeg pipe.");
    }
}

void ReplayBot::stopPlaying() {
    if (state != BotState::PLAYING) return;
    state = BotState::IDLE;
    log::info("[ReplayBot] Playback stopped at frame {}.", frame);
    rendererFinalize();
}

// ═══════════════════════════════════════════════════════════
// onInput — called by handleButton hook
// ═══════════════════════════════════════════════════════════

void ReplayBot::onInput(bool hold, int button, bool isPlayer2) {
    if (state != BotState::RECORDING) return;

    ReplayInput inp;
    inp.frame   = frame;
    inp.holding = hold;
    inp.button  = button;
    inp.player2 = isPlayer2;
    inputs.push_back(inp);
}

// ═══════════════════════════════════════════════════════════
// onUpdate — called by PlayLayer::update hook
// ═══════════════════════════════════════════════════════════

void ReplayBot::onUpdate(float dt) {
    // Increment frame counter
    ++frame;

    // ── Frame stepper: only advance if step is pending ──────
    if (frameStepMode && state == BotState::PLAYING) {
        if (!stepPending) return; // block update until key pressed
        --stepTarget;
        if (stepTarget <= 0) {
            stepPending = false;
            stepTarget  = 1;
        }
    }

    // ── Playback ─────────────────────────────────────────────
    if (state == BotState::PLAYING) {
        auto matchedInputs = inputsAtFrame(frame);
        for (auto& inp : matchedInputs)
            executeInput(inp);

        // Reached end of macro
        if (frame >= totalFrames) {
            stopPlaying();
        }
    }

    // ── Autosave tick ────────────────────────────────────────
    if (state == BotState::RECORDING) {
        autosaveTick();
    }

    // ── Trajectory ───────────────────────────────────────────
    if (showTrajectory) {
        drawTrajectory();
    }

    // ── Renderer: capture frame ───────────────────────────────
    if (renderer.enabled && ffmpegPipe && state == BotState::PLAYING) {
        rendererCaptureFrame();
    }
}

// ═══════════════════════════════════════════════════════════
// onReset — called on level reset
// ═══════════════════════════════════════════════════════════

void ReplayBot::onReset() {
    frame = 0;

    if (state == BotState::PLAYING && loopPlayback) {
        log::info("[ReplayBot] Loop reset.");
        // Don't change state — restart from frame 0
    } else if (state == BotState::PLAYING) {
        // Keep playing from frame 0 (level restart in playback)
        frame = 0;
    } else if (state == BotState::RECORDING) {
        inputs.clear();
        frame = 0;
        log::info("[ReplayBot] Recording reset.");
    }
}

// ═══════════════════════════════════════════════════════════
// onQuit — called on PlayLayer quit
// ═══════════════════════════════════════════════════════════

void ReplayBot::onQuit() {
    if (state == BotState::RECORDING) stopRecording();
    if (state == BotState::PLAYING)   stopPlaying();
    trajectoryNode = nullptr;
}

// ═══════════════════════════════════════════════════════════
// Macro name resolution
// ═══════════════════════════════════════════════════════════

std::string ReplayBot::resolveMacroName(const std::string& userInput) {
    if (!userInput.empty()) return userInput;
    auto* pl = PlayLayer::get();
    if (pl && pl->m_level && !pl->m_level->m_levelName.empty())
        return pl->m_level->m_levelName;
    return "macro_" + RBUtil::timestampString();
}

// ═══════════════════════════════════════════════════════════
// File I/O — Binary .replay format
// ═══════════════════════════════════════════════════════════
// Format:
//   [4 bytes] Magic "RBOT"
//   [4 bytes] Version (uint32 = 1)
//   [4 bytes] totalFrames (int32)
//   [N bytes] name as length-prefixed string (uint32 len + chars)
//   [4 bytes] input count (uint32)
//   [per input: 4+1+4+1 = 10 bytes each]
// ───────────────────────────────────────────────────────────

static void writeU32(std::ofstream& f, uint32_t v) {
    f.write(reinterpret_cast<const char*>(&v), 4);
}
static void writeI32(std::ofstream& f, int32_t v) {
    f.write(reinterpret_cast<const char*>(&v), 4);
}
static void writeBool(std::ofstream& f, bool b) {
    uint8_t v = b ? 1 : 0;
    f.write(reinterpret_cast<const char*>(&v), 1);
}
static void writeString(std::ofstream& f, const std::string& s) {
    uint32_t len = static_cast<uint32_t>(s.size());
    writeU32(f, len);
    f.write(s.data(), len);
}

static bool readU32(std::ifstream& f, uint32_t& v) {
    return static_cast<bool>(f.read(reinterpret_cast<char*>(&v), 4));
}
static bool readI32(std::ifstream& f, int32_t& v) {
    return static_cast<bool>(f.read(reinterpret_cast<char*>(&v), 4));
}
static bool readBool(std::ifstream& f, bool& b) {
    uint8_t v;
    if (!f.read(reinterpret_cast<char*>(&v), 1)) return false;
    b = (v != 0);
    return true;
}
static bool readString(std::ifstream& f, std::string& s) {
    uint32_t len;
    if (!readU32(f, len)) return false;
    s.resize(len);
    return static_cast<bool>(f.read(s.data(), len));
}

bool ReplayBot::saveMacro(const std::string& path, const std::string& name) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        log::error("[ReplayBot] Failed to open for writing: {}", path);
        return false;
    }

    // Magic + version
    file.write("RBOT", 4);
    writeU32(file, 1);

    // Header
    writeI32(file, static_cast<int32_t>(totalFrames));
    writeString(file, name);

    // Inputs
    writeU32(file, static_cast<uint32_t>(inputs.size()));
    for (const auto& inp : inputs) {
        writeI32(file, inp.frame);
        writeBool(file, inp.holding);
        writeI32(file, inp.button);
        writeBool(file, inp.player2);
    }

    macroName = name;
    macroPath = path;
    log::info("[ReplayBot] Saved macro '{}' -> {}", name, path);
    return true;
}

bool ReplayBot::loadMacro(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        log::error("[ReplayBot] Failed to open: {}", path);
        return false;
    }

    char magic[4];
    file.read(magic, 4);
    if (std::string(magic, 4) != "RBOT") {
        log::error("[ReplayBot] Invalid magic bytes in: {}", path);
        return false;
    }

    uint32_t version;
    if (!readU32(file, version) || version != 1) {
        log::error("[ReplayBot] Unsupported replay version.");
        return false;
    }

    int32_t tf;
    readI32(file, tf);
    totalFrames = tf;

    std::string name;
    readString(file, name);

    uint32_t count;
    readU32(file, count);

    inputs.clear();
    inputs.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        ReplayInput inp;
        int32_t fr;
        readI32(file, fr);   inp.frame   = fr;
        readBool(file, inp.holding);
        int32_t btn;
        readI32(file, btn);  inp.button  = btn;
        readBool(file, inp.player2);
        inputs.push_back(inp);
    }

    macroName = name;
    macroPath = path;
    log::info("[ReplayBot] Loaded macro '{}' ({} inputs, {} frames)",
              name, inputs.size(), totalFrames);
    return true;
}

void ReplayBot::autosaveTick() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - lastAutosave).count();

    if (elapsed >= autosaveIntervalSec && !inputs.empty()) {
        std::string path = RBUtil::getSaveDir() + "/autosave.replay";
        std::string name = resolveMacroName("") + "_autosave";
        int savedTotal = totalFrames;
        totalFrames    = frame;
        saveMacro(path, name);
        totalFrames    = savedTotal;
        lastAutosave   = now;
        log::info("[ReplayBot] Autosaved {} inputs.", inputs.size());
    }
}

// ═══════════════════════════════════════════════════════════
// executeInput — inject a recorded input into the game
// ═══════════════════════════════════════════════════════════

void ReplayBot::executeInput(const ReplayInput& inp) {
    auto* pl = PlayLayer::get();
    if (!pl) return;

    // Apply clickbot humanization: shift frame slightly
    int targetFrame = inp.frame;
    if (clickBotHuman) {
        targetFrame += humanOffset(rng);
        if (targetFrame < 0) targetFrame = 0;
    }

    // We are already at the frame, so just call handleButton
    pl->handleButton(inp.holding, inp.button, !inp.player2);
}

std::vector<ReplayInput> ReplayBot::inputsAtFrame(int f) const {
    std::vector<ReplayInput> result;
    for (const auto& inp : inputs) {
        if (inp.frame == f)
            result.push_back(inp);
    }
    return result;
}

// ═══════════════════════════════════════════════════════════
// Renderer
// ═══════════════════════════════════════════════════════════

void ReplayBot::rendererCaptureFrame() {
    if (!ffmpegPipe) return;

    auto* director = CCDirector::sharedDirector();
    auto* target   = CCRenderTexture::create(
        renderer.width, renderer.height,
        kCCTexture2DPixelFormat_RGBA8888
    );
    if (!target) return;

    target->begin();
    director->getRunningScene()->visit();
    target->end();

    auto* img  = target->newCCImage(false);
    if (!img) return;

    // img->getData() is RGBA row-major
    fwrite(img->getData(), 1,
           static_cast<size_t>(renderer.width) * renderer.height * 4,
           ffmpegPipe);
    fflush(ffmpegPipe);
    img->release();
}

void ReplayBot::rendererFinalize() {
    if (!ffmpegPipe) return;
#ifdef _WIN32
    _pclose(ffmpegPipe);
#else
    pclose(ffmpegPipe);
#endif
    ffmpegPipe = nullptr;
    log::info("[ReplayBot] Renderer finalized -> {}", renderer.outputPath);
}

// ═══════════════════════════════════════════════════════════
// Trajectory simulation
// ═══════════════════════════════════════════════════════════

void ReplayBot::drawTrajectory() {
    auto* pl = PlayLayer::get();
    if (!pl) return;

    // Create/reset draw node parented to the game layer
    if (!trajectoryNode) {
        trajectoryNode = CCDrawNode::create();
        trajectoryNode->setZOrder(100);
        pl->addChild(trajectoryNode);
    }
    trajectoryNode->clear();

    auto* player = pl->m_player1;
    if (!player) return;

    // Snapshot physics state
    CCPoint pos   = player->getPosition();
    float   velY  = player->m_yVelocity;
    float   gravity = player->m_gravityMod * -9.8f * 60.0f; // approx

    // Simulate 120 frames ahead (0.5 s at 240 fps)
    const int   SIM_FRAMES  = 120;
    const float SIM_DT      = 1.0f / 240.0f;
    const float POINT_RADIUS = 1.5f;

    ccColor4F color = { 0.2f, 0.9f, 1.0f, 0.7f };
    CCPoint   prev  = pos;
    CCPoint   cur   = pos;
    float     vy    = velY;

    for (int i = 0; i < SIM_FRAMES; ++i) {
        vy  += gravity * SIM_DT;
        cur  = CCPoint{ prev.x + player->m_xVelocity * SIM_DT * 60.0f,
                        prev.y + vy * SIM_DT * 60.0f };

        trajectoryNode->drawDot(cur, POINT_RADIUS, color);
        if (i > 0)
            trajectoryNode->drawSegment(prev, cur, 0.5f, color);

        prev = cur;
    }
}

// ═══════════════════════════════════════════════════════════
// Layout mode
// ═══════════════════════════════════════════════════════════

void ReplayBot::applyLayoutMode(bool on) {
    auto* pl = PlayLayer::get();
    if (!pl) return;

    // Iterate all objects; hide decoration (type > 1)
    auto& objects = pl->m_objects;
    for (int i = 0; i < objects->count(); ++i) {
        auto* obj = static_cast<GameObject*>(objects->objectAtIndex(i));
        if (!obj) continue;

        // GameObjectType::Decoration == 4 in GD internals
        bool isDeco = (obj->m_objectType == GameObjectType::Decoration ||
                       obj->m_objectType == GameObjectType::Ambient);

        if (on) {
            if (isDeco) obj->setVisible(false);
            else {
                // Flatten to solid grey block appearance
                obj->setColor({ 180, 180, 180 });
                obj->setOpacity(220);
            }
        } else {
            obj->setVisible(true);
            obj->setColor({ 255, 255, 255 });
            obj->setOpacity(255);
        }
    }
}

// ═══════════════════════════════════════════════════════════
// RNG seed
// ═══════════════════════════════════════════════════════════

void ReplayBot::applySeed(int s) {
    seed.seed    = s;
    seed.enabled = true;
    // GameManager exposes RNG through m_randomSeed / m_seed fields
    // We set them both to ensure all RNG sources are locked
    auto* gm = GameManager::sharedState();
    if (gm) {
        // These fields exist in GD 2.2+; adjust member names if SDK differs
        gm->m_randomSeed = s;
    }
    log::info("[ReplayBot] Seed locked to {}.", s);
}
