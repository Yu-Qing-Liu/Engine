#include "button.hpp"
#include "assets.hpp"

Button::Button(Scene &scene, const Model::UBO &ubo, Model::ScreenParams &screenParams, uint32_t fontSize) {
	Text::TextParams tp{Assets::fontRootPath + "/arial.ttf", fontSize};
	rectangle = std::make_unique<Rectangle>(scene, ubo, screenParams);
	textModel = std::make_unique<Text>(scene, ubo, screenParams, tp);
}

void Button::updateUniformBuffers(const Model::UBO &ubo) {
    rectangle->updateUniformBuffer(ubo);
    textModel->updateUniformBuffer(ubo);
}

void Button::setParams(const StyleParams &p, std::optional<std::unique_ptr<Model>> iconIn) {
	// RECTANGLE (white bg with black outline)
	rectangle->params.color = p.bgColor;
	rectangle->params.outlineColor = p.outlineColor;
	rectangle->params.outlineWidth = p.outlineWidth;
	rectangle->params.borderRadius = p.borderRadius;

	// place rect: center + scale to pixel dims
	rectangle->updateUniformBuffer(translate(mat4(1.0f), vec3(p.center, 0.0f)) * scale(mat4(1.0f), vec3(p.dim, 1.0f)));

	// TEXT
	if (p.text) {
		label = *p.text;
    }
	if (p.textColor) {
		labelColor = *p.textColor;
    }

	// Put text at same center as the rect. Text::renderText scales by font size,
	// so we only need a translate here.
	textModel->updateUniformBuffer(translate(mat4(1.0f), vec3(p.center, 0.0f)));

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
	rectangle->render();
	textModel->renderText(label, 1.0f, labelColor);
	if (icon) {
		icon->render();
    }
}
