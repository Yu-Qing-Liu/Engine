#pragma once

#include "colors.hpp"
#include "fonts.hpp"
#include "model.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <vulkan/vulkan_core.h>
#include "textraytracing.hpp"

class Text : public Model {
  public:
	Text(Text &&) = delete;
	Text(const Text &) = delete;
	Text &operator=(Text &&) = delete;
	Text &operator=(const Text &) = delete;
	~Text() override;

	struct FontParams {
		std::string fontPath = Fonts::Arial;
		uint32_t pixelHeight = 24;
		std::vector<uint32_t> codepoints;
		uint32_t maxAtlasWidth = 2048;
		uint32_t padding = 1;
	};

	struct BillboardParams {
		glm::vec3 centerWorld;	  // location
		glm::vec2 offsetPx{0, 0}; // pixel offset from the center
		bool on = false;
	};

	struct GlyphVertex {
		glm::vec3 pos;	// world space
		glm::vec2 uv;	// atlas
		float xNorm;	// 0..1 across the quad
		uint32_t flags; // bit0 sel, bit3 bg/caret (shader-side)
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
		glm::ivec2 size;	// bitmap size
		glm::ivec2 bearing; // left/top bearing
		uint32_t advance;	// in pixels
		glm::vec2 uvMin;	// [0..1]
		glm::vec2 uvMax;	// [0..1]
	};

	struct Caret {
		size_t byte = 0;
		float px = 1.0f;
		glm::vec4 color{Colors::White(0.8)};
		bool on = false;
	};

	struct TextParams {
		std::string text;
		glm::vec3 origin = glm::vec3(0.0f);
		float scale = 1.0f;
		glm::vec4 color = Colors::Red;
		std::vector<std::pair<size_t, size_t>> selectionRanges; // [start,end)
		glm::vec4 selectionColor = Colors::Yellow(0.5f);
		Caret caret{};
		BillboardParams billboardParams{};
        float lineAdvancePx = 0.0f;
	};

	Text(Scene *scene, const MVP &ubo, ScreenParams &screenParams, const FontParams &params, const VkRenderPass &renderPass = Engine::renderPass);

	static void Text_ShutdownUploadRings();

	float getPixelWidth(const std::string &text, float scale = 1.0f) const;
	float getPixelHeight() const;

	TextParams textParams{};

	void render() override;

  private:
	// --------- Font/atlas state ----------
	FontParams fontParams;
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

	std::unordered_map<uint32_t, GlyphMeta> glyphs;

	// --- Geometry cache (rebuild only when inputs change) ---
	struct GeoCache {
		std::string text;
		glm::vec3 origin{};
		float scale = 1.f;
		std::vector<std::pair<size_t, size_t>> sel;
		std::optional<size_t> caret;
		float caretPx = 0.f;

		std::vector<GlyphVertex> verts;
		std::vector<uint32_t> idx;
		bool dirty = true;

		void reserve(size_t v, size_t i) {
			if (verts.capacity() < v)
				verts.reserve(v);
			if (idx.capacity() < i)
				idx.reserve(i);
		}
	} cache;

	// kerning cache
	mutable std::unordered_map<uint64_t, float> kerningCache;

	// --------- raytracing ---------
	std::vector<TextRayTracing::GlyphSpanGPU> spansCPU{};
	uint32_t spanCount = 0;
	static constexpr uint32_t kMaxSpans = 8192; // adjust as needed

	// Builds spans from current text params (UTF-8 byte accurate)
	void rebuildPickingSpans(const std::string &s, const glm::vec3 &origin, float scale);

	// --------- helpers ----------
	static std::vector<uint32_t> defaultASCII();
	static std::u32string utf8ToUtf32(const std::string &s);

	static inline uint64_t pairKey(uint32_t a, uint32_t b) { return (uint64_t(a) << 32) | uint64_t(b); }

	float kerning(uint32_t prev, uint32_t curr) const;

	void bake();
	void uploadAtlas(const std::vector<struct RawGlyph> &raws, const std::vector<glm::ivec2> &positions, uint32_t W, uint32_t H);
	void createSampler();

	// Direct emission (no extra reindex pass)
	void emitCaretQuad(float caretX, const glm::vec3 &origin, float scale, float caretWidthPx, std::vector<GlyphVertex> &outVerts, std::vector<uint32_t> &outIdx);
	void emitSelectionQuad(float x0, float x1, const glm::vec3 &origin, float scale, std::vector<GlyphVertex> &outVerts, std::vector<uint32_t> &outIdx);

	void buildGeometryTaggedUTF8(const std::string &s, const glm::vec3 &origin, float scale, const std::vector<std::pair<size_t, size_t>> &selRanges, std::optional<size_t> caretByte, float caretWidthPx, std::vector<GlyphVertex> &outVerts, std::vector<uint32_t> &outIdx);

	void createDescriptorSetLayout() override;
	void createDescriptorPool() override;
	void createDescriptorSets() override;
	void createBindingDescriptions() override;
	void setupGraphicsPipeline() override;
};
