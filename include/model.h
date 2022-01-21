//
// Created by chern0g0r on 06.01.2022.
//

#ifndef MIXAMORENDERER_MODEL_H
#define MIXAMORENDERER_MODEL_H

#include <GL/glew.h>
#include <vector>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <string>
#include <sstream>

#include "errors.h"

#define GLM_FORCE_SWIZZLE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/vec3.hpp>

struct Vertex{
    glm::vec3 m_pos;
    glm::vec3 m_tex;
    glm::vec3 m_normal;

    Vertex() {
        m_pos = glm::vec3(0.f);
        m_tex = glm::vec3(0.f);
        m_normal = glm::vec3(0.f);
    };

    Vertex(glm::vec3 pos, glm::vec3 tex, glm::vec3 normal) {
        m_pos = pos;
        m_tex = tex;
        m_normal = normal;
    }

    Vertex(glm::vec3 pos, glm::vec3 tex) {
        m_pos = pos;
        m_tex = tex;
        m_normal = glm::vec3(0.f);
    }
};

class Model {
public:
    Model() = default;
    ~Model();
    bool load(std::string &filepath);

    void render();

private:
    bool initFromScene(const aiScene* pScene, const std::string & filepath);
    void initMesh(int index, const aiMesh* paiMesh);
    bool initMaterials(const aiScene* pScene, const std::string& filename);
    void clear();

    struct MeshEntry {
        MeshEntry();
        ~MeshEntry();

        bool init(const std::vector<Vertex>& vertices,
                  const std::vector<unsigned int>& indices);

        GLuint VB;
        GLuint IB;
        unsigned int numIndices;
        unsigned int mtlIndex;
    private:
        bool inited;
    };

    std::vector<MeshEntry> mEntries;
//    std::vector<Texture*> mTextures;
};

#endif //MIXAMORENDERER_MODEL_H
