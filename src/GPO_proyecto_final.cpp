/************************  GPO_01 ************************************
ATG, 2019
******************************************************************************/

#include <GpO.h>
#include "obstacle.h"
#include "level.h"       // <-- nivel de minigolf
#include <vector>

// TAMAÑO y TITULO INICIAL de la VENTANA
int ANCHO = 800, ALTO = 600;
const char* prac = "MiniGolf 3D (GpO)";

#define GLSL(src) "#version 330 core\n" #src

// ─── Skybox ──────────────────────────────────────────────────────────────────
GLuint skybox_prog;
GLuint skyboxVAO, skyboxVBO;

const char* skybox_vs = GLSL(
    layout(location = 0) in vec3 pos;
    out vec3 dir;
    uniform mat4 VP;
    void main() {
        dir = pos;
        vec4 p = VP * vec4(pos, 1.0);
        gl_Position = p.xyww;
    }
);

const char* skybox_fs = GLSL(
    in vec3 dir;
    out vec4 FragColor;
    void main() {
        vec3 d = normalize(dir);
        float t = 0.5 * (d.z + 1.0);
        vec3 skyTop    = vec3(0.2, 0.5, 0.9);
        vec3 skyBottom = vec3(0.9, 0.9, 1.0);
        FragColor = vec4(mix(skyBottom, skyTop, t), 1.0);
    }
);

// ─── Shaders principales ─────────────────────────────────────────────────────
const char* vertex_prog = GLSL(
    layout(location = 0) in vec3 pos;
    layout(location = 1) in vec3 normal;
    layout(location = 2) in vec2 uv;

    out vec3 fragNormal;       // normal en mundo (para iluminación)
    out vec3 fragLocalNormal;  // normal en espacio objeto (para mapeo esférico)
    out vec3 fragPos;
    out vec2 fragUV;

    uniform mat4 MVP = mat4(1.0f);
    uniform mat4 M   = mat4(1.0f);

    void main() {
        gl_Position     = MVP * vec4(pos, 1.0);
        fragPos         = vec3(M * vec4(pos, 1.0));
        fragNormal      = mat3(transpose(inverse(M))) * normal;
        fragLocalNormal = normal;     // sin transformar: gira con el objeto
        fragUV          = uv;
    }
);

const char* fragment_prog = GLSL(
    in vec3 fragNormal;
    in vec3 fragLocalNormal;
    in vec3 fragPos;
    in vec2 fragUV;
    out vec3 outputColor;

    uniform vec3      uColor      = vec3(1.0);
    uniform sampler2D tex;
    uniform int       uUseTex;

    uniform vec3  uLightPos   = vec3(5.0, 5.0, 100.0);
    uniform vec3  uLightColor = vec3(1.0);
    uniform float uAmbient    = 0.4f;

    uniform float uSteps     = 6.0;
    uniform float uPixelSize = 128.0;

    uniform float uTile;
    uniform vec2  uTexOffset;
    uniform int   uUseTexOffset;
    uniform int   uMappingType; // 0 = UV normal, 1 = mapeo esférico

    void main() {
        vec2 uv      = fragUV;
        vec4 baseTex = vec4(1.0);

        if (uUseTex >= 1) {

            if (uMappingType == 0) {
                // --- UV normal (cajas, planos) ---
                if (uUseTexOffset == 1) {
                    uv += uTexOffset;
                }

                vec2 tiledUV = uv * uTile;

                vec2 finalUV = (uPixelSize > 1.0)
                    ? floor(tiledUV * uPixelSize) / uPixelSize
                    : tiledUV;

                baseTex = texture(tex, finalUV);
            }
            else if (uMappingType == 1) {
                // --- mapeo esférico por normal local (esferas) ---
                vec3 n = normalize(fragLocalNormal);

                vec2 sphUV = vec2(
                    atan(n.z, n.x) / 6.2831853 + 0.5,  // longitud
                    asin(n.y)      / 3.1415927 + 0.5   // latitud
                );

                vec2 tiledUV = sphUV * uTile;

                vec2 finalUV = (uPixelSize > 1.0)
                    ? floor(tiledUV * uPixelSize) / uPixelSize
                    : tiledUV;

                baseTex = texture(tex, finalUV);
            }

            if (uUseTex == 2 && abs(fragNormal.z) < 0.5) discard;
            if (baseTex.a < 0.1) discard;
        }

        vec3 norm     = normalize(fragNormal);
        vec3 lightDir = normalize(uLightPos - fragPos);

        float diff = max(dot(norm, lightDir), 0.0);
        diff = floor(diff * uSteps) / uSteps;

        vec3 ambient = uAmbient * uLightColor;
        vec3 color   = (ambient + diff * uLightColor) * (baseTex.rgb * uColor);

        float levels = 5.0;
        color = floor(color * levels) / levels;

        outputColor = color;
    }
);

