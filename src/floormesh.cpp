#include "floormesh.h"
#include <GpO.h>
#include <cmath>
#include <algorithm>

struct FVert { float x,y,z, nx,ny,nz, u,v; };

// Área con signo * 2 del polígono (positivo = CCW en XY)
static float signedArea2(const std::vector<glm::vec2>& p)
{
    float a = 0.f;
    int n = (int)p.size();
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        a += p[i].x * p[j].y - p[j].x * p[i].y;
    }
    return a;
}

// Producto vectorial 2D en el vértice o: (a-o) x (b-o)
static float cross2(glm::vec2 o, glm::vec2 a, glm::vec2 b)
{
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

// ¿El punto p está dentro del triángulo (a,b,c)?
static bool pointInTriangle(glm::vec2 p, glm::vec2 a, glm::vec2 b, glm::vec2 c)
{
    float d1 = cross2(p, a, b);
    float d2 = cross2(p, b, c);
    float d3 = cross2(p, c, a);
    bool neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(neg && pos);
}

// Ear-clipping para polígono simple (convexo o cóncavo, sin auto-intersecciones).
// Devuelve triángulos con normal apuntando a +Z.
static std::vector<GLuint> earClip(const std::vector<glm::vec2>& poly)
{
    int n = (int)poly.size();
    if (n < 3) return {};

    // Índices activos del polígono
    std::vector<int> idx(n);
    for (int i = 0; i < n; i++) idx[i] = i;

    bool ccw = signedArea2(poly) > 0.f;

    std::vector<GLuint> tris;
    tris.reserve((n - 2) * 3);

    int maxIter = n * n + n;
    while ((int)idx.size() > 3 && maxIter-- > 0) {
        int m = (int)idx.size();
        bool clipped = false;

        for (int i = 0; i < m; i++) {
            int pi = (i - 1 + m) % m;
            int ni = (i + 1) % m;
            glm::vec2 a = poly[idx[pi]];
            glm::vec2 b = poly[idx[i]];
            glm::vec2 c = poly[idx[ni]];

            // Vértice convexo según la orientación del polígono
            float cr = cross2(a, b, c);
            if (ccw ? (cr <= 0.f) : (cr >= 0.f)) continue;

            // Ningún otro vértice dentro de este triángulo
            bool ear = true;
            for (int j = 0; j < m && ear; j++) {
                if (j == pi || j == i || j == ni) continue;
                glm::vec2 p = poly[idx[j]];
                if (pointInTriangle(p, a, b, c)) ear = false;
            }

            if (ear) {
                // Emitir con CCW para normal +Z
                if (ccw) {
                    tris.push_back((GLuint)idx[pi]);
                    tris.push_back((GLuint)idx[i]);
                    tris.push_back((GLuint)idx[ni]);
                } else {
                    tris.push_back((GLuint)idx[ni]);
                    tris.push_back((GLuint)idx[i]);
                    tris.push_back((GLuint)idx[pi]);
                }
                idx.erase(idx.begin() + i);
                clipped = true;
                break;
            }
        }
        if (!clipped) break; // polígono degenerado
    }

    // Triángulo final
    if ((int)idx.size() == 3) {
        if (ccw) {
            tris.push_back((GLuint)idx[0]);
            tris.push_back((GLuint)idx[1]);
            tris.push_back((GLuint)idx[2]);
        } else {
            tris.push_back((GLuint)idx[2]);
            tris.push_back((GLuint)idx[1]);
            tris.push_back((GLuint)idx[0]);
        }
    }

    return tris;
}

FloorMesh crear_floor_mesh(const std::vector<glm::vec2>& perimeter,
                           float zBase, float uvScale)
{
    std::vector<FVert> verts;
    verts.reserve(perimeter.size());

    for (const auto& p : perimeter) {
        FVert v;
        v.x = p.x;  v.y = p.y;  v.z = zBase;
        v.nx = 0.f; v.ny = 0.f; v.nz = 1.f;
        // UV en coordenadas de mundo escaladas para tiling natural
        v.u = p.x / uvScale;
        v.v = p.y / uvScale;
        verts.push_back(v);
    }

    std::vector<GLuint> idx = earClip(perimeter);

    FloorMesh fm;
    fm.indexCount = (int)idx.size();

    glGenVertexArrays(1, &fm.VAO); glBindVertexArray(fm.VAO);

    glGenBuffers(1, &fm.VBO);
    glBindBuffer(GL_ARRAY_BUFFER, fm.VBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(FVert), verts.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &fm.EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, fm.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(GLuint), idx.data(), GL_STATIC_DRAW);

    int stride = sizeof(FVert);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));

    glBindVertexArray(0);
    return fm;
}

void render_floor_mesh(const FloorMesh& fm, GLuint prog, const glm::mat4& VP)
{
    if (fm.indexCount == 0) return;

    glm::mat4 M   = glm::mat4(1.0f);
    glm::mat4 MVP = VP * M;
    transfer_mat4("MVP", MVP);

    GLint mLoc = glGetUniformLocation(prog, "M");
    if (mLoc != -1) glUniformMatrix4fv(mLoc, 1, GL_FALSE, &M[0][0]);

    GLint cLoc = glGetUniformLocation(prog, "uColor");
    if (cLoc != -1) glUniform3f(cLoc, 1.f, 1.f, 1.f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fm.texID);
    transfer_int("tex", 0);
    transfer_int("uUseTex",       fm.texID ? 1 : 0);
    transfer_int("uMappingType",  0);
    transfer_int("uUseTexOffset", 0);
    transfer_float("uTile", 1.0f); // UVs ya escaladas en el vértice

    glBindVertexArray(fm.VAO);
    glDrawElements(GL_TRIANGLES, fm.indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void destroy_floor_mesh(FloorMesh& fm)
{
    if (fm.VAO) glDeleteVertexArrays(1, &fm.VAO);
    if (fm.VBO) glDeleteBuffers(1, &fm.VBO);
    if (fm.EBO) glDeleteBuffers(1, &fm.EBO);
    fm = {};
}
