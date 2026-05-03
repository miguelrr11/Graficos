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

// ─── Sombras ──────────────────────────────────────────────────────────────────
GLuint shadow_prog;

// ─── Post-proceso ─────────────────────────────────────────────────────────────
GLuint quad_prog;
GLuint quadVAO, quadVBO;
GLuint fbo, fboColorTex, fboDepthTex;
float  pixelSize = 3.0f;   // píxeles de pantalla por "píxel de juego"

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

const char* quad_vs = GLSL(
    layout(location = 0) in vec2 pos;
    layout(location = 1) in vec2 uv;
    out vec2 fragUV;
    void main() {
        fragUV      = uv;
        gl_Position = vec4(pos, 0.0, 1.0);
    }
);

const char* quad_fs = GLSL(
    in vec2 fragUV;
    out vec4 FragColor;

    uniform sampler2D screenTex;
    uniform sampler2D depthTex;
    uniform vec2  resolution;
    uniform float pixelSize;
    uniform float uNear;
    uniform float uFar;
    uniform float uFogStart;
    uniform float uFogEnd;
    uniform vec3  uFogColor;

    float linearDepth(float d) {
        return uNear * uFar / (uFar - d * (uFar - uNear));
    }

    void main() {
        // ── 1. Pixelación ──────────────────────────────────────────────────
        vec2 block = floor(fragUV * resolution / pixelSize);
        vec2 uv    = (block * pixelSize + pixelSize * 0.5) / resolution;
        vec3 color = texture(screenTex, uv).rgb;

        // ── 2. Outlines (bordes por diferencia de profundidad lineal) ─────────
        vec2 off = pixelSize / resolution;
        float rawD = texture(depthTex, uv).r;
        float d    = linearDepth(rawD);
        float dR   = linearDepth(texture(depthTex, uv + vec2(off.x,  0.0)).r);
        float dU   = linearDepth(texture(depthTex, uv + vec2(0.0,  off.y)).r);
        float dL   = linearDepth(texture(depthTex, uv - vec2(off.x,  0.0)).r);
        float dD   = linearDepth(texture(depthTex, uv - vec2(0.0,  off.y)).r);
        float edge = max(max(abs(d-dR), abs(d-dL)), max(abs(d-dU), abs(d-dD)));
        color = mix(color, vec3(0.04, 0.04, 0.08), step(0.15, edge / max(d, 0.1)) * 0.88);

        // ── 3. Scanlines (una línea oscura cada 2 filas de píxeles) ────────
        float row = mod(block.y, 2.0);
        color *= (row < 1.0) ? 0.9 : 1.0;

        // ── 4. Niebla con dithering ordenado (Bayer 4x4) ───────────────────
        float linD = d;  // ya linearizado arriba
        float fogF = clamp((uFogEnd - linD) / (uFogEnd - uFogStart), 0.0, 1.0);

        float bayer[16];
        bayer[0]  =  0.0/16.0; bayer[1]  =  8.0/16.0;
        bayer[2]  =  2.0/16.0; bayer[3]  = 10.0/16.0;
        bayer[4]  = 12.0/16.0; bayer[5]  =  4.0/16.0;
        bayer[6]  = 14.0/16.0; bayer[7]  =  6.0/16.0;
        bayer[8]  =  3.0/16.0; bayer[9]  = 11.0/16.0;
        bayer[10] =  1.0/16.0; bayer[11] =  9.0/16.0;
        bayer[12] = 15.0/16.0; bayer[13] =  7.0/16.0;
        bayer[14] = 13.0/16.0; bayer[15] =  5.0/16.0;

        int bx = int(mod(block.x, 4.0));
        int by = int(mod(block.y, 4.0));
        if (fogF < bayer[by * 4 + bx]) color = uFogColor;

        // ── 5. Vignette ────────────────────────────────────────────────────
        vec2  vc = fragUV - 0.5;
        float dist = dot(vc, vc);
        float v = smoothstep(0.8, 0.2, dist);

        color *= v;

        FragColor = vec4(color, 1.0);
    }
);

const char* shadow_vs = GLSL(
    layout(location = 0) in vec3 pos;
    uniform mat4 MVP;
    void main() {
        gl_Position = MVP * vec4(pos, 1.0);
    }
);

