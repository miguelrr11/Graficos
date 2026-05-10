#include "level.h"
#include <GpO.h>
#include <cstdio>
#include <cmath>
#include <ctime>

// ─── Constantes de física ───────────────────────────────────────────────────
static float rf() { return (float)rand() / (float)RAND_MAX; }
static float rc() { return rf() * 2.0f - 1.0f; }
static float ranBetween(float min, float max) { return min + rf() * (max - min); }

static bool debugGameplay = false;

// HSV -> RGBA helper for firework colors
static glm::vec4 hsv4(float h, float s, float v) {
    float r = glm::clamp(std::fabs(std::fmod(h*6.0f+0.0f, 6.0f)-3.0f)-1.0f, 0.0f, 1.0f);
    float g = glm::clamp(std::fabs(std::fmod(h*6.0f+4.0f, 6.0f)-3.0f)-1.0f, 0.0f, 1.0f);
    float b = glm::clamp(std::fabs(std::fmod(h*6.0f+2.0f, 6.0f)-3.0f)-1.0f, 0.0f, 1.0f);
    return glm::vec4(v*glm::mix(1.0f,r,s), v*glm::mix(1.0f,g,s), v*glm::mix(1.0f,b,s), 1.0f);
}

// pre-defined colors for bonuses
static const glm::vec4 BONUS_COLORS[] = {
    hsv4(200/360.0f, 1.0f, 1.0f), // 0: salto extra (azul)
    hsv4(20/360.0f, 1.0f, 1.0f),  // 1: superman (naranja)
    hsv4(50/360.0f, 0.95f, 1.0f),   // 2: tiempo extra (dorado)
    hsv4(130/360.0f, 0.95f, 1.0f)   // 3: respawn (verde)
};

static const float GRAVITY      = -12.0f;
static const float RESTITUTION  = 0.35f;   // rebote en paredes/suelo
static const float FRICTION     = 0.99f;   // grass: multiplicador por frame (rolling)
static const float FRICTION_ICE = 0.9985f; // ice: casi sin rozamiento
static const float FRICTION_SAND= 0.965f;  // sand: frena rápido
static const float FRICTION_AIR = 0.995f;  // fricción mientras está en el aire
static const float FLOOR_Z      = 0.2f;    // altura del suelo

// ─── Constantes de disparo ──────────────────────────────────────────────────
static float MAX_POWER   = 12.0f;
static float SUPERMAN_MULT = 2.5f; // el superman multiplica tu impulso por este factor
static const float CHARGE_RATE = 0.7f;    // potencia acumulada por segundo
static const float AIM_SPEED   = 90.0f;   // grados/segundo al girar la mira
static const float STOP_SPEED2 = 0.05f;  // velocidad mínima para detener la bola
static const float HOLE_RADIUS = 0.3f;    // radio del hoyo
static const float IMPULSE     = 5.75f;    // impulso del salto

