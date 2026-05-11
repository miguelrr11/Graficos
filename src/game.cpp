#include "game.h"
#include <GpO.h>
#include <cstdio>
#include <embedded_assets.h>

// ─────────────────────────────────────────────────────────────────────────────
void Game::init(SoLoud::Soloud* sol)
{
    res.soloud   = sol;
    res.texCesped = cargar_textura_mem(asset_cesped_jpg,  asset_cesped_jpg_size);
    res.texMadera = cargar_textura_mem(asset_madera2_jpg, asset_madera2_jpg_size);
    res.texHoyo   = cargar_textura_mem(asset_hoyo_png,    asset_hoyo_png_size);
    res.texBola   = cargar_textura_mem(asset_bola2_png,   asset_bola2_png_size);
    res.texIce    = cargar_textura_mem(asset_ice4_png,    asset_ice4_png_size);
    res.texSand   = cargar_textura_mem(asset_sand4_png,   asset_sand4_png_size);
    if (sol) res.sfxBeep.loadMem(asset_beep3_wav, asset_beep3_wav_size, false, false);

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

        // --- CONSUMO INMEDIATO DEL BONUS DE TIEMPO ---
        // Leemos la lista de bonus al revés para poder borrarlos sin que se rompa el bucle
        for (int i = (int)bonusQueue.size() - 1; i >= 0; --i) {
            if (bonusQueue[i] == 2) { // Si es un cubo dorado...
                gameTimer += 10.0f;   // +10 segundos
                printf("¡Cubo Dorado recogido! +10 segundos\n");
                bonusQueue.erase(bonusQueue.begin() + i); // Lo sacamos de la mochila
                goldBonus = 1.0f;
            }
        }
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
    needsCamReset = true;
}

void Game::restartLevel()
{
    level.restartLevel();
    needsCamReset = true;
}

void Game::resetToLevel1()
{
    printf("¡TIEMPO AGOTADO! Game Over. Vuelta al Nivel 1.\n");
    currentLevel = 1;
    gameTimer    = 60.0f;
    bonusQueue.clear();
    level.destroy();
    level.load(currentLevel, res);
    needsCamReset = true;
}
