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
    int tile = 1;                   // para repetir la textura (1 = sin repetir, 2 = el doble, etc.)
    bool ignoreCollision = false;   // para obstáculos decorativos que no bloquean la bola
    bool ignoreRender = false;      // para obstáculos que sí bloquean la bola pero no se dibujan (ej. el suelo)
    bool isBonus = false;           // para distinguir los obstáculos de bonus (que se pintan de blanco y dan puntos al tocar)
    int bonusType = 0;              // 0 = salto extra, 1 = superman, 2 = tiempo extra, 3 = respawn (desactivado), 4 = respawn (activado)

    /**
     * Explicacion del bonus 3
     * Solo spawnea 1 bonus de este tipo por nivel, y solo niveles mas altos que el 8
     * Una vez se coge, el bonus se marca como tipo 4 y el punto de respawn se actualiza a la posición del bonus
     * Una vez el jugador se cae y respawnea donde el bonus, este se muere y no se vuelve poder a coger, ademas
     * el punto de respawn se reinicia al inicial (solo tienes una oportunidad de usar el bonus de respawn)
     */

    glm::vec3 eulerAnglesVel = glm::vec3(0.0f); // velocidad de rotación en grados por segundo, para obstáculos giratorios
    bool isDying = false; // para hacer animaciones de desaparición al recoger un bonus
    bool dead = false; // para marcar obstáculos de bonus que ya fueron recogidos y deben desaparecer

    GLuint VAO, VBO, EBO;
    int indexCount;

    GLuint texID = 0;
    bool isHole  = false;

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
                      glm::vec3 color       = glm::vec3(0.6f, 0.3f, 0.1f),
                      bool ignoreCollision = false,
                      int tile = 1
                    );

void render_box(const BoxObstacle& box, GLuint prog, const glm::mat4& VP, GLuint texID);
void destroy_box(BoxObstacle& box);
void update_box(BoxObstacle& box, float deltaTime);