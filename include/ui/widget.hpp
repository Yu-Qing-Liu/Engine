#pragma once

#include "model.hpp"

using UBO = Model::UBO;
using ScreenParams = Model::ScreenParams;
using std::unique_ptr;

class Widget {
  public:
	Widget(const UBO &ubo, ScreenParams &screenParams);
	Widget(Widget &&) = delete;
	Widget(const Widget &) = delete;
	Widget &operator=(Widget &&) = delete;
	Widget &operator=(const Widget &) = delete;
	~Widget() = default;

	virtual void render();

  private:
	vector<unique_ptr<Model>> components;
};