// ─── Globals ──────────────────────────────────────────────────────────────────
GLFWwindow* window;
GLuint      prog;
Level       level;          // ← el nivel de juego

// Cámara
vec3  up       = vec3(0, 0, 1);
float cam_yaw  = 180.0f;
float cam_pitch = -15.0f;
float lastX = 400.0f, lastY = 300.0f;
bool  firstMouse = true;
float mouseSensitivity = 0.1f;
float fov    = 80.0f;
float aspect = 4.0f / 3.0f;
float cam_speed = 5.0f;
float cam_distance = 3.0f;   // distancia a la bola

// Tiempo para deltaTime real
float lastFrameTime = 0.0f;
float dt = 0.0f;


// ─── Skybox helper ───────────────────────────────────────────────────────────
void crear_skybox()
{
    float v[] = {
        -1,-1, 1,  1,-1, 1,  1, 1, 1,  -1,-1, 1,  1, 1, 1,  -1, 1, 1,
        -1,-1,-1, -1, 1,-1,  1, 1,-1,  -1,-1,-1,  1, 1,-1,   1,-1,-1,
        -1, 1,-1, -1, 1, 1,  1, 1, 1,  -1, 1,-1,  1, 1, 1,   1, 1,-1,
        -1,-1,-1,  1,-1,-1,  1,-1, 1,  -1,-1,-1,  1,-1, 1,  -1,-1, 1,
         1,-1,-1,  1, 1,-1,  1, 1, 1,   1,-1,-1,  1, 1, 1,   1,-1, 1,
        -1,-1,-1, -1,-1, 1, -1, 1, 1,  -1,-1,-1, -1, 1, 1,  -1, 1,-1
    };
    glGenVertexArrays(1, &skyboxVAO);  glBindVertexArray(skyboxVAO);
    glGenBuffers(1, &skyboxVBO);       glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), 0);
    glBindVertexArray(0);
}


// ─── Inicialización ───────────────────────────────────────────────────────────
void init_scene()
{
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    glViewport(0, 0, w, h);
    glEnable(GL_DEPTH_TEST);

    // Shaders principales
    GLuint VS = compilar_shader(vertex_prog,   GL_VERTEX_SHADER);
    GLuint FS = compilar_shader(fragment_prog, GL_FRAGMENT_SHADER);
    prog = glCreateProgram();
    glAttachShader(prog, VS);  glAttachShader(prog, FS);
    glLinkProgram(prog);       check_errores_programa(prog);
    glDetachShader(prog, VS);  glDeleteShader(VS);
    glDetachShader(prog, FS);  glDeleteShader(FS);
    glUseProgram(prog);

    // Cargar nivel
    level.load();

    // Skybox
    crear_skybox();
    GLuint SVS = compilar_shader(skybox_vs, GL_VERTEX_SHADER);
    GLuint SFS = compilar_shader(skybox_fs, GL_FRAGMENT_SHADER);
    skybox_prog = glCreateProgram();
    glAttachShader(skybox_prog, SVS);  glAttachShader(skybox_prog, SFS);
    glLinkProgram(skybox_prog);        check_errores_programa(skybox_prog);
    glDetachShader(skybox_prog, SVS);  glDeleteShader(SVS);
    glDetachShader(skybox_prog, SFS);  glDeleteShader(SFS);

    lastFrameTime = (float)glfwGetTime();
}


