#pragma once

#include "engine.hpp"
#include "rectangle.hpp"
#include "scene.hpp"

class Buttons : public Scene {
  public:
	Buttons(Scenes &scenes);
	Buttons(Buttons &&) = default;
	Buttons(const Buttons &) = delete;
	Buttons &operator=(Buttons &&) = delete;
	Buttons &operator=(const Buttons &) = delete;
	~Buttons() = default;

	static const std::string getName() { return "Buttons"; }

	void updateScreenParams() override;

	void updateComputeUniformBuffers() override;
	void computePass() override;

	void updateUniformBuffers() override;
	void renderPass() override;
	void swapChainUpdate() override;

  private:
	Model::UBO persp{mat4(1.0f), lookAt(vec3(4.0f, 4.0f, 4.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 1.0f)), perspective(radians(45.0f), Engine::swapChainExtent.width / (float)Engine::swapChainExtent.height, 0.1f, 10.0f)};

	Model::UBO orthographic{mat4(1.0f), mat4(1.0f), ortho(0.0f, float(Engine::swapChainExtent.width), 0.0f, -float(Engine::swapChainExtent.height), -1.0f, 1.0f)};

	unique_ptr<Rectangle> button;
};
