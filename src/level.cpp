#include "level.h"
#include <GpO.h>
#include <cstdio>
#include <cmath>

// ─── Constantes de física ───────────────────────────────────────────────────
static const float GRAVITY     = -9.8f;
static const float RESTITUTION = 0.25f;   // rebote en paredes/suelo
static const float FRICTION    = 0.985f;  // multiplicador por frame (rolling)
static const float FLOOR_Z     = 0.0f;    // altura del suelo

// ─── Constantes de disparo ──────────────────────────────────────────────────
static const float MAX_POWER   = 8.0f;
static const float CHARGE_RATE = 0.8f;    // potencia acumulada por segundo
static const float AIM_SPEED   = 90.0f;   // grados/segundo al girar la mira
static const float STOP_SPEED2 = 0.002f;  // velocidad² mínima para detener la bola
static const float HOLE_RADIUS = 0.3f;    // radio del hoyo


// ════════════════════════════════════════════════════════════════════════════
//  LOAD – nivel 1 hardcodeado: un pasillo recto con un obstáculo central
// ════════════════════════════════════════════════════════════════════════════
void Level::load()
{
    completed = false;
    shotAngle = 0.0f;
    shotPower = 0.0f;
    charging  = false;

    //  Obstáculos (posición, tamaño, ángulos euler, color)
    //                           pos                  size               rot   color
    obstacles.push_back(crear_box({ 4.0f,  0.0f, -0.1f}, {10.0f, 4.0f, 0.2f}, {0,0,0}, {0.3f, 0.65f, 0.3f}));  // suelo
    obstacles.push_back(crear_box({ 4.0f,  2.1f,  0.3f}, {10.0f, 0.2f, 0.6f}, {0,0,0}, {0.55f,0.35f, 0.2f}));  // pared norte
    obstacles.push_back(crear_box({ 4.0f, -2.1f,  0.3f}, {10.0f, 0.2f, 0.6f}, {0,0,0}, {0.55f,0.35f, 0.2f}));  // pared sur
    obstacles.push_back(crear_box({ 8.5f,  0.0f,  0.3f}, { 0.2f, 4.4f, 0.6f}, {0,0,0}, {0.55f,0.35f, 0.2f}));  // pared final
    obstacles.push_back(crear_box({-0.5f,  0.0f,  0.3f}, { 0.2f, 4.4f, 0.6f}, {0,0,0}, {0.55f,0.35f, 0.2f}));  // pared inicio
    obstacles.push_back(crear_box({ 4.5f,  0.6f,  0.3f}, { 0.4f, 0.4f, 0.6f}, {0,0,0}, {0.8f, 0.25f, 0.25f})); // obstáculo central

    //  Bola
    ball.pos    = { 0.5f, 0.0f, ball.radius };
    ball.vel    = { 0.0f, 0.0f, 0.0f };
    ball.moving = false;
    ball.mesh   = crear_sphere({0,0,0},
                            ball.radius,
                            {1.0f, 1.0f, 1.0f});

    //  Hoyo (marcador visual: una caja plana oscura)
    holePos = { 7.5f, 0.0f, FLOOR_Z+0.1f};
    obstacles.push_back(crear_box(holePos + glm::vec3(0,0,-0.09f),
                                  {HOLE_RADIUS*2, HOLE_RADIUS*2, 0.02f},
                                  {0,0,0},
                                  {0.05f, 0.05f, 0.05f}, true));

    printf("Nivel cargado. Flechas = apuntar | ESPACIO = cargar/disparar\n");
}


// ════════════════════════════════════════════════════════════════════════════
//  UPDATE – física cada frame
// ════════════════════════════════════════════════════════════════════════════
void Level::update(float dt)
{
    if (!ball.moving) return;

    // Gravedad
    ball.vel.z += GRAVITY * dt;

    // Mover
    ball.pos += ball.vel * dt;

    // Colisiones
    resolveFloor();
    resolveWalls();

    // Fricción de rodadura (solo cuando está apoyada en el suelo)
    if (ball.pos.z <= ball.radius + 0.02f) {
        ball.vel.x *= FRICTION;
        ball.vel.y *= FRICTION;
    }

    // Detener si va muy despacio y está en el suelo
    float spd2 = glm::dot(ball.vel, ball.vel);
    if (spd2 < STOP_SPEED2 && ball.pos.z <= ball.radius + 0.05f) {
        ball.vel    = {0, 0, 0};
        ball.pos.z  = ball.radius;
        ball.moving = false;
    }

    // ¿Entró en el hoyo?
    glm::vec2 d2D = { ball.pos.x - holePos.x, ball.pos.y - holePos.y };
    if (glm::length(d2D) < HOLE_RADIUS) {
        completed   = true;
        ball.moving = false;
        printf("¡Nivel completado!\n");
    }
}


// ════════════════════════════════════════════════════════════════════════════
//  COLISIONES
// ════════════════════════════════════════════════════════════════════════════

