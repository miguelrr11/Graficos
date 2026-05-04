#include "level.h"
#include <GpO.h>
#include <cstdio>
#include <cmath>
#include <ctime>

// ─── Constantes de física ───────────────────────────────────────────────────
static float rf() { return (float)rand() / (float)RAND_MAX; }
static float rc() { return rf() * 2.0f - 1.0f; }

// HSV → RGBA helper for firework colors
static glm::vec4 hsv4(float h, float s, float v) {
    float r = glm::clamp(std::fabs(std::fmod(h*6.0f+0.0f, 6.0f)-3.0f)-1.0f, 0.0f, 1.0f);
    float g = glm::clamp(std::fabs(std::fmod(h*6.0f+4.0f, 6.0f)-3.0f)-1.0f, 0.0f, 1.0f);
    float b = glm::clamp(std::fabs(std::fmod(h*6.0f+2.0f, 6.0f)-3.0f)-1.0f, 0.0f, 1.0f);
    return glm::vec4(v*glm::mix(1.0f,r,s), v*glm::mix(1.0f,g,s), v*glm::mix(1.0f,b,s), 1.0f);
}

static const float GRAVITY     = -12.0f;
static const float RESTITUTION = 0.35f;   // rebote en paredes/suelo
static const float FRICTION    = 0.988f;  // multiplicador por frame (rolling)
static const float FRICTION_AIR = 0.995f; // fricción mientras está en el aire 
static const float FLOOR_Z     = 0.0f;    // altura del suelo

// ─── Constantes de disparo ──────────────────────────────────────────────────
static const float MAX_POWER   = 12.0f;
static const float CHARGE_RATE = 0.7f;    // potencia acumulada por segundo
static const float AIM_SPEED   = 90.0f;   // grados/segundo al girar la mira
static const float STOP_SPEED2 = 0.05f;  // velocidad mínima para detener la bola
static const float HOLE_RADIUS = 0.3f;    // radio del hoyo

