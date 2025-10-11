#pragma once

#include "colors.hpp"
#include "instancedrectangle.hpp"
#include "text.hpp"
#include "widget.hpp"

class TextField : public Widget {
  public:
	TextField(TextField &&) = delete;
	TextField(const TextField &) = delete;
	TextField &operator=(TextField &&) = delete;
	TextField &operator=(const TextField &) = delete;
	~TextField() = default;

	struct Params {
		string text = "Placeholder";
		vec2 center = vec2(0.0f, 0.0f);	 // true center in screen pixels
		vec2 dim = vec2(800.0f, 800.0f); // width/height in pixels
		float lineSpacing = 0.0f;		 // extra spacing added to natural line height
		vec4 margins = vec4(0.0f);		 // L,T,R,B
		vec4 textColor = Colors::Red;
		vec4 sliderColor = Colors::Gray(0.55);
		vec4 sliderColorPressed = Colors::Gray;
		float scrollBarWidth = 12.0f;
		float scrollBarOffset = 0.0f;
		bool crop = true;
	};

	TextField(Scene *scene, const Model::MVP &mvp, Model::ScreenParams &screenParams, const Text::FontParams &textParams, const VkRenderPass &renderPass = Engine::renderPass);

	void swapChainUpdate();
	void updateUniformBuffers(optional<Model::MVP> mvp = std::nullopt);
	void render() override;

	void viewBottom();
	void viewTop();

	bool enableMouseDrag = false;
	bool enableScrolling = false;
	bool enableSlider = false;
    bool showScrollBar = true;

	Params params{};
	Text::FontParams textParams;

	Model::ScreenParams sp;	  // inner scissor/viewport (text)
	Model::ScreenParams bgSp; // outer (+margins) if you want background widgets to use it

	std::unique_ptr<Text> textModel;
	unique_ptr<InstancedRectangle> scrollBar;

	// === Mapping tables built by wrap() ===
	// original-byte → wrapped(para)-byte (size = original.size() + 1)
	std::vector<size_t> mapOrigToParaByte;
	// wrapped(para)-byte → original-byte (size = para.size() + 1)
	std::vector<size_t> mapParaByteToOrig;
	// wrapped codepoint index → wrapped byte offset (size = #cps + 1; last is para.size())
	std::vector<size_t> paraCpIndexToParaByte;

	// If true, caret->byte is already a wrapped (para) byte offset and wrap() won’t remap it.
	bool caretFromWrapped = false;

	// Set caret from a wrapped **codepoint index** (0..#cps).
	// If you want caret after the clicked glyph, pass cpIndex+1 from the caller.
	void setCaretFromWrappedCpIndex(size_t cpIndex);

	void insertCodepointAtCaretInto(std::string &external, unsigned int cp);
	void backspaceAtCaretInto(std::string &external);
	void moveCaretLeftInto(const std::string &external);  // movement only
	void moveCaretRightInto(const std::string &external); // movement only

	// Call this whenever you mutate params.text externally (typing, delete, paste, etc.)
	inline void onTextChangedExternally() { caretFromWrapped = false; }

  private:
	InstancedRectangleData slider{};

	std::string cache;

	// --- internal UTF-8 helpers (no longer in TextInput) ---
	static bool is_cont_byte(unsigned char b);
	static size_t utf8_len_from_lead(unsigned char b);
	static size_t snap_to_cp_start(const std::string &s, size_t pos);
	static size_t cp_left_utf8(const std::string &s, size_t pos);
	static size_t cp_right_utf8(const std::string &s, size_t pos);
	static size_t utf8_append_at(std::string &out, unsigned int cp, size_t position);
	static size_t utf8_delete_prev(std::string &s, size_t position);

	// caret mapping helpers
	size_t caretParaByte() const;	 // current caret in wrapped bytes
	size_t caretOrigByte() const;	 // current caret in original bytes (via mapParaByteToOrig)
	void setCaretOrigByte(size_t b); // set caret as ORIGINAL byte (wrap() will map it)

	void updateScreenParams();
	void recomputeScissorForCurrentView();
	void wrap();

	void createScrollBar();
	void updateSlider();
	void dragSliderToCursor();
	void mouseDragY(float &scrollMinY, float &scrollMaxY, bool inverted);
};
