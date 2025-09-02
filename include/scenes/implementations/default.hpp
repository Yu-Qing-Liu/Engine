#pragma once

#include "engine.hpp"
#include "model.hpp"
#include "objmodel.hpp"
#include "scene.hpp"
#include "text.hpp"

class Default : public Scene {
  public:
	Default(Scenes &scenes);
	Default(Default &&) = default;
	Default(const Default &) = delete;
	Default &operator=(Default &&) = delete;
	Default &operator=(const Default &) = delete;
	~Default() = default;

	const std::string getName() const override { return "Default"; }

	void renderPass() override;
	void swapChainUpdate() override;

  private:
	Model::UBO persp{mat4(1.0f), lookAt(vec3(2.0f, 2.0f, 2.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 1.0f)), perspective(radians(45.0f), Engine::swapChainExtent.width / (float)Engine::swapChainExtent.height, 0.1f, 10.0f)};

	Model::UBO orthographic{mat4(1.0f), mat4(1.0f), ortho(0.0f, float(Engine::swapChainExtent.width), 0.0f, -float(Engine::swapChainExtent.height), -1.0f, 1.0f)};

	Model::ScreenParams screenParams;

	unique_ptr<Model> triangle;
	unique_ptr<Model> example;
	unique_ptr<Model> particles;

	unique_ptr<OBJModel> room;
	unique_ptr<Text> text;
};