void Level::load(int levelNum, const Resources& res)
{
    texBola = res.texBola;

    completed = false;
    shotAngle = 0.0f;
    shotPower = 0.0f;
    charging  = false;
    prevSegments_ = 0;

    int heightChange = 1;
    srand(time(NULL));
    skyColorSeed = (float)(rand() % 1000) / 1000.0f;
    tracks.clear();

    // --- EL DIRECTOR DE ARCHIPIÉLAGOS ---
    // En Nivel 1 y 2 habrá 2 islas. En Nivel 3 y 4 habrá 3 islas, etc.
    int numIslands = int((2 + (levelNum - 1) / 2) * 1.5);

    // El hueco crece con la dificultad (empieza en 3m, sube 0.5m por nivel)
    float jumpDistance = 3.0f + (levelNum * 0.5f); 

    glm::vec2 currentStartPos = {0.0f, 0.0f};
    float currentStartAngle = 0.0f;

    for (int i = 0; i < numIslands; i++) {
        // Generamos tramos cortos para que las islas no sean kilométricas
        int numTramos = 2 + (rand() % 4); // de 2 a 5 tramos por isla
        
        LevelData nuevaIsla = generateTrack(currentStartPos, currentStartAngle, numTramos, levelNum);
        tracks.push_back(nuevaIsla);

        // Preparamos el salto para la siguiente isla:
        // Calculamos la dirección general de esta isla para saber hacia dónde volará el jugador
        glm::vec2 direccionIsla = glm::normalize(nuevaIsla.holePos - nuevaIsla.startPos);
        
        // La siguiente isla nacerá X metros más adelante siguiendo esa dirección
        currentStartPos = nuevaIsla.holePos + (direccionIsla * jumpDistance);
        currentStartAngle = std::atan2(direccionIsla.y, direccionIsla.x);

        if(nuevaIsla.bonusPos != glm::vec2(0.0f) && rf() < 0.7f) { // 70% de probabilidad de que aparezca un bonus en esta isla
            
            // Repartimos: 50% Salto, 30% Superman, 20% Tiempo
            float roll = rf();
            int type;
            if (roll < 0.50f)      type = 0;
            else if (roll < 0.80f) type = 1;
            else {
                if (i >= 2) {
                    type = 2;
                }
                else {
                    type = 0; // en las primeras islas no hay bonus de tiempo para no farmear tiempo.
                }
            }   
            
            // si el nivel es superior a 5, en la plataforma del medio se añade siempre un bonus de respawn (tipo 3)
            if(levelNum > 5 && i == floor(numIslands / 2)) {
                type = 3;
            }

            glm::vec3 bonusColor = BONUS_COLORS[type];
            
            BoxObstacle box = crear_box({nuevaIsla.bonusPos.x, nuevaIsla.bonusPos.y, FLOOR_Z + i*heightChange + 0.35f},
                                          {0.5f, 0.5f, 0.5f},
                                          {30, 0, 45}, bonusColor, true);

            box.isBonus = true;
            box.bonusType = type;
            
            box.eulerAnglesVel = {15.0f, 45.0f, 60.0f}; // El bonus gira sobre sí mismo
            obstacles.push_back(box);
        }
    }

    // 1. LA BOLA (Aparece en la primera isla)
    ball.pos = { tracks[0].startPos.x, tracks[0].startPos.y, FLOOR_Z + ball.radius + 0.25f };
    respawnPos = ball.pos;
    curRespawnPos = respawnPos;
    ball.vel      = { 0, 0, 0 };
    ball.moving   = true;
    ball.rollQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    ball.mesh     = crear_sphere({0,0,0}, ball.radius, {1.0f, 1.0f, 1.0f});

    // 2. EL HOYO (Aparece SOLO en la última isla, ajustado a su altura)
    holePos  = { tracks.back().holePos.x, tracks.back().holePos.y, FLOOR_Z + ((numIslands - 1) * heightChange) + 0.1f };

    float minX = 100000.0f, maxX = -100000.0f, minY = 100000.0f, maxY = -100000.0f;
    for (const auto& tile : tracks.back().floorTiles) {
        for (const auto& vert : tile) {
            if (vert.x < minX) minX = vert.x;
            if (vert.x > maxX) maxX = vert.x;
            if (vert.y < minY) minY = vert.y;
            if (vert.y > maxY) maxY = vert.y;
        }
    }
    edges = {minX, maxX, minY, maxY}; // guardamos los bordes de la última isla para centrar la cámara al empezar el juego

    printf("Nivel %d generado con %d islas. HolePos: (%.2f, %.2f, %.2f). Última isla va de (%.2f, %.2f) a (%.2f, %.2f)\n", 
            levelNum, numIslands, holePos.x, holePos.y, holePos.z, minX, minY, maxX, maxY);

    // Assign a random surface type to each island
    std::vector<SurfaceType> islandSurface(tracks.size());
    for (size_t t = 0; t < tracks.size(); ++t) {
        int r = rand() % 10;
        if(r < 6) islandSurface[t] = SurfaceType::GRASS;
        else if(r < 8) islandSurface[t] = SurfaceType::ICE;
        else islandSurface[t] = SurfaceType::SAND;

        //las primeras islas que sean cesped siempre
        if(levelNum == 1 && t < 2) islandSurface[t] = SurfaceType::GRASS;
    }

    // 3. CONSTRUCCIÓN GEOMÉTRICA (Suelo, Muros y Físicas)
    for(size_t t = 0; t < tracks.size(); ++t) {
        float alturaIsla = FLOOR_Z + t * heightChange;

        SurfaceType surf = islandSurface[t];
        GLuint      tex  = (surf == SurfaceType::ICE)  ? res.texIce
                         : (surf == SurfaceType::SAND) ? res.texSand
                                                       : res.texCesped;
        float       fric = (surf == SurfaceType::ICE)  ? FRICTION_ICE
                         : (surf == SurfaceType::SAND) ? FRICTION_SAND
                                                       : FRICTION;

        // Suelo (baldosa a baldosa)
        for (const auto& tile : tracks[t].floorTiles) {
            floorMeshes.push_back(crear_floor_mesh(tile, alturaIsla, 2.0f));
            floorMeshes.back().texID         = tex;
            floorMeshes.back().perimeter     = tile;
            floorMeshes.back().zBase         = alturaIsla;
            floorMeshes.back().surfaceType   = surf;
            floorMeshes.back().friction      = fric;
            floorMeshes.back().useCheckerboard = (surf == SurfaceType::GRASS);
        }
        // Muros visuales

        float heightWall = 0.65f;
        wallMeshes.push_back(crear_wall_mesh(tracks[t].perimeter, true, 0.4f, heightWall, alturaIsla, 1.0f));
        wallMeshes.back().texID = res.texMadera;

        // Muros físicos (invisibles)
        for (size_t i = 0; i < tracks[t].perimeter.size(); i++) {
            glm::vec2 pA = tracks[t].perimeter[i];
            glm::vec2 pB = tracks[t].perimeter[(i + 1) % tracks[t].perimeter.size()];
            glm::vec2 dir = pB - pA;
            float longitud = glm::length(dir);
            float angulo = std::atan2(dir.y, dir.x);
            glm::vec2 centro = (pA + pB) * 0.5f;

            obstacles.push_back(crear_box({ centro.x, centro.y, alturaIsla }, 
                                          { longitud, 0.4f, heightWall*2.0f }, 
                                          { 0.0f, 0.0f, glm::degrees(angulo) }, 
                                          {1,1,1}, false));
            obstacles.back().ignoreRender = true; 
        }

        // Si es la última isla, dibujamos la "pegatina" del hoyo
        if (t == tracks.size() - 1) {
            obstacles.push_back(crear_box(holePos + glm::vec3(0,0,-0.01f),
                                          { HOLE_RADIUS*2.5f, HOLE_RADIUS*2.5f, 0.05f },
                                          {0,0,0}, {1,1,1}, true, 1));
            obstacles.back().texID = res.texHoyo;
            obstacles.back().isHole = true;
        }
    }

    particles.init();
    fireworksEmitted_ = false;

    printf("Archipielago Nivel %d Generado. ¡A saltar!\n", levelNum);
}