void Level::resolveFloor()
{
    if (ball.pos.z < FLOOR_Z + ball.radius) {
        ball.pos.z = FLOOR_Z + ball.radius;
        if (ball.vel.z < 0.0f)
            ball.vel.z *= -RESTITUTION;
    }
}

void Level::resolveWalls()
{
    for (const auto& obs : obstacles) {
        if(obs.ignoreCollision) continue;

        // AABB del obstáculo (sin rotación – válido para paredes axis-aligned)
        glm::vec3 half = obs.size * 0.5f;
        glm::vec3 minB = obs.position - half;
        glm::vec3 maxB = obs.position + half;

        // Inflar por el radio de la bola
        glm::vec3 minE = minB - glm::vec3(ball.radius);
        glm::vec3 maxE = maxB + glm::vec3(ball.radius);

        // ¿Hay intersección?
        if (ball.pos.x < minE.x || ball.pos.x > maxE.x) continue;
        if (ball.pos.y < minE.y || ball.pos.y > maxE.y) continue;
        if (ball.pos.z < minE.z || ball.pos.z > maxE.z) continue;

        // Penetración en cada eje (6 caras)
        float pen[6] = {
            maxE.x - ball.pos.x,  ball.pos.x - minE.x,   // +x, -x
            maxE.y - ball.pos.y,  ball.pos.y - minE.y,   // +y, -y
            maxE.z - ball.pos.z,  ball.pos.z - minE.z    // +z, -z
        };

        // Eje de mínima penetración
        int best = 0;
        for (int i = 1; i < 6; ++i)
            if (pen[i] < pen[best]) best = i;

        // Resolver
        int   axis = best / 2;
        float sign = (best % 2 == 0) ? 1.0f : -1.0f;   // +1 empuja hacia +eje, -1 hacia -eje

        ball.pos[axis] += sign * pen[best];

        // Reflejar velocidad si va hacia el obstáculo
        if (sign * ball.vel[axis] < 0.0f)
            ball.vel[axis] *= -RESTITUTION;
    }
}


// ════════════════════════════════════════════════════════════════════════════
//  RENDER
// ════════════════════════════════════════════════════════════════════════════
void Level::render(GLuint prog, const glm::mat4& VP)
{
    // Obstáculos
    for (const auto& obs : obstacles)
        render_box(obs, prog, VP);

    // Bola: copiamos el mesh y le actualizamos la posición antes de renderizar
    SphereObstacle bm = ball.mesh;
    bm.position = ball.pos;
    render_sphere(bm, prog, VP);

    // Indicador de dirección de disparo (caja pequeña delante de la bola)
    if (!ball.moving) {
        float rad = glm::radians(shotAngle);
        glm::vec3 dir = { std::cos(rad), std::sin(rad), 0.0f };
        glm::vec3 arrowPos = ball.pos + dir * (ball.radius * 3.0f);

        SphereObstacle arrow = ball.mesh;
        arrow.position = arrowPos;
        arrow.radius     = ball.radius;
        arrow.color    = { 1.0f, 1.0f - shotPower, 0.0f };   // amarillo → rojo al cargar
        render_sphere(arrow, prog, VP);
    }
}


// ════════════════════════════════════════════════════════════════════════════
//  INPUT del jugador
// ════════════════════════════════════════════════════════════════════════════
void Level::handleInput(GLFWwindow* window, float dt)
{
    if (ball.moving) return;   // no se puede disparar mientras la bola rueda

    // ── Apuntar con flechas ────────────────────────────────────────────────
    if (glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS) shotAngle += AIM_SPEED * dt;
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) shotAngle -= AIM_SPEED * dt;

    // ── Cargar y disparar con ESPACIO ──────────────────────────────────────
    static bool prevSpace = false;
    bool curSpace = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);

    if (curSpace && !prevSpace) {
        // Empieza a cargar
        charging  = true;
        shotPower = 0.0f;
    }

    if (charging && curSpace) {
        shotPower += CHARGE_RATE * dt;
        if (shotPower > 1.0f) shotPower = 1.0f;
    }

    if (charging && !curSpace && prevSpace) {
        // Suelta el espacio → disparar
        float rad   = glm::radians(shotAngle);
        float power = shotPower * MAX_POWER;

        ball.vel.x  = std::cos(rad) * power;
        ball.vel.y  = std::sin(rad) * power;
        ball.vel.z  = 0.3f;         // pequeño bote inicial

        ball.moving = true;
        charging    = false;
        shotPower   = 0.0f;

        printf("Disparo → ángulo: %.1f°  potencia: %.1f m/s\n", shotAngle, power);
    }

    prevSpace = curSpace;
}


// ════════════════════════════════════════════════════════════════════════════
//  DESTROY
// ════════════════════════════════════════════════════════════════════════════
void Level::destroy()
{
    for (auto& obs : obstacles) destroy_box(obs);
    obstacles.clear();
    destroy_sphere(ball.mesh);
}