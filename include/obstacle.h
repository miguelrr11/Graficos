#pragma once

// OpenGL
#include <glad/glad.h>

// GLM - estos tres son los que suelen faltar
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>   // translate, rotate, scale, radians
#include <glm/gtc/type_ptr.hpp>           // value_ptr

#include <vector>

struct BoxObstacle {
    glm::vec3 position;
    glm::vec3 size;
    glm::vec3 eulerAngles;
    glm::vec3 color;

    GLuint VAO, VBO, EBO;
    int indexCount;

    glm::vec3 halfExtents() const { return size * 0.5f; }

    glm::mat4 modelMatrix() const {
        glm::mat4 M = glm::translate(glm::mat4(1.0f), position);
        M = glm::rotate(M, glm::radians(eulerAngles.x), glm::vec3(1,0,0));
        M = glm::rotate(M, glm::radians(eulerAngles.y), glm::vec3(0,1,0));
        M = glm::rotate(M, glm::radians(eulerAngles.z), glm::vec3(0,0,1));
        M = glm::scale(M, size);
        return M;
    }
};

BoxObstacle crear_box(glm::vec3 position, glm::vec3 size,
                      glm::vec3 eulerAngles = glm::vec3(0.0f),
                      glm::vec3 color       = glm::vec3(0.6f, 0.3f, 0.1f));

void render_box(const BoxObstacle& box, GLuint prog, const glm::mat4& VP);
void destroy_box(BoxObstacle& box);