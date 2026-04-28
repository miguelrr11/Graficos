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
static const float MAX_POWER   = 30.0f;
static const float CHARGE_RATE = 0.8f;    // potencia acumulada por segundo
static const float AIM_SPEED   = 90.0f;   // grados/segundo al girar la mira
static const float STOP_SPEED2 = 0.002f;  // velocidad² mínima para detener la bola
static const float HOLE_RADIUS = 0.3f;    // radio del hoyo


// ════════════════════════════════════════════════════════════════════════════
//  LOAD – nivel 1 hardcodeado: un pasillo recto con un obstáculo central
// ════════════════════════════════════════════════════════════════════════════
void Level::load()
{
    // Cargar texturas 
    texCesped = cargar_textura("/Users/miguelrodriguezmbp/Desktop/Upm/MASTER-1/Segundo_Sem/Graficos/assets/cesped.jpg");
    texMadera = cargar_textura("/Users/miguelrodriguezmbp/Desktop/Upm/MASTER-1/Segundo_Sem/Graficos/assets/madera.jpg");
    texHoyo   = cargar_textura("/Users/miguelrodriguezmbp/Desktop/Upm/MASTER-1/Segundo_Sem/Graficos/assets/hoyo.png");
    texBola   = cargar_textura("/Users/miguelrodriguezmbp/Desktop/Upm/MASTER-1/Segundo_Sem/Graficos/assets/bola.jpg");

    // texCesped = cargar_textura("../../assets/cesped.png");
    // texMadera = cargar_textura("../../assets/madera.jpg");
    // texHoyo   = cargar_textura("../../assets/hoyo.png");
    // texBola   = cargar_textura("../../assets/bola.jpg");


    completed = false;
    shotAngle = 0.0f;
    shotPower = 0.0f;
    charging  = false;

    // PEGAR AQUI LO DE LA HERRAMIENTA DE CRAER NIVELES
    
    std::vector<glm::vec2> corners = {{40.0f, 72.0f}, {45.0f, 69.0f}, {47.0f, 56.0f}, {54.0f, 49.0f}, {67.0f, 47.0f}, {69.0f, 42.0f}, {56.0f, 45.0f}, {42.0f, 40.0f}, {40.0f, 45.0f}, {45.0f, 49.0f}, {40.0f, 54.0f}, {40.0f, 67.0f}};
    //SUELO
    int tiling = 8;
    obstacles.push_back(crear_box({ 54.0f,  56.0f, -0.1f}, {29.0f, 32.0f, 0.2f}, {0,0,0}, {1,1,1}, true, tiling));
    obstacles.back().texID = texCesped;
    ball.pos = { 42.8f, 67.5f, ball.radius };
    holePos = { 65.3f, 45.0f, FLOOR_Z+0.1f };

    int thickness = 1.0f;

    // Física (invisible)
    for (int i = 0; i < corners.size(); i++) {
        glm::vec2 A = corners[i];
        glm::vec2 B = corners[(i + 1) % corners.size()];
        glm::vec2 dir  = B - A;
        float     len  = glm::length(dir);
        float     ang  = glm::degrees(std::atan2(dir.y, dir.x));
        glm::vec2 ctr  = (A + B) * 0.5f;
        auto box = crear_box({ctr.x, ctr.y, 0.3f}, {len, thickness, 0.6f}, {0,0,ang}, {1,1,1});
        box.ignoreRender = true;
        obstacles.push_back(box);
    }

    // Render (un solo mesh limpio)
    wallMesh = crear_wall_mesh(corners, /*closed=*/true, thickness, /*height=*/0.6f, 0.0f, 4.0f);
    wallMesh.texID = texMadera;

    //  Bola 
    ball.vel    = { 0.0f, 0.0f, 0.0f };
    ball.moving = false;
    ball.mesh   = crear_sphere({0,0,0},
                            ball.radius,
                            {1.0f, 1.0f, 1.0f});

    //  Hoyo (marcador visual: una caja plana oscura)
    obstacles.push_back(crear_box(holePos + glm::vec3(0,0,-0.09f),
                                  {HOLE_RADIUS*2, HOLE_RADIUS*2, 0.02f},
                                  {0,0,0},
                                  {0.05f, 0.05f, 0.05f}, true));
    obstacles.back().texID = texHoyo; // <--- TEXTURA
    obstacles.back().isHole = true;

    printf("Nivel cargado. Flechas = apuntar | ESPACIO = cargar/disparar\n");
}


// ════════════════════════════════════════════════════════════════════════════
//  UPDATE – física cada frame
// ════════════════════════════════════════════════════════════════════════════
void Level::update(float dt)
{
    if (!ball.moving) return;

    // Física básica
    ball.vel.z += GRAVITY * dt;
    ball.pos += ball.vel * dt;

    resolveFloor();
    resolveWalls();

    glm::vec2 d2D = { ball.pos.x - holePos.x, ball.pos.y - holePos.y };
    bool enHoyo = (glm::length(d2D) < HOLE_RADIUS);

    // Fricción y parada NORMAL (solo si no está en el agujero)
    if (!enHoyo) {
        if (ball.pos.z <= ball.radius + 0.02f) {
            ball.vel.x *= FRICTION;
            ball.vel.y *= FRICTION;
        }

        float spd2 = glm::dot(ball.vel, ball.vel);
        if (spd2 < STOP_SPEED2 && ball.pos.z <= ball.radius + 0.05f) {
            ball.vel    = {0, 0, 0};
            ball.pos.z  = ball.radius;
            ball.moving = false;
        }
    } 
    // Lógica si está cayendo por el HOYO
    else {
        // Succión al centro para que baje recta
        ball.vel.x = (holePos.x - ball.pos.x) * 4.0f;
        ball.vel.y = (holePos.y - ball.pos.y) * 4.0f;

        // Si ya se ha hundido suficiente, paramos el juego
        if (ball.pos.z < -0.5f) {
            if (!completed) {
                completed = true;
                printf("¡Nivel completado! Pulsa 'R' para reiniciar.\n");
            }
            ball.moving = false;       // Cortamos la física
            ball.vel = {0, 0, 0};      // Quitamos velocidad
            ball.pos.z = -0.5f;        // La dejamos atascada aquí, no cae infinito
        }
    }
}