// ─── Render ───────────────────────────────────────────────────────────────────
void render_scene()
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // deltaTime real
    float now = (float)glfwGetTime();
    dt  = now - lastFrameTime;
    lastFrameTime = now;
    if (dt > 0.05f) dt = 0.05f;  // cap para evitar saltos si la ventana se mueve

    // Input de juego (flechas + espacio)
    level.handleInput(window, dt);

    // Física
    level.update(dt);

	//camara sigue a la bola
	vec3 target = level.ball.pos;

	vec3 camPos;

	camPos.x = target.x + cam_distance * cos(glm::radians(cam_pitch)) * cos(glm::radians(cam_yaw));
	camPos.y = target.y + cam_distance * cos(glm::radians(cam_pitch)) * sin(glm::radians(cam_yaw));
	camPos.z = target.z + cam_distance * sin(glm::radians(cam_pitch));

    // Matrices
    mat4 P = perspective(glm::radians(fov), aspect, 0.5f, 40.0f);
    mat4 V = lookAt(camPos, target, up);
    mat4 VP = P * V;

    // Renderizar nivel (obstáculos + bola + indicador de disparo)
    glUseProgram(prog);
    level.render(prog, VP);

    // Skybox
    glDepthFunc(GL_LEQUAL);
    glUseProgram(skybox_prog);
    mat4 VP_sky = P * mat4(mat3(V));    // sin traslación
    transfer_mat4("VP", VP_sky);
    glBindVertexArray(skyboxVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS);
}


// ─── Main ─────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[])
{
    init_GLFW();
    window = Init_Window(prac);
    load_Opengl();
    init_scene();

    glfwSwapInterval(1);
    while (!glfwWindowShouldClose(window)) {
        render_scene();
        glfwSwapBuffers(window);
        glfwPollEvents();
        show_info();
    }

    level.destroy();
    glfwTerminate();
    exit(EXIT_SUCCESS);
}


// ─── Callbacks ────────────────────────────────────────────────────────────────
void show_info()
{
    static int fps = 0;
    static double last_tt = 0;
    char buf[128];
    fps++;
    double tt = glfwGetTime();
    double elapsed = tt - last_tt;
    if (elapsed >= 0.5) {
        const char* estado = level.completed  ? "COMPLETADO!" :
                             level.ball.moving ? "Rodando..." :
                             level.charging    ? "Cargando..." : "Listo";
        sprintf_s(buf, 128, "%s | %.0f FPS | %s | Potencia: %.0f%%",
                  prac, fps/elapsed, estado, level.shotPower * 100.0f);
        glfwSetWindowTitle(window, buf);
        last_tt = tt; fps = 0;
    }
}

void ResizeCallback(GLFWwindow* window, int width, int height)
{
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    ALTO = height; ANCHO = width;
    aspect = (float)width / (float)height;
}

static void KeyCallback(GLFWwindow* window, int key, int code, int action, int mode)
{
    if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, true);
    // R: reiniciar nivel
    if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        level.destroy();
        level.load();
    }
}

void MouseCallback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse) {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
    }

    float dx = (float)xpos - lastX;
    float dy = (float)ypos - lastY;

    cam_yaw   += dx * mouseSensitivity;
    cam_pitch -= -dy * mouseSensitivity;  // invertido (más natural)

    cam_pitch = glm::clamp(cam_pitch, -89.0f, 89.0f);

    lastX = (float)xpos;
    lastY = (float)ypos;
}

void asigna_funciones_callback(GLFWwindow* window)
{
    glfwSetWindowSizeCallback(window,  ResizeCallback);
    glfwSetKeyCallback(window,         KeyCallback);
    glfwSetCursorPosCallback(window,   MouseCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}