void Level::load()
{
    // Cargar texturas 
    texCesped = cargar_textura(getAssetPath("cesped.jpg").c_str());
    texMadera = cargar_textura(getAssetPath("madera2.jpg").c_str());
    texHoyo   = cargar_textura(getAssetPath("hoyo.png").c_str());
    texBola   = cargar_textura(getAssetPath("bola.jpg").c_str());
    completed = false;
    shotAngle = 0.0f;
    shotPower = 0.0f;
    charging  = false;

//     int heightChange = 1;

//     std::vector<std::vector<glm::vec2>> trackPerimeters = {
//     {{63.0f, 81.0f}, {67.0f, 83.0f}, {69.0f, 81.0f}, {69.0f, 76.0f}, {67.0f, 72.0f}, {63.0f, 72.0f}, {56.0f, 67.0f}, {56.0f, 69.0f}, {60.0f, 74.0f}},

//     {{51.0f, 67.0f}, {54.0f, 65.0f}, {47.0f, 56.0f}, {42.0f, 58.0f}, {42.0f, 63.0f}},
//     {{40.0f, 56.0f}, {47.0f, 54.0f}, {47.0f, 51.0f}, {42.0f, 49.0f}, {40.0f, 49.0f}, {38.0f, 54.0f}},
//     {{45.0f, 47.0f}, {49.0f, 49.0f}, {56.0f, 49.0f}, {58.0f, 42.0f}, {54.0f, 40.0f}, {54.0f, 45.0f}},
//     {{56.0f, 38.0f}, {60.0f, 42.0f}, {65.0f, 40.0f}, {63.0f, 36.0f}, {58.0f, 33.0f}}

//     };
//     ball.pos = { 67.5f, 81.0f, 0*heightChange };
//     holePos = { 60.8f, 38.3f, (FLOOR_Z+0.1f)+4*heightChange };
    

//     size_t numTracks = trackPerimeters.size();
    
//     for(size_t t = 0; t < numTracks; ++t) {
//         // 2. GENERAR LA PISTA (6 segmentos de longitud)
//         // Usamos el algoritmo SAW + Anchura Variable que diseñamos
//         LevelData track;
//         track.perimeter = trackPerimeters[t];
//         tracks.push_back(track);

//         // 3. CÉSPED SOLO EN EL AREA DEL PERIMETRO
//         floorMeshes.push_back(crear_floor_mesh(tracks.back().perimeter, FLOOR_Z + t*heightChange, 2.0f));
//         floorMeshes.back().texID          = texCesped;
//         floorMeshes.back().perimeter      = trackPerimeters[t];
//         floorMeshes.back().zBase          = FLOOR_Z + t * heightChange;
//         floorMeshes.back().useCheckerboard = true;

//         // 4. EL MURO VISUAL
//         // Pasamos: (puntos, cerrado, grosor=0.4f, altura=1.0f, zBase=FLOOR_Z, uvTile=1.0f)
//         wallMeshes.push_back(crear_wall_mesh(tracks.back().perimeter, true, 0.4f, 0.5f, FLOOR_Z + t*heightChange, 1.0f));
//         wallMeshes.back().texID = texMadera;

//         // 6. EL HOYO
//         if(t == numTracks - 1){
//             //holePos = { 0, 0, FLOOR_Z + 0.02f };
//             obstacles.push_back(crear_box(holePos + glm::vec3(0,0,-0.01f),
//                                         { HOLE_RADIUS*2.5f, HOLE_RADIUS*2.5f, 0.05f },
//                                         {0,0,0}, {1,1,1}, true, 1));
//             obstacles.back().texID = texHoyo;
//             obstacles.back().isHole = true;
//         }
//     }

//     // 7. LA BOLA
//     //ball.pos      = { 15, 70, FLOOR_Z + ball.radius };
//     ball.vel      = { 0, 0, 0 };
//     ball.moving   = true;
//     ball.rollQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
//     ball.mesh     = crear_sphere({0,0,0}, ball.radius, {1.0f, 1.0f, 1.0f});

//     ball.pos.z += ball.radius + 0.25f; // para que empiece ligeramente por encima del suelo

//     // 5. LAS FÍSICAS (Cajas invisibles rotadas)
//     for(size_t t = 0; t < tracks.size(); t++) {
//         const LevelData& track = this->tracks[t];
//         for (size_t i = 0; i < track.perimeter.size(); i++) {
//             glm::vec2 pA = track.perimeter[i];
//             glm::vec2 pB = track.perimeter[(i + 1) % track.perimeter.size()];
            
//             glm::vec2 dir = pB - pA;
//             float longitud = glm::length(dir);
//             float angulo = std::atan2(dir.y, dir.x);
//             glm::vec2 centro = (pA + pB) * 0.5f;

//             // Creamos la caja física: Invisible pero colisionable
//             obstacles.push_back(crear_box({ centro.x, centro.y, FLOOR_Z + t*heightChange }, 
//                                         { longitud, 0.4f, 1.0f }, // Grosor del muro de 0.4f
//                                         { 0.0f, 0.0f, glm::degrees(angulo) }, 
//                                         {1,1,1}, false));
//             obstacles.back().ignoreRender = true; 
//         }
//     }

    

//     printf("Nivel Procedural Cargado. ¡A jugar!\n");
// }
    int heightChange = 1;
    srand(time(NULL));
    skyColorSeed = (float)(rand() % 1000) / 1000.0f;
    tracks.clear();

    // --- EL DIRECTOR DE ARCHIPIÉLAGOS ---
    // En Nivel 1 y 2 habrá 2 islas. En Nivel 3 y 4 habrá 3 islas, etc.
    int numIslands = int((2 + (currentLevel - 1) / 2) * 1.5); 
    
    // El hueco crece con la dificultad (empieza en 3m, sube 0.5m por nivel)
    float jumpDistance = 3.0f + (currentLevel * 0.5f); 

    glm::vec2 currentStartPos = {0.0f, 0.0f};
    float currentStartAngle = 0.0f;

    for (int i = 0; i < numIslands; i++) {
        // Generamos tramos cortos para que las islas no sean kilométricas
        int numTramos = 2 + (rand() % 4); // de 2 a 5 tramos por isla
        
        LevelData nuevaIsla = generateTrack(currentStartPos, currentStartAngle, numTramos, currentLevel);
        tracks.push_back(nuevaIsla);

        // Preparamos el salto para la siguiente isla:
        // Calculamos la dirección general de esta isla para saber hacia dónde volará el jugador
        glm::vec2 direccionIsla = glm::normalize(nuevaIsla.holePos - nuevaIsla.startPos);
        
        // La siguiente isla nacerá X metros más adelante siguiendo esa dirección
        currentStartPos = nuevaIsla.holePos + (direccionIsla * jumpDistance);
        currentStartAngle = std::atan2(direccionIsla.y, direccionIsla.x);
    }

    // 1. LA BOLA (Aparece en la primera isla)
    ball.pos = { tracks[0].startPos.x, tracks[0].startPos.y, FLOOR_Z + ball.radius + 0.25f };
    ball.vel      = { 0, 0, 0 };
    ball.moving   = true;
    ball.rollQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    ball.mesh     = crear_sphere({0,0,0}, ball.radius, {1.0f, 1.0f, 1.0f});

    // 2. EL HOYO (Aparece SOLO en la última isla, ajustado a su altura)
    holePos  = { tracks.back().holePos.x, tracks.back().holePos.y, FLOOR_Z + ((numIslands - 1) * heightChange) + 0.1f };

    // 3. CONSTRUCCIÓN GEOMÉTRICA (Césped, Muros y Físicas)
    for(size_t t = 0; t < tracks.size(); ++t) {
        float alturaIsla = FLOOR_Z + t * heightChange;

        // Césped (Ahora lo montamos baldosa a baldosa)
        for (const auto& tile : tracks[t].floorTiles) {
            floorMeshes.push_back(crear_floor_mesh(tile, alturaIsla, 2.0f));
            floorMeshes.back().texID = texCesped;
            floorMeshes.back().perimeter = tile; // El "perímetro" físico de esta baldosa
            floorMeshes.back().zBase = alturaIsla;
            floorMeshes.back().useCheckerboard = true;
        }
        // Muros visuales
        wallMeshes.push_back(crear_wall_mesh(tracks[t].perimeter, true, 0.4f, 0.5f, alturaIsla, 1.0f));
        wallMeshes.back().texID = texMadera;

        // Muros físicos (invisibles)
        for (size_t i = 0; i < tracks[t].perimeter.size(); i++) {
            glm::vec2 pA = tracks[t].perimeter[i];
            glm::vec2 pB = tracks[t].perimeter[(i + 1) % tracks[t].perimeter.size()];
            glm::vec2 dir = pB - pA;
            float longitud = glm::length(dir);
            float angulo = std::atan2(dir.y, dir.x);
            glm::vec2 centro = (pA + pB) * 0.5f;

            obstacles.push_back(crear_box({ centro.x, centro.y, alturaIsla }, 
                                          { longitud, 0.4f, 1.0f }, 
                                          { 0.0f, 0.0f, glm::degrees(angulo) }, 
                                          {1,1,1}, false));
            obstacles.back().ignoreRender = true; 
        }

        // Si es la última isla, dibujamos la "pegatina" del hoyo
        if (t == tracks.size() - 1) {
            obstacles.push_back(crear_box(holePos + glm::vec3(0,0,-0.01f),
                                          { HOLE_RADIUS*2.5f, HOLE_RADIUS*2.5f, 0.05f },
                                          {0,0,0}, {1,1,1}, true, 1));
            obstacles.back().texID = texHoyo;
            obstacles.back().isHole = true;
        }
    }

    particles.init();
    fireworksEmitted_ = false;

    printf("Archipielago Nivel %d Generado. ¡A saltar!\n", currentLevel);
}
void Level::restartLevel() {
    // reiniciar estado de bola para volver a empezar el mismo nivel
    ball.pos = { tracks[0].startPos.x, tracks[0].startPos.y, FLOOR_Z + ball.radius + 0.25f };
    ball.vel      = { 0, 0, 0 };
    ball.moving   = true;
}


