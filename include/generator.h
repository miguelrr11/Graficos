#pragma once
#include <vector>
#include <glm/glm.hpp>

// La estructura de nuestro "Esqueleto"
struct TrackNode {
    glm::vec2 pos;
    float width;
};

// Lo que el generador le devuelve al motor del juego
struct LevelData {
    std::vector<glm::vec2> perimeter; 
    std::vector<std::vector<glm::vec2>> floorTiles; // NUEVO: La lista de baldosas del suelo
    glm::vec2 startPos;               
    glm::vec2 holePos;                
};

// Función principal
LevelData generateTrack(glm::vec2 startPos, float startAngle, int numSegments, int difficulty);