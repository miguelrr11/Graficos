#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

struct FloorMesh {
    GLuint VAO = 0, VBO = 0, EBO = 0;
    int    indexCount = 0;
    GLuint texID = 0;
};

// Triangula el polígono del perímetro y crea un mesh plano en z=zBase.
// uvScale: unidades de mundo por repetición de textura (ej. 2.0 = la textura se repite cada 2u).
FloorMesh crear_floor_mesh(const std::vector<glm::vec2>& perimeter,
                           float zBase, float uvScale = 2.0f);

void render_floor_mesh(const FloorMesh& fm, GLuint prog, const glm::mat4& VP);
void destroy_floor_mesh(FloorMesh& fm);
