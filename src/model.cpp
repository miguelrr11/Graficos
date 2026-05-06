#include "model.h"
#include <GpO.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <cstddef>   // offsetof

void ModelMesh::upload(const std::vector<ModelVertex>& verts,
                       const std::vector<unsigned int>& indices)
{
    indexCount = (int)indices.size();
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(ModelVertex), verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    int stride = sizeof(ModelVertex);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(ModelVertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(ModelVertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(ModelVertex, uv));

    glBindVertexArray(0);
}

void ModelMesh::draw(GLuint prog, const glm::mat4& MVP, const glm::mat4& M) const
{
    glUseProgram(prog);
    glUniformMatrix4fv(glGetUniformLocation(prog, "MVP"), 1, GL_FALSE, &MVP[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(prog, "M"),   1, GL_FALSE, &M[0][0]);
    glUniform1i(glGetUniformLocation(prog, "uUseTex"), 0);
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void ModelMesh::destroy()
{
    if (EBO) { glDeleteBuffers(1, &EBO);        EBO = 0; }
    if (VBO) { glDeleteBuffers(1, &VBO);        VBO = 0; }
    if (VAO) { glDeleteVertexArrays(1, &VAO);   VAO = 0; }
    indexCount = 0;
}

bool Model::load(const std::string& path)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_FlipUVs);

    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
        printf("[Model] Assimp error: %s\n", importer.GetErrorString());
        return false;
    }

    meshes.reserve(scene->mNumMeshes);
    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        aiMesh* aiM = scene->mMeshes[i];

        std::vector<ModelVertex>   verts;
        std::vector<unsigned int>  indices;
        verts.reserve(aiM->mNumVertices);

        for (unsigned int j = 0; j < aiM->mNumVertices; j++) {
            ModelVertex v;
            v.pos = { aiM->mVertices[j].x, aiM->mVertices[j].y, aiM->mVertices[j].z };
            v.normal = aiM->HasNormals()
                ? glm::vec3(aiM->mNormals[j].x, aiM->mNormals[j].y, aiM->mNormals[j].z)
                : glm::vec3(0.f, 1.f, 0.f);
            v.uv = (aiM->mTextureCoords[0])
                ? glm::vec2(aiM->mTextureCoords[0][j].x, aiM->mTextureCoords[0][j].y)
                : glm::vec2(0.f);
            verts.push_back(v);
        }

        for (unsigned int j = 0; j < aiM->mNumFaces; j++) {
            const aiFace& face = aiM->mFaces[j];
            for (unsigned int k = 0; k < face.mNumIndices; k++)
                indices.push_back(face.mIndices[k]);
        }

        ModelMesh mesh;
        mesh.name = aiM->mName.C_Str();

        // Compute bounds from vertex positions
        for (const auto& v : verts) {
            mesh.boundsMin = glm::min(mesh.boundsMin, v.pos);
            mesh.boundsMax = glm::max(mesh.boundsMax, v.pos);
        }
        mesh.center = (mesh.boundsMin + mesh.boundsMax) * 0.5f;
        glm::vec3 ext = mesh.boundsMax - mesh.boundsMin;
        mesh.size = std::max({ext.x, ext.y, ext.z, 0.001f});

        mesh.upload(verts, indices);
        meshes.push_back(std::move(mesh));
    }

    printf("[Model] Loaded '%s'  (%d meshes)\n", path.c_str(), (int)meshes.size());
    return true;
}

void Model::draw(GLuint prog, const glm::mat4& MVP, const glm::mat4& M) const
{
    for (const auto& mesh : meshes)
        mesh.draw(prog, MVP, M);
}

void Model::drawMesh(int index, GLuint prog, const glm::mat4& MVP, const glm::mat4& M) const
{
    if (index >= 0 && index < (int)meshes.size())
        meshes[index].draw(prog, MVP, M);
}

