#include "button.hpp"
#include "geometry.hpp"

Button::Button(Scene *scene, const Model::MVP &ubo, Model::ScreenParams &screenParams, const Text::FontParams &textParams) : Widget(scene, ubo, screenParams) { textModel = std::make_unique<Text>(scene, ubo, screenParams, textParams); }

void Button::swapChainUpdate() {
	auto &p = styleParams;
	container->params.color = p.bgColor;
	container->params.outlineColor = p.outlineColor;
	container->params.outlineWidth = p.outlineWidth;
	container->params.borderRadius = p.borderRadius;
	container->updateMVP(translate(mat4(1.0f), vec3(p.center, 0.0f)) * scale(mat4(1.0f), vec3(p.dim, 1.0f)), mvp.view, mvp.proj);
	textModel->updateMVP(translate(mat4(1.0f), vec3(p.textCenter, 0.0f)), mvp.view, mvp.proj);
}

void Button::render() {
	Widget::render();
	if (!styleParams.text.empty()) {
		textModel->textParams.origin = Geometry::alignTextCentered(*textModel, styleParams.text);
		textModel->textParams.text = styleParams.text;
		textModel->textParams.color = styleParams.textColor;
		textModel->render();
	}
}
