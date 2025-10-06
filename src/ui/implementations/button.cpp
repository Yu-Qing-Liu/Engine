#include "button.hpp"
#include "geometry.hpp"

Button::Button(Scene *scene, const Model::MVP &ubo, Model::ScreenParams &screenParams, const Text::FontParams &textParams) : Widget(scene, ubo, screenParams) { textModel = std::make_unique<Text>(scene, ubo, screenParams, textParams); }

void Button::updateUniformBuffers(const Model::MVP &ubo) {
	Widget::updateUniformBuffers(ubo);
	textModel->updateUniformBuffer(ubo);
}

void Button::renderPass() {
	Widget::renderPass();
	if (!styleParams.text.empty()) {
		textModel->textParams.origin = Geometry::alignTextCentered(*textModel, styleParams.text);
		textModel->textParams.text = styleParams.text;
		textModel->textParams.color = styleParams.textColor;
		textModel->render();
	}
}
