#include "level.h"
#include <GpO.h>
#include <cstdio>
#include <cmath>

// ─── Constantes de física ───────────────────────────────────────────────────
static const float GRAVITY     = -9.8f;
static const float RESTITUTION = 0.5f;   // rebote en paredes/suelo
static const float FRICTION    = 0.985f;  // multiplicador por frame (rolling)
static const float FLOOR_Z     = 0.0f;    // altura del suelo

// ─── Constantes de disparo ──────────────────────────────────────────────────
static const float MAX_POWER   = 15.0f;
static const float CHARGE_RATE = 0.8f;    // potencia acumulada por segundo
static const float AIM_SPEED   = 90.0f;   // grados/segundo al girar la mira
static const float STOP_SPEED2 = 0.002f;  // velocidad² mínima para detener la bola
static const float HOLE_RADIUS = 0.3f;    // radio del hoyo

void Level::load()
{
    // Cargar texturas 
    texCesped = cargar_textura("/Users/miguelrodriguezmbp/Desktop/Upm/MASTER-1/Segundo_Sem/Graficos/assets/cesped.jpg");
    texMadera = cargar_textura("/Users/miguelrodriguezmbp/Desktop/Upm/MASTER-1/Segundo_Sem/Graficos/assets/madera2.jpg");
    texHoyo   = cargar_textura("/Users/miguelrodriguezmbp/Desktop/Upm/MASTER-1/Segundo_Sem/Graficos/assets/hoyo.png");
    texBola   = cargar_textura("/Users/miguelrodriguezmbp/Desktop/Upm/MASTER-1/Segundo_Sem/Graficos/assets/bola2.png");

    // texCesped = cargar_textura("../../assets/cesped.jpg");
    // texMadera = cargar_textura("../../assets/madera2.jpg");
    // texHoyo   = cargar_textura("../../assets/hoyo.png");
    // texBola   = cargar_textura("../../assets/bola.jpg");

    // texCesped = cargar_textura("C:\\Users\\mrodriguez\\Desktop\\Graficos\\assets\\cesped.jpg");
    // texMadera = cargar_textura("C:\\Users\\mrodriguez\\Desktop\\Graficos\\assets\\madera2.jpg");
    // texHoyo   = cargar_textura("C:\\Users\\mrodriguez\\Desktop\\Graficos\\assets\\hoyo.png");
    // texBola   = cargar_textura("C:\\Users\\mrodriguez\\Desktop\\Graficos\\assets\\bola2.png");

    completed = false;
    shotAngle = 0.0f;
    shotPower = 0.0f;
    charging  = false;

    std::vector<std::vector<glm::vec2>> trackPerimeters = {
        {{45.0f, 67.0f}, {45.0f, 78.0f}, {49.0f, 78.0f}, {49.0f, 67.0f}},
        {{45.0f, 65.0f}, {49.0f, 65.0f}, {49.0f, 56.0f}, {45.0f, 56.0f}},
        {{45.0f, 54.0f}, {49.0f, 54.0f}, {49.0f, 45.0f}, {45.0f, 45.0f}},
        {{45.0f, 42.0f}, {49.0f, 42.0f}, {49.0f, 33.0f}, {45.0f, 33.0f}},
        {{45.0f, 31.0f}, {49.0f, 31.0f}, {49.0f, 22.0f}, {45.0f, 22.0f}},
    };
    

    size_t numTracks = trackPerimeters.size();
    int heightChange = 1;
    for(size_t t = 0; t < numTracks; ++t) {
        // 2. GENERAR LA PISTA (6 segmentos de longitud)
        // Usamos el algoritmo SAW + Anchura Variable que diseñamos
        LevelData track;
        track.perimeter = trackPerimeters[t];
        tracks.push_back(track);

        // 3. CÉSPED SOLO EN EL AREA DEL PERIMETRO
        floorMeshes.push_back(crear_floor_mesh(tracks.back().perimeter, FLOOR_Z + t*heightChange, 2.0f));
        floorMeshes.back().texID          = texCesped;
        floorMeshes.back().perimeter      = trackPerimeters[t];
        floorMeshes.back().zBase          = FLOOR_Z + t * heightChange;
        floorMeshes.back().useCheckerboard = true;

        // 4. EL MURO VISUAL
        // Pasamos: (puntos, cerrado, grosor=0.4f, altura=1.0f, zBase=FLOOR_Z, uvTile=1.0f)
        wallMeshes.push_back(crear_wall_mesh(tracks.back().perimeter, true, 0.4f, 0.5f, FLOOR_Z + t*heightChange, 1.0f));
        wallMeshes.back().texID = texMadera;

        // 6. EL HOYO
        if(t == numTracks - 1){
            //holePos = { 0, 0, FLOOR_Z + 0.02f };
            obstacles.push_back(crear_box(holePos + glm::vec3(0,0,-0.01f),
                                        { HOLE_RADIUS*2.5f, HOLE_RADIUS*2.5f, 0.05f },
                                        {0,0,0}, {1,1,1}, true, 1));
            obstacles.back().texID = texHoyo;
            obstacles.back().isHole = true;
        }
    }

    // 7. LA BOLA
    //ball.pos      = { 15, 70, FLOOR_Z + ball.radius };
    ball.vel      = { 0, 0, 0 };
    ball.moving   = false;
    ball.rollQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    ball.mesh     = crear_sphere({0,0,0}, ball.radius, {1.0f, 1.0f, 1.0f});

    ball.pos = { 47.3f, 76.5f, 0*heightChange };
    holePos = { 47.3f, 24.8f, (FLOOR_Z+0.1f)+4*heightChange };

    // 5. LAS FÍSICAS (Cajas invisibles rotadas)
    for(size_t t = 0; t < tracks.size(); t++) {
        const LevelData& track = this->tracks[t];
        for (size_t i = 0; i < track.perimeter.size(); i++) {
            glm::vec2 pA = track.perimeter[i];
            glm::vec2 pB = track.perimeter[(i + 1) % track.perimeter.size()];
            
            glm::vec2 dir = pB - pA;
            float longitud = glm::length(dir);
            float angulo = std::atan2(dir.y, dir.x);
            glm::vec2 centro = (pA + pB) * 0.5f;

            // Creamos la caja física: Invisible pero colisionable
            obstacles.push_back(crear_box({ centro.x, centro.y, FLOOR_Z + t*heightChange }, 
                                        { longitud, 0.4f, 1.0f }, // Grosor del muro de 0.4f
                                        { 0.0f, 0.0f, glm::degrees(angulo) }, 
                                        {1,1,1}, false));
            obstacles.back().ignoreRender = true; 
        }
    }

    

    printf("Nivel Procedural Cargado. ¡A jugar!\n");
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

    // Rotación de rodadura: eje perpendicular a la velocidad horizontal (Z arriba)
    // ω = (-vy, vx, 0) / radius  →  eje = normalize(-vy, vx, 0), ángulo = speed/radius * dt
    {
        glm::vec3 horizVel = { ball.vel.x, ball.vel.y, 0.0f };
        float speed = glm::length(horizVel);
        if (speed > 0.001f) {
            glm::vec3 rollAxis = glm::vec3(-ball.vel.y, ball.vel.x, 0.0f) / speed;
            float rollAngle    = speed * dt / ball.radius;
            glm::quat dRot     = glm::angleAxis(rollAngle, rollAxis);
            ball.rollQuat      = glm::normalize(dRot * ball.rollQuat);
        }
    }

    glm::vec2 d2D = { ball.pos.x - holePos.x, ball.pos.y - holePos.y };
    bool enHoyo = (glm::length(d2D) < HOLE_RADIUS);

    // Fricción y parada NORMAL (solo si no está en el agujero)
    if (!enHoyo) {
        if (ball.pos.z <= currentFloorZ + ball.radius + 0.02f) {
            ball.vel.x *= FRICTION;
            ball.vel.y *= FRICTION;
        }

        float spd2 = glm::dot(ball.vel, ball.vel);
        if (spd2 < STOP_SPEED2 && ball.pos.z <= currentFloorZ + ball.radius + 0.05f) {
            ball.vel    = {0, 0, 0};
            ball.pos.z  = currentFloorZ + ball.radius;
            ball.moving = false;
        }
    } 
    // Lógica si está cayendo por el HOYO
    else {
        // Succión al centro para que baje recta
        ball.vel.x = (holePos.x - ball.pos.x) * 4.0f;
        ball.vel.y = (holePos.y - ball.pos.y) * 4.0f;

        //magnitud de la velocidad
        float spd = glm::length(ball.vel);
        if(spd < 2.0f) {
            ball.vel.x = 0;
            ball.vel.y = 0;
        }

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

static bool pointInTriangle(glm::vec2 p, glm::vec2 a, glm::vec2 b, glm::vec2 c)
{
    float d1 = (p.x - b.x) * (a.y - b.y) - (a.x - b.x) * (p.y - b.y);
    float d2 = (p.x - c.x) * (b.y - c.y) - (b.x - c.x) * (p.y - c.y);
    float d3 = (p.x - a.x) * (c.y - a.y) - (c.x - a.x) * (p.y - a.y);
    bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(hasNeg && hasPos);
}

void Level::resolveFloor()
{
    // Si la bola está en el hoyo, cancelamos el suelo para que caiga
    glm::vec2 ballXY = { ball.pos.x, ball.pos.y };
    glm::vec2 d2D    = ballXY - glm::vec2(holePos.x, holePos.y);
    if (glm::length(d2D) < HOLE_RADIUS) return;

    // Buscar el floor mesh más alto cuyo polígono contiene la posición XY de la bola
    float bestZ = -1e9f;
    for (const auto& floor : floorMeshes) {
        if (floor.zBase > ball.pos.z + ball.radius) continue; // suelo sobre la bola, ignorar
        if (floor.zBase <= bestZ) continue;                   // ya encontramos uno más alto
        const auto& poly = floor.perimeter;
        // Fan-triangulation desde el vértice 0, igual que crear_floor_mesh
        for (size_t i = 1; i + 1 < poly.size(); ++i) {
            if (pointInTriangle(ballXY, poly[0], poly[i], poly[i + 1])) {
                bestZ = floor.zBase;
                break;
            }
        }
    }

    if (bestZ > -1e9f) {
        currentFloorZ = bestZ;
        if (ball.pos.z < bestZ + ball.radius) {
            ball.pos.z = bestZ + ball.radius;
            if (ball.vel.z < 0.0f)
                ball.vel.z *= -RESTITUTION;
        }
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

    // Suelos
    for (const auto& floorMesh : floorMeshes) {
        render_floor_mesh(floorMesh, prog, VP);
    }

    // Wall Mesh
    for (const auto& wallMesh : wallMeshes) {
        render_wall_mesh(wallMesh, prog, VP);
    }

    // Bola: copiamos el mesh, actualizamos posición y rotación acumulada
    SphereObstacle bm = ball.mesh;
    bm.position = ball.pos;
    bm.rotation = ball.rollQuat;
    render_sphere(bm, prog, VP, texBola);

    // Indicador de dirección de disparo (caja pequeña delante de la bola)
    if (!ball.moving && !completed) {
        float rad = glm::radians(shotAngle);
        glm::vec3 dir = { std::cos(rad), std::sin(rad), 0.0f };
        glm::vec3 arrowPos = ball.pos + dir * (ball.radius * 3.0f);

        SphereObstacle arrow = ball.mesh;
        arrow.position = arrowPos;
        arrow.radius     = ball.radius * 0.25f;
        // arrow.color    = { 1.0f, 1.0f - shotPower, 0.0f };   // amarillo → rojo al cargar
        //render_sphere(arrow, prog, VP, 0); // <-- AÑADIDO: Pasamos 0 (sin textura)

        int numSegments = fmax(1, floor(shotPower * 10.0f)); // De 0 a 10 segmentos según la potencia
        for(int i = 1; i <= numSegments; ++i) {
            glm::vec3 segPos = ball.pos + dir * (ball.radius * (3.0f + i));
            arrow.position = segPos;
            arrow.color = { 1.0f, ((1.0f - shotPower)*i/numSegments), 0.0f }; // semitransparente
            render_sphere(arrow, prog, VP, 0);
        }
    }
}


// ════════════════════════════════════════════════════════════════════════════
//  SHADOWS
// ════════════════════════════════════════════════════════════════════════════
void Level::renderShadows(GLuint shadow_prog, const glm::mat4& VP, const glm::mat4& shadowMat)
{
    glUseProgram(shadow_prog);

    // Obstáculos: excluir invisibles y el suelo (área XY > 50 u²)
    for (const auto& obs : obstacles) {
        if (obs.ignoreRender) continue;
        if (obs.size.x * obs.size.y > 50.0f) continue;
        glm::mat4 MVP = VP * shadowMat * obs.modelMatrix();
        transfer_mat4("MVP", MVP);
        glBindVertexArray(obs.VAO);
        glDrawElements(GL_TRIANGLES, obs.indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    // Wall mesh
    for(const auto& wallMesh : wallMeshes) {
        glm::mat4 MVP = VP * shadowMat;   // model = identidad
        transfer_mat4("MVP", MVP);
        glBindVertexArray(wallMesh.VAO);
        glDrawElements(GL_TRIANGLES, wallMesh.indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    // Bola
    {
        SphereObstacle bm = ball.mesh;
        bm.position = ball.pos;
        bm.rotation = ball.rollQuat;
        glm::mat4 MVP = VP * shadowMat * bm.modelMatrix();
        transfer_mat4("MVP", MVP);
        glBindVertexArray(bm.VAO);
        glDrawElements(GL_TRIANGLES, bm.indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
}


// ════════════════════════════════════════════════════════════════════════════
//  INPUT del jugador
// ════════════════════════════════════════════════════════════════════════════
void Level::handleInput(GLFWwindow* window, float dt)
{
    // debug: si se presiona espacio, se aplica una velocidad instantanea hacia arriba (salto basicamente)
    static bool prevSpace = false;
    bool inAir = (ball.pos.z > ball.radius + 0.02f);
    if(glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && !prevSpace) {  // && !prevSpace && !inAir
        ball.vel.z += 5.0f; // impulso hacia arriba
        ball.moving = true;
    }
    prevSpace = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);

    if (ball.moving || completed) return;   // no se puede disparar mientras la bola rueda o cuando se ha completado el nivel

    // ── Disparar con click izquierdo ──────────────────────────────────────
    static bool prevClick = false;
    bool curClick = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);

    if (curClick && !prevClick) {
        // Empieza a cargar
        charging  = true;
        shotPower = 0.0f;
    }

    if (charging && curClick) {
        shotPower += CHARGE_RATE * dt;
        if (shotPower > 1.0f) shotPower = 1.0f;
    }

    

    if (charging && !curClick && prevClick) {
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

    prevClick = curClick;
}


// ════════════════════════════════════════════════════════════════════════════
//  DESTROY
// ════════════════════════════════════════════════════════════════════════════
void Level::destroy()
{
    for (auto& obs : obstacles) destroy_box(obs);
    obstacles.clear();
    destroy_sphere(ball.mesh);
    for (auto& floorMesh : floorMeshes) {
        destroy_floor_mesh(floorMesh);
    }
    floorMeshes.clear();
    for (auto& wallMesh : wallMeshes) {
        destroy_wall_mesh(wallMesh);
    }
    wallMeshes.clear();
}