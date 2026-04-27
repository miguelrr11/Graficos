#pragma once

#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

struct SphereObstacle {
    glm::vec3 position;
    float radius;
    glm::vec3 color;
    bool ignoreCollision = false;

    GLuint VAO, VBO, EBO;
    int indexCount;

    glm::mat4 modelMatrix() const {
        glm::mat4 M = glm::translate(glm::mat4(1.0f), position);
        M = glm::scale(M, glm::vec3(radius * 2.0f)); // diámetro
        return M;
    }
};

SphereObstacle crear_sphere(glm::vec3 position, float radius,
                            glm::vec3 color = glm::vec3(0.7f, 0.7f, 0.7f),
                            bool ignoreCollision = false,
                            int stacks = 16, int slices = 16);

void render_sphere(const SphereObstacle& sphere, GLuint prog, const glm::mat4& VP);
void destroy_sphere(SphereObstacle& sphere);