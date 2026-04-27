#include "sphere.h"
#include <vector>
#include <cmath>
#include <GpO.h>

SphereObstacle crear_sphere(glm::vec3 position, float radius,
                            glm::vec3 color, bool ignoreCollision,
                            int stacks, int slices)
{
    SphereObstacle s;
    s.position = position;
    s.radius   = radius;
    s.color    = color;
    s.ignoreCollision = ignoreCollision;

    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    // Generación de vértices
    for (int i = 0; i <= stacks; ++i) {
        float v = (float)i / stacks;
        float phi = v * M_PI;

        for (int j = 0; j <= slices; ++j) {
            float u = (float)j / slices;
            float theta = u * 2.0f * M_PI;

            float x = sin(phi) * cos(theta);
            float y = cos(phi);
            float z = sin(phi) * sin(theta);

            // posición (unit sphere)
            vertices.push_back(x * 0.5f);
            vertices.push_back(y * 0.5f);
            vertices.push_back(z * 0.5f);

            // normal
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
        }
    }

    // Índices
    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            int first  = i * (slices + 1) + j;
            int second = first + slices + 1;

            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);

            indices.push_back(second);
            indices.push_back(second + 1);
            indices.push_back(first + 1);
        }
    }

    s.indexCount = (int)indices.size();

    // OpenGL buffers
    glGenVertexArrays(1, &s.VAO);
    glBindVertexArray(s.VAO);

    glGenBuffers(1, &s.VBO);
    glBindBuffer(GL_ARRAY_BUFFER, s.VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &s.EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    int stride = 6 * sizeof(float);

    // posición
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);

    // normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));

    glBindVertexArray(0);

    return s;
}

void render_sphere(const SphereObstacle& s, GLuint prog, const glm::mat4& VP)
{
    glm::mat4 M   = s.modelMatrix();
    glm::mat4 MVP = VP * M;

    transfer_mat4("MVP", MVP);

    GLint mLoc     = glGetUniformLocation(prog, "M");
    GLint colorLoc = glGetUniformLocation(prog, "uColor");

    glUniformMatrix4fv(mLoc, 1, GL_FALSE, glm::value_ptr(M));
    glUniform3fv(colorLoc, 1, glm::value_ptr(s.color));

    glBindVertexArray(s.VAO);
    glDrawElements(GL_TRIANGLES, s.indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void destroy_sphere(SphereObstacle& s)
{
    glDeleteBuffers(1, &s.VBO);
    glDeleteBuffers(1, &s.EBO);
    glDeleteVertexArrays(1, &s.VAO);
}