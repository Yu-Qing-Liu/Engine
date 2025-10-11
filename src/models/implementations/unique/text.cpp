#include "text.hpp"
#include "engine.hpp"
#include "textraytracing.hpp"
#include <algorithm>
#include <cstring>
#include <numeric>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

// ============================
// Persistent per-frame upload rings (host-visible, mapped)
// ============================
namespace {
struct UploadRing {
	VkBuffer buf = VK_NULL_HANDLE;
	VkDeviceMemory mem = VK_NULL_HANDLE;
	uint8_t *map = nullptr;
	VkDeviceSize cap = 0;
	VkDeviceSize head = 0;
};

UploadRing g_textVB[Engine::MAX_FRAMES_IN_FLIGHT];
UploadRing g_textIB[Engine::MAX_FRAMES_IN_FLIGHT];
uint32_t g_text_lastResetFrame = ~0u;

void ensureCapacity(UploadRing &r, VkDeviceSize need, VkBufferUsageFlags usage) {
	if (r.cap >= need)
		return;
	VkDeviceSize newCap = std::max<VkDeviceSize>(need, r.cap ? r.cap * 2 : (1 << 20)); // start 1MB
	if (r.buf) {
		vkDestroyBuffer(Engine::device, r.buf, nullptr);
		vkFreeMemory(Engine::device, r.mem, nullptr);
		r.buf = VK_NULL_HANDLE;
		r.mem = VK_NULL_HANDLE;
		r.map = nullptr;
		r.cap = 0;
	}
	Engine::createBuffer(newCap, usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, r.buf, r.mem);
	if (vkMapMemory(Engine::device, r.mem, 0, newCap, 0, (void **)&r.map) != VK_SUCCESS) {
		throw std::runtime_error("failed to map memory");
	}
	r.cap = newCap;
}

void beginFrameIfNeeded() {
	const uint32_t fi = Engine::currentFrame;
	if (g_text_lastResetFrame == fi)
		return;
	// initialize buffers on first use
	ensureCapacity(g_textVB[fi], 0, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
	ensureCapacity(g_textIB[fi], 0, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
	g_textVB[fi].head = 0;
	g_textIB[fi].head = 0;
	g_text_lastResetFrame = fi;
}

void *ringAlloc(UploadRing &r, VkDeviceSize size, VkDeviceSize &offset) {
	VkDeviceSize newHead = r.head + size;
	ensureCapacity(r, newHead, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
	offset = r.head;
	r.head = newHead;
	return r.map + offset;
}

// UTF-8 decode util
static inline uint32_t utf8_decode_at(const std::string &s, size_t i, size_t &adv) {
	const unsigned char *p = (const unsigned char *)s.data();
	const size_t n = s.size();
	if (i >= n) {
		adv = 0;
		return 0;
	}
	unsigned char c = p[i];
	if (c < 0x80u) {
		adv = 1;
		return c;
	}
	if ((c >> 5) == 0x6 && i + 1 < n) {
		adv = 2;
		return ((c & 0x1Fu) << 6) | (p[i + 1] & 0x3Fu);
	}
	if ((c >> 4) == 0xEu && i + 2 < n) {
		adv = 3;
		return ((c & 0x0Fu) << 12) | ((p[i + 1] & 0x3F) << 6) | (p[i + 2] & 0x3F);
	}
	if ((c >> 3) == 0x1Eu && i + 3 < n) {
		adv = 4;
		return ((c & 0x07u) << 18) | ((p[i + 1] & 0x3F) << 12) | ((p[i + 2] & 0x3F) << 6) | (p[i + 3] & 0x3F);
	}
	adv = 1;
	return 0xFFFD;
}

// selection overlap helper
static inline bool overlapsAny(size_t a, size_t b, const std::vector<std::pair<size_t, size_t>> &ranges) {
	for (auto &r : ranges)
		if (std::max(a, r.first) < std::min(b, r.second))
			return true;
	return false;
}

} // namespace

// ============ Text implementation ============

struct RawGlyph {
	uint32_t cp{};
	int w{}, h{}, pitch{};
	glm::ivec2 bearing{};
	uint32_t advance{};
	std::vector<uint8_t> pixels;
};

Text::Text(Scene *scene, const MVP &ubo, ScreenParams &screenParams, const FontParams &fontParams, const VkRenderPass &renderPass) : fontParams(fontParams), Model(scene, ubo, screenParams, Assets::shaderRootPath + "/unique/text", renderPass) {
	rayTracing = std::make_unique<TextRayTracing>(this);
	if (FT_Init_FreeType(&ft)) {
		throw std::runtime_error("FREETYPE: Could not init library");
	}
	fontBlob = Assets::loadBytes(fontParams.fontPath);
	if (fontBlob.empty()) {
		throw std::runtime_error("FREETYPE: Font bytes not found");
	}
	const FT_Error err = FT_New_Memory_Face(ft, reinterpret_cast<const FT_Byte *>(fontBlob.data()), static_cast<FT_Long>(fontBlob.size()), 0, &face);
	if (err) {
		throw std::runtime_error("FREETYPE: Failed to load font");
	}
	if (FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(fontParams.pixelHeight)) != 0) {
		throw std::runtime_error("FREETYPE: FT_Set_Pixel_Sizes failed");
	}

	bake();
	createDescriptorSetLayout();
	createUniformBuffers();
	createDescriptorPool();
	createDescriptorSets();
	createBindingDescriptions();
	createGraphicsPipeline();

	// reasonable default cache capacity
	cache.reserve(512, 768);
}

Text::~Text() {
	if (atlasView)
		vkDestroyImageView(Engine::device, atlasView, nullptr);
	if (atlasImage)
		vkDestroyImage(Engine::device, atlasImage, nullptr);
	if (atlasMemory)
		vkFreeMemory(Engine::device, atlasMemory, nullptr);
	if (atlasSampler)
		vkDestroySampler(Engine::device, atlasSampler, nullptr);

	if (face)
		FT_Done_Face(face);
	if (ft)
		FT_Done_FreeType(ft);
}

void Text::Text_ShutdownUploadRings() {
	// Free per-frame vertex/index rings
	for (uint32_t i = 0; i < Engine::MAX_FRAMES_IN_FLIGHT; ++i) {
		auto &rv = g_textVB[i];
		auto &ri = g_textIB[i];

		if (rv.mem && rv.map) {
			vkUnmapMemory(Engine::device, rv.mem);
			rv.map = nullptr;
		}
		if (ri.mem && ri.map) {
			vkUnmapMemory(Engine::device, ri.mem);
			ri.map = nullptr;
		}
		if (rv.buf) {
			vkDestroyBuffer(Engine::device, rv.buf, nullptr);
			rv.buf = VK_NULL_HANDLE;
		}
		if (ri.buf) {
			vkDestroyBuffer(Engine::device, ri.buf, nullptr);
			ri.buf = VK_NULL_HANDLE;
		}
		if (rv.mem) {
			vkFreeMemory(Engine::device, rv.mem, nullptr);
			rv.mem = VK_NULL_HANDLE;
		}
		if (ri.mem) {
			vkFreeMemory(Engine::device, ri.mem, nullptr);
			ri.mem = VK_NULL_HANDLE;
		}

		rv.cap = rv.head = 0;
		ri.cap = ri.head = 0;
	}
	g_text_lastResetFrame = ~0u;
}

std::vector<uint32_t> Text::defaultASCII() {
	std::vector<uint32_t> v;
	v.reserve(95);
	for (uint32_t c = 32; c <= 126; ++c)
		v.push_back(c);
	return v;
}

void Text::createSampler() {
	VkSamplerCreateInfo info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
	info.magFilter = VK_FILTER_LINEAR;
	info.minFilter = VK_FILTER_LINEAR;
	info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST; // single mip
	info.addressModeU = info.addressModeV = info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	info.compareEnable = VK_FALSE;
	info.unnormalizedCoordinates = VK_FALSE;
	if (vkCreateSampler(Engine::device, &info, nullptr, &atlasSampler) != VK_SUCCESS) {
		throw std::runtime_error("Text: failed to create sampler");
	}
}

void Text::bake() {
	std::vector<uint32_t> cps = fontParams.codepoints.empty() ? defaultASCII() : fontParams.codepoints;

	std::vector<RawGlyph> raws;
	raws.reserve(cps.size());

	ascenderPx_ = float(face->size->metrics.ascender) / 64.f;
	descenderPx_ = float(-face->size->metrics.descender) / 64.f; // positive

	for (uint32_t cp : cps) {
		if (FT_Load_Char(face, cp, FT_LOAD_RENDER))
			continue;
		FT_GlyphSlot g = face->glyph;
		if (g->bitmap.pixel_mode != FT_PIXEL_MODE_GRAY) {
			if (FT_Load_Char(face, cp, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL))
				continue;
		}
		RawGlyph rg{};
		rg.cp = cp;
		rg.w = g->bitmap.width;
		rg.h = g->bitmap.rows;
		rg.pitch = g->bitmap.pitch;
		rg.bearing = {g->bitmap_left, g->bitmap_top};
		rg.advance = static_cast<uint32_t>(g->advance.x >> 6);
		if (rg.w > 0 && rg.h > 0) {
			rg.pixels.resize(size_t(rg.w) * rg.h);
			for (int r = 0; r < rg.h; ++r) {
				const uint8_t *src = (rg.pitch >= 0) ? g->bitmap.buffer + r * rg.pitch : g->bitmap.buffer + (rg.h - 1 - r) * (-rg.pitch);
				std::memcpy(&rg.pixels[size_t(r) * rg.w], src, rg.w);
			}
		}
		raws.push_back(std::move(rg));
	}
	if (raws.empty())
		throw std::runtime_error("Text: no glyphs baked");

	// Shelf pack (height-sorted)
	const uint32_t pad = std::max<uint32_t>(1, fontParams.padding);
	std::vector<size_t> order(raws.size());
	std::iota(order.begin(), order.end(), size_t(0));
	std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return raws[a].h > raws[b].h; });