// ════════════════════════════════════════════════════════════════════════════
//  UPDATE – física cada frame
// ════════════════════════════════════════════════════════════════════════════
void Level::update(float dt)
{
    particles.update(dt);   // always ticks, even when ball is stopped

    ball.prevPos  = ball.pos;
    ball.onGround = false;
    if (!ball.moving) return;

    // gravedad
    ball.vel.z += GRAVITY * dt;
    ball.pos += ball.vel * dt;

    resolveFloor();
    resolveWalls();

    // ── Partículas: hojas al rodar ────────────────────────────────────────────
    if (ball.onGround && ball.moving) {
        static float leafTimer = 0.0f;
        leafTimer -= dt;
        float horizSpd = glm::length(glm::vec2(ball.vel.x, ball.vel.y));
        if (horizSpd > 0.5f) {  // leafTimer <= 0.0f && 
            leafTimer = 0.03f;
            EmitParams ep;
            ep.pos        = {ball.pos.x + rc() * 0.1f, ball.pos.y + rc() * 0.1f, ball.pos.z - ball.radius * 0.5f};
            ep.vel        = {-ball.vel.x * 0.05f + rc() * 0.5, -ball.vel.y * 0.05f + rc() * 0.5, 0.4f + rc() * 0.4f};
            //normalize ep.vel
            ep.vel = glm::normalize(ep.vel) * (0.5f + rc() * 0.5f);
            ep.vel.z = 1.0f + rc() * 0.2f; // velocidad vertical más alta para que se vean mejor
            ep.velSpread  = {0.2f, 0.2f, 0.1f};
            ep.acc        = {0, 0, 1.0f};
            ep.life       = 0.65f;
            ep.lifeSpread = 0.12f;
            ep.size       = 0.05f + rc() * 0.025f;
            ep.endSize    = 0.0f;
            ep.color      = {0.2f, 0.6f + rc() * 0.3f, 0.1f, 1.0f};
            ep.endColor   = {0.4f, 0.85f, 0.1f, 0.0f};
            ep.rotVelSpread = 8.0f;
            ep.count      = 5;
            particles.emit(ep);
        }
    }

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

    // ── Partículas: fuegos artificiales al caer en el hoyo ────────────────────
    if (enHoyo && !fireworksEmitted_) {
        fireworksEmitted_ = true;
        for (int i = 0; i < 8; i++) {
            float angle = (float)i / 8.0f * 6.2831853f;
            glm::vec4 col = hsv4((float)i / 8.0f, 1.0f, 1.0f);
            EmitParams ep;
            ep.pos        = holePos + glm::vec3(0, 0, 0.15f);
            ep.vel        = {std::cos(angle) * 1.5f, std::sin(angle) * 1.5f, 3.5f};
            ep.velSpread  = {0.6f, 0.6f, 0.8f};
            ep.acc        = {0, 0, -6.0f};
            ep.life       = 1.4f;
            ep.lifeSpread = 0.4f;
            ep.size       = 0.13f;
            ep.endSize    = 0.02f;
            ep.color      = col;
            ep.endColor   = {col.r, col.g, col.b, 0.0f};
            ep.rotVelSpread = 6.0f;
            ep.count      = 8;
            particles.emit(ep);
        }
    }

    // Fricción y parada NORMAL (solo si no está en el agujero)
    if (!enHoyo) {
        if (ball.pos.z <= currentFloorZ + ball.radius + 0.02f) {
            ball.vel.x *= FRICTION;
            ball.vel.y *= FRICTION;
        }
        else{
            ball.vel.x *= FRICTION_AIR;
            ball.vel.y *= FRICTION_AIR;
            //ball.vel.z *= FRICTION_AIR;
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

        // Si ya se ha hundido suficiente, ¡PASAMOS DE NIVEL!
        if (ball.pos.z < -0.5f + currentFloorZ) {
            printf("¡NIVEL %d COMPLETADO! Avanzando...\n", currentLevel);
            currentLevel++;  // Subimos la dificultad
            destroy();       // Limpiamos la memoria del nivel anterior
            load();          // Generamos el nuevo nivel
            return;          // Salimos del update inmediatamente
        }
    
    }
    // --- NUEVA CONDICIÓN DE DERROTA ---
    if (ball.pos.z < -3.0f) {
        // printf("¡TE HAS CAÍDO AL VACÍO! Game Over. Vuelta al Nivel 1.\n");
        // currentLevel = 1; // Castigo brutal
        // destroy();
        printf("¡TE HAS CAIDO AL VACIO! Intentalo de nuevo en el mismo nivel.\n");
        restartLevel(); // Simplemente reiniciamos el mismo nivel para que lo intente de nuevo
        //load();
        return;
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
            ball.onGround = true;
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

        // ball resting on top face of a wall
        if (axis == 2 && sign == 1.0f) ball.onGround = true;

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
    //if (!ball.moving && !completed) {
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
    //}

    particles.render(VP, camRight, camUp);
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
    static bool hasClickedInAir = false;
    static bool prevSpace = false;
    static int  numTimesSpacePressed = 0;

    bool inAir = !ball.onGround;

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && !prevSpace) {
        numTimesSpacePressed++;
        float impulse = 5.5f / (numTimesSpacePressed * numTimesSpacePressed);
        ball.vel.z += impulse;
        ball.moving = true;
        inAir = true;
        printf("¡Salto! Veces presionado: %d, Velocidad Z aplicada: %.2f\n",
               numTimesSpacePressed, impulse);
    }
    prevSpace = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);

    if (!inAir) {
        numTimesSpacePressed = 0;
        hasClickedInAir      = false;
    }

    // ── Disparar con click izquierdo ──────────────────────────────────────
    static bool prevClick = false;
    bool curClick = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);

    if (curClick && !prevClick) {
        charging  = true;
        shotPower = 0.0f;
    }

    if (charging && curClick) {
        shotPower += CHARGE_RATE * dt;
        // dont make it linear
        shotPower *= 1.01f;
        if (shotPower > 1.0f) shotPower = 1.0f;
    }

    if (charging && !curClick && prevClick && (!inAir || !hasClickedInAir)) {
        float rad   = glm::radians(shotAngle);
        float power = shotPower * MAX_POWER;

        ball.vel.x += std::cos(rad) * power;
        ball.vel.y += std::sin(rad) * power;

        ball.moving = true;
        charging    = false;
        shotPower   = 0.0f;

        if (inAir) hasClickedInAir = true;

        printf("Disparo → angulo: %.1f°  potencia: %.1f m/s    inAir: %s\n",
               shotAngle, power, inAir ? "Si" : "No");
    }

    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
        printf("Posición bola: (%.2f, %.2f, %.2f)   Velocidad: (%.2f, %.2f, %.2f)   En aire: %s  hasClickedInAir: %s skyColorSeed: %.2f\n",
            ball.pos.x, ball.pos.y, ball.pos.z,
            ball.vel.x, ball.vel.y, ball.vel.z,
            inAir ? "Si" : "No",
            hasClickedInAir ? "Si" : "No",
            skyColorSeed);
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
    particles.destroy();
}