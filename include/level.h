#pragma once
#include <vector>
#include "obstacle.h"

#include <glad/glad.h> 

//###include <glfw\glfw3.h>
#include <GLFW/glfw3.h>


// GLM LIB 
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp> 
#include <glm/gtc/matrix_transform.hpp>

// ─────────────────────────────────────────
//  Ball  –  estado simple de la bola
// ─────────────────────────────────────────
struct Ball {
    glm::vec3 pos    = { 0.5f, 0.0f, 0.15f };
    glm::vec3 vel    = { 0.0f, 0.0f, 0.0f  };
    float     radius = 0.15f;
    bool      moving = false;
    BoxObstacle mesh;           // renderizamos la bola como una caja pequeña (por ahora)
};

// ─────────────────────────────────────────
//  Level  –  gestiona un nivel completo
// ─────────────────────────────────────────
class Level {
public:
    std::vector<BoxObstacle> obstacles;
    Ball      ball;
    glm::vec3 holePos   = { 7.0f, 0.0f, 0.0f };
    bool      completed = false;

    // Estado del golpe (público para que el HUD lo pueda leer si hiciera falta)
    float shotAngle = 0.0f;   // ángulo de disparo en grados (plano XY)
    float shotPower = 0.0f;   // potencia acumulada [0..1]
    bool  charging  = false;

    // Ciclo de vida
    void load();                                            // carga el nivel hardcodeado
    void update(float dt);                                  // física + comprobaciones
    void render(GLuint prog, const glm::mat4& VP);          // dibuja obstáculos + bola
    void handleInput(GLFWwindow* window, float dt);         // input del jugador
    void destroy();                                         // libera GPU resources

private:
    void resolveFloor();        // colisión con el suelo plano (z = 0)
    void resolveWalls();        // colisión con los obstáculos (AABB simple)
};