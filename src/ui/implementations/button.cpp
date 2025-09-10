#include "button.hpp"

Button::Button(Scene &scene, const Model::UBO &ubo, Model::ScreenParams &screenParams, const Text::TextParams &textParams) : Widget(scene, ubo, screenParams) {
	textModel = std::make_unique<Text>(scene, ubo, screenParams, textParams);
}

void Button::updateUniformBuffers(const Model::UBO &ubo) {
    Widget::updateUniformBuffers(ubo);
	textModel->updateUniformBuffer(ubo);
}

void Button::setParams(const StyleParams &p, std::optional<std::unique_ptr<Model>> icon) {
    styleParams = p;

	container->params.color = p.bgColor;
	container->params.outlineColor = p.outlineColor;
	container->params.outlineWidth = p.outlineWidth;
	container->params.borderRadius = p.borderRadius;
	container->updateUniformBuffer(translate(mat4(1.0f), vec3(p.center, 0.0f)) * scale(mat4(1.0f), vec3(p.dim, 1.0f)));
	textModel->updateUniformBuffer(translate(mat4(1.0f), vec3(p.textCenter, 0.0f)));

	// ICON (optional)
	if (icon.has_value()) {
		this->icon = std::move(*icon);
		if (p.iconDim) {
			this->icon->updateUniformBuffer(translate(mat4(1.0f), vec3(p.iconCenter, 0.0f)) * scale(mat4(1.0f), *p.iconDim));
		} else {
			this->icon->updateUniformBuffer(translate(mat4(1.0f), vec3(p.iconCenter, 0.0f)));
		}
	}
}

void Button::render() {
    Widget::render();
    if (!styleParams.text.empty()) {
        textModel->renderText(styleParams.text, 1.0f, styleParams.textColor);
    }
	if (icon) {
		icon->render();
	}
}
