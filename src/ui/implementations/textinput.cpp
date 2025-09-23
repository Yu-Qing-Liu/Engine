#include "textinput.hpp"
#include "events.hpp"

namespace {

// ---- Portable keycodes (match GLFW numeric values) ----
constexpr int KEY_ESCAPE     = 256;
constexpr int KEY_ENTER      = 257;
constexpr int KEY_TAB        = 258;
constexpr int KEY_BACKSPACE  = 259;
constexpr int KEY_INSERT     = 260;
constexpr int KEY_DELETE     = 261;
constexpr int KEY_RIGHT      = 262;
constexpr int KEY_LEFT       = 263;
// Numpad Enter in GLFW:
constexpr int KEY_KP_ENTER   = 335;

inline bool is_press_or_repeat(int action) {
	return action == Events::ACTION_PRESS || action == Events::ACTION_REPEAT;
}

static inline bool is_cont_byte(unsigned char b) { return (b & 0xC0) == 0x80; }

static inline size_t utf8_len_from_lead(unsigned char b) {
	if (b < 0x80u) return 1;
	if ((b >> 5) == 0x6) return 2; // 110xxxxx
	if ((b >> 4) == 0xE) return 3; // 1110xxxx
	if ((b >> 3) == 0x1E) return 4; // 11110xxx
	return 1; // fallback on malformed
}

// Snap a byte index to the start of a codepoint (move left over continuation bytes)
static inline size_t snap_to_cp_start(const std::string &s, size_t pos) {
	pos = std::min(pos, s.size());
	while (pos > 0 && is_cont_byte(static_cast<unsigned char>(s[pos - 1]))) --pos;
	return pos;
}

// Move left by one UTF-8 codepoint from byte position 'pos'
static inline size_t cp_left(const std::string& s, size_t pos) {
	if (s.empty() || pos == 0) return 0;
	pos = std::min(pos, s.size());
	size_t i = pos - 1;
	// back up over continuation bytes to a lead byte (or start)
	while (i > 0 && is_cont_byte(static_cast<unsigned char>(s[i]))) --i;
	return i;
}

// Move right by one UTF-8 codepoint from byte position 'pos'
static inline size_t cp_right(const std::string& s, size_t pos) {
	if (s.empty()) return 0;
	pos = std::min(pos, s.size());
	// if we're at a continuation byte, snap to start of this cp first
	pos = snap_to_cp_start(s, pos);
	if (pos == s.size()) return pos;
	const unsigned char lead = static_cast<unsigned char>(s[pos]);
	const size_t len = utf8_len_from_lead(lead);
	return std::min(pos + len, s.size());
}

} // namespace

TextInput::TextInput(Scene *scene, const Model::MVP &ubo, Model::ScreenParams &screenParams, const Text::TextParams &textParams)
	: Widget(scene, ubo, screenParams) {
	textModel = std::make_unique<Text>(scene, ubo, screenParams, textParams);

	auto charInputCallback = [this](unsigned int codepoint) {
		if (!selected) return;

		if (codepoint == '\n' || codepoint == '\r' || codepoint == '\t') return;
		if (codepoint < 0x20u) return;

		utf8_append(text, codepoint, caret.byte);
	};

	auto kbPress = [this](int key, int scancode, int action, int mods) {
		(void)scancode; (void)mods;
		if (!selected || !is_press_or_repeat(action)) return;

		switch (key) {
		case KEY_BACKSPACE:
			utf8_pop_back(text, caret.byte);
			break;
		case KEY_ENTER:
		case KEY_KP_ENTER:
			// Single-line behavior: "commit" or just lose focus
			selected = false;
			break;
		case KEY_ESCAPE:
			selected = false;
			break;
		case KEY_LEFT:
			caret.byte = cp_left(text, caret.byte);
			break;
		case KEY_RIGHT:
			caret.byte = cp_right(text, caret.byte);
			break;
		default:
			// ignore other keys here; text comes from char callback
			break;
		}
	};

	auto mousePress = [this](int button, int action, int mods) {
		(void)button; (void)mods;
		// Selection toggled by whether the mouse/pointer is over us on click
		if (action == Events::ACTION_PRESS) {
			selected = container->mouseIsOver;
			if (selected) {
				container->params.color = styleParams.activeBgColor;
				container->params.outlineColor = styleParams.activeOutlineColor;
                #if ANDROID_VK
                Events::showSoftKeyboard(true);
                #endif
			} else {
				container->params.color = styleParams.bgColor;
				container->params.outlineColor = styleParams.outlineColor;
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

// UTF-8 safe “append” (append codepoint)
void TextInput::utf8_append(std::string &out, unsigned int cp, size_t position) {
	// Skip UTF-16 surrogate halves (invalid in UTF-8 scalar range)
	if (cp >= 0xD800u && cp <= 0xDFFFu) return;

	// Snap to a valid codepoint boundary
	size_t pos = snap_to_cp_start(out, position);

	// Encode cp to UTF-8
	char buf[4];
	size_t n = 0;
	if (cp <= 0x7Fu) {
		buf[n++] = static_cast<char>(cp);
	} else if (cp <= 0x7FFu) {
		buf[n++] = static_cast<char>(0xC0u | (cp >> 6));
		buf[n++] = static_cast<char>(0x80u | (cp & 0x3Fu));
	} else if (cp <= 0xFFFFu) {
		buf[n++] = static_cast<char>(0xE0u | (cp >> 12));
		buf[n++] = static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
		buf[n++] = static_cast<char>(0x80u | (cp & 0x3Fu));
	} else if (cp <= 0x10FFFFu) {
		buf[n++] = static_cast<char>(0xF0u | (cp >> 18));
		buf[n++] = static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu));
		buf[n++] = static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
		buf[n++] = static_cast<char>(0x80u | (cp & 0x3Fu));
	} else {
		return; // out of Unicode range
	}

	out.insert(out.begin() + static_cast<std::ptrdiff_t>(pos), buf, buf + n);

	// Move caret to just after inserted codepoint
	caret.byte = pos + n;
}

// UTF-8 safe “pop back” (removes prev codepoint before caret)
void TextInput::utf8_pop_back(std::string &s, size_t position) {
	if (s.empty()) return;

	// Clamp to string end, then if position is 0 => nothing to delete
	size_t pos = std::min(position, s.size());
	if (pos == 0) return;

	// Move to the start of the codepoint immediately BEFORE 'pos'
	size_t i = cp_left(s, pos);

	// Determine cp length from lead
	size_t len = utf8_len_from_lead(static_cast<unsigned char>(s[i]));
	size_t end = std::min(i + len, s.size());
	// Guard against malformed: extend end over continuation bytes if needed
	while (end < s.size() && is_cont_byte(static_cast<unsigned char>(s[end]))) ++end;

	s.erase(i, end - i);

	// Move caret to new position (start of where that cp was)
	caret.byte = i;
}

void TextInput::updateUniformBuffers(const Model::MVP &ubo) {
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
			if (text.empty()) {
				textModel->renderText(" ", Text::Caret{}, styleParams.activeTextColor);
			} else {
				textModel->renderText(text, caret, styleParams.activeTextColor);
			}
		} else {
			textModel->renderText(text, styleParams.textColor);
		}
	}
}