bool Model::drawMesh(const std::string& name, GLuint prog, const glm::mat4& MVP, const glm::mat4& M) const
{
    int idx = findMesh(name);
    if (idx < 0) return false;
    meshes[idx].draw(prog, MVP, M);
    return true;
}

int Model::findMesh(const std::string& name) const
{
    for (int i = 0; i < (int)meshes.size(); i++)
        if (meshes[i].name == name) return i;
    return -1;
}

void Model::setCharMap(std::initializer_list<std::pair<const char, int>> entries)
{
    charMap.clear();
    for (const auto& e : entries)
        charMap[e.first] = e.second;
}

bool Model::drawChar(char c, GLuint prog, const glm::mat4& MVP, const glm::mat4& M) const
{
    auto it = charMap.find(c);
    if (it == charMap.end()) {
        // Try case-insensitive fallback
        char alt = (c >= 'a' && c <= 'z') ? (c - 32) : (c >= 'A' && c <= 'Z') ? (c + 32) : 0;
        if (alt) it = charMap.find(alt);
    }
    if (it == charMap.end()) return false;
    drawMesh(it->second, prog, MVP, M);
    return true;
}

void Model::drawString(const char* str, GLuint prog, const glm::mat4& VP,
                       glm::vec3 startPos, float charSize, float spacing,
                       glm::vec3 color) const
{
    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "uUseTex"), 0);
    glUniform3f(glGetUniformLocation(prog, "uColor"), color.r, color.g, color.b);

    float advance = 0.f;
    for (const char* c = str; *c; c++) {
        if (*c == ' ') { advance += charSize * 0.6f + spacing; continue; }

        auto it = charMap.find(*c);
        if (it == charMap.end()) {
            char alt = (*c >= 'a' && *c <= 'z') ? (*c - 32)
                     : (*c >= 'A' && *c <= 'Z') ? (*c + 32) : 0;
            if (alt) it = charMap.find(alt);
        }

        if (it != charMap.end() && it->second < (int)meshes.size()) {
            const ModelMesh& m = meshes[it->second];
            float scale = charSize / m.size;
            glm::vec3 pos = startPos + glm::vec3(advance, 0.f, 0.f);
            glm::mat4 M = glm::translate(glm::mat4(1.f), pos - m.center * scale)
                        * glm::scale(glm::mat4(1.f), glm::vec3(scale));
            m.draw(prog, VP * M, M);
        }

        advance += charSize + spacing;
    }

    glUniform3f(glGetUniformLocation(prog, "uColor"), 1.f, 1.f, 1.f);
}

void Model::printMeshNames() const
{
    printf("[Model] %d meshes:\n", (int)meshes.size());
    for (int i = 0; i < (int)meshes.size(); i++)
        printf("  [%d] \"%s\"\n", i, meshes[i].name.c_str());
}

void Model::drawDebugGrid(GLuint prog, const glm::mat4& VP,
                          glm::vec3 origin, float cellSize, int cols) const
{
    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "uUseTex"), 0);

    for (int i = 0; i < (int)meshes.size(); i++) {
        const ModelMesh& m = meshes[i];
        int col = i % cols;
        int row = i / cols;

        // Place cell center, normalize mesh to fit within cellSize
        glm::vec3 cellCenter = origin + glm::vec3(col * cellSize, 0.f, row * cellSize);
        float scale = (cellSize * 0.8f) / m.size;

        glm::mat4 M = glm::translate(glm::mat4(1.f), cellCenter - m.center * scale)
                    * glm::scale(glm::mat4(1.f), glm::vec3(scale));

        // Alternate colors per row to make rows easier to count
        float t = (float)(row % 2);
        glUniform3f(glGetUniformLocation(prog, "uColor"), 1.f - t*0.3f, 0.8f, 0.4f + t*0.4f);
        m.draw(prog, VP * M, M);
    }

    glUniform3f(glGetUniformLocation(prog, "uColor"), 1.f, 1.f, 1.f);
}

void Model::destroy()
{
    for (auto& m : meshes) m.destroy();
    meshes.clear();
}
