#define _USE_MATH_DEFINES
#include <cmath>
#include "sphere.h"
#include <vector>
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

    // Generación de vértices (DENTRO DE crear_sphere)
    for (int i = 0; i <= stacks; ++i) {
        float v = (float)i / stacks;
        float phi = v * M_PI;

        for (int j = 0; j <= slices; ++j) {
            float u = (float)j / slices;
            float theta = u * 2.0f * M_PI;

            float x = sin(phi) * cos(theta);
            float y = cos(phi);
            float z = sin(phi) * sin(theta);

            // posición 
            vertices.push_back(x * 0.5f);
            vertices.push_back(y * 0.5f);
            vertices.push_back(z * 0.5f);
            // normal
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
            // TEXTURA (U, V) <-- LO NUEVO
            vertices.push_back(u);
            vertices.push_back(v);
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

    // Atributos
    int stride = 8 * sizeof(float); // ANTES ERA 6, AHORA 8

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2); // TEXTURA
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));

    glBindVertexArray(0);

    return s;
}

void render_sphere(const SphereObstacle& s, GLuint prog, const glm::mat4& VP, GLuint texID)
{
    glm::mat4 M   = s.modelMatrix();
    glm::mat4 MVP = VP * M;

    // 1. Enviamos las matrices a la GPU
    transfer_mat4("MVP", MVP);
    GLint mLoc = glGetUniformLocation(prog, "M");
    if (mLoc != -1) glUniformMatrix4fv(mLoc, 1, GL_FALSE, &M[0][0]);

    // 2. Enviamos el color (ahora usando "uColor" para que coincida con el shader)
    GLint colorLoc = glGetUniformLocation(prog, "uColor");
    if (colorLoc != -1) glUniform3fv(colorLoc, 1, &s.color[0]);

    // 3. Activamos la textura
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texID);
    transfer_int("tex", 0);
    
    // 4. El Interruptor: Si texID es 0 (como en el indicador), mandamos un 0.
    // Si tiene una textura (como en la bola), mandamos un 1.
    transfer_int("uUseTex", (texID == 0) ? 0 : 1);

    // 5. Dibujamos
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