#include "objmodel.hpp"
#include "polygon.hpp"
#include "texture.hpp"
#include <assimp/postprocess.h>

OBJModel::OBJModel(const std::string &objPath) : objPath(objPath) {
    loadModel();
}

void OBJModel::updateComputeUniformBuffer() {
    for (const auto &m : meshes) {
        m->updateComputeUniformBuffer();
    }
}

void OBJModel::updateUniformBuffer(optional<mat4> model, optional<mat4> view, optional<mat4> proj) {
    if (!ubo.has_value()) {
        return;
    }
    if (model.has_value()) {
        ubo->model = model.value();
    }
    if (view.has_value()) {
        ubo->view = view.value();
    }
    if (proj.has_value()) {
        ubo->proj = proj.value();
    }
    for (const auto &m : meshes) {
        m->updateUniformBuffer(ubo->model, ubo->view, ubo->proj);
    }
}

void OBJModel::updateUniformBuffer(const Model::UBO &ubo) {
    if (!this->ubo.has_value()) {
        return;
    }
    this->ubo = ubo;
}

void OBJModel::loadModel() {
    Assimp::Importer import;
    const aiScene *scene = import.ReadFile(objPath, aiProcess_Triangulate | aiProcess_FlipUVs);
    
    if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) 
    {
        std::cout << "ERROR::ASSIMP::" << import.GetErrorString() << std::endl;
        return;
    }

    directory = objPath.substr(0, objPath.find_last_of('/'));

    processNode(scene->mRootNode, scene);
}

void OBJModel::processNode(aiNode * node, const aiScene * scene) {
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
        meshes.push_back(processMesh(mesh, scene));
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        processNode(node->mChildren[i], scene);
    }
}

std::unique_ptr<Model> OBJModel::processMesh(aiMesh* mesh, const aiScene* scene) {
    // -------- With texture coordinates --------
    if (mesh->HasTextureCoords(0)) {
        std::vector<Model::TexVertex> vertices;
        std::vector<uint16_t> indices;

        // vertices
        vertices.reserve(mesh->mNumVertices);
        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
            glm::vec3 pos{
                mesh->mVertices[i].x,
                mesh->mVertices[i].y,
                mesh->mVertices[i].z
            };

            glm::vec4 col{1.0f, 1.0f, 1.0f, 1.0f};
            if (mesh->HasVertexColors(0)) {
                const aiColor4D& c = mesh->mColors[0][i];
                col = glm::vec4(c.r, c.g, c.b, c.a);
            }

            glm::vec2 tex{
                mesh->mTextureCoords[0][i].x,
                mesh->mTextureCoords[0][i].y
            };

            vertices.push_back({pos, col, tex});
        }

        // indices
        indices.reserve(mesh->mNumFaces * 3);
        for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
            const aiFace& face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; ++j) {
                indices.push_back(static_cast<uint16_t>(face.mIndices[j]));
            }
        }

        // material → texture path (diffuse/baseColor)
        if (mesh->mMaterialIndex >= 0 && scene) {
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
            aiString str;
            if (material->GetTexture(aiTextureType_DIFFUSE, 0, &str) == AI_SUCCESS ||
                material->GetTexture(aiTextureType_BASE_COLOR, 0, &str) == AI_SUCCESS) {

                const std::string texPath = directory + "/" + str.C_Str();
                // std::cout << texPath << std::endl;

                // Embedded? path like "*0"
                if (!texPath.empty() && texPath[0] == '*') {
                    const int idx = std::atoi(texPath.c_str() + 1);
                    if (idx >= 0 && static_cast<unsigned>(idx) < scene->mNumTextures) {
                        return std::make_unique<Texture>(*scene->mTextures[idx], vertices, indices);
                    }
                } else {
                    // External file
                    return std::make_unique<Texture>(texPath, vertices, indices);
                }
            }
        }

        // If we got here, no material texture found return a polygon model
        std::vector<Model::Vertex> polyVerts;
        polyVerts.reserve(vertices.size());
        for (const auto& v : vertices) polyVerts.push_back({v.pos, v.color});
        return std::make_unique<Polygon>(polyVerts, indices);
    }

    // -------- No texture coordinates → plain colored mesh --------
    {
        std::vector<Model::Vertex> vertices;
        std::vector<uint16_t> indices;

        // vertices
        vertices.reserve(mesh->mNumVertices);
        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
            glm::vec3 pos{
                mesh->mVertices[i].x,
                mesh->mVertices[i].y,
                mesh->mVertices[i].z
            };

            glm::vec4 col{1.0f, 1.0f, 1.0f, 1.0f};
            if (mesh->HasVertexColors(0)) {
                const aiColor4D& c = mesh->mColors[0][i];
                col = glm::vec4(c.r, c.g, c.b, c.a);
            }

            vertices.push_back({pos, col});
        }

        // indices
        indices.reserve(mesh->mNumFaces * 3);
        for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
            const aiFace& face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; ++j) {
                indices.push_back(static_cast<uint16_t>(face.mIndices[j]));
            }
        }

        return std::make_unique<Polygon>(vertices, indices);
    }
}

void OBJModel::setOnHover(const std::function<void()> &callback) {
    for (const auto &m : meshes) {
        m->setOnHover(callback);
    }
}

void OBJModel::compute() {
    for (const auto &m : meshes) {
        m->compute();
    }
}

void OBJModel::render(const Model::UBO &ubo, const Model::ScreenParams &screenParams) {
    if (!this->ubo.has_value()) {
        this->ubo = ubo;
    }
    for (const auto &m : meshes) {
        m->render(ubo, screenParams);
    }
}

