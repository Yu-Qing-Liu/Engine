#pragma once

#include "engine.hpp"
#include "polygon.hpp"
#include "scene.hpp"

class RayTracing : public Scene {
  public:
	RayTracing(Scenes &scenes);
	RayTracing(RayTracing &&) = delete;
	RayTracing(const RayTracing &) = delete;
	RayTracing &operator=(RayTracing &&) = delete;
	RayTracing &operator=(const RayTracing &) = delete;
	~RayTracing() = default;

	std::string getName() override { return "RayTracing"; }

	void updateScreenParams() override;

	void updateComputeUniformBuffers() override;
	void computePass() override;

	void updateUniformBuffers() override;
	void renderPass() override;
	void swapChainUpdate() override;

  private:
	Model::MVP persp{mat4(1.0f), lookAt(vec3(4.0f, 4.0f, 4.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 1.0f)), perspective(radians(45.0f), Engine::swapChainExtent.width / (float)Engine::swapChainExtent.height, 0.1f, 10.0f)};

	Model::MVP orthographic{mat4(1.0f), mat4(1.0f), ortho(0.0f, float(Engine::swapChainExtent.width), 0.0f, -float(Engine::swapChainExtent.height), -1.0f, 1.0f)};

	unique_ptr<Polygon> cube1;
	unique_ptr<Polygon> cube2;
};
