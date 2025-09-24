#pragma once

#include "engine.hpp"
#include "object.hpp"
#include "scene.hpp"
#include "text.hpp"

class Default : public Scene {
  public:
	Default(Scenes &scenes);
	Default(Default &&) = delete;
	Default(const Default &) = delete;
	Default &operator=(Default &&) = delete;
	Default &operator=(const Default &) = delete;
	~Default() = default;

	std::string getName() override { return "Default"; }

	void updateScreenParams() override;

	void updateComputeUniformBuffers() override;
	void computePass() override;

	void updateUniformBuffers() override;
	void renderPass() override;
	void swapChainUpdate() override;

  private:
	Model::UBO persp{mat4(1.0f), lookAt(vec3(2.0f, 2.0f, 2.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 1.0f)), perspective(radians(45.0f), Engine::swapChainExtent.width / (float)Engine::swapChainExtent.height, 0.1f, 10.0f)};

	Model::UBO orthographic{mat4(1.0f), mat4(1.0f), ortho(0.0f, float(Engine::swapChainExtent.width), 0.0f, -float(Engine::swapChainExtent.height), -1.0f, 1.0f)};

	unique_ptr<Model> triangle;
	unique_ptr<Model> example;
	unique_ptr<Model> particles;

	unique_ptr<Object> room;
	unique_ptr<Text> text;
};
