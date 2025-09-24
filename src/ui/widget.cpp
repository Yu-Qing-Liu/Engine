#include "widget.hpp"

Widget::Widget(Scene *scene, const Model::MVP &ubo, Model::ScreenParams &screenParams) {
	container = std::make_unique<Rectangle>(scene, ubo, screenParams);
	container->enableRayTracing(true);
}

void Widget::updateUniformBuffers(const Model::MVP &ubo) {
    container->updateUniformBuffer(ubo);
}

void Widget::render() {
    container->render();
}

void Widget::setOnMouseHover(std::function<void()> cb) { container->onMouseHover = cb; }
void Widget::setOnMouseEnter(std::function<void()> cb) { container->onMouseEnter = cb; }
void Widget::setOnMouseExit(std::function<void()> cb) { container->onMouseExit = cb; }
void Widget::setOnMouseClick(std::function<void(int, int, int)> cb) { container->setOnMouseClick(cb); }
void Widget::setOnKeyboardKeyPress(std::function<void(int, int, int, int)> cb) { container->setOnKeyboardKeyPress(cb); }
