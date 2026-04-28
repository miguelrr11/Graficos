#include "generator.h"
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <iostream>

// Función matemática auxiliar: ¿Se cruzan dos líneas (A-B y C-D)?
bool ccw(glm::vec2 A, glm::vec2 B, glm::vec2 C) {
    return (C.y - A.y) * (B.x - A.x) > (B.y - A.y) * (C.x - A.x);
}
bool intersect(glm::vec2 p1, glm::vec2 p2, glm::vec2 p3, glm::vec2 p4) {
    return ccw(p1, p3, p4) != ccw(p2, p3, p4) && ccw(p1, p2, p3) != ccw(p1, p2, p4);
}

LevelData generateTrack(int numSegments) {
    LevelData level;
    srand(time(NULL));

    // --- FASE 1: LA ESPINA DORSAL (Self-Avoiding Walk) ---
    std::vector<TrackNode> spine;
    
    // Nodo 0: El Tee de salida en el origen
    spine.push_back({ glm::vec2(0.0f, 0.0f), 6.0f }); 
    float currentAngle = 0.0f; 

    for (int i = 0; i < numSegments; i++) {
        bool validPoint = false;
        glm::vec2 candidatePos;
        float newAngle;

        // Intentamos generar un punto válido (máximo 30 intentos antes de abortar)
        for (int attempts = 0; attempts < 30; attempts++) {
            // 1. Tramos más cortos pero con curvas más bestias
            float dist = 4.0f + static_cast<float>(rand() % 60) / 10.0f; // De 4m a 10m
            float angleDeg = -90.0f + static_cast<float>(rand() % 181);  // De -90º a +90º (curvas cerradas)
            newAngle = currentAngle + glm::radians(angleDeg);

            candidatePos.x = spine.back().pos.x + std::cos(newAngle) * dist;
            candidatePos.y = spine.back().pos.y + std::sin(newAngle) * dist;

            // Comprobar colisión con tramos anteriores
            bool collision = false;
            if (spine.size() > 2) {
                // Comprobamos contra todos los segmentos excepto el último (que obviamente tocamos)
                for (size_t j = 0; j < spine.size() - 2; j++) {
                    if (intersect(spine[j].pos, spine[j+1].pos, spine.back().pos, candidatePos)) {
                        collision = true;
                        break;
                    }
                }
            }

            if (!collision) {
                validPoint = true;
                break;
            }
        }

        if (validPoint) {
            TrackNode newNode;
            newNode.pos = candidatePos;
            
            // --- FASE 2: ANCHURA VARIABLE ---
            int roll = rand() % 10;
            if (i == numSegments - 1) newNode.width = 12.0f;      // Green Final INMENSO
            else if (roll <= 2)       newNode.width = 1.5f;       // 30% Embudo cabrón
            else if (roll >= 6)       newNode.width = 10.0f;      // 40% Plaza gigante
            else                      newNode.width = 4.0f;       // 30% Pasillo normal
            
            spine.push_back(newNode);
            currentAngle = newAngle;
        } else {
            std::cout << "[INFO] El circuito se cerró. Abortando en segmento " << i << "\n";
            spine.back().width = 8.0f; // Forzamos el green final aquí
            break; 
        }
    }

    // --- FASE 3: EXTRUSIÓN DEL PERÍMETRO ---
    std::vector<glm::vec2> leftSide;
    std::vector<glm::vec2> rightSide;

    for (size_t i = 0; i < spine.size(); i++) {
        glm::vec2 dir;
        // Calcular la dirección para sacar la perpendicular
        if (i == 0) {
            dir = glm::normalize(spine[1].pos - spine[0].pos);
        } else if (i == spine.size() - 1) {
            dir = glm::normalize(spine[i].pos - spine[i-1].pos);
        } else {
            // Suavizado de curva: media del vector anterior y el siguiente
            glm::vec2 d1 = glm::normalize(spine[i].pos - spine[i-1].pos);
            glm::vec2 d2 = glm::normalize(spine[i+1].pos - spine[i].pos);
            dir = glm::normalize(d1 + d2); 
            if (glm::length(dir) < 0.1f) dir = d1; // Evitar división por 0 en curvas extremas
        }

        glm::vec2 perp(-dir.y, dir.x); // Rotar 90º

        // Desplazamos el punto a la izquierda y a la derecha según la anchura
        leftSide.push_back(spine[i].pos + perp * (spine[i].width * 0.5f));
        rightSide.push_back(spine[i].pos - perp * (spine[i].width * 0.5f));
    }

    // Construir el polígono final (Vamos por la izquierda y volvemos por la derecha)
    for (size_t i = 0; i < leftSide.size(); i++) {
        level.perimeter.push_back(leftSide[i]);
    }
    for (int i = rightSide.size() - 1; i >= 0; i--) {
        level.perimeter.push_back(rightSide[i]);
    }

    // Guardar posiciones críticas
    level.startPos = spine.front().pos;
    level.holePos = spine.back().pos;

    // Empujamos la bola 2 metros hacia adentro del circuito
    glm::vec2 startDir = glm::normalize(spine[1].pos - spine[0].pos);
    level.startPos = spine[0].pos + startDir * 2.0f;

    // Empujamos el hoyo 2 metros hacia atrás desde el final
    glm::vec2 endDir = glm::normalize(spine[spine.size() - 2].pos - spine.back().pos);
    level.holePos = spine.back().pos + endDir * 2.0f;

    return level;
}