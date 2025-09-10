#include "textinput.hpp"
#include "events.hpp"

namespace {

// UTF-8 safe “pop back” (removes last codepoint)
inline void utf8_append(std::string &out, unsigned int cp) {
	if (cp <= 0x7Fu) {
		out.push_back(static_cast<char>(cp));
	} else if (cp <= 0x7FFu) {
		out.push_back(static_cast<char>(0xC0u | (cp >> 6)));
		out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
	} else if (cp <= 0xFFFFu) {
		if (cp >= 0xD800u && cp <= 0xDFFFu)
			return; // skip surrogate halves
		out.push_back(static_cast<char>(0xE0u | (cp >> 12)));
		out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
		out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
	} else if (cp <= 0x10FFFFu) {
		out.push_back(static_cast<char>(0xF0u | (cp >> 18)));
		out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
		out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
		out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
	}
}

// UTF-8 safe “pop back” (removes last codepoint)
inline void utf8_pop_back(std::string &s) {
	if (s.empty())
		return;
	size_t i = s.size() - 1;
	// Walk left over continuation bytes (10xxxxxx)
	while (i > 0 && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80)
		--i;
	s.erase(i);
}

inline bool is_press_or_repeat(int action) { return action == GLFW_PRESS || action == GLFW_REPEAT; }

} // namespace

TextInput::TextInput(Scene &scene, const Model::UBO &ubo, Model::ScreenParams &screenParams, const Text::TextParams &textParams) : Widget(scene, ubo, screenParams) {
	textModel = std::make_unique<Text>(scene, ubo, screenParams, textParams);

	auto charInputCallback = [this](unsigned int codepoint) {
		if (!selected)
			return;

		if (codepoint == '\n' || codepoint == '\r' || codepoint == '\t')
			return;
		if (codepoint < 0x20u)
			return;

		utf8_append(text, codepoint);
	};

	auto kbPress = [this](int key, int scancode, int action, int mods) {
		if (!selected || !is_press_or_repeat(action))
			return;

		switch (key) {
		case GLFW_KEY_BACKSPACE:
			utf8_pop_back(text);
			break;
		case GLFW_KEY_ENTER:
		case GLFW_KEY_KP_ENTER:
			// Single-line behavior: "commit" or just lose focus
			selected = false;
			break;
		case GLFW_KEY_ESCAPE:
			selected = false;
			break;
		default:
			// ignore other keys here; text comes from char callback
			break;
		}
	};

	auto mousePress = [this](int button, int action, int mods) {
		selected = container->mouseIsOver;
        if (selected) {
            container->params.color = styleParams.activeBgColor;
            container->params.outlineColor = styleParams.activeOutlineColor;
        } else {
            container->params.color = styleParams.bgColor;
            container->params.outlineColor = styleParams.outlineColor;
        }
	};

	Events::characterInputCallbacks.push_back(charInputCallback);
	Events::keyboardCallbacks.push_back(kbPress);
	Events::mouseCallbacks.push_back(mousePress);
}

void TextInput::updateUniformBuffers(const Model::UBO &ubo) {
	Widget::updateUniformBuffers(ubo);
	textModel->updateUniformBuffer(ubo);
}

void TextInput::setParams(const StyleParams &p) {
    styleParams = p;
	container->params.color = p.bgColor;
	container->params.outlineColor = p.outlineColor;
	container->params.outlineWidth = p.outlineWidth;
	container->params.borderRadius = p.borderRadius;
	container->updateUniformBuffer(translate(mat4(1.0f), vec3(p.center, 0.0f)) * scale(mat4(1.0f), vec3(p.dim, 1.0f)));
    textModel->updateUniformBuffer(translate(mat4(1.0f), vec3(p.textCenter, 0.0f)));
}

void TextInput::render() {
	Widget::render();
	if (text.empty() && !selected) {
		textModel->renderText(styleParams.placeholderText, styleParams.placeholderTextColor);
	} else {
        if (selected) {
            textModel->renderText(" ", Text::Caret{}, styleParams.activeTextColor);
        } else {
            textModel->renderText(text, styleParams.textColor);
        }
	}
}
