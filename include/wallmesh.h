#pragma once
#include <vector>
#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

struct WallMesh {
    GLuint VAO = 0, VBO = 0, EBO = 0;
    int indexCount = 0;
    GLuint texID = 0;
};

// points: esquinas en orden (cerrado automáticamente si closed=true)
// uvTile: cada cuántas unidades de mundo se repite la textura
WallMesh crear_wall_mesh(const std::vector<glm::vec2>& points,
                         bool  closed,
                         float thickness,
                         float height,
                         float zBase  = 0.0f,
                         float uvTile = 1.0f);

void render_wall_mesh(const WallMesh& wm, GLuint prog, const glm::mat4& VP);
void destroy_wall_mesh(WallMesh& wm);