// ════════════════════════════════════════════════════════════════════════════
//  COLISIONES
// ════════════════════════════════════════════════════════════════════════════

void Level::resolveFloor()
{
    // Si la bola está en el hoyo, cancelamos el suelo para que caiga
    glm::vec2 d2D = { ball.pos.x - holePos.x, ball.pos.y - holePos.y };
    if (glm::length(d2D) < HOLE_RADIUS) {
        return; 
    }

    // Suelo normal
    if (ball.pos.z < FLOOR_Z + ball.radius) {
        ball.pos.z = FLOOR_Z + ball.radius;
        if (ball.vel.z < 0.0f)
            ball.vel.z *= -RESTITUTION;
    }
}

void Level::resolveWalls()
{
    for (const auto& obs : obstacles) {
        if (obs.ignoreCollision) continue;

        // ── 1. Rotar la posición de la bola al espacio LOCAL del obstáculo ──
        float angle = glm::radians(obs.eulerAngles.z);
        float cosA  =  std::cos(angle);
        float sinA  =  std::sin(angle);

        // Vector desde el centro del obstáculo hasta la bola
        glm::vec3 d = ball.pos - obs.position;

        // Rotación inversa en Z (lleva d al espacio local)
        glm::vec3 local;
        local.x =  cosA * d.x + sinA * d.y;
        local.y = -sinA * d.x + cosA * d.y;
        local.z =  d.z;

        // ── 2. AABB en espacio local (inflado por radio de la bola) ─────────
        glm::vec3 half = obs.size * 0.5f;
        glm::vec3 minE = -(half + glm::vec3(ball.radius));
        glm::vec3 maxE =  (half + glm::vec3(ball.radius));

        if (local.x < minE.x || local.x > maxE.x) continue;
        if (local.y < minE.y || local.y > maxE.y) continue;
        if (local.z < minE.z || local.z > maxE.z) continue;

        // ── 3. Cara de mínima penetración ───────────────────────────────────
        float pen[6] = {
            maxE.x - local.x,  local.x - minE.x,
            maxE.y - local.y,  local.y - minE.y,
            maxE.z - local.z,  local.z - minE.z
        };

        int best = 0;
        for (int i = 1; i < 6; ++i)
            if (pen[i] < pen[best]) best = i;

        int   axis = best / 2;
        float sign = (best % 2 == 0) ? 1.0f : -1.0f;

        // ── 4. Corregir posición en espacio local y volver al mundo ─────────
        local[axis] += sign * pen[best];

        glm::vec3 corrected;
        corrected.x = cosA * local.x - sinA * local.y;
        corrected.y = sinA * local.x + cosA * local.y;
        corrected.z = local.z;
        ball.pos = obs.position + corrected;

        // ── 5. Reflejar velocidad en espacio local y volver al mundo ────────
        glm::vec3 velLocal;
        velLocal.x =  cosA * ball.vel.x + sinA * ball.vel.y;
        velLocal.y = -sinA * ball.vel.x + cosA * ball.vel.y;
        velLocal.z =  ball.vel.z;

        if (sign * velLocal[axis] < 0.0f)
            velLocal[axis] *= -RESTITUTION;

        ball.vel.x = cosA * velLocal.x - sinA * velLocal.y;
        ball.vel.y = sinA * velLocal.x + cosA * velLocal.y;
        ball.vel.z = velLocal.z;
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  RENDER
// ════════════════════════════════════════════════════════════════════════════
void Level::render(GLuint prog, const glm::mat4& VP)
{
    // Obstáculos
    for (const auto& obs : obstacles)
        render_box(obs, prog, VP, obs.texID); // Le pasamos un 0 temporalmente

    // Wall Mesh
    render_wall_mesh(wallMesh, prog, VP);

    // Bola: copiamos el mesh y le actualizamos la posición antes de renderizar
    SphereObstacle bm = ball.mesh;
    bm.position = ball.pos;
    render_sphere(bm, prog, VP, texBola); // <-- AÑADIDO: Pasamos texBola

    // Indicador de dirección de disparo (caja pequeña delante de la bola)
    if (!ball.moving && !completed) {
        float rad = glm::radians(shotAngle);
        glm::vec3 dir = { std::cos(rad), std::sin(rad), 0.0f };
        glm::vec3 arrowPos = ball.pos + dir * (ball.radius * 3.0f);

        SphereObstacle arrow = ball.mesh;
        arrow.position = arrowPos;
        arrow.radius     = ball.radius;
        arrow.color    = { 1.0f, 1.0f - shotPower, 0.0f };   // amarillo → rojo al cargar
        render_sphere(arrow, prog, VP, 0); // <-- AÑADIDO: Pasamos 0 (sin textura)
    }
}


// ════════════════════════════════════════════════════════════════════════════
//  INPUT del jugador
// ════════════════════════════════════════════════════════════════════════════
void Level::handleInput(GLFWwindow* window, float dt)
{
    if (ball.moving || completed) return;   // no se puede disparar mientras la bola rueda o cuando se ha completado el nivel

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