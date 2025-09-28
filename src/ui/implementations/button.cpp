#include "button.hpp"

Button::Button(Scene *scene, const Model::MVP &ubo, Model::ScreenParams &screenParams, const Text::FontParams &textParams) : Widget(scene, ubo, screenParams) { textModel = std::make_unique<Text>(scene, ubo, screenParams, textParams); }

void Button::updateUniformBuffers(const Model::MVP &ubo) {
	Widget::updateUniformBuffers(ubo);
	textModel->updateUniformBuffer(ubo);
}

void Button::setParams(const StyleParams &p, std::optional<std::unique_ptr<Model>> icon) {
	styleParams = p;

	container->params.color = p.bgColor;
	container->params.outlineColor = p.outlineColor;
	container->params.outlineWidth = p.outlineWidth;
	container->params.borderRadius = p.borderRadius;
	container->updateMVP(translate(mat4(1.0f), vec3(p.center, 0.0f)) * scale(mat4(1.0f), vec3(p.dim, 1.0f)));
	textModel->updateMVP(translate(mat4(1.0f), vec3(p.textCenter, 0.0f)));

	// ICON (optional)
	if (icon.has_value()) {
		this->icon = std::move(*icon);
		if (p.iconDim) {
			this->icon->updateMVP(translate(mat4(1.0f), vec3(p.iconCenter, 0.0f)) * scale(mat4(1.0f), *p.iconDim));
		} else {
			this->icon->updateMVP(translate(mat4(1.0f), vec3(p.iconCenter, 0.0f)));
		}
	}
}

void Button::render() {
	Widget::render();
	if (!styleParams.text.empty()) {
		textModel->textParams.origin = vec3{-textModel->getPixelWidth(styleParams.text) * textModel->textParams.scale / 2.0f, textModel->getPixelHeight() * textModel->textParams.scale / 3.3, 0.0f};
		textModel->textParams.text = styleParams.text;
		textModel->textParams.color = styleParams.textColor;
		textModel->render();
	}
	if (icon) {
		icon->render();
	}
}
