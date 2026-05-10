#pragma once
#include <vector>
#include "obstacle.h"
#include "sphere.h"
#include "wallmesh.h"
#include "floormesh.h"
#include "generator.h"
#include "particles.h"
#include "soloud.h"
#include "soloud_wav.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

// ─────────────────────────────────────────────────────────────────────────────
//  Resources  –  declared here so level.h has no dependency on game.h
// ─────────────────────────────────────────────────────────────────────────────
// Resources are owned by Game and passed by const-ref to Level::load().
// Level keeps raw pointers to soloud/sfxBeep (set by Game before any load).
struct Resources {
    GLuint texCesped = 0;
    GLuint texMadera = 0;
    GLuint texHoyo   = 0;
    GLuint texBola   = 0;
    GLuint texIce    = 0;
    GLuint texSand   = 0;

    SoLoud::Soloud* soloud  = nullptr;
    SoLoud::Wav     sfxBeep;           // VALUE – owned by the Resources holder (Game::res)
};

// ─────────────────────────────────────────────────────────────────────────────
//  Ball  –  physics + render state for the player ball
// ─────────────────────────────────────────────────────────────────────────────
struct Ball {
    glm::vec3 pos     = { 0.5f, 0.0f, 0.15f };
    glm::vec3 prevPos = pos;
    glm::vec3 vel     = { 0.0f, 0.0f, 0.0f  };
    float     radius  = 0.15f;
    bool      moving   = false;
    bool      onGround = false;
    SphereObstacle mesh;

    glm::quat rollQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
};

// ─────────────────────────────────────────────────────────────────────────────
//  Level  –  one playable level: geometry, physics, rendering, input
//            Does NOT own textures or audio – those come from Resources.
// ─────────────────────────────────────────────────────────────────────────────
class Level {
public:
    // ── Geometry & physics ───────────────────────────────────────────────────
    std::vector<BoxObstacle> obstacles;
    Ball      ball;
    glm::vec3 holePos       = { 7.0f, 0.0f, 0.0f };
    glm::vec3 curRespawnPos    = { 0.5f, 0.0f, 0.15f };  // el respawn que se actualiza si se coge un bonus tipo 3
    glm::vec3 respawnPos       = curRespawnPos;          // el respawn inicial
    float     currentFloorZ = 0.0f;

    std::vector<WallMesh>  wallMeshes;
    std::vector<FloorMesh> floorMeshes;
    std::vector<LevelData> tracks;

    std::vector<float> edges; // para centrar la camara al empezar el juego

    // ── Visual / gameplay state ──────────────────────────────────────────────
    bool      completed    = false;
    float     skyColorSeed = 0.0f;
    float       currentFriction    = 0.99f;          // updated each frame by resolveFloor()
    SurfaceType currentSurfaceType = SurfaceType::GRASS;

    // ── Shot input state ─────────────────────────────────────────────────────
    float shotAngle = 0.0f;
    float shotPower = 0.0f;
    bool  charging  = false;

    

    // ── Particles & billboard helpers ────────────────────────────────────────
    ParticleSystem particles;
    bool      particleEmitEnabled = true;
    glm::vec3 camRight = { 1, 0, 0 };
    glm::vec3 camUp    = { 0, 0, 1 };

    // ── Audio (pointers – owned by Game::res) ────────────────────────────────
    SoLoud::Soloud* soloud  = nullptr;
    SoLoud::Wav*    sfxBeep = nullptr;

    // ── Transition signal ────────────────────────────────────────────────────
    enum class PendingTransition { NONE, NEXT_LEVEL, RESTART_LEVEL };
    PendingTransition pendingTransition = PendingTransition::NONE;

    // ── Texture used directly in render (set from Resources in load()) ───────
    GLuint texBola = 0;

    // ── Lifecycle ────────────────────────────────────────────────────────────

    // Build level geometry for levelNum using pre-loaded Resources.
    void load(int levelNum, const Resources& res);

    // Per-frame physics + game logic. nBonus is the player's jump count (owned
    // by Game) and may be incremented when the ball collects a bonus.
    void update(float dt, std::vector<int>& bonusQueue);

    // Read input and mutate ball + nBonus (decrement on jump spend).
    void handleInput(GLFWwindow* window, float dt, std::vector<int>& bonusQueue);

    // Draw the level. nBonus controls the ball aura effect.
    void render(GLuint prog, const glm::mat4& VP, const std::vector<int>& bonusQueue);

    void renderShadows(GLuint shadow_prog, const glm::mat4& VP, glm::vec3 lightPos);

    // Free all GPU resources (VBOs, VAOs, etc.). Textures are owned by Game.
    void destroy();

    // Reset the ball to the start of the current level (no geometry reload).
    void restartLevel();

private:
    void resolveFloor();
    void resolveWalls(std::vector<int>& bonusQueue);
    bool fireworksEmitted_ = false;
    int  prevSegments_     = 0;
};
