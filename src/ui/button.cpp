#include "button.hpp"

Button::Button(Scene &scene, const UBO &ubo, ScreenParams &screenParams, uint32_t fontSize) {
    Text::TextParams tp{ Assets::fontRootPath + "/arial.ttf", fontSize };
    container = make_unique<Rectangle>(scene, ubo, screenParams);
    textModel = make_unique<Text>(scene, ubo, screenParams, tp);
}

void Button::setParams() {
    container->updateUniformBuffer(
        translate(container->ubo.model, vec3(btnCenter, 0.0)) * 
        scale(container->ubo.model, vec3(dim, 1.0))
    );
    icon->updateUniformBuffer(
        translate(icon->ubo.model, vec3(iconCenter, 0.0)) *
        scale(icon->ubo.model, iconDim)
    );
}

void Button::render() {
    container->render();
    textModel->renderText(text, 1.0, textColor);
    if (icon) {
        icon->render();
    }
}
