#include "button.hpp"

Button::Button(Scene &scene, const Model::UBO &ubo, Model::ScreenParams &screenParams, const Text::TextParams &textParams) : Widget(scene, ubo, screenParams) {
	textModel = std::make_unique<Text>(scene, ubo, screenParams, textParams);
}

void Button::updateUniformBuffers(const Model::UBO &ubo) {
    Widget::updateUniformBuffers(ubo);
	textModel->updateUniformBuffer(ubo);
}

void Button::setParams(const StyleParams &p, std::optional<std::unique_ptr<Model>> iconIn) {
	// RECTANGLE (white bg with black outline)
	container->params.color = p.bgColor;
	container->params.outlineColor = p.outlineColor;
	container->params.outlineWidth = p.outlineWidth;
	container->params.borderRadius = p.borderRadius;

	// place rect: center + scale to pixel dims
	container->updateUniformBuffer(translate(mat4(1.0f), vec3(p.center, 0.0f)) * scale(mat4(1.0f), vec3(p.dim, 1.0f)));

	// TEXT
	if (p.text) {
		label = *p.text;
	}
	if (p.textColor) {
		labelColor = *p.textColor;
	}

	// Put text at same center as the rect. Text::renderText scales by font size,
	// so we only need a translate here.
	textModel->updateUniformBuffer(translate(mat4(1.0f), vec3(p.textCenter, 0.0f)));

	// ICON (optional)
	if (iconIn.has_value()) {
		icon = std::move(*iconIn);
		if (p.iconDim) {
			icon->updateUniformBuffer(translate(mat4(1.0f), vec3(p.iconCenter, 0.0f)) * scale(mat4(1.0f), *p.iconDim));
		} else {
			icon->updateUniformBuffer(translate(mat4(1.0f), vec3(p.iconCenter, 0.0f)));
		}
	}
}

void Button::render() {
    Widget::render();
	textModel->renderText(label, 1.0f, labelColor);
	if (icon) {
		icon->render();
	}
}
