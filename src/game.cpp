#include "game.h"
#include <GpO.h>
#include <cstdio>

// ─────────────────────────────────────────────────────────────────────────────
void Game::init(SoLoud::Soloud* sol)
{
    res.soloud   = sol;
    res.texCesped = cargar_textura(getAssetPath("cesped.jpg").c_str());
    res.texMadera = cargar_textura(getAssetPath("madera2.jpg").c_str());
    res.texHoyo   = cargar_textura(getAssetPath("hoyo.png").c_str());
    res.texBola   = cargar_textura(getAssetPath("bola2.png").c_str());
    res.texIce    = cargar_textura(getAssetPath("ice4.png").c_str());
    res.texSand   = cargar_textura(getAssetPath("sand4.png").c_str());
    if (sol) res.sfxBeep.load(getAssetPath("beep3.wav").c_str());

    level.soloud  = res.soloud;
    level.sfxBeep = &res.sfxBeep;
    level.load(currentLevel, res);
}

// ─────────────────────────────────────────────────────────────────────────────
void Game::destroy()
{
    res.sfxBeep.mSoloud = nullptr;

    level.destroy();

    glDeleteTextures(1, &res.texCesped);
    glDeleteTextures(1, &res.texMadera);
    glDeleteTextures(1, &res.texHoyo);
    glDeleteTextures(1, &res.texBola);
    glDeleteTextures(1, &res.texIce);
    glDeleteTextures(1, &res.texSand);
    res.texCesped = res.texMadera = res.texHoyo = res.texBola = res.texIce = res.texSand = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
void Game::update(float dt, bool startedGame)
{
    if (level.pendingTransition != Level::PendingTransition::NONE && dimState == DimState::IDLE)
        dimState = DimState::FADE_OUT;

    if (dimState == DimState::FADE_OUT) {
        dimValue -= dt * kFadeSpeed;
        if (dimValue <= 0.0f) {
            dimValue = 0.0f;
            if (level.pendingTransition == Level::PendingTransition::NEXT_LEVEL)
                nextLevel();
            else if (level.pendingTransition == Level::PendingTransition::RESTART_LEVEL)
                restartLevel();
            level.pendingTransition = Level::PendingTransition::NONE;
            dimState = DimState::FADE_IN;
        }
    } else if (dimState == DimState::FADE_IN) {
        dimValue += dt * kFadeSpeed;
        if (dimValue >= 1.0f) { dimValue = 1.0f; dimState = DimState::IDLE; }
    }

    //timer de la pantalla
    if (startedGame) {
        gameTimer -= dt;
    }
    if (gameTimer <= 0.0f && dimState == DimState::IDLE) {
        resetToLevel1();
        dimValue = 0.0f;
        dimState = DimState::FADE_IN;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void Game::nextLevel()
{
    currentLevel+=1;
    gameTimer += 30.0f;
    level.destroy();
    level.load(currentLevel, res);
}

void Game::restartLevel()
{
    level.restartLevel();
}

void Game::resetToLevel1()
{
    printf("¡TIEMPO AGOTADO! Game Over. Vuelta al Nivel 1.\n");
    currentLevel = 1;
    gameTimer    = 60.0f;
    bonusQueue.clear();
    level.destroy();
    level.load(currentLevel, res);
}
