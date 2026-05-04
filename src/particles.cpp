#include "particles.h"
#include <GpO.h>
#include <cstdlib>
#include <cmath>

#define GLSL(src) "#version 330 core\n" #src

static const char* PART_VS = GLSL(
    layout(location=0) in vec2  quad;    // unit quad vertex [-0.5..0.5]
    layout(location=1) in vec3  iPos;    // world position
    layout(location=2) in float iSize;   // diameter in world units
    layout(location=3) in vec4  iColor;  // rgba
    layout(location=4) in float iRot;    // rotation in radians

    uniform mat4 VP;
    uniform vec3 camRight;
    uniform vec3 camUp;

    out vec4 vColor;
    out vec2 vUV;

    void main() {
        float c = cos(iRot);
        float s = sin(iRot);
        vec2  r  = vec2(c*quad.x - s*quad.y, s*quad.x + c*quad.y);
        vec3  wp = iPos + (camRight * r.x + camUp * r.y) * iSize;
        gl_Position = VP * vec4(wp, 1.0);
        vColor = iColor;
        vUV    = quad + 0.5;
    }
);

static const char* PART_FS = GLSL(
    in  vec4 vColor;
    in  vec2 vUV;
    out vec4 FragColor;

    void main() {
        float d = length(vUV - 0.5) * 2.0;
        if (d > 1.0) discard;
        float a = (1.0 - d * d) * vColor.a;
        FragColor = vec4(vColor.rgb, a);
    }
);

static float rf() { return (float)rand() / (float)RAND_MAX; }
static float rc() { return rf() * 2.0f - 1.0f; }

void ParticleSystem::init(int maxParticles) {
    if (vao_) destroy();
    maxP_ = maxParticles;
    next_ = 0;
    pool_.assign(maxP_, Particle{});
    instBuf_.reserve(maxP_ * 9);

    float quad[] = {
        -0.5f,-0.5f,  0.5f,-0.5f,  0.5f, 0.5f,
        -0.5f,-0.5f,  0.5f, 0.5f, -0.5f, 0.5f
    };

    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);

    glGenBuffers(1, &quadVBO_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    // Instance buffer: [pos(3), size(1), color(4), rot(1)] = 9 floats, stride 36 bytes
    glGenBuffers(1, &instVBO_);
    glBindBuffer(GL_ARRAY_BUFFER, instVBO_);
    glBufferData(GL_ARRAY_BUFFER, maxP_ * 9 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    int stride = 9 * sizeof(float);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(0));
    glVertexAttribDivisor(1, 1);

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glVertexAttribDivisor(2, 1);

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)(4 * sizeof(float)));
    glVertexAttribDivisor(3, 1);

    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));
    glVertexAttribDivisor(4, 1);

    glBindVertexArray(0);

    GLuint VS = compilar_shader(PART_VS, GL_VERTEX_SHADER);
    GLuint FS = compilar_shader(PART_FS, GL_FRAGMENT_SHADER);
    prog_ = glCreateProgram();
    glAttachShader(prog_, VS); glAttachShader(prog_, FS);
    glLinkProgram(prog_);
    check_errores_programa(prog_);
    glDetachShader(prog_, VS); glDeleteShader(VS);
    glDetachShader(prog_, FS); glDeleteShader(FS);
}

void ParticleSystem::emit(const EmitParams& p) {
    for (int i = 0; i < p.count; i++) {
        Particle& part = pool_[next_];
        next_ = (next_ + 1) % maxP_;

        part.pos     = p.pos;
        part.vel     = p.vel + glm::vec3(rc()*p.velSpread.x, rc()*p.velSpread.y, rc()*p.velSpread.z);
        part.acc     = p.acc;
        part.maxLife = p.life + rc() * p.lifeSpread;
        if (part.maxLife < 0.05f) part.maxLife = 0.05f;
        part.life    = part.maxLife;
        part.size    = p.size;
        part.endSize = p.endSize;
        part.color   = p.color;
        part.endColor= p.endColor;
        part.rot     = rf() * 6.2831853f;
        part.rotVel  = rc() * p.rotVelSpread;
        part.alive   = true;
    }
}

void ParticleSystem::update(float dt) {
    for (auto& p : pool_) {
        if (!p.alive) continue;
        p.life -= dt;
        if (p.life <= 0.0f) { p.alive = false; continue; }
        p.vel += p.acc * dt;
        p.pos += p.vel * dt;
        p.rot += p.rotVel * dt;
    }
}

void ParticleSystem::render(const glm::mat4& VP,
                             const glm::vec3& camRight,
                             const glm::vec3& camUp)
{
    if (!prog_) return;
    instBuf_.clear();

    for (const auto& p : pool_) {
        if (!p.alive) continue;
        float t   = 1.0f - (p.life / p.maxLife);
        float sz  = p.size + (p.endSize - p.size) * t;
        glm::vec4 col = p.color + (p.endColor - p.color) * t;
        instBuf_.push_back(p.pos.x);  instBuf_.push_back(p.pos.y);  instBuf_.push_back(p.pos.z);
        instBuf_.push_back(sz);
        instBuf_.push_back(col.r); instBuf_.push_back(col.g); instBuf_.push_back(col.b); instBuf_.push_back(col.a);
        instBuf_.push_back(p.rot);
    }
    if (instBuf_.empty()) return;

    int count = (int)(instBuf_.size() / 9);
    glBindBuffer(GL_ARRAY_BUFFER, instVBO_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, instBuf_.size() * sizeof(float), instBuf_.data());

    glUseProgram(prog_);
    glUniformMatrix4fv(glGetUniformLocation(prog_, "VP"),       1, GL_FALSE, &VP[0][0]);
    glUniform3fv      (glGetUniformLocation(prog_, "camRight"), 1, &camRight[0]);
    glUniform3fv      (glGetUniformLocation(prog_, "camUp"),    1, &camUp[0]);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    glBindVertexArray(vao_);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, count);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void ParticleSystem::destroy() {
    if (vao_)     { glDeleteVertexArrays(1, &vao_);  vao_     = 0; }
    if (quadVBO_) { glDeleteBuffers(1, &quadVBO_);   quadVBO_ = 0; }
    if (instVBO_) { glDeleteBuffers(1, &instVBO_);   instVBO_ = 0; }
    if (prog_)    { glDeleteProgram(prog_);           prog_    = 0; }
    pool_.clear();
    instBuf_.clear();
    maxP_ = 0;
    next_ = 0;
}
