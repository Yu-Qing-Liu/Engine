#include "textlabel.hpp"
#include "events.hpp"
#include "geometry.hpp"

TextLabel::TextLabel(Scene *scene, const Model::MVP &mvp, Model::ScreenParams &screenParams, const Text::FontParams &textParams, const VkRenderPass &renderPass) : Widget(scene, mvp, screenParams, renderPass) {
	textField = std::make_unique<TextField>(scene, mvp, screenParams, textParams, renderPass);

	auto mousePress = [this](int button, int action, int mods) {
		(void)button;
		(void)mods;
		// Selection toggled by whether the mouse/pointer is over us on click
		if (action == Events::ACTION_PRESS) {
			selected = textModel->mouseIsOver || container->mouseIsOver;
			if (selected) {
				container->params.color = params.activeBgColor;
				container->params.outlineColor = params.activeOutlineColor;
#if ANDROID_VK
				Events::showSoftKeyboard(true);
#endif
			} else {
				container->params.color = params.bgColor;
				container->params.outlineColor = params.outlineColor;
				textField->viewTop();
#if ANDROID_VK
				Events::hideSoftKeyboard(true);
#endif
			}
		}
	};

	Events::mouseCallbacks.push_back(mousePress);
}

void TextLabel::swapChainUpdate() {
	auto &p = params;
	container->params.color = p.bgColor;
	container->params.outlineColor = p.outlineColor;
	container->params.outlineWidth = p.outlineWidth;
	container->params.borderRadius = p.borderRadius;
	container->updateMVP(translate(mat4(1.0f), vec3(p.center, 0.0f)) * scale(mat4(1.0f), vec3(p.dim, 1.0f)), mvp.view, mvp.proj);

	textField->params.center = vec2(p.center.x - p.dim.x * 0.5, p.center.y - p.dim.y * 0.5);
    textField->params.crop = false;
	textField->params.dim = p.dim;
	textField->params.lineSpacing = p.lineSpacing;
    textField->params.scrollBarOffset = p.borderRadius;
	textField->mvp = mvp;
	textField->swapChainUpdate();

	textModel = textField->textModel.get();
}

void TextLabel::updateUniformBuffers(optional<Model::MVP> mvp) {
	textField->updateUniformBuffers(mvp);
    container->updateMVP(std::nullopt, mvp->view, mvp->proj);
}

void TextLabel::render() {
	Widget::render();
	textField->params.text = text;
	textField->params.textColor = params.textColor;
	textModel->textParams.caret.on = false;
	textField->render();
}
