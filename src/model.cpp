//
// Created by chern0g0r on 06.01.2022.
//

#include "model.h"
#include "glm/common.hpp"


Model::Model() {
    sumBbox = std::make_pair(glm::vec3(0.f), glm::vec3(0.f));
}

Model::~Model() {
    clear();
}

bool Model::load(std::string & filepath) {
    Assimp::Importer importer;
    bool ret = false;

    const aiScene *scene = importer.ReadFile(
            filepath.c_str(),
            aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs);

    if (scene) {
        ret = initFromScene(scene, filepath);
    } else {
        std::stringstream mes;
        mes << "Error parsing " << filepath << " : " << importer.GetErrorString() << '\n';
        assimp_fail(mes.str());
    }

    return ret;
}

bool Model::initFromScene(const aiScene *pScene, const std::string &filepath) {
    mEntries.resize(pScene->mNumMeshes);
//    mTextures.resize(pScene->mNumMaterials);

    std::cout << mEntries.size() << '\n';

    for (auto i = 0; i<mEntries.size(); i++) {
        const aiMesh* paiMesh = pScene->mMeshes[i];
        initMesh(i, paiMesh);
    }

    initBboxes();

    auto [vmin, vmax] = sumBbox;
    std::cout << "Bbox: \n" << vmin.x << ' ' << vmin.y << ' ' << vmin.z << '\n';
    std::cout << vmax.x << ' ' << vmax.y << ' ' << vmax.z << '\n';

    return initMaterials(pScene, filepath);
}

void Model::initMesh(int index, const aiMesh *paiMesh) {
//    mEntries[index].mtlIndex = paiMesh->mMaterialIndex;

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    const aiVector3D Zero3D(0.f, 0.f, 0.f);

    for (auto i = 0; i < paiMesh->mNumVertices; i++) {
        const aiVector3D* pPos = &(paiMesh->mVertices[i]);
        const aiVector3D* pNormal = &(paiMesh->mNormals[i]);
        const aiVector3D* pTexCoord = (paiMesh->HasTextureCoords(0) ?
                &(paiMesh->mTextureCoords[0][i]): &Zero3D);

        Vertex v(
                glm::vec3(pPos->x, pPos->y, pPos->z),
                glm::vec3(pNormal->x, pNormal->y, pNormal->z),
                glm::vec3(pTexCoord->x, pTexCoord->y, pTexCoord->z)
                );
        vertices.push_back(v);
    }

    for (auto i = 0; i < paiMesh->mNumFaces; i++) {
        const aiFace& face = paiMesh->mFaces[i];
        assert(face.mNumIndices == 3);
        for (auto j = 0; j < 3; j++)
            indices.push_back(face.mIndices[j]);
    }

    std::cout << "Vertices: " << vertices.size() << '\n';
    std::cout << "Indices: " << indices.size() << '\n';


    mEntries[index].initBbox(vertices);

    mEntries[index].init(vertices, indices);
}

bool Model::initMaterials(const aiScene *pScene, const std::string &filename) {
    return true;
}

void Model::initBboxes() {
    glm::vec3 minV(mEntries[0].bbox.first);
    glm::vec3 maxV(mEntries[0].bbox.second);
    for (auto &m : mEntries) {
        auto [v1, v2] = m.bbox;
        minV = glm::min(v1, minV);
        maxV = glm::max(v2, maxV);
    }

    sumBbox = std::make_pair(minV, maxV);
}

void Model::clear() {

}

Model::MeshEntry::MeshEntry() {
    inited = false;
}

Model::MeshEntry::~MeshEntry() {
    if (inited) {
        glDeleteBuffers(1, &VB);
        glDeleteBuffers(1, &IB);
    }
}

bool Model::MeshEntry::init(const std::vector<Vertex> &vertices,
                            const std::vector<unsigned int> &indices) {

    numIndices = indices.size();
    inited = true;

    glGenBuffers(1, &VB);
    glBindBuffer(GL_ARRAY_BUFFER, VB);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &IB);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IB);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * numIndices, indices.data(), GL_STATIC_DRAW);

    return false;
}

void Model::MeshEntry::initBbox(const std::vector<Vertex> &vertices) {
    glm::vec3 minV(vertices[0].m_pos);
    glm::vec3 maxV(vertices[0].m_pos);
    for (auto v: vertices) {
        glm::vec3 pos = v.m_pos;

        minV = glm::min(minV, pos);
        maxV = glm::max(maxV, pos);
    }
    bbox = std::make_pair(minV, maxV);
}
