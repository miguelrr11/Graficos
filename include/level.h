#pragma once
#include <vector>
#include "obstacle.h"
#include "sphere.h"
#include "wallmesh.h"
#include "floormesh.h"
#include "generator.h"

#include <glad/glad.h> 

//###include <glfw\glfw3.h>
#include <GLFW/glfw3.h>


// GLM LIB
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

// ─────────────────────────────────────────
//  Ball  –  estado simple de la bola
// ─────────────────────────────────────────
struct Ball {
    glm::vec3 pos    = { 0.5f, 0.0f, 0.15f };
    glm::vec3 prevPos = pos;
    glm::vec3 vel    = { 0.0f, 0.0f, 0.0f  };
    float     radius = 0.15f;
    bool      moving   = false;
    bool      onGround = false;
    SphereObstacle mesh;

    // Rotación acumulada de rodadura: se actualiza cada frame según la velocidad
    glm::quat rollQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
};

// ─────────────────────────────────────────
//  Level  –  gestiona un nivel completo
// ─────────────────────────────────────────
class Level {
public:
    std::vector<BoxObstacle> obstacles;
    Ball      ball;
    glm::vec3 holePos      = { 7.0f, 0.0f, 0.0f };
    float     currentFloorZ = 0.0f;
    bool      completed = false;
    int currentLevel = 1;

    // Estado del golpe (público para que el HUD lo pueda leer si hiciera falta)
    float shotAngle = 0.0f;   // ángulo de disparo en grados (plano XY)
    float shotPower = 0.0f;   // potencia acumulada [0..1]
    bool  charging  = false;

    // Texturas
    GLuint texCesped;
    GLuint texMadera;
    GLuint texHoyo;
    GLuint texBola;

    std::vector<WallMesh>  wallMeshes;
    std::vector<FloorMesh> floorMeshes;
    std::vector<LevelData> tracks;

    // Ciclo de vida
    void load();                                            // carga el nivel hardcodeado
    void update(float dt);                                  // física + comprobaciones
    void render(GLuint prog, const glm::mat4& VP);          // dibuja obstáculos + bola
    void renderShadows(GLuint shadow_prog, const glm::mat4& VP, const glm::mat4& shadowMat);
    void handleInput(GLFWwindow* window, float dt);         // input del jugador
    void destroy();                                         // libera GPU resources
    void restartLevel();                                    // reinicia el estado de la bola para volver a empezar el mismo nivel

private:
    void resolveFloor();        // colisión con el suelo plano (z = 0)
    void resolveWalls();        // colisión con los obstáculos (AABB simple)
};