	std::vector<glm::ivec2> pos(raws.size());
	uint32_t shelfX = pad, shelfY = pad, shelfH = 0, curW = std::max(256u, fontParams.maxAtlasWidth);
	for (size_t k = 0; k < order.size(); ++k) {
		auto &rg = raws[order[k]];
		uint32_t needW = uint32_t(rg.w) + pad;
		if (shelfX + needW > curW) {
			shelfY += shelfH + pad;
			shelfX = pad;
			shelfH = 0;
		}
		pos[order[k]] = {int(shelfX), int(shelfY)};
		shelfX += needW;
		shelfH = std::max<uint32_t>(shelfH, uint32_t(rg.h));
	}
	uint32_t W = curW, H = shelfY + shelfH + pad;
	atlasW = W;
	atlasH = H;

	uploadAtlas(raws, pos, W, H);

	// glyph table
	glyphs.reserve(raws.size());
	for (size_t i = 0; i < raws.size(); ++i) {
		const auto &rg = raws[i];
		glm::ivec2 p = pos[i];
		GlyphMeta m{};
		m.size = {rg.w, rg.h};
		m.bearing = rg.bearing;
		m.advance = rg.advance;
		m.uvMin = {(p.x + 0.0f) / atlasW, (p.y + 0.0f) / atlasH};
		m.uvMax = {(p.x + rg.w * 1.0f) / atlasW, (p.y + rg.h * 1.0f) / atlasH};
		glyphs.emplace(rg.cp, m);
	}
}

