#include "model.hpp"
#include "rectangle.hpp"

class Widget {
  public:
	Widget(Scene &scene, const Model::UBO &ubo, Model::ScreenParams &screenParams);
	Widget(Widget &&) = delete;
	Widget(const Widget &) = delete;
	Widget &operator=(Widget &&) = delete;
	Widget &operator=(const Widget &) = delete;
	~Widget() = default;

	virtual void updateUniformBuffers(const Model::UBO &ubo);
	virtual void render();

	virtual void setOnMouseHover(std::function<void()> cb);
	virtual void setOnMouseEnter(std::function<void()> cb);
	virtual void setOnMouseExit(std::function<void()> cb);

	virtual void setOnMouseClick(std::function<void(int, int, int)> cb);
	virtual void setOnKeyboardKeyPress(std::function<void(int, int, int, int)> cb);

	std::unique_ptr<Rectangle> container;
};
