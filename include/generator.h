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
    std::vector<glm::vec2> perimeter; // El polígono final de los muros
    glm::vec2 startPos;               // Donde ponemos la bola
    glm::vec2 holePos;                // Donde ponemos el hoyo
};

// Función principal
LevelData generateTrack(glm::vec2 startPos, float startAngle, int numSegments, int difficulty);