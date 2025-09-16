#pragma once

#include "model.hpp"
#include <memory>
using std::unique_ptr;

class Scenes;

class Scene {
  public:
	Scene(Scenes &scenes);
	Scene(Scene &&) = default;
	Scene(const Scene &) = delete;
	Scene &operator=(Scene &&) = delete;
	Scene &operator=(const Scene &) = delete;
	virtual ~Scene() = default;

	vector<Model *> models;

	void updateRayTraceUniformBuffers();
	void rayTraces();

	virtual void updateScreenParams();

	virtual void updateComputeUniformBuffers();
	virtual void computePass();

	virtual void updateUniformBuffers();
	virtual void renderPass();
	virtual void swapChainUpdate();

  protected:
	Scenes &scenes;
	Model::ScreenParams screenParams;
};