void Level::restartLevel() {
    // reiniciar estado de bola para volver a empezar el mismo nivel
    ball.pos = curRespawnPos;
    ball.vel      = { 0, 0, 0 };
    ball.moving   = true;


    // Reiniciar obstáculos de bonus
    for (auto& obs : obstacles) {
        if (obs.isBonus) {
            printf("Reiniciando bonus en posición (%.2f, %.2f)\n", obs.position.x, obs.position.y);
            obs.isDying = false;
            if(obs.bonusType == 4) {
                obs.isDying = true;
                curRespawnPos = respawnPos;
            }
            obs.dead = false;
            obs.size = {0.5f, 0.5f, 0.5f};
        }
    }
}


// ════════════════════════════════════════════════════════════════════════════
//  UPDATE – física cada frame
// ════════════════════════════════════════════════════════════════════════════
void Level::update(float dt, std::vector<int>& bonusQueue)
{
    if (pendingTransition != PendingTransition::NONE) return;

    //update de rotación de obstáculos giratorios
    for (auto& obs : obstacles) {
        update_box(obs, dt);

        if(obs.isDying) {
            // Animación de desaparición: se hunde y se hace transparente
            obs.size *= 0.965f; // se hace más pequeño

            if(obs.size.x < 0.05f) {
                obs.dead = true;
            }
        }
    }

    particles.update(dt);   // always ticks, even when ball is stopped

    ball.prevPos  = ball.pos;
    ball.onGround = false;
    if (!ball.moving) return;

    // gravedad
    ball.vel.z += GRAVITY * dt;

    // --- MOVIMIENTO CON SUB-STEPPING (ANTI-TUNNELING) ---
    int numSteps = 1;
    float currentSpeed = glm::length(ball.vel);
    float distanceThisFrame = currentSpeed * dt;
    
    // Si la bola se va a mover de golpe más de la mitad de su radio, 
    // partimos el tiempo en cachitos más pequeños.
    if (distanceThisFrame > (ball.radius * 0.5f)) {
        numSteps = (int)std::ceil(distanceThisFrame / (ball.radius * 0.5f));
    }
    if (numSteps > 10) numSteps = 10; // Tope de seguridad para no fundir el PC
    
    float subDt = dt / numSteps;
    
    // En vez de mover y comprobar una vez, movemos un poquito y comprobamos, 
    // movemos otro poquito y comprobamos...
    for (int i = 0; i < numSteps; i++) {
        ball.pos += ball.vel * subDt;
        
        // --- COLISIONES (ahora dentro del sub-paso) ---
        resolveFloor();
        resolveWalls(bonusQueue);
    }

    // ── Partículas: hojas al rodar ────────────────────────────────────────────
    if ((ball.onGround || ball.pos.z < currentFloorZ + ball.radius + 0.2f) && ball.moving) {
        static float leafTimer = 0.0f;
        leafTimer -= dt;
        float horizSpd = glm::length(glm::vec2(ball.vel.x, ball.vel.y));
        if (horizSpd > 0.5f) {  // leafTimer <= 0.0f && 
            leafTimer = 0.015f;
            EmitParams ep;
            ep.pos        = {ball.pos.x + rc() * 0.1f, ball.pos.y + rc() * 0.1f, ball.pos.z - ball.radius * 0.5f};
            ep.vel        = {ball.vel.x * 0.05f - rf() * 0.5, ball.vel.y * 0.05f - rf() * 0.5, 0.4f + rc() * 0.4f};
            //normalize ep.vel
            ep.vel = glm::normalize(ep.vel) * (0.5f + rc() * 0.5f);
            ep.vel.z = 1.0f + rc() * 0.2f; // velocidad vertical más alta para que se vean mejor
            ep.velSpread  = {0.2f, 0.2f, 0.1f};
            ep.acc        = {0, 0, 1.0f};
            ep.life       = 0.65f;
            ep.lifeSpread = 0.12f;
            ep.size       = 0.05f + rc() * 0.025f;
            ep.endSize    = 0.0f;
            if (currentSurfaceType == SurfaceType::ICE) {
                float t   = ranBetween(0.f, 1.f);
                ep.color    = {0.6f + t*0.3f, 0.85f + t*0.1f, 1.0f, 1.0f};
                ep.endColor = {0.5f, 0.8f, 1.0f, 0.0f};
            } else if (currentSurfaceType == SurfaceType::SAND) {
                float t   = ranBetween(0.f, 1.f);
                ep.color    = {1.0f, 0.85f + t*0.1f, 0.4f + t*0.2f, 1.0f};
                ep.endColor = {0.95f, 0.75f, 0.3f, 0.0f};
            } else {
                int range   = rf()*60.0f;
                ep.color    = hsv4((range + 80)/360.0f, ranBetween(0.2f, 0.9f), ranBetween(0.75f, 0.95f));
                ep.endColor = {0.4f, 0.85f, 0.1f, 0.0f};
            }
            ep.rotVelSpread = 8.0f;
            float speed = glm::length(ball.vel);
            ep.count      = clamp(int(speed * 0.5f), 1, 3); // más velocidad = más hojas
            particles.emit(ep);
        }
    }

    // Rotación de rodadura: eje perpendicular a la velocidad horizontal (Z arriba)
    // ω = (-vy, vx, 0) / radius  ->  eje = normalize(-vy, vx, 0), ángulo = speed/radius * dt
    {
        glm::vec3 horizVel = { ball.vel.x, ball.vel.y, 0.0f };
        float speed = glm::length(horizVel) * 0.01f;
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
        for (int i = 0; i < 20; i++) {
            float angle = (float)i / 20.0f * 6.2831853f;
            glm::vec4 col = hsv4((float)i / 20.0f, 1.0f, 1.0f);
            EmitParams ep;
            ep.pos        = holePos + glm::vec3(0, 0, 0.15f);
            ep.vel        = {std::cos(angle) * 1.5f, std::sin(angle) * 1.5f, 3.5f};
            ep.velSpread  = {0.6f, 0.6f, 2.0f};
            ep.acc        = {0, 0, ranBetween(-3.0f, -5.0f)};
            ep.life       = 1.4f;
            ep.lifeSpread = 0.4f;
            ep.size       = ranBetween(0.08f, 0.15f);
            ep.endSize    = 0.02f;
            ep.color      = col;
            ep.endColor   = {col.r, col.g, col.b, 0.0f};
            ep.rotVelSpread = 6.0f;
            ep.count      = 12;
            particles.emit(ep);
        }
    }

    // Fricción y parada NORMAL (solo si no está en el agujero)
    if (!enHoyo) { // Si es superman, no frenamos nada
        if (ball.pos.z <= currentFloorZ + ball.radius + 0.02f) {
            ball.vel.x *= currentFriction;
            ball.vel.y *= currentFriction;
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

        // Si ya se ha hundido suficiente, señalamos la transición (render_scene la ejecuta)
        if (ball.pos.z < holePos.z - 0.5f) {
            printf("¡NIVEL COMPLETADO! Avanzando...\n");
            pendingTransition = PendingTransition::NEXT_LEVEL;
            return;
        }
    
    }
    // --- NUEVA CONDICIÓN DE DERROTA ---
    if (ball.pos.z < -3.0f) {
        printf("¡TE HAS CAIDO AL VACIO! Intentalo de nuevo en el mismo nivel.\n");
        pendingTransition = PendingTransition::RESTART_LEVEL;
        return;
    }

    // eliminamos obstaculos de tiempo o de respawn que ya han sido recogidos y su animación de desaparición ha terminado
    obstacles.erase(std::remove_if(obstacles.begin(), obstacles.end(),
        [](const BoxObstacle& obs) { return (obs.dead && (obs.bonusType == 4 || obs.bonusType == 2)); }), obstacles.end());
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
    float       bestZ        = -1e9f;
    float       bestFriction = FRICTION;
    SurfaceType bestSurface  = SurfaceType::GRASS;
    for (const auto& floor : floorMeshes) {
        if (floor.zBase > ball.pos.z + ball.radius) continue; // suelo sobre la bola, ignorar
        if (floor.zBase <= bestZ) continue;                   // ya encontramos uno más alto
        const auto& poly = floor.perimeter;
        // Fan-triangulation desde el vértice 0, igual que crear_floor_mesh
        for (size_t i = 1; i + 1 < poly.size(); ++i) {
            if (pointInTriangle(ballXY, poly[0], poly[i], poly[i + 1])) {
                bestZ        = floor.zBase;
                bestFriction = floor.friction;
                bestSurface  = floor.surfaceType;
                break;
            }
        }
    }

    if (bestZ > -1e9f) {
        currentFloorZ      = bestZ;
        currentFriction    = bestFriction;
        currentSurfaceType = bestSurface;
        if (ball.pos.z < bestZ + ball.radius) {
            ball.pos.z = bestZ + ball.radius;
            if (ball.vel.z < 0.0f)
                ball.vel.z *= -RESTITUTION;
            ball.onGround = true;
        }
    }
}

void Level::resolveWalls(std::vector<int>& bonusQueue)
{
    for (auto& obs : obstacles) {
        if (obs.ignoreCollision && !obs.isBonus && !obs.isDying && !obs.dead) continue;

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
        if (!obs.isBonus) ball.pos = obs.position + corrected;

        // ── 5. Reflejar velocidad en espacio local y volver al mundo ────────
        glm::vec3 velLocal;
        velLocal.x =  cosA * ball.vel.x + sinA * ball.vel.y;
        velLocal.y = -sinA * ball.vel.x + cosA * ball.vel.y;
        velLocal.z =  ball.vel.z;

        if (sign * velLocal[axis] < 0.0f)
            velLocal[axis] *= -RESTITUTION;

        if (!obs.isBonus) ball.vel.x = cosA * velLocal.x - sinA * velLocal.y;
        if (!obs.isBonus) ball.vel.y = sinA * velLocal.x + cosA * velLocal.y;
        if (!obs.isBonus) ball.vel.z = velLocal.z;

        if(obs.isBonus && !obs.isDying && !obs.dead && obs.bonusType != 4) {
            if(obs.bonusType >= 0 && obs.bonusType < 3){ 
                bonusQueue.push_back(obs.bonusType);
                obs.isDying = true;
            }
            else if(obs.bonusType == 3) {
                obs.bonusType = 4; // el punto de respawn se activa
                obs.eulerAnglesVel *= 5.0f;
                curRespawnPos = obs.position + glm::vec3(0,0,1.0f);
            }

            for (int i = 0; i < 20; i++) {
                float angle = (float)i / 20.0f * 6.2831853f;
                EmitParams ep;
                ep.pos        = obs.position + glm::vec3(0, 0, 0.15f);
                ep.vel        = {std::cos(angle) * 1.5f, std::sin(angle) * 1.5f, 3.5f};
                ep.velSpread  = {0.6f, 0.6f, 2.0f};
                ep.acc        = {0, 0, ranBetween(-3.0f, -5.0f)};
                ep.life       = 1.4f;
                ep.lifeSpread = 0.4f;
                ep.size       = ranBetween(0.08f, 0.15f);
                ep.endSize    = 0.02f;
                int range     = rf()*60.0f;
                ep.color      = {obs.color.r, obs.color.g, obs.color.b, 1.0f};
                ep.endColor   = hsv4((range + 180)/360.0f, ranBetween(0.2f, 0.9f), ranBetween(0.75f, 0.95f));
                ep.rotVelSpread = 6.0f;
                ep.count      = 12;
                particles.emit(ep);
            }
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  RENDER
// ════════════════════════════════════════════════════════════════════════════
void Level::render(GLuint prog, const glm::mat4& VP, const std::vector<int>& bonusQueue)
{
    // Obstáculos
    for (const auto& obs : obstacles){
        render_box(obs, prog, VP, obs.texID); // Le pasamos un 0 temporalmente

        if(obs.isBonus && !obs.dead) {
            EmitParams ep;
            float ranAngle = rf() * 6.2831853f;
            if(obs.bonusType != 4){ 
                ep.vel = {-std::sin(ranAngle)*1.5f, std::cos(ranAngle)*1.5f, ranBetween(0.5f, 1.0f)};
                ep.pos        = {obs.position.x + std::cos(ranAngle)*0.8f, obs.position.y + std::sin(ranAngle)*0.8f, obs.position.z};
                ep.life       = 3.0f;
            }
            if(obs.bonusType == 4){ 
                ep.vel = {-std::sin(ranAngle)*0.8f, std::cos(ranAngle)*0.8f, ranBetween(3.0f, 4.0f)};
                ep.pos        = {obs.position.x + std::cos(ranAngle)*0.25f, obs.position.y + std::sin(ranAngle)*0.25f, obs.position.z};
                ep.life       = 20.0f;
            }
            ep.velSpread  = {0,0,0};
            ep.acc        = {0,0,0};
            ep.lifeSpread = 0.12f;
            ep.size       = 0.1f + rc() * 0.025f;
            ep.endSize    = 0.0f;
            int range     = rf()*60.0f;
            ep.color      = {obs.color.r, obs.color.g, obs.color.b, 1.0f};
            ep.endColor   = {obs.color.r, obs.color.g, obs.color.b, 0.0f};
            ep.rotVelSpread = 8.0f;
            ep.count      = 2;
            particles.emit(ep);
        }

    }

    if(!bonusQueue.empty()){
        int lastBonusType = bonusQueue.back();
        EmitParams ep;
        float ranAngle = rf() * 6.2831853f;
        ep.pos        = {ball.pos.x + std::cos(ranAngle)*0.4f, ball.pos.y + std::sin(ranAngle)*0.4f, ball.pos.z};
        ep.vel        = {-std::sin(ranAngle)*1.5f, std::cos(ranAngle)*1.5f, ranBetween(0.4f, 0.7f)};
        ep.velSpread  = {0,0,0};
        ep.acc        = {0,0,0};
        ep.life       = 3.0f;
        ep.lifeSpread = 0.12f;
        ep.size       = 0.05f + rc() * 0.025f;
        ep.endSize    = 0.0f;
        int range     = rf()*60.0f;
        ep.color      = BONUS_COLORS[lastBonusType];
        ep.endColor   = {BONUS_COLORS[lastBonusType].r, BONUS_COLORS[lastBonusType].g, BONUS_COLORS[lastBonusType].b, 0.0f};
        ep.rotVelSpread = 8.0f;
        ep.count      = 1;
        particles.emit(ep);
    }

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
    float rad = glm::radians(shotAngle);
    glm::vec3 dir = { std::cos(rad), std::sin(rad), 0.0f };
    glm::vec3 arrowPos = ball.pos + dir * (ball.radius * 3.0f);

    SphereObstacle arrow = ball.mesh;
    arrow.position = arrowPos;
    arrow.radius     = ball.radius * 0.25f;
    // arrow.color    = { 1.0f, 1.0f - shotPower, 0.0f };   // amarillo -> rojo al cargar
    //render_sphere(arrow, prog, VP, 0); // <-- AÑADIDO: Pasamos 0 (sin textura)

    int max_segs = 10;
    if(bonusQueue.size() > 0 && bonusQueue.back() == 1) { // Si el último bonus es superman, aumentamos el número de segmentos
        max_segs = int(max_segs * SUPERMAN_MULT);
    }
    int numSegments = fmax(1, floor(shotPower * max_segs)); // De 0 a max_segs segmentos según la potencia
    for(int i = 1; i <= numSegments; ++i) {
        glm::vec3 segPos = ball.pos + dir * (ball.radius * (3.0f + i));
        arrow.position = segPos;
        arrow.color = { 1.0f, ((1.0f - shotPower)*i/numSegments), 0.0f }; // semitransparente
        render_sphere(arrow, prog, VP, 0);
    }
    

}


// ════════════════════════════════════════════════════════════════════════════
//  SHADOWS
// ════════════════════════════════════════════════════════════════════════════
void Level::renderShadows(GLuint shadow_prog, const glm::mat4& VP, glm::vec3 lightPos)
{
    // Collect one floor height per island (floors are pushed in island order,
    // so the first tile of each new zBase marks a new island).
    std::vector<float> islandZ;
    for (const auto& fm : floorMeshes) {
        if (islandZ.empty() || fm.zBase != islandZ.back())
            islandZ.push_back(fm.zBase);
    }
    if (islandZ.empty()) return;

    // Build a shadow matrix that projects onto the plane z = gz.
    float lx = lightPos.x, ly = lightPos.y, lz = lightPos.z;
    auto shadowMat = [&](float gz) -> glm::mat4 {
        float d = lz - gz;
        return glm::mat4(
             d,       0.f,   0.f,  0.f,
             0.f,     d,     0.f,  0.f,
            -lx,     -ly,   -gz,  -1.f,
             gz*lx,   gz*ly, gz*lz, lz
        );
    };

    // Find the highest floor height at or below a given world-z (for obstacles / ball).
    auto floorBelow = [&](float z) -> float {
        float best = islandZ[0];
        for (float h : islandZ)
            if (h <= z + 0.01f && h > best) best = h;
        return best;
    };

    glUseProgram(shadow_prog);

    // Wall meshes: wallMeshes[t] belongs to island t.
    for (size_t t = 0; t < wallMeshes.size(); ++t) {
        float gz  = (t < islandZ.size()) ? islandZ[t] : islandZ.back();
        glm::mat4 MVP = VP * shadowMat(gz);
        transfer_mat4("MVP", MVP);
        glBindVertexArray(wallMeshes[t].VAO);
        glDrawElements(GL_TRIANGLES, wallMeshes[t].indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    // Obstacles (bonus boxes, hole decal – skip invisible physics boxes).
    for (const auto& obs : obstacles) {
        if (obs.ignoreRender || obs.isHole) continue;
        if (obs.size.x * obs.size.y > 50.0f) continue;
        float gz  = floorBelow(obs.position.z);
        glm::mat4 MVP = VP * shadowMat(gz) * obs.modelMatrix();
        transfer_mat4("MVP", MVP);
        glBindVertexArray(obs.VAO);
        glDrawElements(GL_TRIANGLES, obs.indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    // Ball: project onto whatever floor it is currently on.
    {
        float gz = currentFloorZ;
        SphereObstacle bm = ball.mesh;
        bm.position = ball.pos;
        bm.rotation = ball.rollQuat;
        glm::mat4 MVP = VP * shadowMat(gz) * bm.modelMatrix();
        transfer_mat4("MVP", MVP);
        glBindVertexArray(bm.VAO);
        glDrawElements(GL_TRIANGLES, bm.indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
}


void Level::handleInput(GLFWwindow* window, float dt, std::vector<int>& bonusQueue)
{
    static bool hasClickedInAir = false;
    static bool prevSpace = false;
    static int  numTimesSpacePressed = 0;

    bool inAir = !ball.onGround;

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && !prevSpace) {
        numTimesSpacePressed++;
        float impulse = IMPULSE / (numTimesSpacePressed * (numTimesSpacePressed * 0.95f));
        //if the last bonusqueue is a jump bonus, we add more impulse (0 es salto extra)
        if(!bonusQueue.empty() && bonusQueue.back() == 0) {
            impulse += IMPULSE;
            bonusQueue.pop_back();
        }
        ball.vel.z += impulse;
        if(debugGameplay) ball.vel.z += IMPULSE; // modo dios
        ball.moving = true;
        inAir = true;
        float delta = (numTimesSpacePressed)*0.1f;
        for (float i = 0; i < 6.2831853f; i+=delta) {
            float angle = i;
            glm::vec4 col = hsv4((rf()*30.0f)/360.0f, 1.0f, 1.0f);
            EmitParams ep;
            ep.pos        = ball.pos;
            float r = ranBetween(-0.15f, 0.15f);
            ep.vel        = {std::cos(angle) * 1.5f + r, std::sin(angle) * 1.5f + r, 1.5f};
            ep.velSpread  = {0, 0, 0.5f};
            ep.acc        = {0.2f, 0.2f, 0};
            ep.life       = 1.0f;
            ep.lifeSpread = 0.3f;
            ep.size       = 0.1f;
            ep.endSize    = 0.02f;
            ep.color      = col;
            ep.endColor   = {col.r, col.g, col.b, 0.0f};
            ep.rotVelSpread = 6.0f;
            ep.count      = 1;
            particles.emit(ep);
        }
        printf("¡Salto! x%d  impulso=%.2f  bonus=%d\n",
               numTimesSpacePressed, impulse, (int)bonusQueue.size());
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
        shotPower *= 1.0125f;
        int maxShotPower = 1.0f;
        if(bonusQueue.size() > 0 && bonusQueue.back() == 1) { // Si el último bonus es superman, aumentamos el número de segmentos
            maxShotPower *= SUPERMAN_MULT;
        }
        if (shotPower > maxShotPower) shotPower = maxShotPower;

        int max_segs = 10;
        if(bonusQueue.size() > 0 && bonusQueue.back() == 1) { // Si el último bonus es superman, aumentamos el número de segmentos
            max_segs = int(max_segs * SUPERMAN_MULT);
        }

        int newSeg = (int)fmax(1.0f, floor(shotPower * max_segs));
        if (newSeg > prevSegments_ && soloud && sfxBeep) {
            float pitch = 0.8f + (newSeg - 1) * 0.07f; //cambiamos de pitch para cada segmento
            SoLoud::handle h = soloud->play(*sfxBeep);
            soloud->setVolume(h, 0.1f);
            soloud->setRelativePlaySpeed(h, pitch);
        }
        prevSegments_ = newSeg;
    }

    if (charging && !curClick && prevClick) {  //  && (!inAir || !hasClickedInAir) no se porque puse esta restriccion, la quito
        float rad   = glm::radians(shotAngle);
        float power = shotPower * MAX_POWER;

        if(bonusQueue.size() > 0 && bonusQueue.back() == 1) { // Si el último bonus es superman, aumentamos el número de segmentos
            bonusQueue.pop_back(); // Consumimos el bonus de superman al disparar
            printf("¡Bonus Superman consumido! Disparo potenciado.\n");
        }

        ball.vel.x += std::cos(rad) * power;
        ball.vel.y += std::sin(rad) * power;

        ball.moving = true;
        charging    = false;
        shotPower   = 0.0f;
        prevSegments_ = 0;

        if (inAir) hasClickedInAir = true;

        printf("Disparo -> angulo: %.1f  potencia: %.1f m/s    inAir: %s\n",
               shotAngle, power, inAir ? "Si" : "No");
    }

    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
        printf("Posicion bola: (%.2f, %.2f, %.2f)   Velocidad: (%.2f, %.2f, %.2f)   En aire: %s  hasClickedInAir: %s skyColorSeed: %.2f, nObs: %d\n",
            ball.pos.x, ball.pos.y, ball.pos.z,
            ball.vel.x, ball.vel.y, ball.vel.z,
            inAir ? "Si" : "No",
            hasClickedInAir ? "Si" : "No",
            skyColorSeed, (int)obstacles.size());
    }

    prevClick = curClick;
}

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