void Text::uploadAtlas(const std::vector<RawGlyph> &raws, const std::vector<glm::ivec2> &positions, uint32_t W, uint32_t H) {

	std::vector<uint8_t> pixels(size_t(W) * H, 0);
	for (size_t i = 0; i < raws.size(); ++i) {
		const auto &rg = raws[i];
		auto p = positions[i];
		if (rg.w == 0 || rg.h == 0)
			continue;
		for (int r = 0; r < rg.h; ++r) {
			uint8_t *dst = &pixels[(p.y + r) * W + p.x];
			const uint8_t *src = &rg.pixels[size_t(r) * rg.w];
			std::memcpy(dst, src, rg.w);
		}
	}

	// staging-like path but using your helpers; atlas is immutable → device-local
	VkBuffer staging{};
	VkDeviceMemory stagingMem{};
	VkDeviceSize size = VkDeviceSize(W) * H;

	Engine::createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging, stagingMem);
	void *mapped = nullptr;
	vkMapMemory(Engine::device, stagingMem, 0, size, 0, &mapped);
	std::memcpy(mapped, pixels.data(), size_t(size));
	vkUnmapMemory(Engine::device, stagingMem);

	Engine::createImage(W, H, atlasFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, atlasImage, atlasMemory);

	Engine::transitionImageLayout(atlasImage, atlasFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	Engine::copyBufferToImage(staging, atlasImage, W, H);

	Engine::transitionImageLayout(atlasImage, atlasFormat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	atlasView = Engine::createImageView(atlasImage, atlasFormat, VK_IMAGE_ASPECT_COLOR_BIT);
	createSampler();

	vkDestroyBuffer(Engine::device, staging, nullptr);
	vkFreeMemory(Engine::device, stagingMem, nullptr);
}

std::u32string Text::utf8ToUtf32(const std::string &s) {
	std::u32string out;
	out.reserve(s.size());
	size_t i = 0, n = s.size();
	while (i < n) {
		size_t adv = 0;
		uint32_t cp = utf8_decode_at(s, i, adv);
		if (!adv)
			break;
		out.push_back(cp);
		i += adv;
	}
	return out;
}

float Text::kerning(uint32_t prev, uint32_t curr) const {
	if (!face || !FT_HAS_KERNING(face) || !prev || !curr)
		return 0.0f;
	const uint64_t k = pairKey(prev, curr);
	if (auto it = kerningCache.find(k); it != kerningCache.end())
		return it->second;

	FT_Vector d{};
	float v = 0.f;
	FT_UInt l = FT_Get_Char_Index(face, prev), r = FT_Get_Char_Index(face, curr);
	if (FT_Get_Kerning(face, l, r, FT_KERNING_DEFAULT, &d) == 0)
		v = float(d.x) / 64.0f;

	kerningCache.emplace(k, v);
	return v;
}

void Text::emitCaretQuad(float caretX, const glm::vec3 &origin, float scale, float caretWidthPx, std::vector<GlyphVertex> &outVerts, std::vector<uint32_t> &outIdx) {
	const float y0 = origin.y - ascenderPx_ * scale;
	const float y1 = origin.y + descenderPx_ * scale;
	const float x0 = caretX;
	const float w = std::max(caretWidthPx, 1.0f);
	const float x1 = x0 + w;

	const uint32_t f = 8u; // bit3 = caret quad
	const uint32_t base = (uint32_t)outVerts.size();

	outVerts.push_back({{x0, y0, origin.z}, {0, 0}, 0.0f, f, w});
	outVerts.push_back({{x0, y1, origin.z}, {0, 0}, 0.0f, f, w});
	outVerts.push_back({{x1, y1, origin.z}, {0, 0}, 1.0f, f, w});
	outVerts.push_back({{x1, y0, origin.z}, {0, 0}, 1.0f, f, w});
	outIdx.insert(outIdx.end(), {base + 0, base + 1, base + 2, base + 2, base + 3, base + 0});
}

void Text::emitSelectionQuad(float x0, float x1, const glm::vec3 &origin, float scale, std::vector<GlyphVertex> &outVerts, std::vector<uint32_t> &outIdx) {
	const float y0 = origin.y - ascenderPx_ * scale;
	const float y1 = origin.y + descenderPx_ * scale;
	const float w = std::max(x1 - x0, 0.0f);

	const uint32_t f = 8u | 1u; // bit3 = background quad, bit0 = selection
	const uint32_t base = (uint32_t)outVerts.size();

	outVerts.push_back({{x0, y0, origin.z}, {0, 0}, 0.0f, f, w});
	outVerts.push_back({{x0, y1, origin.z}, {0, 0}, 0.0f, f, w});
	outVerts.push_back({{x1, y1, origin.z}, {0, 0}, 1.0f, f, w});
	outVerts.push_back({{x1, y0, origin.z}, {0, 0}, 1.0f, f, w});
	outIdx.insert(outIdx.end(), {base + 0, base + 1, base + 2, base + 2, base + 3, base + 0});
}

void Text::buildGeometryTaggedUTF8(const std::string &s, const glm::vec3 &origin, float scale, const std::vector<std::pair<size_t, size_t>> &selRanges, std::optional<size_t> caretByte, float caretWidthPx, std::vector<GlyphVertex> &outVerts, std::vector<uint32_t> &outIdx) {
	outVerts.clear();
	outIdx.clear();

	float x = origin.x, y = origin.y, z = origin.z;
	uint32_t prev = 0;

	// Track caret position on the *correct* line
	float caretX = std::numeric_limits<float>::quiet_NaN();
	float caretY = origin.y; // baseline of the line where the caret lives

	// Line advance in *unscaled* pixels (then multiplied by 'scale' when used)
	const float lineAdvancePx = (textParams.lineAdvancePx > 0.0f ? textParams.lineAdvancePx : getPixelHeight());

	// Selection background batching (one rect per continuous run)
	bool inSelRun = false;
	float selRunX0 = 0.0f;

	auto endSelRunIfAny = [&](float runX1) {
		if (inSelRun) {
			emitSelectionQuad(selRunX0, runX1, origin, scale, outVerts, outIdx);
			inSelRun = false;
		}
	};

	for (size_t i = 0; i < s.size();) {
		size_t adv = 0;
		const uint32_t cp = utf8_decode_at(s, i, adv);
		if (adv == 0)
			break;

		const size_t giStart = i;	  // byte index before this codepoint
		const size_t giEnd = i + adv; // byte index after this codepoint

		// --- caret BEFORE this codepoint (at giStart) ---
		if (caretByte && *caretByte == giStart) {
			caretX = x;
			caretY = y; // capture current line baseline
		}

		// --- newline handling ---
		if (cp == '\n') {
			// Close any active selection run on this line
			endSelRunIfAny(x);

			// caret AFTER newline (at giEnd) = start of next line
			if (caretByte && *caretByte == giEnd) {
				// we'll reset x/y to next-line baseline, so set after the move
			}

			// Move to next line
			x = origin.x;
			y += lineAdvancePx * scale;

			if (caretByte && *caretByte == giEnd) {
				caretX = x; // start of next line
				caretY = y; // baseline of next line
			}

			prev = 0;
			i = giEnd;
			continue;
		}

		// --- glyph lookup ---
		auto it = glyphs.find(cp);
		if (it == glyphs.end()) {
			// Missing glyph: advance like a space (fallback)
			auto sp = glyphs.find(uint32_t(' '));
			x += (sp != glyphs.end() ? sp->second.advance : fontParams.pixelHeight * 0.5f) * scale;

			// caret AFTER this (giEnd)
			if (caretByte && *caretByte == giEnd) {
				caretX = x;
				caretY = y;
			}

			i = giEnd;
			prev = 0;
			continue;
		}

		const GlyphMeta &g = it->second;

		// Kerning before placing this glyph
		x += kerning(prev, cp) * scale;

		// Selection bookkeeping for this codepoint
		const bool selectedNow = overlapsAny(giStart, giEnd, selRanges);
		if (selectedNow && !inSelRun) {
			inSelRun = true;
			selRunX0 = x;
		}
		if (!selectedNow && inSelRun) {
			endSelRunIfAny(x);
		}

		// Emit glyph quad (bitmap coords from atlas)
		const float w = g.size.x * scale;
		const float h = g.size.y * scale;
		const float x0 = x + g.bearing.x * scale;
		const float x1 = x0 + w;
		const float y0 = y - g.bearing.y * scale;
		const float y1 = y0 + h;

		const uint32_t flags = selectedNow ? 1u : 0u;
		const uint32_t base = (uint32_t)outVerts.size();

		outVerts.push_back({{x0, y0, z}, {g.uvMin.x, g.uvMin.y}, 0.0f, flags, w});
		outVerts.push_back({{x0, y1, z}, {g.uvMin.x, g.uvMax.y}, 0.0f, flags, w});
		outVerts.push_back({{x1, y1, z}, {g.uvMax.x, g.uvMax.y}, 1.0f, flags, w});
		outVerts.push_back({{x1, y0, z}, {g.uvMax.x, g.uvMin.y}, 1.0f, flags, w});
		outIdx.insert(outIdx.end(), {base + 0, base + 1, base + 2, base + 2, base + 3, base + 0});

		// Advance pen by glyph advance
		x += g.advance * scale;
		prev = cp;
		i = giEnd;

		// --- caret AFTER this codepoint (at giEnd) ---
		if (caretByte && *caretByte == giEnd) {
			caretX = x;
			caretY = y; // stays on current line
		}
	}

	// Finish any selection at end-of-line
	endSelRunIfAny(x);

	// Emit caret (if requested)
	if (caretByte) {
		// If caret never matched (e.g., at end of string), put it at the final pen position
		if (std::isnan(caretX)) {
			caretX = x;
			caretY = y;
		}
		// IMPORTANT: use caretY (line baseline), not origin.y
		glm::vec3 caretOrigin = {origin.x, caretY, origin.z};
		emitCaretQuad(caretX, caretOrigin, scale, caretWidthPx, outVerts, outIdx);
	}
}

float Text::getPixelWidth(const std::string &s, float scale) const {
	// Uses glyph advances + kerning cache
	float x = 0.f;
	uint32_t prev = 0;
	for (size_t i = 0; i < s.size();) {
		size_t adv = 0;
		const uint32_t cp = utf8_decode_at(s, i, adv);
		if (!adv)
			break;
		i += adv;
		auto it = glyphs.find(cp);
		if (it == glyphs.end())
			continue;
		x += kerning(prev, cp);
		x += it->second.advance;
		prev = cp;
	}
	return x * scale;
}

float Text::getPixelHeight() const { return float(fontParams.pixelHeight); }

void Text::createDescriptorSetLayout() {
	// binding 0: UBO (MVP); binding 1: atlas
	mvpLayoutBinding.binding = 0;
	mvpLayoutBinding.descriptorCount = 1;
	mvpLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	mvpLayoutBinding.pImmutableSamplers = nullptr;
	mvpLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	samplerLayoutBinding.binding = 1;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.pImmutableSamplers = nullptr;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	std::array<VkDescriptorSetLayoutBinding, 2> bindings{mvpLayoutBinding, samplerLayoutBinding};
	layoutInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();
	if (vkCreateDescriptorSetLayout(Engine::device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("Text: descriptor set layout failed");
	}
}

void Text::createDescriptorPool() {
	std::array<VkDescriptorPoolSize, 2> sizes{};
	sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	sizes[0].descriptorCount = Engine::MAX_FRAMES_IN_FLIGHT;
	sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	sizes[1].descriptorCount = Engine::MAX_FRAMES_IN_FLIGHT;

	poolInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
	poolInfo.poolSizeCount = static_cast<uint32_t>(sizes.size());
	poolInfo.pPoolSizes = sizes.data();
	poolInfo.maxSets = Engine::MAX_FRAMES_IN_FLIGHT;

	if (vkCreateDescriptorPool(Engine::device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("Text: descriptor pool failed");
	}
}

void Text::createDescriptorSets() {
	std::vector<VkDescriptorSetLayout> layouts(Engine::MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
	allocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = Engine::MAX_FRAMES_IN_FLIGHT;
	allocInfo.pSetLayouts = layouts.data();

	descriptorSets.resize(Engine::MAX_FRAMES_IN_FLIGHT);
	if (vkAllocateDescriptorSets(Engine::device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
		throw std::runtime_error("Text: allocate descriptor sets failed");
	}

	for (size_t i = 0; i < Engine::MAX_FRAMES_IN_FLIGHT; ++i) {
		VkDescriptorBufferInfo buf{};
		buf.buffer = mvpBuffers[i];
		buf.offset = 0;
		buf.range = sizeof(MVP);

		VkDescriptorImageInfo img{};
		img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		img.imageView = atlasView;
		img.sampler = atlasSampler;

		std::array<VkWriteDescriptorSet, 2> w{};
		w[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr};
		w[0].dstSet = descriptorSets[i];
		w[0].dstBinding = 0;
		w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		w[0].descriptorCount = 1;
		w[0].pBufferInfo = &buf;

		w[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr};
		w[1].dstSet = descriptorSets[i];
		w[1].dstBinding = 1;
		w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w[1].descriptorCount = 1;
		w[1].pImageInfo = &img;

		vkUpdateDescriptorSets(Engine::device, static_cast<uint32_t>(w.size()), w.data(), 0, nullptr);
	}
}

void Text::createBindingDescriptions() {
	bindingDescription = GlyphVertex::getBindingDescription();
	auto attrs = GlyphVertex::getAttributeDescriptions();
	attributeDescriptions.assign(attrs.begin(), attrs.end());
}

void Text::setupGraphicsPipeline() {
	rasterizer.cullMode = VK_CULL_MODE_NONE;

	depthStencil.depthWriteEnable = VK_FALSE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

	pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pc.offset = 0;
	pc.size = sizeof(glm::vec4) * 7; // matches your PC struct

	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pc;
}

void Text::rebuildPickingSpans(const std::string &s, const glm::vec3 &origin, float scale) {
	spansCPU.clear();
	spansCPU.reserve(std::max<size_t>(s.size(), 16));

	float x = origin.x, y = origin.y, z = origin.z;
	uint32_t prev = 0;
	uint32_t letterIdx = 0; // 0-based codepoint index

	const float lineAdvancePx = (textParams.lineAdvancePx > 0.0f ? textParams.lineAdvancePx : getPixelHeight());

	for (size_t i = 0; i < s.size();) {
		size_t adv = 0;
		const uint32_t cp = utf8_decode_at(s, i, adv);
		if (adv == 0)
			break;

		const size_t giStart = i;
		const size_t giEnd = i + adv;

		if (cp == '\n') {
			x = origin.x;
			y += lineAdvancePx * scale;
			prev = 0;
			i = giEnd;
			++letterIdx;
			continue;
		}

		auto it = glyphs.find(cp);
		if (it == glyphs.end()) {
			// advance by space if missing glyph (mirror buildGeometryTaggedUTF8)
			auto sp = glyphs.find(uint32_t(' '));
			x += (sp != glyphs.end() ? sp->second.advance : fontParams.pixelHeight * 0.5f) * scale;

			i = giEnd;
			prev = 0;
			++letterIdx; // still count this codepoint position
			continue;
		}

		const GlyphMeta &g = it->second;
		x += kerning(prev, cp) * scale;

		const float w = g.size.x * scale;
		const float h = g.size.y * scale;
		const float x0 = x + g.bearing.x * scale;
		const float x1 = x0 + w;
		const float y0 = y - g.bearing.y * scale;
		const float y1 = y0 + h;

		// --- IMPORTANT: std430-friendly struct: 4x vec4 + uvec4 (index + padding) ---
		TextRayTracing::GlyphSpanGPU span{};
		span.p0 = {x0, y0, z, 0.0f};
		span.p1 = {x0, y1, z, 0.0f};
		span.p2 = {x1, y1, z, 0.0f};
		span.p3 = {x1, y0, z, 0.0f};
		span.letterIndex = static_cast<uint32_t>(letterIdx);
		span._p0 = span._p1 = span._p2 = 0u; // pad to 16B

		spansCPU.push_back(span);

		x += g.advance * scale;
		prev = cp;
		i = giEnd;
		++letterIdx;
	}

	if (spansCPU.size() > kMaxSpans)
		spansCPU.resize(kMaxSpans);
}

std::optional<bool> Text::isRightHalfClick(size_t letterIdx) const {
	if (!rayTracing)
		return std::nullopt;
	const auto &rt = *rayTracing;
	if (!rt.hitPos)
		return std::nullopt; // no click/hit this frame

	// Bounds check
	if (letterIdx >= spansCPU.size())
		return std::nullopt;

	const auto &span = spansCPU[letterIdx];
	// span.p0 = x0,y0 (left), span.p3 = x1,y0 (right) as you emitted in rebuildPickingSpans
	const float x0 = span.p0.x;
	const float x1 = span.p3.x;
	const float mid = 0.5f * (x0 + x1);

	// Compare hit x against mid. Coordinates are in the same model space the shader used.
	return rt.hitPos->x >= mid;
}

void Text::render() {
	copyUBO(); // keep your existing UBO path

	const bool billboard = textParams.billboardParams.on;
	const auto caretOpt = textParams.caret;
	const auto bbOpt = textParams.billboardParams;

	// --- Geometry cache update ---
	bool needRebuild = false;
	if (cache.text != textParams.text) {
		cache.text = textParams.text;
		needRebuild = true;
	}
	if (cache.origin != textParams.origin) {
		cache.origin = textParams.origin;
		needRebuild = true;
	}
	if (cache.scale != textParams.scale) {
		cache.scale = textParams.scale;
		needRebuild = true;
	}
	if (cache.sel != textParams.selectionRanges) {
		cache.sel = textParams.selectionRanges;
		needRebuild = true;
	}
	{
		std::optional<size_t> c = caretOpt.on ? std::optional<size_t>(caretOpt.byte) : std::nullopt;
		if (cache.caret != c || cache.caretPx != (caretOpt.on ? caretOpt.px : 0.0f)) {
			cache.caret = c;
			cache.caretPx = caretOpt.on ? caretOpt.px : 0.0f;
			needRebuild = true;
		}
	}
	if (needRebuild) {
		// estimate: chars * 4 verts, 6 idx per glyph + a few for selection/caret
		size_t est = std::max<size_t>(cache.text.size(), 16);
		cache.reserve(est * 4 + 32, est * 6 + 48);
		buildGeometryTaggedUTF8(cache.text, cache.origin, cache.scale, cache.sel, cache.caret, cache.caretPx, cache.verts, cache.idx);
		cache.dirty = false;

		rebuildPickingSpans(cache.text, cache.origin, cache.scale);
		spanCount = static_cast<uint32_t>(spansCPU.size());
		if (rayTracing) {
			dynamic_cast<TextRayTracing *>(rayTracing.get())->uploadSpans(spansCPU);
		}
	}
	if (cache.idx.empty())
		return;

	beginFrameIfNeeded();

	// --- Upload into per-frame rings ---
	const uint32_t fi = Engine::currentFrame;
	VkDeviceSize vSize = sizeof(cache.verts[0]) * cache.verts.size();
	VkDeviceSize iSize = sizeof(cache.idx[0]) * cache.idx.size();

	VkDeviceSize vOff = 0, iOff = 0;
	void *vDst = ringAlloc(g_textVB[fi], vSize, vOff);
	void *iDst = ringAlloc(g_textIB[fi], iSize, iOff);
	std::memcpy(vDst, cache.verts.data(), (size_t)vSize);
	std::memcpy(iDst, cache.idx.data(), (size_t)iSize);

	// --- Push constants ---
	struct PC {
		glm::vec4 textColor;
		glm::vec4 selectColor;
		glm::vec4 caretColor;
		glm::vec4 misc;			// x=caretPx, y=caretOn, z=time, w=mode (0=normal,1=billboard)
		glm::vec4 bbCenter;		// xyz = world center
		glm::vec4 bbOffsetPx;	// xy = pixel offset
		glm::vec4 bbScreenSize; // xy = framebuffer size in px
	} pc{};

	pc.textColor = textParams.color;
	pc.selectColor = textParams.selectionColor;
	pc.caretColor = caretOpt.on ? caretOpt.color : glm::vec4(0, 0, 0, 0);
	pc.misc = glm::vec4(caretOpt.on ? caretOpt.px : 0.0f, caretOpt.on ? 1.0f : 0.0f, Engine::time, billboard ? 1.0f : 0.0f);

	if (billboard) {
		pc.bbCenter = glm::vec4(bbOpt.centerWorld, 0.0f);
		pc.bbOffsetPx = glm::vec4(bbOpt.offsetPx, 0.0f, 0.0f);
		pc.bbScreenSize = glm::vec4(float(Engine::swapChainExtent.width), float(Engine::swapChainExtent.height), 0.f, 0.f);
	} else {
		pc.bbCenter = pc.bbOffsetPx = pc.bbScreenSize = glm::vec4(0);
	}

	// --- Bind state & draw ---
	VkCommandBuffer cmd = Engine::currentCommandBuffer();
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
	vkCmdSetViewport(cmd, 0, 1, &screenParams.viewport);
	vkCmdSetScissor(cmd, 0, 1, &screenParams.scissor);

	VkBuffer vbs[]{g_textVB[fi].buf};
	VkDeviceSize offs[]{vOff};
	vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offs);
	vkCmdBindIndexBuffer(cmd, g_textIB[fi].buf, iOff, VK_INDEX_TYPE_UINT32);

	vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PC), &pc);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[Engine::currentFrame], 0, nullptr);

	vkCmdDrawIndexed(cmd, (uint32_t)cache.idx.size(), 1, 0, 0, 0);
}
