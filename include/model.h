#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <unordered_map>
#include <initializer_list>
#include <utility>

struct ModelVertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
};

// Single GPU-ready mesh. Vertex layout matches the main shader (locations 0,1,2).
struct ModelMesh {
    GLuint      VAO = 0, VBO = 0, EBO = 0;
    int         indexCount = 0;
    std::string name;               // object name from the .obj file
    glm::vec3   boundsMin = glm::vec3( 1e9f);
    glm::vec3   boundsMax = glm::vec3(-1e9f);
    glm::vec3   center    = glm::vec3(0.f);
    float       size      = 1.f;    // max extent across all axes

    void upload(const std::vector<ModelVertex>& verts, const std::vector<unsigned int>& indices);
    void draw(GLuint prog, const glm::mat4& MVP, const glm::mat4& M = glm::mat4(1.f)) const;
    void destroy();
};

// A 3D model composed of one or more meshes (one per Assimp mesh / object group).
struct Model {
    std::vector<ModelMesh> meshes;
    std::unordered_map<char, int> charMap;  // optional: char → mesh index

    bool load(const std::string& path);
    bool loadMem(const unsigned char* data, unsigned int size);

    // Draw all meshes
    void draw(GLuint prog, const glm::mat4& MVP, const glm::mat4& M = glm::mat4(1.f)) const;

    // Draw mesh by numeric index
    void drawMesh(int index, GLuint prog, const glm::mat4& MVP, const glm::mat4& M = glm::mat4(1.f)) const;

    // Draw mesh by name (e.g. "A", "b") — returns false if not found
    bool drawMesh(const std::string& name, GLuint prog, const glm::mat4& MVP, const glm::mat4& M = glm::mat4(1.f)) const;

    // Find index by name (-1 if not found)
    int  findMesh(const std::string& name) const;

    // Character map — call once after load() to enable drawChar()
    void setCharMap(std::initializer_list<std::pair<const char, int>> entries);

    // Draw a character using the charMap. Returns false if character not mapped.
    bool drawChar(char c, GLuint prog, const glm::mat4& MVP, const glm::mat4& M = glm::mat4(1.f)) const;

    // Draw a string of characters in a line along the X axis.
    // startPos: world-space anchor of the first character.
    // charSize: uniform size for every character (world units).
    // spacing:  extra gap between characters (world units). 0 = tight.
    // color:    RGB colour applied as uColor uniform.
    // rotation: optional rotation matrix applied to each character.
    void drawString(const char* str, GLuint prog, const glm::mat4& VP,
                    glm::vec3 startPos, float charSize, float spacing = 0.05f,
                    glm::vec3 color = glm::vec3(1.f),
                    glm::mat4 rotation = glm::mat4(1.f),
                    float spinTime = 0.f) const;

    // Total advance width of a string (same logic as drawString, no GL calls).
    float stringWidth(const char* str, float charSize, float spacing = 0.05f) const;

    // Print all mesh names to stdout (useful for inspecting a new .obj)
    void printMeshNames() const;

    // Render all meshes in a cols-wide grid, each normalized to cellSize world units.
    // origin = world-space bottom-left corner of the grid.
    // Use this to visually identify which index = which character.
    void drawDebugGrid(GLuint prog, const glm::mat4& VP,
                       glm::vec3 origin, float cellSize = 1.2f, int cols = 8) const;

    int meshCount() const { return (int)meshes.size(); }

    void destroy();
};
