#include "ReplayBot.hpp"
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/GameStatsManager.hpp>
#include <Geode/modify/CCDirector.hpp>

using namespace geode::prelude;

// ═══════════════════════════════════════════════════════════
// PlayLayer hooks
// ═══════════════════════════════════════════════════════════

class $modify(RBPlayLayer, PlayLayer) {

    // ── update ─────────────────────────────────────────────
    void update(float dt) {
        auto& bot = ReplayBot::get();

        // Speedhack: scale dt
        float scaledDt = dt * bot.speedMultiplier;

        // Frame stepper: if mode active and no step pending, skip real update
        if (bot.frameStepMode && bot.state == BotState::PLAYING) {
            if (!bot.stepPending) {
                // Still call bot logic so frame counter advances correctly
                // but don't advance physics
                return;
            }
        }

        // Let bot process this tick BEFORE the real update
        bot.onUpdate(scaledDt);

        // Call original update with (possibly scaled) dt
        PlayLayer::update(scaledDt);
    }

    // ── resetLevel ─────────────────────────────────────────
    void resetLevel() {
        ReplayBot::get().onReset();
        PlayLayer::resetLevel();
    }

    // ── onQuit ─────────────────────────────────────────────
    void onQuit() {
        ReplayBot::get().onQuit();
        PlayLayer::onQuit();
    }

    // ── setupHasCompleted (called when level objects are loaded) ─
    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();

        auto& bot = ReplayBot::get();

        // Apply layout mode if enabled when level first loads
        if (bot.layoutMode) bot.applyLayoutMode(true);

        // Apply seed lock if enabled
        if (bot.seed.enabled) bot.applySeed(bot.seed.seed);
    }

    // ── levelComplete — stop recording/playing gracefully ──
    void levelComplete() {
        auto& bot = ReplayBot::get();
        if (bot.state == BotState::RECORDING) bot.stopRecording();
        if (bot.state == BotState::PLAYING)   bot.stopPlaying();
        PlayLayer::levelComplete();
    }
};

// ═══════════════════════════════════════════════════════════
// GJBaseGameLayer — handleButton
// ═══════════════════════════════════════════════════════════

class $modify(RBBaseLayer, GJBaseGameLayer) {

    void handleButton(bool hold, int button, bool isPlayer1) {
        auto& bot = ReplayBot::get();

        // Record input
        bot.onInput(hold, button, !isPlayer1);

        // During playback in safe mode, still allow original call
        // (playback inputs are injected from bot.onUpdate via direct call)
        GJBaseGameLayer::handleButton(hold, button, isPlayer1);
    }
};

// ═══════════════════════════════════════════════════════════
// PlayerObject — death / respawn hooks
// ═══════════════════════════════════════════════════════════

class $modify(RBPlayerObject, PlayerObject) {

    // ── playDeathEffect ────────────────────────────────────
    void playDeathEffect() {
        auto& bot = ReplayBot::get();

        // No death effect — suppress visual entirely
        if (bot.noDeathEffect) return;

        // Instant respawn — trigger reset immediately, skip animation
        if (bot.instantRespawn) {
            auto* pl = PlayLayer::get();
            if (pl) {
                pl->resetLevel();
            }
            return;
        }

        PlayerObject::playDeathEffect();
    }

    // ── activateObject (used for noclip — skip collision death) ──
    // Note: collision death is triggered through PlayerObject::collidedWithObject
    // We hook playerDestroyedObject as an alternative entry point
    void playerDestroyedObject(GameObject* obj) {
        if (ReplayBot::get().noclip) return;
        PlayerObject::playerDestroyedObject(obj);
    }
};

// ═══════════════════════════════════════════════════════════
// CCDirector — speedhack via animation interval
// ═══════════════════════════════════════════════════════════
// We don't override the interval globally as that breaks UI.
// Instead we scale dt in PlayLayer::update (above).
// This is the preferred approach for GD Geode mods.

// ═══════════════════════════════════════════════════════════
// GameStatsManager — safe mode (block stat submission)
// ═══════════════════════════════════════════════════════════

class $modify(RBStatsManager, GameStatsManager) {

    // Award stars/coins/diamonds — blocked in safe mode
    void incrementUserCoinCount(GJGameLevel* level, bool b1) {
        if (ReplayBot::get().safeMode) return;
        GameStatsManager::incrementUserCoinCount(level, b1);
    }

    void addStarsForLevel(GJGameLevel* level, int stars) {
        if (ReplayBot::get().safeMode) return;
        GameStatsManager::addStarsForLevel(level, stars);
    }

    void addDiamondsForLevel(GJGameLevel* level, int diamonds) {
        if (ReplayBot::get().safeMode) return;
        GameStatsManager::addDiamondsForLevel(level, diamonds);
    }

    void setStat(char const* key, int value) {
        if (ReplayBot::get().safeMode) return;
        GameStatsManager::setStat(key, value);
    }
};

// ═══════════════════════════════════════════════════════════
// Keyboard listener — frame stepper keybind (F key)
// ═══════════════════════════════════════════════════════════
// Registered as a global key listener through Geode's dispatcher.

$execute {
    // Listen for KEY_F to advance frame stepper
    auto* dispatcher = CCDirector::sharedDirector()->getKeyboardDispatcher();
    // Geode provides a clean way to listen for keys via EventListener
    listenForAllEvents<cocos2d::CCKeyboardDispatcher::KeyboardEvent>(
        [](cocos2d::CCKeyboardDispatcher::KeyboardEvent* event) {
            auto& bot = ReplayBot::get();
            if (!bot.frameStepMode) return;
            // KEY_F = step forward
            if (event->keyCode == enumKeyCodes::KEY_F && event->isKeyDown) {
                bot.stepPending = true;
                bot.stepTarget  = 1;
            }
        }
    );
}
