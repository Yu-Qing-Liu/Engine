#pragma once

#include "scene.hpp"

class Default : public Scene {
  public:
	Default() = default;
	Default(Default &&) = default;
	Default(const Default &) = delete;
	Default &operator=(Default &&) = delete;
	Default &operator=(const Default &) = delete;
	~Default() = default;

	void renderPass() override;
	void drawFrame() override;
};
