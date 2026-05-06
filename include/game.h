#pragma once
#include "level.h"   // brings in Resources, Level, and all their dependencies

// ─────────────────────────────────────────────────────────────────────────────
//  DimState  –  fade-in / fade-out for level transitions
// ─────────────────────────────────────────────────────────────────────────────
enum class DimState { IDLE, FADE_OUT, FADE_IN };

// ─────────────────────────────────────────────────────────────────────────────
//  Game  –  top-level owner of everything that survives a level reload
// ─────────────────────────────────────────────────────────────────────────────
class Game {
public:
    Resources res;
    Level     level;

    // Player state – persists across levels and restarts
    int   currentLevel = 1;

    // tus bonus son una cola de los tipos de bonus que has ido recogiendo, y se gastan en orden (primero el salto extra, luego el superman, etc.)
    std::vector<int> bonusQueue;  // cada int es un tipo de bonus: 0 = salto extra, 1 = superman

    // Countdown timer; extended on each level completion
    float gameTimer = 60.0f;

    // Screen-fade state for level transitions
    DimState dimState = DimState::FADE_IN;
    float    dimValue = 0.0f;

    // Load textures + audio then build the first level.
    // Must be called after OpenGL and SoLoud are both initialised.
    void init(SoLoud::Soloud* sol);

    // Free all GPU geometry and textures.
    void destroy();

    // Per-frame logic: advance the fade and fire level transitions/resets.
    void update(float dt, bool startedGame);

private:
    static constexpr float kFadeSpeed = 1.75f;  // full fade in ~0.57 s

    void nextLevel();
    void restartLevel();
    void resetToLevel1();
};
