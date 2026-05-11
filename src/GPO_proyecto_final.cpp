/************************  GPO_01 ************************************
ATG, 2019
******************************************************************************/

#include <GpO.h>
#include "obstacle.h"
#include "level.h"       // <-- nivel de minigolf
#include "model.h"
#include <vector>
#include <cstring>
#include <algorithm>
#include "soloud.h"
#include "soloud_wavstream.h"
#include "game.h"
#include <embedded_assets.h>


// TAMAÑO y TITULO INICIAL de la VENTANA
int ANCHO = 800*1.25, ALTO = 600*1.25;
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
GLuint skyFBO, skyColorTex;
float  pixelSize = 3.0f;   // píxeles de pantalla por "píxel de juego"

static bool startedGame = false;
static bool startedAnimationStartingGame = false;

// Variables para controlar la pantalla completa desde cualquier parte
static bool isFullScreenGlobal = false;
static int windowed_x = 0, windowed_y = 0, windowed_width = 800, windowed_height = 600;

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
    uniform float uTime;
    uniform float uColorSeed;

    float hash(vec3 p) {
        p = fract(p * vec3(443.897, 441.423, 437.195));
        p += dot(p, p.yzx + 19.19);
        return fract((p.x + p.y) * p.z);
    }

    float vnoise(vec3 p) {
        vec3 i = floor(p);
        vec3 f = fract(p);
        vec3 u = f * f * (3.0 - 2.0 * f);
        return mix(
            mix(mix(hash(i),             hash(i+vec3(1,0,0)), u.x),
                mix(hash(i+vec3(0,1,0)), hash(i+vec3(1,1,0)), u.x), u.y),
            mix(mix(hash(i+vec3(0,0,1)), hash(i+vec3(1,0,1)), u.x),
                mix(hash(i+vec3(0,1,1)), hash(i+vec3(1,1,1)), u.x), u.y), u.z);
    }

    float fbm(vec3 p) {
        float v = 0.0;
        float a = 0.5;
        for (int i = 0; i < 5; i++) {
            v += a * vnoise(p);
            p  = p * 2.1 + vec3(1.7, 9.2, 3.4);
            a *= 0.5;
        }
        return v;
    }

    vec3 hsv2rgb(float h, float s, float v) {
        vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
        return v * mix(vec3(1.0), rgb, s);
    }

    vec3 lineColor(float t, float seed) {
        vec3 a = vec3(0.55, 0.55, 0.55);
        vec3 b = vec3(0.45, 0.45, 0.45);
        vec3 c = vec3(1.00, 1.00, 1.00);
        vec3 d = vec3(seed, seed + 0.3333, seed + 0.6667);
        return clamp(a + b * cos(6.28318 * (c * t + d)), 0.0, 1.0);
    }

    vec3 bgColor(float t, float seed) {
        vec3 a = vec3(0.15, 0.15, 0.16);
        vec3 b = vec3(0.07, 0.09, 0.1);
        vec3 c = vec3(1.20, 0.90, 1.10);
        vec3 d = vec3(seed + 0.50, seed + 0.85, seed + 0.15);
        return clamp(a + b * cos(6.28318 * (c * t + d)), 0.0, 1.0);
    }

    void main() {
        vec3  d    = normalize(dir);
        vec3  p    = d * 2.5 + vec3(uTime * 0.035, uTime * 0.022, uTime * 0.014);
        float h    = clamp(fbm(p), 0.0, 0.9999);
        float LEVELS = 10.0;
        float scaled = h * LEVELS;
        float band   = floor(scaled);
        float t      = fract(scaled);
        float norm   = band / LEVELS;
        vec3  col    = (t < 0.15) ? lineColor(norm+t, uColorSeed) : bgColor(norm+t*2.5, uColorSeed);
        FragColor    = vec4(col, 1.0);
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
    uniform sampler2D skyTex;
    uniform sampler2D hudTex;
    uniform vec2  resolution;
    uniform float pixelSize;
    uniform float uNear;
    uniform float uFar;
    uniform float uFogStart;
    uniform float uFogEnd;
    uniform float uDim = 1.0;

    float linearDepth(float d) {
        return uNear * uFar / (uFar - d * (uFar - uNear));
    }

    void main() {
        // ── 1. Pixelación ──────────────────────────────────────────────────
        vec2 block = floor(fragUV * resolution / pixelSize);
        vec2 uv    = (block * pixelSize + pixelSize * 0.5) / resolution;
        vec3 color = texture(screenTex, uv).rgb;

        // ── 0. Color aberration
        vec2  toCenter = fragUV - 0.5;
        float distC    = length(toCenter);

        float abMask   = smoothstep(0.3, 0.65, distC);

        // importante normalizar para que el desplazamiento sea consistente independientemente de la resolución o del pixelSize
        vec2  abDir    = distC > 0.001 ? toCenter / distC : vec2(0.0);
        vec2  abOffset = abDir * (distC * distC) * 0.03;

        // clampeamos para no leer fuera de la textura
        float aR = texture(screenTex, clamp(fragUV + abOffset, vec2(0.0), vec2(1.0))).r;
        float aB = texture(screenTex, clamp(fragUV - abOffset, vec2(0.0), vec2(1.0))).b;

        color.r = mix(color.r, aR, abMask);
        color.b = mix(color.b, aB, abMask);

        // ── 2. Outlines
        vec2 off = (pixelSize / resolution);
        float rawD = texture(depthTex, uv).r;
        float d    = linearDepth(rawD);
        float dR   = linearDepth(texture(depthTex, uv + vec2(off.x,  0.0)).r);
        float dU   = linearDepth(texture(depthTex, uv + vec2(0.0,  off.y)).r);
        float dL   = linearDepth(texture(depthTex, uv - vec2(off.x,  0.0)).r);
        float dD   = linearDepth(texture(depthTex, uv - vec2(0.0,  off.y)).r);
        float edge = max(max(abs(d-dR), abs(d-dL)), max(abs(d-dU), abs(d-dD)));
        // Normalizamos según el game pixel para que el grosor sea consistente independientemente de la resolución o del pixelSize
        float edgeNorm = edge * (resolution.x / pixelSize) / max(d, 0.01);
        color = mix(color, vec3(0.04, 0.04, 0.08), step(40.0, edgeNorm) * 0.88);

        // ── 3. Scanlines
        float row = mod(block.y, 2.0);
        color *= (row < 1.0) ? 0.9 : 1.0;

        // ── 4. Niebla con dithering de bayer (no usado)
        if (rawD < 0.9999) {
            float linD = d;
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
            float threshold = bayer[by * 4 + bx];

            float band = 0.0;
            float t    = smoothstep(threshold - band, threshold + band, fogF);

            vec3 skyColor = texture(skyTex, uv).rgb;
            color = mix(skyColor, color, t);
        }

        // ── 5. Viñeta
        vec2  vc = fragUV - 0.5;
        float dist = dot(vc, vc);
        float v = smoothstep(0.8, 0.2, dist);

        color *= v;

        // HUD (texto pixelado)
        {
            vec4 hud = texture(hudTex, vec2(fragUV.x, 1.0 - fragUV.y));
            if (hud.a > 0.5) color = hud.rgb;
        }

        color *= uDim;
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

// ─── Shaders para objetos
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
        fragLocalNormal = normal;
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
                // uv normal con tiling, offset y pixelación
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
                // mapeo esferico
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


GLFWwindow* window;
GLuint      prog;
Game        game;
Model       alphabetModel;

// debug letras 3d
bool        g_debugAlphabetGrid = true;
int         g_debugMeshIdx      = 0;

// Cámara
vec3  up       = vec3(0, 0, 1);
float cam_yaw  = 180.0f;
float cam_pitch = -15.0f;
float lastX = 400.0f, lastY = 300.0f;
bool  firstMouse = true;
float mouseSensitivity = 0.1f;
float fov    = 80.0f;
float aspect = ANCHO / ALTO;
float cam_speed = 5.0f;
float cam_distance = 3.0f;

// Tiempo para deltaTime real
float lastFrameTime = 0.0f;
float dt = 0.0f;

// Estado compartido entre update_scene y render_scene
static float     g_now     = 0.0f;
static glm::vec3 g_camPos;
static glm::vec3 g_titlePos;
static glm::vec3 g_target;
static glm::mat4 g_P, g_V, g_VP;
static glm::vec3 g_lightPos;
static bool      g_paused       = false;
static bool      g_showControls = false;

// Statics de la animación del intro (promovidos para que los actualice update_scene)
static float anim_Timer      = 0.f;
static float anim_StartYaw   = 0.f;
static float anim_StartPitch = 0.f;
static float anim_TargetYaw  = 0.f;
static bool  anim_Inited     = false;

// Para el spin de la letra O en el titulo
static float g_oSpin = 0.0f;

//sistema HUD
static const uint8_t FONT_ROWS[36][7] = {
    // 0-9
    {14,17,17,17,17,17,14}, {4,12,4,4,4,4,14}, {14,17,1,6,8,16,31}, {14,17,1,6,1,17,14},
    {17,17,17,31,1,1,1}, {31,16,30,1,1,17,14}, {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8},
    {14,17,17,14,17,17,14}, {14,17,17,15,1,1,14},
    // A-Z (10 al 35)
    {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30}, {14,17,16,16,16,17,14}, {30,17,17,17,17,17,30},
    {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16}, {14,17,16,23,17,17,14}, {17,17,17,31,17,17,17},
    {14,4,4,4,4,4,14},      {15,2,2,2,2,18,12},     {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
    {17,27,21,17,17,17,17}, {17,25,21,19,17,17,17}, {14,17,17,17,17,17,14}, {30,17,17,30,16,16,16},
    {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17}, {14,17,16,14,1,17,14},  {31,4,4,4,4,4,4},
    {17,17,17,17,17,17,14}, {17,17,17,17,17,10,4},  {17,17,17,21,21,27,17}, {17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4},     {31,1,2,4,8,16,31}
};

static std::vector<uint8_t> hudBuf;
static GLuint hudTex = 0;
static int    hudW   = 0, hudH = 0;
static int    hudDX1, hudDY1, hudDX2, hudDY2;

static void hud_resize(int w, int h) {
    hudW = w; hudH = h;
    hudBuf.assign((size_t)w * h * 4, 0);
    if (!hudTex) {
        glGenTextures(1, &hudTex);
        glBindTexture(GL_TEXTURE_2D, hudTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    } else {
        glBindTexture(GL_TEXTURE_2D, hudTex);
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
}

static void hud_clear() {
    memset(hudBuf.data(), 0, hudBuf.size());
    hudDX1 = hudW; hudDY1 = hudH; hudDX2 = -1; hudDY2 = -1;
}

static inline void hud_put(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if ((unsigned)x >= (unsigned)hudW || (unsigned)y >= (unsigned)hudH) return;
    uint8_t* p = hudBuf.data() + (y * hudW + x) * 4;
    p[0]=r; p[1]=g; p[2]=b; p[3]=255;
}

static int hud_text_width(const char* str, int size) {
    int n = 0;
    for (const char* c = str; *c; c++) {
        if ((*c >= '0' && *c <= '9') || (*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z')) n++;
    }
    return n > 0 ? n * (5 * size) + (n - 1) : 0;
}

static void hud_text(const char* str, int x, int y, int size, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255) {
    int cx = x;
    for (const char* c = str; *c; c++) {
        int d = -1;
        if (*c >= '0' && *c <= '9') d = *c - '0';
        else if (*c >= 'A' && *c <= 'Z') d = *c - 'A' + 10;
        else if (*c >= 'a' && *c <= 'z') d = *c - 'a' + 10;

        if (d == -1) { cx += size * 3; continue; } // Espacio o caracter no soportado

        hudDX1 = std::min(hudDX1, cx - 1); hudDY1 = std::min(hudDY1, y - 1);
        hudDX2 = std::max(hudDX2, cx + 5 * size); hudDY2 = std::max(hudDY2, y + 7 * size);
        for (int row = 0; row < 7; row++) {
            int bits = FONT_ROWS[d][row];
            for (int col = 0; col < 5; col++) {
                if ((bits >> (4 - col)) & 1) {
                    // Pintar letra y borde negro simultáneamente
                    for (int sy = -1; sy <= size; sy++) {
                        for (int sx = -1; sx <= size; sx++) {
                            int px = cx + col*size + sx;
                            int py = y + row*size + sy;
                            if ((unsigned)px >= (unsigned)hudW || (unsigned)py >= (unsigned)hudH) continue;
                            uint8_t* p = hudBuf.data() + (py * hudW + px) * 4;

                            // Centro de la letra (color)
                            if (sx >= 0 && sx < size && sy >= 0 && sy < size) {
                                p[0]=r; p[1]=g; p[2]=b; p[3]=255;
                            } 
                            // Borde externo (solo pintar de negro si está vacío para no pisar el color)
                            else if (p[3] == 0) {
                                p[0]=0; p[1]=0; p[2]=0; p[3]=255;
                            }
                        }
                    }
                }
            }
        }
        cx += 5 * size + 1;
    }
}

static void hud_flush() {
    if (!hudTex || hudW == 0 || hudDX2 < hudDX1) return;
    glBindTexture(GL_TEXTURE_2D, hudTex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, hudW, hudH, GL_RGBA, GL_UNSIGNED_BYTE, hudBuf.data());
}


// Proyecta geometría sobre el plano z=groundZ desde la posición de luz L
static glm::mat4 makeShadowMatrix(glm::vec3 L, float groundZ = 0.0f)
{
    float lx = L.x, ly = L.y, lz = L.z;
    float d  = lz - groundZ;   // distancia vertical luz→plano
    return glm::mat4(
         d,           0.f,  0.f,     0.f,   // col 0
         0.f,         d,    0.f,     0.f,   // col 1
        -lx,         -ly,  -groundZ,-1.f,   // col 2
         groundZ*lx,  groundZ*ly, groundZ*lz, lz  // col 3
    );
}

// FBO para post-proceso
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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, w, h, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, fboDepthTex, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    hud_resize(w, h);
}

void destroy_fbo()
{
    glDeleteTextures(1, &fboColorTex);
    glDeleteTextures(1, &fboDepthTex);
    glDeleteFramebuffers(1, &fbo);
}

void setup_sky_fbo(int w, int h)
{
    glGenFramebuffers(1, &skyFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, skyFBO);
    glGenTextures(1, &skyColorTex);
    glBindTexture(GL_TEXTURE_2D, skyColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, skyColorTex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
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

    // FBO + quad de post-proceso
    setup_fbo(w, h);
    setup_sky_fbo(w, h);
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

    // Load alphabet model
    alphabetModel.loadMem(asset_alphabet_obj_obj, asset_alphabet_obj_obj_size);

    alphabetModel.setCharMap({
        // digitos
        {'6', 0}, {'8', 1}, {'9', 2}, {'4', 3},
        {'3', 4}, {'w', 5},
        {'0',56}, {'7',57}, {'1',58}, {'2',59}, {'5',55},
        // letras
        {'P', 6}, {'g', 7},
        {'Q', 8}, {'W', 9},
        {'p',10}, {'m',11}, {'f',12}, {'h',13}, {'a',14}, {'x',15},
        {'S',16}, {'t',17}, {'e',18}, {'s',19},
        {'G',20}, {'k',21}, {'E',22}, {'U',23},
        {'d',24}, {'q',25}, {'v',26}, {'J',27},
        {'u',28}, {'K',29}, {'c',30}, {'D',31},
        {'r',32}, {'b',33}, {'V',34}, {'R',35},
        {'X',36}, {'F',37}, {'O',38}, {'n',39},
        {'B',40}, {'y',41}, {'L',42}, {'I',43},
        {'o',44}, {'z',45}, {'Z',46}, {'M',47},
        {'H',48}, {'C',49}, {'N',50}, {'A',51},
        {'i',52}, {'T',53}, {'Y',54},
        {'.',60}, {'j',61}, {'l',62},
    });

    // Inicializar estado de cámara compartido
    g_camPos   = glm::vec3(game.level.holePos.x, game.level.holePos.y, game.level.holePos.z + 4.0f);
    g_target   = glm::vec3(game.level.ball.pos.x, game.level.ball.pos.y, 0.0f);
    g_titlePos = g_camPos + glm::normalize(g_target - g_camPos) * 2.0f;
}

void update_scene() {
    g_now = (float)glfwGetTime();
    dt    = g_now - lastFrameTime;
    lastFrameTime = g_now;
    if (dt > 0.05f) dt = 0.05f;
    g_oSpin += dt * 2.0f;

    game.level.shotAngle = cam_yaw + 180.0f;
    if (startedGame) game.level.handleInput(window, dt, game.bonusQueue);
    game.level.update(dt, game.bonusQueue);

    g_target = game.level.ball.pos;

    if (startedGame) {
        g_camPos.x = g_target.x + cam_distance * cos(glm::radians(cam_pitch)) * cos(glm::radians(cam_yaw));
        g_camPos.y = g_target.y + cam_distance * cos(glm::radians(cam_pitch)) * sin(glm::radians(cam_yaw));
        g_camPos.z = g_target.z + cam_distance * sin(glm::radians(cam_pitch));
    }

    if (!startedGame) {
        g_target = glm::vec3(game.level.ball.pos.x, game.level.ball.pos.y, 0.0f);
        const float animDuration = 1.25f;

        if (startedAnimationStartingGame) {
            if (!anim_Inited) {
                glm::vec3 d = g_camPos - g_target;
                float horiz = glm::length(glm::vec2(d.x, d.y));
                anim_StartYaw   = glm::degrees(atan2f(d.y, d.x));
                anim_StartPitch = glm::degrees(atan2f(d.z, horiz));
                glm::vec2 hole  = game.level.tracks[0].holePos;
                anim_TargetYaw  = glm::degrees(atan2f(
                    game.level.ball.pos.y - hole.y,
                    game.level.ball.pos.x - hole.x));
                anim_Inited = true;
            }

            anim_Timer += dt;
            float rawT = glm::clamp(anim_Timer / animDuration, 0.f, 1.f);
            float t    = rawT * rawT * (3.f - 2.f * rawT);

            float yawDiff = anim_TargetYaw - anim_StartYaw;
            if (yawDiff >  180.f) yawDiff -= 360.f;
            if (yawDiff < -180.f) yawDiff += 360.f;
            cam_yaw   = anim_StartYaw + yawDiff * t;
            cam_pitch = lerp(anim_StartPitch, 45.0f, t);

            glm::vec3 gameCamPos;
            gameCamPos.x = g_target.x + cam_distance * cosf(glm::radians(cam_pitch)) * cosf(glm::radians(cam_yaw));
            gameCamPos.y = g_target.y + cam_distance * cosf(glm::radians(cam_pitch)) * sinf(glm::radians(cam_yaw));
            gameCamPos.z = g_target.z + cam_distance * sinf(glm::radians(cam_pitch));
            const float camSnapSpeed = 4.0f;
            g_camPos = lerpVector(g_camPos, gameCamPos, 1.0f - expf(-camSnapSpeed * dt));

            if (rawT >= 1.0f) {
                g_camPos = gameCamPos;
                startedGame = true;
            }
        }
    }

    g_P  = perspective(glm::radians(fov), aspect, 0.5f, 1000.0f);
    g_V  = lookAt(g_camPos, g_target, up);
    g_VP = g_P * g_V;

    game.level.camRight = glm::normalize(glm::vec3(g_V[0][0], g_V[1][0], g_V[2][0]));
    game.level.camUp    = glm::normalize(glm::vec3(g_V[0][1], g_V[1][1], g_V[2][1]));

    g_lightPos = glm::vec3(300.0f, 0, 400.0f);

    game.update(dt, startedGame);

    if (game.needsCamReset && !game.level.tracks.empty()) {
        glm::vec2 start = game.level.tracks[0].startPos;
        glm::vec2 hole  = game.level.tracks[0].holePos;
        glm::vec2 fwd   = hole - start;
        if (glm::length(fwd) > 0.001f) {
            fwd = glm::normalize(fwd);
            cam_yaw   = glm::degrees(std::atan2f(fwd.y, fwd.x)) + 180.0f;
            cam_pitch = 30.0f;
        }
        game.needsCamReset = false;
    }
}


// Render
void render_scene()
{
    // ── 0. Skybox solo al sky FBO (usado después para el color de niebla) ────
    glBindFramebuffer(GL_FRAMEBUFFER, skyFBO);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glUseProgram(skybox_prog);
    transfer_mat4("VP", g_P * mat4(mat3(g_V)));
    transfer_float("uTime", g_now);
    transfer_float("uColorSeed", game.level.skyColorSeed);
    glBindVertexArray(skyboxVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);

    // ── 1. Renderizar escena al FBO ───────────────────────────────────────────
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    glUseProgram(prog);
    transfer_float("uPixelSize", 1.0f);
    transfer_vec3("uLightPos", g_lightPos);
    game.level.particleEmitEnabled = !g_paused;
    game.level.render(prog, g_VP, game.bonusQueue);

    if (!startedGame) {
        glm::vec3 viewDir   = glm::normalize(g_camPos - g_target);
        glm::vec3 worldZ    = glm::vec3(0.f, 0.f, 1.f);
        glm::vec3 textRight = glm::normalize(glm::cross(worldZ, viewDir));
        glm::vec3 textFwd   = -viewDir + 0.5f * worldZ;
        glm::vec3 textUp    = glm::normalize(glm::cross(textRight, textFwd));
        glm::mat4 rot(1.f);
        rot[0] = glm::vec4(textRight, 0.f);
        rot[1] = glm::vec4(textFwd,   0.f);
        rot[2] = glm::vec4(textUp,    0.f);

        float charSize = 0.3f, spacing = 0.03f;
        float w = alphabetModel.stringWidth("HOP IN ONE", charSize, spacing);
        glm::vec3 startPos = g_titlePos - textRight * (w * 0.5f);
        alphabetModel.drawString("HOP IN ONE", prog, g_VP,
            startPos, charSize, spacing, glm::vec3(1.f, 0.8f, 0.2f), rot, g_oSpin);

        charSize = 0.17f, spacing = 0.02f;
        char instr[12] = "PRESS SPACE";
        w = alphabetModel.stringWidth(instr, charSize, spacing);
        startPos = g_titlePos - textRight * (w * 0.5f);
        startPos.z -= 0.4f;
        alphabetModel.drawString(instr, prog, g_VP,
            startPos, charSize, spacing, glm::vec3(1.f, 0.8f, 0.2f), rot, g_oSpin);
    }

    // Sombras con máscara de stencil: sombras solo donde hay suelo
    {
        glEnable(GL_STENCIL_TEST);
        glStencilMask(0xFF);
        glStencilFunc(GL_ALWAYS, 1, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glDepthMask(GL_FALSE);
        glDepthFunc(GL_LEQUAL);
        glUseProgram(shadow_prog);
        for (const auto& fm : game.level.floorMeshes) {
            transfer_mat4("MVP", g_VP);
            glBindVertexArray(fm.VAO);
            glDrawElements(GL_TRIANGLES, fm.indexCount, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
        }
        glDepthFunc(GL_LESS);

        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glStencilFunc(GL_EQUAL, 1, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        glStencilMask(0x00);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(-1.0f, -1.0f);
        game.level.renderShadows(shadow_prog, g_VP, g_lightPos);
        glDisable(GL_POLYGON_OFFSET_FILL);
        glDepthFunc(GL_LESS);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glStencilMask(0xFF);
        glDisable(GL_STENCIL_TEST);
    }

    // Skybox
    glDepthFunc(GL_LEQUAL);
    glUseProgram(skybox_prog);
    transfer_mat4("VP", g_P * mat4(mat3(g_V)));
    transfer_float("uTime", g_now);
    transfer_float("uColorSeed", game.level.skyColorSeed);
    glBindVertexArray(skyboxVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS);

    // Partículas
    game.level.particles.render(g_VP, game.level.camRight, game.level.camUp);

    // ── 2. Post-proceso: pixelación sobre el framebuffer por defecto
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
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, skyColorTex);
    glUniform1i(glGetUniformLocation(quad_prog, "skyTex"), 2);
    glUniform2f(glGetUniformLocation(quad_prog, "resolution"), (float)ANCHO, (float)ALTO);
    glUniform1f(glGetUniformLocation(quad_prog, "pixelSize"),  pixelSize);
    glUniform1f(glGetUniformLocation(quad_prog, "uNear"),      0.5f);
    glUniform1f(glGetUniformLocation(quad_prog, "uFar"),       40.0f);
    glUniform1f(glGetUniformLocation(quad_prog, "uFogStart"),  100.0f);
    glUniform1f(glGetUniformLocation(quad_prog, "uFogEnd"),    200.0f);

    // HUD
    float screenRatio = (float)ALTO / 750.0f;

    static float scaleFont = 6.0f;
    if (game.gameTimer < 10.0f) scaleFont = lerp(scaleFont, 10.0f, 0.1f);
    else if (game.gameTimer < 5.0f)  scaleFont += cosf(g_now * 10.0f) * 0.5f;
    else scaleFont = 6.0f;

    int sf = std::max(1, (int)(scaleFont * screenRatio));

    if (startedGame) {
        hud_clear();
        {
            int timerSec = std::max(0, (int)std::ceil(game.gameTimer));
            char timerStr[8];
            sprintf_s(timerStr, sizeof(timerStr), "%d", timerSec);
            int tw = hud_text_width(timerStr, sf);
            bool urgent = game.gameTimer < 5.0f;
            hud_text(timerStr, (ANCHO - tw) / 2, (int)(10 * screenRatio), sf,
                    255, urgent ? 51 : 255, urgent ? 51 : 255);

            char msStr[8];
            int ms = (int)((game.gameTimer - std::floor(game.gameTimer)) * 100);
            sprintf_s(msStr, sizeof(msStr), "%02d", ms);
            int msw = hud_text_width(msStr, sf / 2);
            hud_text(msStr, (ANCHO - msw) / 2, (int)((10 + scaleFont * 7 + 5) * screenRatio), std::max(1, sf / 2),
                        255, urgent ? 51 : 255, urgent ? 51 : 255);

            char levelStr[16];
            sprintf_s(levelStr, sizeof(levelStr), "NIVEL %d", game.currentLevel);
            int lvlSf = std::max(1, (int)(3.0f * screenRatio));
            hud_text(levelStr, (int)(15 * screenRatio), (int)(10 * screenRatio), lvlSf);

            char msgStr[64];
            if (!isFullScreenGlobal) {
                sprintf_s(msgStr, sizeof(msgStr), "PULSE F PARA PANTALLA COMPLETA");
                int msgSf = std::max(1, (int)(2.0f * screenRatio));
                int msgW = hud_text_width(msgStr, msgSf);
                hud_text(msgStr, (ANCHO - msgW) / 2, ALTO - (int)(40 * screenRatio), msgSf, 200, 200, 200);
            }

            {
                // Hint fijo: "PULSE TAB PARA CONTROLES"
                const char* tabHint = "PULSE TAB PARA CONTROLES";
                int hintSf = std::max(1, (int)(1.75f * screenRatio));
                int hintW  = hud_text_width(tabHint, hintSf);
                hud_text(tabHint, (ANCHO - hintW) / 2, ALTO - (int)(20 * screenRatio), hintSf, 180, 180, 180);

                // Panel de controles mientras TAB esté pulsado
                if (g_showControls) {
                    const char* controls[] = {
                        "CLICK PARA MOVERTE",
                        "ESPACIO REPETIDAS VECES PARA SALTAR",
                        "ENTER PARA PAUSA",
                        "ESC PARA SALIR"
                    };
                    int ctrlSf      = std::max(1, (int)(3.0f * screenRatio));
                    int lineHeight  = (int)(ctrlSf * 10 * screenRatio);
                    int numLines    = sizeof(controls) / sizeof(controls[0]);
                    int totalH      = numLines * lineHeight;
                    int startY      = (ALTO - totalH) / 2;

                    for (int i = 0; i < numLines; i++) {
                        int w = hud_text_width(controls[i], ctrlSf);
                        hud_text(controls[i], (ANCHO - w) / 2, startY + i * lineHeight, ctrlSf, 255, 255, 255);
                    }
                }
            }

            if (game.goldBonus > 0) {
                char goldBonusStr[64];
                sprintf_s(goldBonusStr, sizeof(goldBonusStr), "10");
                int tutorialSf = std::max(1, (int)(3.0f * screenRatio));
                int tutorialW = hud_text_width(goldBonusStr, tutorialSf);
                float off = game.goldBonus * 50;
                hud_text(goldBonusStr, (ANCHO - tutorialW) / 2 + 70, (int)(10 * screenRatio) + off, tutorialSf, 200, 200, 200);
                game.goldBonus = lerp(game.goldBonus, 0.0f, 0.01f);
                if (game.goldBonus < 0.05f) game.goldBonus = 0.0f;
            }
        }
        hud_flush();
    }

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, hudTex);
    glUniform1i(glGetUniformLocation(quad_prog, "hudTex"), 3);

    glUniform1f(glGetUniformLocation(quad_prog, "uDim"), game.dimValue);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
}

glm::vec3 lerpVector(const glm::vec3& a, const glm::vec3& b, float t) {
    return a + t * (b - a);
}

float map(float value, float inMin, float inMax, float outMin, float outMax) {
    return (value - inMin) / (inMax - inMin) * (outMax - outMin) + outMin;
}

float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

int main(int argc, char* argv[])
{
    init_GLFW();
    window = Init_Window(prac);
    load_Opengl();

    SoLoud::Soloud *gSoloud = new SoLoud::Soloud;
    SoLoud::WavStream gMusic;

    int initErr = gSoloud->init(SoLoud::Soloud::CLIP_ROUNDOFF, SoLoud::Soloud::MINIAUDIO);
    printf("SoLoud init error: %d\n", initErr);
    int error = gMusic.loadMem(asset_Balatro_mp3, asset_Balatro_mp3_size, false, false);
    printf("Audio load error: %d\n", error);
    gMusic.setLooping(true);
    int handle = gSoloud->play(gMusic);
    gSoloud->setVolume(handle, 1.0f); //0 for debug, deberia ser 1.0

    game.init(gSoloud);
    init_scene();

    glfwSwapInterval(1);
    while (!glfwWindowShouldClose(window)) {
        if (!g_paused) update_scene();
        render_scene();
        glfwSwapBuffers(window);
        glfwPollEvents();
        show_info();
    }

    gSoloud->deinit();
    delete gSoloud;
    game.destroy();
    alphabetModel.destroy();
    glfwTerminate();
    return 0;
}


void show_info()
{
    static int fps = 0;
    static double last_tt = 0;
    char buf[128];
    fps++;
    double tt = glfwGetTime();
    double elapsed = tt - last_tt;
    if (elapsed >= 0.5) {
        const char* estado = game.level.completed  ? "COMPLETADO!" :
                             game.level.ball.moving ? "Rodando..." :
                             game.level.charging    ? "Cargando..." : "Listo";
        sprintf_s(buf, 128, "%s | %.0f FPS | %s | Potencia: %.0f%%",
                  prac, fps/elapsed, estado, game.level.shotPower * 100.0f);
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

    if (key == GLFW_KEY_TAB) {
        if (action == GLFW_PRESS)   g_showControls = true;
        if (action == GLFW_RELEASE) g_showControls = false;
    }

    if (key == GLFW_KEY_ENTER && action == GLFW_PRESS) {
        g_paused = !g_paused;
        if (!g_paused) lastFrameTime = (float)glfwGetTime(); // evitar spike de dt al retomar
    }
    // R: reiniciar nivel
    if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        game.level.destroy();
        game.level.load(game.currentLevel, game.res);
        game.needsCamReset = true;
    }

    // Espacio: iniciar juego desde pantalla de título
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS && !startedAnimationStartingGame) {
        startedAnimationStartingGame = true;
    }
    
    // F: Alternar Pantalla Completa
    if (key == GLFW_KEY_F && action == GLFW_PRESS) {
        if (!isFullScreenGlobal) {
            // Guardar posición y tamaño actuales para restaurarlos luego
            glfwGetWindowPos(window, &windowed_x, &windowed_y);
            glfwGetWindowSize(window, &windowed_width, &windowed_height);

            GLFWmonitor* monitor = getCurrentMonitor(window);
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            
            // Pasar a pantalla completa
            glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
            isFullScreenGlobal = true;
        } else {
            // Volver a modo ventana recuperando las dimensiones
            glfwSetWindowMonitor(window, NULL, windowed_x, windowed_y, windowed_width, windowed_height, 0);
            isFullScreenGlobal = false;
        }
    }
}

GLFWmonitor* getCurrentMonitor(GLFWwindow* window)
{
    int nmonitors, i;
    GLFWmonitor** monitors = glfwGetMonitors(&nmonitors);

    int wx, wy, ww, wh;
    glfwGetWindowPos(window, &wx, &wy);
    glfwGetWindowSize(window, &ww, &wh);

    GLFWmonitor* bestMonitor = nullptr;
    int bestArea = 0;

    for (i = 0; i < nmonitors; i++)
    {
        int mx, my;
        glfwGetMonitorPos(monitors[i], &mx, &my);

        const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);

        int mw = mode->width;
        int mh = mode->height;

        int overlap =
            std::max(0, std::min(wx + ww, mx + mw) - std::max(wx, mx)) *
            std::max(0, std::min(wy + wh, my + mh) - std::max(wy, my));

        if (overlap > bestArea)
        {
            bestArea = overlap;
            bestMonitor = monitors[i];
        }
    }

    return bestMonitor;
}

void MouseCallback(GLFWwindow* window, double xpos, double ypos)
{
    if (g_paused) { lastX = (float)xpos; lastY = (float)ypos; return; }
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