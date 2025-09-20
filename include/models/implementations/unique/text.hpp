#pragma once

#include "colors.hpp"
#include "model.hpp"
#include "fonts.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <vulkan/vulkan_core.h>

class Text : public Model {
  public:
	Text(Text &&) = delete;
	Text(const Text &) = delete;
	Text &operator=(Text &&) = delete;
	Text &operator=(const Text &) = delete;
	~Text() override;

	struct TextParams {
		string fontPath = Fonts::Arial;
		uint32_t pixelHeight = 24;
		vector<uint32_t> codepoints;
		uint32_t maxAtlasWidth = 2048;
		uint32_t padding = 1;
	};

	struct BillboardParams {
		vec3 centerWorld;	 // location
		vec2 offsetPx{0, 0}; // pixel offset from the center (e.g. {-w/2, -h} to center/above)
	};

	struct GlyphVertex {
		glm::vec3 pos;	// world space
		glm::vec2 uv;	// atlas
		float xNorm;	// 0 .. 1 across the quad
		uint32_t flags; // bit0 sel, bit1 caretBefore, bit2 caretAfter
		float quadW;	// quad width in pixels

		static VkVertexInputBindingDescription getBindingDescription() {
			VkVertexInputBindingDescription d{};
			d.binding = 0;
			d.stride = sizeof(GlyphVertex);
			d.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			return d;
		}
		static std::array<VkVertexInputAttributeDescription, 5> getAttributeDescriptions() {
			std::array<VkVertexInputAttributeDescription, 5> a{};
			a[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(GlyphVertex, pos)};
			a[1] = {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(GlyphVertex, uv)};
			a[2] = {2, 0, VK_FORMAT_R32_SFLOAT, offsetof(GlyphVertex, xNorm)};
			a[3] = {3, 0, VK_FORMAT_R32_UINT, offsetof(GlyphVertex, flags)};
			a[4] = {4, 0, VK_FORMAT_R32_SFLOAT, offsetof(GlyphVertex, quadW)};
			return a;
		}
	};

	struct GlyphMeta {
		ivec2 size;		  // bitmap size
		ivec2 bearing;	  // left/top bearing
		uint32_t advance; // in pixels
		vec2 uvMin;		  // [0..1]
		vec2 uvMax;		  // [0..1]
	};

	struct Caret {
		size_t byte = 0;
		float px = 1.0f;
		glm::vec4 color{Colors::White(0.8)};
		bool on = true;
	};

	struct SelectionRange {
		size_t start = 0, end = 0;		 // byte offsets [start,end)
		glm::vec4 color{0, 0, 1, 0.25f}; // default blue-ish bg
	};

	struct SelectionMatches {
		std::string needle; // plain byte-wise match
		bool caseSensitive = true;
		glm::vec4 color{1, 1, 0, 0.25f}; // default yellow-ish bg
	};

	Text(Scene *scene, const UBO &ubo, ScreenParams &screenParams, const TextParams &params);

	float getPixelWidth(const std::string &text, float scale = 1.0f) const;
	float getPixelHeight();

    string text = "Some text!";
    vec4 color = Colors::Red;

    void render() override;

	// 1) normal (no caret, no selection) – existing one calls this behavior
	void renderText(const std::string &text, const glm::vec4 &color = Colors::Red, float scale = 1.0f, std::optional<glm::vec3> origin = std::nullopt);

	// 2) caret at byte index
	void renderText(const std::string &text, const Caret &caret, const glm::vec4 &color = Colors::Red, float scale = 1.0f, std::optional<glm::vec3> origin = std::nullopt);

	// 3) selection [start,end)
	void renderText(const std::string &text, const SelectionRange &sel, std::optional<Caret> caret = std::nullopt, const glm::vec4 &color = Colors::Red, float scale = 1.0f, std::optional<glm::vec3> origin = std::nullopt);

	// 4) select all matches of `needle`
	void renderText(const std::string &text, const SelectionMatches &matches, std::optional<Caret> caret = std::nullopt, const glm::vec4 &color = Colors::Red, float scale = 1.0f, std::optional<glm::vec3> origin = std::nullopt);

	void renderBillboard(const std::string &text, const BillboardParams &bb, const glm::vec4 &color = Colors::Red, float scale = 1.0);

  private:
	struct RawGlyph {
		uint32_t cp{};
		int w{}, h{}, pitch{};
		glm::ivec2 bearing{};
		uint32_t advance{};
		std::vector<uint8_t> pixels;
	};

	TextParams textParams;

	float ascenderPx_ = 0.f;
	float descenderPx_ = 0.f;

	FT_Library ft{};
	FT_Face face{};
	std::vector<uint8_t> fontBlob;

	VkImage atlasImage{};
	VkDeviceMemory atlasMemory{};
	VkImageView atlasView{};
	VkSampler atlasSampler{};
	VkFormat atlasFormat{VK_FORMAT_R8_UNORM};
	uint32_t atlasW = 0, atlasH = 0;

	VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	VkPushConstantRange pc{};

	std::array<VkBuffer, Engine::MAX_FRAMES_IN_FLIGHT> frameVB{};
	std::array<VkDeviceMemory, Engine::MAX_FRAMES_IN_FLIGHT> frameVBMem{};
	std::array<VkDeviceSize, Engine::MAX_FRAMES_IN_FLIGHT> frameVBSize{};

	std::array<VkBuffer, Engine::MAX_FRAMES_IN_FLIGHT> frameIB{};
	std::array<VkDeviceMemory, Engine::MAX_FRAMES_IN_FLIGHT> frameIBMem{};
	std::array<VkDeviceSize, Engine::MAX_FRAMES_IN_FLIGHT> frameIBSize{};

	std::unordered_map<uint32_t, GlyphMeta> glyphs;

	static std::vector<uint32_t> defaultASCII();
	static std::u32string utf8ToUtf32(const std::string &s);
	float kerning(uint32_t prev, uint32_t curr) const;

	void bake();
	void uploadAtlas(const std::vector<RawGlyph> &raws, const std::vector<glm::ivec2> &positions, uint32_t W, uint32_t H);
	void createSampler();

	void emitCaretQuad(float caretX, const glm::vec3 &origin, float scale, float caretWidthPx, std::vector<GlyphVertex> &outVerts, std::vector<uint32_t> &outIdx);
	void emitSelectionQuad(float x0, float x1, const glm::vec3 &origin, float scale, std::vector<GlyphVertex> &outVerts, std::vector<uint32_t> &outIdx);
	void buildGeometryTaggedUTF8(const std::string &s, const glm::vec3 &origin, float scale, const std::vector<std::pair<size_t, size_t>> &selRanges, std::optional<size_t> caretByte, float caretWidthPx, std::vector<GlyphVertex> &outVerts, std::vector<uint32_t> &outIdx);

	void createDescriptorSetLayout() override;
	void createDescriptorPool() override;
	void createDescriptorSets() override;
	void createBindingDescriptions() override;
	void setupGraphicsPipeline() override;

	void renderTextEx(const std::string &text, const std::optional<glm::vec3> &origin, float scale, const glm::vec4 &textColor, const std::vector<std::pair<size_t, size_t>> &selRanges, const glm::vec4 &selColor, const std::optional<Caret> &caretOpt, const BillboardParams* bbOpt = nullptr);
};
