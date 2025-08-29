#pragma once

#include "model.hpp"
#include "objmodel.hpp"
#include "scene.hpp"

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
	unique_ptr<Model> triangle;
	unique_ptr<Model> example;

    unique_ptr<OBJModel> room;
};
