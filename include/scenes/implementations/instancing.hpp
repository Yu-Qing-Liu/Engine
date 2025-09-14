#pragma once

#include "engine.hpp"
#include "instancedpolygon.hpp"
#include "instancedrectangle.hpp"
#include "scene.hpp"

using std::shared_ptr;
using std::unordered_map;

class Instancing : public Scene {
  public:
	Instancing(Scenes &scenes);
	Instancing(Instancing &&) = default;
	Instancing(const Instancing &) = delete;
	Instancing &operator=(Instancing &&) = delete;
	Instancing &operator=(const Instancing &) = delete;
	~Instancing() = default;

	static const std::string getName() { return "Instancing"; }

	void updateScreenParams() override;

	void updateComputeUniformBuffers() override;
	void computePass() override;

	void updateUniformBuffers() override;
	void renderPass() override;
	void swapChainUpdate() override;

  private:
	Model::UBO persp{mat4(1.0f), lookAt(vec3(4.0f, 4.0f, 4.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 1.0f)), perspective(radians(45.0f), Engine::swapChainExtent.width / (float)Engine::swapChainExtent.height, 0.1f, 10.0f)};

	Model::UBO orthographic{mat4(1.0f), mat4(1.0f), ortho(0.0f, float(Engine::swapChainExtent.width), 0.0f, -float(Engine::swapChainExtent.height), -1.0f, 1.0f)};

	shared_ptr<unordered_map<int, InstancedRectangleData>> cells;
	unique_ptr<InstancedRectangle> grid;

	shared_ptr<unordered_map<int, InstancedPolygonData>> instances;
	unique_ptr<InstancedPolygon> polygons;
};
