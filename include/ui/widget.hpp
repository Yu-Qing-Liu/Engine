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
	const VkRenderPass &renderPass;
	std::unique_ptr<Rectangle> container;

  protected:
	vec3 camPosOrtho = vec3(0.0f);
	vec3 lookAtCoords = vec3(0.0f);
	vec3 camTarget = vec3(0.0f);
	float zoom = 1.0f;

	double lastPointerX = -1.0;
	double lastPointerY = -1.0;

	float scrollMinY = 0.0f;
	float scrollMaxY = 0.0f;

	float contentH = 0.0f;

	float trackX = 0.0f;
	float trackY = 0.0f;
	float trackH = 0.0f;

	bool usingSlider = false;
	bool s_hookedScroll = false;
	bool s_initY = false;
	float s_initialY = 0.0f;

	void applyVerticalDeltaClamped(float dy, float minY, float maxY);
};
