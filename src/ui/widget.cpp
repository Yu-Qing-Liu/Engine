#include "widget.hpp"

Widget::Widget(Scene &scene, const UBO &ubo, ScreenParams &screenParams) : scene(scene), ubo(ubo), screenParams(screenParams) {}

void Widget::render() {
    for (const auto &m : components) {
        m->render();
    }
}
