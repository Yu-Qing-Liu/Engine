#pragma once

#include "object.hpp"
#include "scene.hpp"

class Background : public Scene {
  public:
	Background(Scenes &scenes);
	Background(Background &&) = default;
	Background(const Background &) = delete;
	Background &operator=(Background &&) = delete;
	Background &operator=(const Background &) = delete;
	~Background() = default;

	static const std::string getName() { return "Background"; }

	void updateScreenParams() override;

	void updateComputeUniformBuffers() override;
	void computePass() override;

	void updateUniformBuffers() override;
	void renderPass() override;
	void swapChainUpdate() override;

  private:
	Model::UBO kitchenUBO{mat4(1.0f), lookAt(vec3(1.0f, 1.0f, 1.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 1.0f))};

	std::unique_ptr<Object> kitchen;
	std::unique_ptr<Object> walls;
};
