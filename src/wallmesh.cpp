#include "wallmesh.h"
#include <cmath>
#include <algorithm>
#include <GpO.h>

struct WVert { float x,y,z, nx,ny,nz, u,v; };

// Rota 90° a la izquierda (CCW)
static glm::vec2 perp_left(glm::vec2 d) { return {-d.y, d.x}; }

// Calcula el vector miter y su longitud en el vértice central b
// devuelve la dirección del miter (lado "izquierdo" de la poligonal)
static glm::vec2 miter(glm::vec2 a, glm::vec2 b, glm::vec2 c, float half)
{
    glm::vec2 d0 = glm::normalize(b - a);
    glm::vec2 d1 = glm::normalize(c - b);
    glm::vec2 p0 = perp_left(d0);
    glm::vec2 p1 = perp_left(d1);
    glm::vec2 m  = glm::normalize(p0 + p1);
    float dot    = glm::dot(m, p0);
    // Limitamos el miter a 3× el grosor para ángulos muy agudos
    float len    = (dot > 0.01f) ? std::min(half / dot, half * 3.0f) : half;
    return m * len;
}

WallMesh crear_wall_mesh(const std::vector<glm::vec2>& pts,
                         bool  closed,
                         float thickness,
                         float height,
                         float zBase,
                         float uvTile)
{
    int   N    = (int)pts.size();
    float half = thickness * 0.5f;
    float zT   = zBase + height;

    // ── 1. Calcular outer/inner para cada vértice ───────────────────────────
    std::vector<glm::vec2> outer(N), inner(N);

    for (int i = 0; i < N; i++) {
        bool hasPrev = closed || (i > 0);
        bool hasNext = closed || (i < N - 1);

        glm::vec2 m;
        if (hasPrev && hasNext) {
            glm::vec2 prev = pts[(i - 1 + N) % N];
            glm::vec2 next = pts[(i + 1) % N];
            m = miter(prev, pts[i], next, half);
        } else if (hasNext) {
            // Extremo inicial: sin miter, solo perpendicular
            glm::vec2 d = glm::normalize(pts[i + 1] - pts[i]);
            m = perp_left(d) * half;
        } else {
            // Extremo final
            glm::vec2 d = glm::normalize(pts[i] - pts[i - 1]);
            m = perp_left(d) * half;
        }
        outer[i] = pts[i] + m;
        inner[i] = pts[i] - m;
    }

    // ── 2. Generar geometría ────────────────────────────────────────────────
    std::vector<WVert>  verts;
    std::vector<GLuint> idx;

    auto addQuad = [&](glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3,
                       glm::vec3 n, float u0, float u1)
    {
        GLuint b = (GLuint)verts.size();
        verts.push_back({p0.x,p0.y,p0.z, n.x,n.y,n.z, u0, 0.f});
        verts.push_back({p1.x,p1.y,p1.z, n.x,n.y,n.z, u1, 0.f});
        verts.push_back({p2.x,p2.y,p2.z, n.x,n.y,n.z, u1, 1.f});
        verts.push_back({p3.x,p3.y,p3.z, n.x,n.y,n.z, u0, 1.f});
        idx.insert(idx.end(), {b,b+1,b+2, b+2,b+3,b});
    };

    int segCount = closed ? N : N - 1;
    float uAccum = 0.0f;

    for (int i = 0; i < segCount; i++) {
        int j = (i + 1) % N;

        float segLen = glm::length(pts[j] - pts[i]) / uvTile;
        float u0 = uAccum, u1 = uAccum + segLen;
        uAccum = u1;

        // Normal de cara exterior: perpendicular derecha de la dirección
        glm::vec2 d2 = glm::normalize(pts[j] - pts[i]);
        glm::vec3 nOut = { d2.y, -d2.x, 0.f};
        glm::vec3 nIn  = {-d2.y,  d2.x, 0.f};

        // Cara exterior
        addQuad({outer[i].x, outer[i].y, zBase},
                {outer[j].x, outer[j].y, zBase},
                {outer[j].x, outer[j].y, zT},
                {outer[i].x, outer[i].y, zT},
                nOut, u0, u1);

        // Cara interior
        addQuad({inner[j].x, inner[j].y, zBase},
                {inner[i].x, inner[i].y, zBase},
                {inner[i].x, inner[i].y, zT},
                {inner[j].x, inner[j].y, zT},
                nIn, u0, u1);

        // Tapa superior
        addQuad({outer[i].x, outer[i].y, zT},
                {outer[j].x, outer[j].y, zT},
                {inner[j].x, inner[j].y, zT},
                {inner[i].x, inner[i].y, zT},
                {0,0,1}, u0, u1);
    }

    // Tapas en los extremos (solo si es abierto)
    if (!closed) {
        auto endCap = [&](int i, glm::vec2 dir) {
            glm::vec3 n = {dir.x, dir.y, 0.f};
            addQuad({inner[i].x, inner[i].y, zBase},
                    {outer[i].x, outer[i].y, zBase},
                    {outer[i].x, outer[i].y, zT},
                    {inner[i].x, inner[i].y, zT},
                    n, 0.f, 1.f);
        };
        endCap(0,     -glm::normalize(pts[1]   - pts[0]));
        endCap(N - 1,  glm::normalize(pts[N-1] - pts[N-2]));
    }

    // ── 3. Subir a GPU ──────────────────────────────────────────────────────
    WallMesh wm;
    wm.indexCount = (int)idx.size();

    glGenVertexArrays(1, &wm.VAO);  glBindVertexArray(wm.VAO);
    glGenBuffers(1, &wm.VBO);
    glBindBuffer(GL_ARRAY_BUFFER, wm.VBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(WVert), verts.data(), GL_STATIC_DRAW);
    glGenBuffers(1, &wm.EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, wm.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(GLuint), idx.data(), GL_STATIC_DRAW);

    int stride = sizeof(WVert);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6*sizeof(float)));
    glBindVertexArray(0);

    return wm;
}

void render_wall_mesh(const WallMesh& wm, GLuint prog, const glm::mat4& VP)
{
    glm::mat4 M   = glm::mat4(1.0f);
    glm::mat4 MVP = VP * M;
    transfer_mat4("MVP", MVP);

    GLint mLoc = glGetUniformLocation(prog, "M");
    if (mLoc != -1) glUniformMatrix4fv(mLoc, 1, GL_FALSE, &M[0][0]);

    GLint cLoc = glGetUniformLocation(prog, "uColor");
    if (cLoc != -1) glUniform3f(cLoc, 1, 1, 1);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, wm.texID);
    transfer_int("tex", 0);
    transfer_int("uUseTex", wm.texID ? 1 : 0);

    glBindVertexArray(wm.VAO);
    glDrawElements(GL_TRIANGLES, wm.indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void destroy_wall_mesh(WallMesh& wm)
{
    glDeleteBuffers(1, &wm.VBO);
    glDeleteBuffers(1, &wm.EBO);
    glDeleteVertexArrays(1, &wm.VAO);
    wm = {};
}