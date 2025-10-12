#include "textinput.hpp"
#include "events.hpp"
#include "geometry.hpp"

namespace {

// ---- Portable keycodes (match GLFW numeric values) ----
constexpr int KEY_ESCAPE = 256;
constexpr int KEY_ENTER = 257;
constexpr int KEY_TAB = 258;
constexpr int KEY_BACKSPACE = 259;
constexpr int KEY_INSERT = 260;
constexpr int KEY_DELETE = 261;
constexpr int KEY_RIGHT = 262;
constexpr int KEY_LEFT = 263;
// Numpad Enter in GLFW:
constexpr int KEY_KP_ENTER = 335;

inline bool is_press_or_repeat(int action) { return action == Events::ACTION_PRESS || action == Events::ACTION_REPEAT; }

} // namespace

TextInput::TextInput(Scene *scene, const Model::MVP &mvp, Model::ScreenParams &screenParams, const Text::FontParams &textParams, const VkRenderPass &renderPass) : Widget(scene, mvp, screenParams, renderPass) {
	textField = std::make_unique<TextField>(scene, mvp, screenParams, textParams, renderPass);
    textField->enableScrolling = true;
    textField->enableSlider = true;
    textField->enableMouseDrag = true;

	auto charInputCallback = [this](unsigned int codepoint) {
		if (!selected)
			return;
		if (codepoint == '\n' || codepoint == '\r' || codepoint == '\t')
			return;
		if (codepoint < 0x20u)
			return;

		// 1) Edit via TextField (wrapped-aware; operates in ORIGINAL space)
		textField->insertCodepointAtCaretInto(text, codepoint);

		// 2) Tell the field its text changed (so wrap() will map caret correctly next update)
		textField->onTextChangedExternally();

		// 3) Optional: scroll after the change
		textField->viewBottom();
	};

	auto kbPress = [this](int key, int scancode, int action, int mods) {
		(void)scancode;
		(void)mods;
		if (!selected || !is_press_or_repeat(action))
			return;

		switch (key) {
		case KEY_BACKSPACE:
			textField->backspaceAtCaretInto(text);
			textField->onTextChangedExternally();
			textField->viewBottom();
			break;

		case KEY_ENTER:
		case KEY_KP_ENTER:
			selected = false;
			break;

		case KEY_ESCAPE:
			selected = false;
			break;

		case KEY_LEFT:
			textField->moveCaretLeftInto(text);
			// movement doesn't change text; no onTextChangedExternally() needed
			break;

		case KEY_RIGHT:
			textField->moveCaretRightInto(text);
			break;

		default:
			break;
		}
	};

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

	Events::characterInputCallbacks.push_back(charInputCallback);
	Events::keyboardCallbacks.push_back(kbPress);
	Events::mouseCallbacks.push_back(mousePress);
}

void TextInput::swapChainUpdate() {
	auto &p = params;
	container->params.color = p.bgColor;
	container->params.outlineColor = p.outlineColor;
	container->params.outlineWidth = p.outlineWidth;
	container->params.borderRadius = p.borderRadius;
	container->updateMVP(translate(mat4(1.0f), vec3(p.center, 0.0f)) * scale(mat4(1.0f), vec3(p.dim, 1.0f)), mvp.view, mvp.proj);

	textField->params.center = vec2(p.center.x - p.dim.x * 0.5, p.center.y - p.dim.y * 0.5);
	textField->params.dim = p.dim;
	textField->params.lineSpacing = p.lineSpacing;
    textField->params.scrollBarOffset = p.borderRadius;
	textField->mvp = mvp;
	textField->swapChainUpdate();

	textModel = textField->textModel.get();
	caret = &textModel->textParams.caret;
	textModel->enableRayTracing(true);

	textModel->setOnMouseClick([&](int button, int action, int mods) {
		if (!textField->textModel->rayTracing->hitMapped)
			return;
		if (action != Events::ACTION_PRESS || button != Events::MOUSE_BUTTON_LEFT)
			return;

		// primId == caret position (codepoint index) in the *wrapped* string
		const int prim = textField->textModel->rayTracing->hitMapped->primId;
		size_t cp = prim < 0 ? 0u : static_cast<size_t>(prim);

		// Decide left vs right half of the clicked glyph
		size_t glyphIndex = cp;
		if (auto right = textModel->isRightHalfClick(glyphIndex)) {
			if (*right) {
				// place caret *after* the clicked glyph
				cp = glyphIndex + 1;
			} else {
				// place caret *before* the clicked glyph (keep cp as glyphIndex)
				cp = glyphIndex;
			}
		} else {
			// Fallback: behave like "after" to feel natural
			cp = glyphIndex + 1;
		}

		// We set caret directly in *wrapped* coordinates
		textField->setCaretFromWrappedCpIndex(cp);
	});
}

void TextInput::updateUniformBuffers(optional<Model::MVP> mvp) {
	textField->updateUniformBuffers(mvp);
    container->updateMVP(std::nullopt, mvp->view, mvp->proj);
}

void TextInput::render() {
	Widget::render();
	if (text.empty() && !selected) {
		textField->params.text = params.placeholderText;
		textField->params.textColor = params.placeholderTextColor;
		textModel->textParams.caret.on = false;
		textField->render();
	} else {
		if (selected) {
			if (text.empty()) {
				textField->params.text = "";
				textField->params.textColor = params.activeTextColor;
				textModel->textParams.caret.on = true;
				textField->render();
			} else {
				textField->params.text = text;
				textField->params.textColor = params.activeTextColor;
				textModel->textParams.caret.on = true;
				textField->render();
			}
		} else {
			textField->params.text = text;
			textField->params.textColor = params.textColor;
			textModel->textParams.caret.on = false;
			textField->render();
		}
	}
}
