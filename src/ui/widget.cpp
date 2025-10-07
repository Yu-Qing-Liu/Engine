#include "widget.hpp"

Widget::Widget(Scene *scene, const Model::MVP &mvp, Model::ScreenParams &screenParams, const VkRenderPass &renderPass) : scene(scene), mvp(mvp), screenParams(screenParams) {
	container = std::make_unique<Rectangle>(scene, mvp, screenParams, renderPass);
	container->enableRayTracing(true);
}

void Widget::render() { container->render(); }

void Widget::setOnMouseHover(std::function<void()> cb) { container->onMouseHover = cb; }
void Widget::setOnMouseEnter(std::function<void()> cb) { container->onMouseEnter = cb; }
void Widget::setOnMouseExit(std::function<void()> cb) { container->onMouseExit = cb; }
void Widget::setOnMouseClick(std::function<void(int, int, int)> cb) { container->setOnMouseClick(cb); }
void Widget::setOnKeyboardKeyPress(std::function<void(int, int, int, int)> cb) { container->setOnKeyboardKeyPress(cb); }
