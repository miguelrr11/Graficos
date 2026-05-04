#pragma once
#include <vector>
#include <glm/glm.hpp>
#include <glad/glad.h>

struct Particle {
    glm::vec3 pos     = {};
    glm::vec3 vel     = {};
    glm::vec3 acc     = {};
    float     life    = 0.f;
    float     maxLife = 1.f;
    float     size    = 0.1f;
    float     endSize = 0.0f;
    glm::vec4 color    = {1,1,1,1};
    glm::vec4 endColor = {1,1,1,0};
    float     rot    = 0.f;
    float     rotVel = 0.f;
    bool      alive  = false;
};


struct EmitParams {
    glm::vec3 pos          = {};
    glm::vec3 vel          = {};
    glm::vec3 velSpread    = {0.3f, 0.3f, 0.3f};
    glm::vec3 acc          = {0, 0, -5.f};
    float     life         = 0.8f;
    float     lifeSpread   = 0.2f;
    float     size         = 0.08f;
    float     endSize      = 0.0f;
    glm::vec4 color        = {1,1,1,1};
    glm::vec4 endColor     = {1,1,1,0};
    float     rotVelSpread = 4.0f;
    int       count        = 1;
};

class ParticleSystem {
public:
    void init(int maxParticles = 2000);
    void emit(const EmitParams& p);
    void update(float dt);
    void render(const glm::mat4& VP, const glm::vec3& camRight, const glm::vec3& camUp);
    void destroy();

// pooling para mejorar rendimiento
private:
    std::vector<Particle> pool_;
    std::vector<float>    instBuf_;
    int    maxP_    = 0;
    int    next_    = 0;
    GLuint vao_     = 0;
    GLuint quadVBO_ = 0;
    GLuint instVBO_ = 0;
    GLuint prog_    = 0;
};
