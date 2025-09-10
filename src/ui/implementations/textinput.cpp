#include "textinput.hpp"

TextInput::TextInput(Scene &scene, const Model::UBO &ubo, Model::ScreenParams &screenParams, const Text::TextParams &textParams) : Widget(scene, ubo, screenParams) {
	textModel = std::make_unique<Text>(scene, ubo, screenParams, textParams);
}

void TextInput::updateUniformBuffers(const Model::UBO &ubo) {
    Widget::updateUniformBuffers(ubo);
    textModel->updateUniformBuffer(ubo);
}

void TextInput::setParams(const StyleParams &p) {
    container->params.color = p.bgColor;
    container->params.outlineColor = p.outlineColor;
    container->params.outlineWidth = p.outlineWidth;
    container->params.borderRadius = p.borderRadius;
    container->updateUniformBuffer(translate(mat4(1.0f), vec3(p.center, 0.0f)) * scale(mat4(1.0f), vec3(p.dim, 1.0f)));
    
    placeholder = p.placeholderText;
    placeholderTextColor = p.placeholderTextColor;

    textModel->updateUniformBuffer(translate(mat4(1.0f), vec3(p.textCenter, 0.0f)));
}

void TextInput::render() {
    Widget::render();
    if (text.empty()) {
        textModel->renderText(placeholder, 1.0, placeholderTextColor);
    } else {
        textModel->renderText(text, 1.0, textColor);
    }
}
