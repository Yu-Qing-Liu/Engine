#include "textinput.hpp"
#include "events.hpp"
#include "geometry.hpp"
#include <algorithm>

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

inline bool is_empty(const std::string &s) {
	return std::all_of(s.begin(), s.end(), [](unsigned char ch) {
		return std::isspace(ch); // ' ', '\t', '\n', '\r', '\f', '\v'
	});
}

} // namespace

TextInput::TextInput(Scene *scene, const Model::MVP &mvp, Model::ScreenParams &screenParams, const Text::FontParams &textParams, const VkRenderPass &renderPass) : Widget(scene, mvp, screenParams, renderPass) {
	textField = std::make_unique<TextField>(scene, mvp, screenParams, textParams, renderPass);
	textField->enableScrolling = true;
	textField->enableSlider = true;
	textField->enableMouseDrag = true;

	// --- character input callback ---
	Events::characterInputCallbacks.push_back([this](unsigned int codepoint) {
		if (!selected || !textField || !textField->textModel)
			return;

		// Allow space ' ' (U+0020); skip other ASCII whitespace; allow non-ASCII
		if (codepoint < 128 && std::isspace(static_cast<unsigned char>(codepoint)) && codepoint != 32) {
			return;
		}

		textField->insertCodepointAtCaretInto(text, codepoint);
		textField->onTextChangedExternally();
		textField->wrap();
		textField->viewBottom();
	});

	// --- keyboard press handler ---
	Events::keyboardCallbacks.push_back([this](int key, int scancode, int action, int mods) {
		(void)scancode;
		(void)mods;
		if (!selected || !is_press_or_repeat(action))
			return;
		if (!textField || !textField->textModel)
			return;

		switch (key) {
		case KEY_BACKSPACE:
			textField->backspaceAtCaretInto(text);
			textField->onTextChangedExternally();
			textField->wrap();
			textField->viewBottom();
			break;

		case KEY_ENTER:
		case KEY_KP_ENTER:
			textField->insertCodepointAtCaretInto(text, '\n');
			textField->onTextChangedExternally();
			textField->wrap();
			textField->viewBottom();
			break;

		case KEY_TAB:
			// insert 4 spaces
			textField->insertCodepointAtCaretInto(text, ' ');
			textField->insertCodepointAtCaretInto(text, ' ');
			textField->insertCodepointAtCaretInto(text, ' ');
			textField->insertCodepointAtCaretInto(text, ' ');
			textField->onTextChangedExternally();
			textField->wrap();
			textField->viewBottom();
			break;

		case KEY_ESCAPE:
			selected = false;
			break;

		case KEY_LEFT:
			textField->moveCaretLeftInto(text);
			break;

		case KEY_RIGHT:
			textField->moveCaretRightInto(text);
			break;

		default:
			break;
		}
	});

	// --- mouse press handler ---
	Events::mouseCallbacks.push_back([this](int button, int action, int mods) {
		(void)button;
		(void)mods;
		if (action != Events::ACTION_PRESS)
			return;

		// Re-fetch internals safely each time
		Text *tm = (textField ? textField->textModel.get() : nullptr);
		const bool hitText = (tm && tm->rayTracing && tm->rayTracing->hitPos);
		const bool hitBox = (container && container->rayTracing && container->rayTracing->hitPos);

		selected = hitText || hitBox;

		if (selected) {
			container->params.color = params.activeBgColor;
			container->params.outlineColor = params.activeOutlineColor;
#if ANDROID_VK
			Events::showSoftKeyboard(true);
#endif
		} else {
			container->params.color = params.bgColor;
			container->params.outlineColor = params.outlineColor;
			if (textField)
				textField->viewTop();
#if ANDROID_VK
			Events::hideSoftKeyboard(true);
#endif
		}
	});
}

void TextInput::swapChainUpdate() {
	auto &p = params;

	// Container visuals + transform
	container->params.color = p.bgColor;
	container->params.outlineColor = p.outlineColor;
	container->params.outlineWidth = p.outlineWidth;
	container->params.borderRadius = p.borderRadius;
	container->updateMVP(translate(mat4(1.0f), vec3(p.center, 0.0f)) * scale(mat4(1.0f), vec3(p.dim, 1.0f)), mvp.view, mvp.proj);

	// TextField layout + MVP
	textField->params.center = vec2(p.center.x - p.dim.x * 0.5f, p.center.y - p.dim.y * 0.5f);
	textField->params.dim = p.dim;
	textField->params.lineSpacing = p.lineSpacing;
	textField->params.scrollBarOffset = p.borderRadius;
	textField->mvp = mvp;

	// This recreates textField->textModel internally.
	textField->swapChainUpdate();

	// Enable ray tracing after creation
	if (textField->textModel) {
		textField->textModel->enableRayTracing(true);

		// Rebind click handler on the *current* textModel; guard every access
		textField->textModel->setOnMouseClick([this](int button, int action, int mods) {
			(void)mods;
			if (action != Events::ACTION_PRESS || button != Events::MOUSE_BUTTON_LEFT)
				return;
			if (!selected || !textField || !textField->textModel)
				return;
			Text *tm = textField->textModel.get();
			if (!tm || !tm->rayTracing || !tm->rayTracing->hitMapped)
				return;

			// primId == caret position (codepoint index) in the *wrapped* string
			const int prim = tm->rayTracing->hitMapped->primId;
			size_t cp = prim < 0 ? 0u : static_cast<size_t>(prim);

			// Decide left vs right half of the clicked glyph
			size_t glyphIndex = cp;
			if (auto right = tm->isRightHalfClick(glyphIndex)) {
				cp = *right ? glyphIndex + 1 : glyphIndex;
			} else {
				// Fallback: behave like "after" to feel natural
				cp = glyphIndex + 1;
			}

			// Set caret directly in *wrapped* coordinates
			textField->setCaretFromWrappedCpIndex(cp);
		});
	}
}

void TextInput::updateUniformBuffers(std::optional<Model::MVP> mvpOpt) {
	// Keep the field responsive even if no external MVP is passed this frame
	if (textField) {
		textField->updateUniformBuffers(mvpOpt);
	}

	if (mvpOpt) {
		container->updateMVP(std::nullopt, mvpOpt->view, mvpOpt->proj);
	} else {
		// No override: safest is to *not* fabricate view/proj from empties.
		container->updateMVP(std::nullopt, std::nullopt, std::nullopt);
	}
}

void TextInput::render() {
	Widget::render();

	// Not ready yet? Skip.
	if (!textField || !textField->textModel)
		return;

	Text *tm = textField->textModel.get();

	// Decide content + colors
	if (selected) {
		textField->params.text = is_empty(text) ? "" : text;
		tm->textParams.color = params.activeTextColor;
		tm->textParams.caret.on = true;
	} else {
		if (is_empty(text)) {
			textField->params.text = params.placeholderText;
			tm->textParams.color = params.placeholderTextColor;
			tm->textParams.caret.on = false;
		} else {
			textField->params.text = text;
			tm->textParams.color = params.textColor;
			tm->textParams.caret.on = false;
		}
	}

	textField->render();
}
