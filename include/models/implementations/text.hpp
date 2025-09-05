#pragma once

#include "model.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <vulkan/vulkan_core.h>

class Text : public Model {
  public:
	Text(Text &&) = default;
	Text(const Text &) = delete;
	Text &operator=(Text &&) = delete;
	Text &operator=(const Text &) = delete;
	~Text() override;

	struct TextParams {
		string fontPath;
		uint32_t pixelHeight = 48;
		vector<uint32_t> codepoints;
		uint32_t maxAtlasWidth = 2048;
		uint32_t padding = 1;
	};

	struct GlyphVertex {
		glm::vec3 pos; // world space (x,y,z)
		glm::vec2 uv;  // atlas uv
		static VkVertexInputBindingDescription getBindingDescription() {
			VkVertexInputBindingDescription d{};
			d.binding = 0;
			d.stride = sizeof(GlyphVertex);
			d.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			return d;
		}
		static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
			std::array<VkVertexInputAttributeDescription, 2> a{};
			a[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(GlyphVertex, pos)}; // location 0
			a[1] = {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(GlyphVertex, uv)};	   // location 1
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

	Text(Scene &scene, const TextParams &params);

	void renderText(const UBO &ubo, const ScreenParams &screenParams, const std::string &utf8, const vec3 &origin, float scale = 1.0f, const vec4 &color = glm::vec4(1, 0, 0, 1));
	void renderText(const UBO &ubo, const ScreenParams &screenParams, const std::string &utf8, float scale = 1.0f, const vec4 &color = glm::vec4(1, 0, 0, 1));
	float measureUTF8(const std::string &utf8, float scale = 1.0f) const;
	float getPixelHeight();

  private:
	struct RawGlyph {
		uint32_t cp{};
		int w{}, h{}, pitch{};
		glm::ivec2 bearing{};
		uint32_t advance{};
		std::vector<uint8_t> pixels;
	};

	TextParams textParams;

	FT_Library ft{};
	FT_Face face{};

	VkImage atlasImage{};
	VkDeviceMemory atlasMemory{};
	VkImageView atlasView{};
	VkSampler atlasSampler{};
	VkFormat atlasFormat{VK_FORMAT_R8_UNORM};
	uint32_t atlasW = 0, atlasH = 0;

	VkDescriptorSetLayoutBinding samplerLayoutBinding{};

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

	void buildGeometryUTF8(const std::string &utf8, const glm::vec3 &origin, float scale, std::vector<GlyphVertex> &outVerts, std::vector<uint32_t> &outIdx) const;

	void createDescriptorSetLayout() override;
	void createDescriptorPool() override;
	void createDescriptorSets() override;
	void createBindingDescriptions() override;
	void createGraphicsPipeline() override;
};
