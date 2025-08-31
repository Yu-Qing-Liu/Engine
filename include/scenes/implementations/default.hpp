#pragma once

#include "model.hpp"
#include "objmodel.hpp"
#include "scene.hpp"
#include "text.hpp"


class Default : public Scene {
  public:
	Default();
	Default(Default &&) = default;
	Default(const Default &) = delete;
	Default &operator=(Default &&) = delete;
	Default &operator=(const Default &) = delete;
	~Default() = default;

	void renderPass() override;

  private:
    Model::UBO ubo {
        mat4(1.0f),
        lookAt(vec3(2.0f, 2.0f, 2.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 1.0f)),
        perspective(radians(45.0f), Engine::swapChainExtent.width / (float) Engine::swapChainExtent.height, 0.1f, 10.0f)
    };

	unique_ptr<Model> triangle;
	unique_ptr<Model> example;

	unique_ptr<OBJModel> room;
	unique_ptr<Text> text;
};
