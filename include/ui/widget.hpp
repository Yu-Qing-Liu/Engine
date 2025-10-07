#pragma once

#include "engine.hpp"
#include "model.hpp"
#include "rectangle.hpp"

using std::make_unique;
using std::unique_ptr;

class Widget {
  public:
	Widget(Scene *scene, const Model::MVP &mvp, Model::ScreenParams &screenParams, const VkRenderPass &renderPass = Engine::renderPass);
	Widget(Widget &&) = delete;
	Widget(const Widget &) = delete;
	Widget &operator=(Widget &&) = delete;
	Widget &operator=(const Widget &) = delete;
	~Widget() = default;

	virtual void render();

	virtual void setOnMouseHover(std::function<void()> cb);
	virtual void setOnMouseEnter(std::function<void()> cb);
	virtual void setOnMouseExit(std::function<void()> cb);

	virtual void setOnMouseClick(std::function<void(int, int, int)> cb);
	virtual void setOnKeyboardKeyPress(std::function<void(int, int, int, int)> cb);

	Scene *scene;
	Model::MVP mvp;
	Model::ScreenParams &screenParams;
	std::unique_ptr<Rectangle> container;
};
