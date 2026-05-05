#include "obstacle.h"
#include <GpO.h>

// Caja unitaria centrada en el origen [-0.5, 0.5] en cada eje.
// El scale de modelMatrix() la estira a las dimensiones reales.
// Así los half-extents son siempre size*0.5, independiente de la malla.

// Ahora cada vértice tiene: x, y, z, nx, ny, nz
// Usamos 24 vértices (4 por cara) para que las normales sean correctas por cara
static const GLfloat box_vertices[] = {
    // pos                  // normal      // UV (u, v)
    // -Z
    -0.5f,-0.5f,-0.5f,   0, 0,-1,  0.0f, 0.0f,
     0.5f,-0.5f,-0.5f,   0, 0,-1,  1.0f, 0.0f,
     0.5f, 0.5f,-0.5f,   0, 0,-1,  1.0f, 1.0f,
    -0.5f, 0.5f,-0.5f,   0, 0,-1,  0.0f, 1.0f,
    // +Z
    -0.5f,-0.5f, 0.5f,   0, 0, 1,  0.0f, 0.0f,
     0.5f,-0.5f, 0.5f,   0, 0, 1,  1.0f, 0.0f,
     0.5f, 0.5f, 0.5f,   0, 0, 1,  1.0f, 1.0f,
    -0.5f, 0.5f, 0.5f,   0, 0, 1,  0.0f, 1.0f,
    // -X
    -0.5f, 0.5f, 0.5f,  -1, 0, 0,  1.0f, 0.0f,
    -0.5f, 0.5f,-0.5f,  -1, 0, 0,  1.0f, 1.0f,
    -0.5f,-0.5f,-0.5f,  -1, 0, 0,  0.0f, 1.0f,
    -0.5f,-0.5f, 0.5f,  -1, 0, 0,  0.0f, 0.0f,
    // +X
     0.5f, 0.5f, 0.5f,   1, 0, 0,  1.0f, 0.0f,
     0.5f, 0.5f,-0.5f,   1, 0, 0,  1.0f, 1.0f,
     0.5f,-0.5f,-0.5f,   1, 0, 0,  0.0f, 1.0f,
     0.5f,-0.5f, 0.5f,   1, 0, 0,  0.0f, 0.0f,
    // -Y
    -0.5f,-0.5f,-0.5f,   0,-1, 0,  0.0f, 1.0f,
     0.5f,-0.5f,-0.5f,   0,-1, 0,  1.0f, 1.0f,
     0.5f,-0.5f, 0.5f,   0,-1, 0,  1.0f, 0.0f,
    -0.5f,-0.5f, 0.5f,   0,-1, 0,  0.0f, 0.0f,
    // +Y
    -0.5f, 0.5f,-0.5f,   0, 1, 0,  0.0f, 1.0f,
     0.5f, 0.5f,-0.5f,   0, 1, 0,  1.0f, 1.0f,
     0.5f, 0.5f, 0.5f,   0, 1, 0,  1.0f, 0.0f,
    -0.5f, 0.5f, 0.5f,   0, 1, 0,  0.0f, 0.0f
};

static const GLuint box_indices[] = {
     0, 1, 2,  2, 3, 0,   // -Z
     4, 5, 6,  6, 7, 4,   // +Z
     8, 9,10, 10,11, 8,   // -X
    12,13,14, 14,15,12,   // +X
    16,17,18, 18,19,16,   // -Y
    20,21,22, 22,23,20,   // +Y
};

BoxObstacle crear_box(glm::vec3 position, glm::vec3 size,
                      glm::vec3 eulerAngles, glm::vec3 color, bool ignoreCollision, int tile)
{
    BoxObstacle box;
    box.position    = position;
    box.size        = size;
    box.eulerAngles = eulerAngles;
    box.color       = color;
    box.ignoreCollision = ignoreCollision || false;
    box.indexCount  = 36;
    box.tile        = tile;

    box.eulerAnglesVel = glm::vec3(0.0f); // Por defecto, sin rotación

    glGenVertexArrays(1, &box.VAO);
    glBindVertexArray(box.VAO);

    // VBO
    glGenBuffers(1, &box.VBO);
    glBindBuffer(GL_ARRAY_BUFFER, box.VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(box_vertices), box_vertices, GL_STATIC_DRAW);

    // EBO
    glGenBuffers(1, &box.EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, box.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(box_indices), box_indices, GL_STATIC_DRAW);

    // Atributos (IMPORTANTE: dentro del VAO)
    int stride = 8 * sizeof(float);

    // posición (location = 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);

    // normal (location = 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));

    // TEXTURA (location = 2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));

    glBindVertexArray(0);

    box.indexCount = 36;

    return box;
}

void render_box(const BoxObstacle& box, GLuint prog, const glm::mat4& VP, GLuint texID)
{
    if(box.ignoreRender) return; // No renderizar este obstáculo
    glm::mat4 M   = box.modelMatrix();
    glm::mat4 MVP = VP * M;

    transfer_mat4("MVP", MVP);

    GLint mLoc     = glGetUniformLocation(prog, "M");
    if (mLoc != -1) glUniformMatrix4fv(mLoc, 1, GL_FALSE, &M[0][0]);

    // ARREGLO: Antes ponía "baseColor". Ahora usa el mismo que la bola.
    GLint colorLoc = glGetUniformLocation(prog, "uColor");
    if (colorLoc != -1) glUniform3fv(colorLoc, 1, &box.color[0]);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, box.texID); 
    transfer_int("tex", 0);
    transfer_float("uTile", box.tile);

    // Si texID es 0, apagamos el interruptor. Si tiene textura, lo encendemos.
    int modoTextura = 0;
    if (texID != 0) modoTextura = 1;      // Caja normal con textura
    if (box.isHole) modoTextura = 2;      // Hoyo
    transfer_int("uMappingType", 0); // Para cajas, siempre usar mapeo UV normal
    transfer_int("uUseTex", modoTextura);
    transfer_int("uUseTexOffset", 0); // Por defecto, no usar desplazamiento de textura

    glBindVertexArray(box.VAO);
    glDrawElements(GL_TRIANGLES, box.indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void destroy_box(BoxObstacle& box)
{
    glDeleteBuffers(1, &box.VBO);
    glDeleteBuffers(1, &box.EBO);
    glDeleteVertexArrays(1, &box.VAO);
}

void update_box(BoxObstacle& box, float deltaTime)
{
    // Si el obstáculo tiene velocidad de rotación, actualizamos sus ángulos
    if (box.eulerAnglesVel != glm::vec3(0.0f)) {
        box.eulerAngles += box.eulerAnglesVel * deltaTime;

        // Mantener los ángulos entre 0 y 360 grados para evitar overflow
        box.eulerAngles = glm::mod(box.eulerAngles, glm::vec3(360.0f));
    }
}