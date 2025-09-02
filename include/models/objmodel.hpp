#pragma once

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "model.hpp"
#include <optional>
#include <vulkan/vulkan_core.h>

using std::make_unique;
using std::unique_ptr;

class OBJModel {
  public:
	OBJModel(const string &objPath);
	OBJModel(OBJModel &&) = default;
	OBJModel(const OBJModel &) = delete;
	OBJModel &operator=(OBJModel &&) = delete;
	OBJModel &operator=(const OBJModel &) = delete;
	~OBJModel() = default;

	const string objPath;
	string directory;

	void updateUniformBuffer(optional<mat4> model = std::nullopt, optional<mat4> view = std::nullopt, optional<mat4> proj = std::nullopt);
	void updateUniformBuffer(const Model::UBO &ubo);
	void render(const Model::UBO &ubo, const Model::ScreenParams &screenParams);

  private:
	optional<Model::UBO> ubo = std::nullopt;

	vector<unique_ptr<Model>> meshes;

	void loadModel();
	void processNode(aiNode *node, const aiScene *scene);
	unique_ptr<Model> processMesh(aiMesh *mesh, const aiScene *scene);
};