const char* shadow_fs = GLSL(
    out vec4 FragColor;
    void main() {
        FragColor = vec4(0.0, 0.0, 0.0, 0.45);
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
    uniform int   uMappingType;    // 0 = UV normal, 1 = mapeo esférico
    uniform int   uCheckerboard;   // 1 = aplicar patrón ajedrez sobre la textura

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

                if (uCheckerboard == 1) {
                    ivec2 cell = ivec2(floor(tiledUV*2));
                    if (((cell.x + cell.y) & 1) == 1)
                        baseTex.rgb *= 0.7;

                }
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


// Proyecta geometría sobre el plano z=0 desde la posición de luz L
static glm::mat4 makeShadowMatrix(glm::vec3 L)
{
    float lx = L.x, ly = L.y, lz = L.z;
    return glm::mat4(
         lz,  0.f, 0.f, 0.f,   // col 0
         0.f, lz,  0.f, 0.f,   // col 1
        -lx, -ly,  0.f,-1.f,   // col 2
         0.f, 0.f, 0.f, lz     // col 3
    );
}

// ─── FBO para post-proceso ───────────────────────────────────────────────────
void setup_fbo(int w, int h)
{
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenTextures(1, &fboColorTex);
    glBindTexture(GL_TEXTURE_2D, fboColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboColorTex, 0);

    glGenTextures(1, &fboDepthTex);
    glBindTexture(GL_TEXTURE_2D, fboDepthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, fboDepthTex, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void destroy_fbo()
{
    glDeleteTextures(1, &fboColorTex);
    glDeleteTextures(1, &fboDepthTex);
    glDeleteFramebuffers(1, &fbo);
}

void setup_quad()
{
    float v[] = {
        -1.f,-1.f, 0.f,0.f,   1.f,-1.f, 1.f,0.f,   1.f, 1.f, 1.f,1.f,
        -1.f,-1.f, 0.f,0.f,   1.f, 1.f, 1.f,1.f,  -1.f, 1.f, 0.f,1.f
    };
    glGenVertexArrays(1, &quadVAO);  glBindVertexArray(quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
    glBindVertexArray(0);
}

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
    ANCHO = w; ALTO = h;
    aspect = (float)w / (float)h;
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

    // FBO + quad de post-proceso
    setup_fbo(w, h);
    setup_quad();
    {
        GLuint QVS = compilar_shader(quad_vs, GL_VERTEX_SHADER);
        GLuint QFS = compilar_shader(quad_fs, GL_FRAGMENT_SHADER);
        quad_prog = glCreateProgram();
        glAttachShader(quad_prog, QVS);  glAttachShader(quad_prog, QFS);
        glLinkProgram(quad_prog);        check_errores_programa(quad_prog);
        glDetachShader(quad_prog, QVS);  glDeleteShader(QVS);
        glDetachShader(quad_prog, QFS);  glDeleteShader(QFS);
    }

    // Shader de sombras
    {
        GLuint SVS = compilar_shader(shadow_vs, GL_VERTEX_SHADER);
        GLuint SFS = compilar_shader(shadow_fs, GL_FRAGMENT_SHADER);
        shadow_prog = glCreateProgram();
        glAttachShader(shadow_prog, SVS);  glAttachShader(shadow_prog, SFS);
        glLinkProgram(shadow_prog);        check_errores_programa(shadow_prog);
        glDetachShader(shadow_prog, SVS);  glDeleteShader(SVS);
        glDetachShader(shadow_prog, SFS);  glDeleteShader(SFS);
    }

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
    // deltaTime real
    float now = (float)glfwGetTime();
    dt  = now - lastFrameTime;
    lastFrameTime = now;
    if (dt > 0.05f) dt = 0.05f;

    // La mira sigue la dirección de la cámara (el mouse apunta)
    level.shotAngle = cam_yaw + 180.0f;
    level.handleInput(window, dt);
    level.update(dt);

    vec3 target = level.ball.pos;
    vec3 camPos;
    camPos.x = target.x + cam_distance * cos(glm::radians(cam_pitch)) * cos(glm::radians(cam_yaw));
    camPos.y = target.y + cam_distance * cos(glm::radians(cam_pitch)) * sin(glm::radians(cam_yaw));
    camPos.z = target.z + cam_distance * sin(glm::radians(cam_pitch));

    mat4 P  = perspective(glm::radians(fov), aspect, 0.5f, 100.0f);
    mat4 V  = lookAt(camPos, target, up);
    mat4 VP = P * V;

    // ── 1. Renderizar escena al FBO ───────────────────────────────────────────
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    // Objetos (sin pixelación por textura: la hace el post-proceso)
    glUseProgram(prog);
    transfer_float("uPixelSize", 1.0f);
    level.render(prog, VP);

    // Sombras
    {
        static const glm::vec3 LIGHT_POS = {12.0f, 5.0f, 100.0f};
        glm::mat4 shadowMat = makeShadowMatrix(LIGHT_POS);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(-1.0f, -1.0f);
        level.renderShadows(shadow_prog, VP, shadowMat);
        glDisable(GL_POLYGON_OFFSET_FILL);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
        glDisable(GL_BLEND);
    }

    // Skybox
    glDepthFunc(GL_LEQUAL);
    glUseProgram(skybox_prog);
    transfer_mat4("VP", P * mat4(mat3(V)));
    glBindVertexArray(skyboxVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS);

    // ── 2. Post-proceso: pixelación sobre el framebuffer por defecto ──────────
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    glUseProgram(quad_prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fboColorTex);
    glUniform1i(glGetUniformLocation(quad_prog, "screenTex"), 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, fboDepthTex);
    glUniform1i(glGetUniformLocation(quad_prog, "depthTex"), 1);
    glUniform2f(glGetUniformLocation(quad_prog, "resolution"), (float)ANCHO, (float)ALTO);
    glUniform1f(glGetUniformLocation(quad_prog, "pixelSize"),  pixelSize);
    glUniform1f(glGetUniformLocation(quad_prog, "uNear"),      0.5f);
    glUniform1f(glGetUniformLocation(quad_prog, "uFar"),       40.0f);
    glUniform1f(glGetUniformLocation(quad_prog, "uFogStart"),  12.0f);
    glUniform1f(glGetUniformLocation(quad_prog, "uFogEnd"),    25.0f);
    glUniform3f(glGetUniformLocation(quad_prog, "uFogColor"),  0.82f, 0.87f, 0.96f);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
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
    destroy_fbo();
    setup_fbo(width, height);
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

    cam_yaw   -= dx * mouseSensitivity;
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