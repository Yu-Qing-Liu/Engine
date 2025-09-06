#pragma once

#include "model.hpp"

using UBO = Model::UBO;
using ScreenParams = Model::ScreenParams;
using std::unique_ptr;

class Widget {
  public:
	Widget(Scene &scene, const UBO &ubo, ScreenParams &screenParams);
	Widget(Widget &&) = delete;
	Widget(const Widget &) = delete;
	Widget &operator=(Widget &&) = delete;
	Widget &operator=(const Widget &) = delete;
	~Widget() = default;

	std::function<void()> onHover;
	std::function<void()> onMouseEnter;
	std::function<void()> onMouseExit;

	void setOnMouseClick(std::function<void(int, int, int)> cb);
	void setOnKeyboardKeyPress(std::function<void(int, int, int, int)> cb);

	void render();

  protected:
	Scene &scene;
	const UBO &ubo;
	ScreenParams &screenParams;

  private:
	vector<unique_ptr<Model>> components;
};
