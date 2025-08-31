#pragma once

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "model.hpp"
#include <vulkan/vulkan_core.h>

using std::unique_ptr;
using std::make_unique;

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

	void render(optional<Model::UBO> ubo = std::nullopt);

  private:
	vector<unique_ptr<Model>> meshes;

	void loadModel();
	void processNode(aiNode *node, const aiScene *scene);
	unique_ptr<Model> processMesh(aiMesh *mesh, const aiScene *scene);
};
