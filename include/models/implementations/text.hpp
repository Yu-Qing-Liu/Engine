// text.hpp
#pragma once

#include "assets.hpp"
#include "colors.hpp"
#include "model.hpp"
#include "rectangle.hpp"

#include <freetype/freetype.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan_core.h>

using std::pair;
using std::string;

class Text : public Model {
  public:
	Text(Scene *scene);
	~Text();

	struct InstanceData {}; // unused

	struct Vertex {
		vec2 pos;	 // loc 0
		vec2 uv;	 // loc 1
		vec4 color;	 // loc 2
		float sdfPX; // loc 3
	};

	struct Features {
		bool prewarm = false;
		bool caret = false;
		bool selection = false;
	};

	struct FTData {
		FT_Library lib = nullptr;
		FT_Face face = nullptr;
		int pixelHeight = 48;
		int sdfSpread = 12;
	};

	struct Glyph {
		int advanceX = 0;
		int bearingX = 0;
		int bearingY = 0;
		int width = 0, height = 0;
		float u0 = 0, v0 = 0, u1 = 0, v1 = 0;
		float sdfSpreadPx = 8.f;
	};

	struct TextPC {
		mat4 model = mat4(1.0f);
		float time = 0.0f;
		float textOriginX;
		float textOriginY;
		float textExtentX;
		float textExtentY;
	};

	void init() override; // pipeline created exactly once here
	void record(VkCommandBuffer cmd) override;

	// API
	void setFont(const string &fontPath);
	void setText(const std::string &utf8);
	void setSize(int size);
	void setLocation(const vec3 &location);
	void setMaxTextWidthPx(float w);
	void setCaret(size_t pos);
	void setSelectionColor(const glm::vec4 &color);
	void setLineSpacing(float px);
	void setColor(const vec4 &rgba);

	void showCaret();
	void hideCaret();
	void enableTextSelection(bool enable);
	void setCaretColor(const glm::vec4 &color);

	void enableDepth() { cull_ = true; }

	uint32_t getCaretPosition() { return caretPosition; }
	uint32_t getCaretHoverPosition() { return caretHoverPosition; }

	float fontAscentPx() const { return ft ? (float)ft->face->size->metrics.ascender / 64.0f : 0.0f; }

	void rebuild(); // CPU layout + VB/IB upload only (no pipeline work)

	Model *caretHitboxesModel() { return caretHitboxes.get(); }
	Model *charHitboxesModel() { return charHitboxes.get(); }

	uint32_t textLength() { return textLength_; }

	float getContentHeightScreenPx(float bottomPadding) const;
	void setViewHeightPx(float h) {
		viewHeightPx_ = (std::max)(1.0f, h);
		needRebuild = true;
	}

	Features features{};

	// ---- atlas/SDF ----
	struct Atlas {
		std::unordered_map<uint32_t, Glyph> glyphs;
		int texW = 0, texH = 0;
		VkImage image = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		VkImageView view = VK_NULL_HANDLE;
		VkSampler sampler = VK_NULL_HANDLE;
	};

	// Shared per-font atlas; this points into a global registry.
	Atlas *atlas = nullptr;

	// Atlas packing constants (shared across all fonts)
	static constexpr int kPad = 2;
	static constexpr int kGutter = 2;
	static constexpr int kMaxW = 2048;
	static constexpr int kMaxH = 2048;

	// Shared-ARENA bookkeeping (this object’s slice)
	VkDeviceSize vbOffset_ = 0, ibOffset_ = 0;
	VkDeviceSize vbCapacity_ = 0, ibCapacity_ = 0; // reserved slice sizes
	bool arenaAllocated_ = false;

	// CPU geometry
	std::vector<Vertex> cpuVerts;
	std::vector<uint32_t> cpuIdx;

	void writeAtlasDescriptor();

  protected:
	void syncPickingInstances() override;
	void createGraphicsPipeline() override;	  // sets depth/cull
	uint32_t createDescriptorPool() override; // adds binding 2 (sampler)
	void createDescriptors() override;

  private:
	// ---- config/state ----
	string fontPath = Assets::fontRootPath + "/arialBd.ttf";
	int fontSize = 48;
	bool cull_ = false;

	string text = "\033[0;31mSome \033[0mText: abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!";
	float maxTextWidthPx = 800.f;
	TextPC pc{};

	uint32_t textLength_;
	vec4 caretColor = Colors::White;
	bool caretOn = false;

	std::vector<pair<size_t, size_t>> selectionRanges{};
	glm::vec4 selectionColor = Colors::Yellow(0.5f);
	bool selecting = false;
	bool enableTextSelection_ = false;
	float lineSpacing = 0.f;

	vec4 baseColor = Colors::White;

	uint32_t caretPosition = 0;
	uint32_t caretHoverPosition = 0;
	std::unique_ptr<Model> caretHitboxes;
	size_t lastCaretInstanceCount_ = 0;
	bool caretHitboxesInited_ = false;

	std::vector<vec4> charRects{};
	float selectionMinDragDistancePx_ = 4.0f;
	bool dragSelectionStarted = false;
	std::unique_ptr<Model> charHitboxes;
	size_t lastcharInstanceCount_ = 0;
	bool charHitboxesInited_ = false;
	vec2 selectionBoxStartPx_{0.0f};
	vec2 selectionBoxEndPx_{0.0f};
	bool selectionBoxActive_ = false;
	glm::vec2 dragStartPx_{0.0f};
	glm::vec2 lastMousePx_{0.0f};

	std::string textSelectMouseClickEventId = "";
	std::string textSelectMouseMoveEventId = "";

	// dirty flags
	bool needRebuild = true;
	bool needAtlas = true;

	bool registeredInSharedAtlas_ = false;

	// FT
	std::unique_ptr<FTData> ft;
	void ensureFT();

	bool addedBinding2 = false; // guard to avoid re-adding binding 2 on swapchain rebuilds

	// Text scroll params
	float scrollOffsetPx_ = 0.f;
	float contentHeightPx_ = 0.f;
	float viewHeightPx_ = 0.f;

	// helpers
	void prewarmBasicLatinAndBox();

	void ensureAtlas();

	void layoutAndBuild();
	vec2 measureTextBox(std::u32string &u32);
	void setSelection(const std::vector<pair<size_t, size_t>> &ranges);

	glm::vec2 localToScreen(const glm::vec2 &p) const;
	glm::vec2 windowToViewportPx(float mx, float my) const;

	void buildCaretVisualAndHitboxes(const std::vector<glm::vec4> &carets);
	void buildCharVisualAndHitboxes(const std::vector<glm::vec4> &boxes);

	void updateBuffersGPU();

	void setModel(const glm::mat4 &m) { pc.model = m; }

	void setSelectionBoxPx(const glm::vec2 &startPx, const glm::vec2 &endPx);
	void clearSelectionBox();

	struct ColorRun {
		size_t start = 0, end = 0;
		vec4 color{};
	};
	std::u32string parseAnsiToRuns(std::vector<ColorRun> &runs) const;

	static std::vector<uint8_t> bitmapToSDF(const uint8_t *alpha, int w, int h, int spreadPx);
	static vec4 ansiIndexToColor(int idx, const vec4 &fallback);

	// GPU upload (buffers only)
	void uploadVBIB();
};
