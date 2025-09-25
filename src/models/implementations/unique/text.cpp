#include "text.hpp"
#include "engine.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

Text::Text(Scene *scene, const MVP &ubo, ScreenParams &screenParams, const TextParams &textParams, const VkRenderPass &renderPass) : textParams(textParams), Model(scene, ubo, screenParams, Assets::shaderRootPath + "/unique/text", renderPass) {
	if (FT_Init_FreeType(&ft)) {
		throw std::runtime_error("FREETYPE: Could not init library");
	}
	fontBlob = Assets::loadBytes(textParams.fontPath);
	if (fontBlob.empty()) {
		throw std::runtime_error("FREETYPE: Font bytes not found");
	}
	const FT_Error err = FT_New_Memory_Face(ft, reinterpret_cast<const FT_Byte *>(fontBlob.data()), static_cast<FT_Long>(fontBlob.size()), 0, &face);
	if (err) {
		throw std::runtime_error("FREETYPE: Failed to load font");
	}
	if (FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(textParams.pixelHeight)) != 0) {
		throw std::runtime_error("FREETYPE: FT_Set_Pixel_Sizes failed");
	}
	bake();
	createDescriptorSetLayout();
	createUniformBuffers();
	createDescriptorPool();
	createDescriptorSets();
	createBindingDescriptions();
	createGraphicsPipeline();
}

Text::~Text() {
	if (atlasView) {
		vkDestroyImageView(Engine::device, atlasView, nullptr);
	}
	if (atlasImage) {
		vkDestroyImage(Engine::device, atlasImage, nullptr);
	}
	if (atlasMemory) {
		vkFreeMemory(Engine::device, atlasMemory, nullptr);
	}
	if (atlasSampler) {
		vkDestroySampler(Engine::device, atlasSampler, nullptr);
	}

	for (uint32_t i = 0; i < Engine::MAX_FRAMES_IN_FLIGHT; ++i) {
		if (frameVB[i]) {
			vkDestroyBuffer(Engine::device, frameVB[i], nullptr);
		}
		if (frameVBMem[i]) {
			vkFreeMemory(Engine::device, frameVBMem[i], nullptr);
		}
		if (frameIB[i]) {
			vkDestroyBuffer(Engine::device, frameIB[i], nullptr);
		}
		if (frameIBMem[i]) {
			vkFreeMemory(Engine::device, frameIBMem[i], nullptr);
		}
	}

	if (face) {
		FT_Done_Face(face);
	}
	if (ft) {
		FT_Done_FreeType(ft);
	}
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
	std::vector<uint32_t> cps = textParams.codepoints.empty() ? defaultASCII() : textParams.codepoints;

	std::vector<RawGlyph> raws;
	raws.reserve(cps.size());

	// FT_Matrix flipY;
	// flipY.xx = 0x10000;  //  1.0
	// flipY.xy = 0;
	// flipY.yx = 0;
	// flipY.yy = -0x10000; // -1.0

	// FT_Vector shift;
	// shift.x = 0;
	// shift.y = face->size->metrics.ascender;

	// FT_Set_Transform(face, &flipY, &shift);

	ascenderPx_ = float(face->size->metrics.ascender) / 64.f;
	descenderPx_ = float(-face->size->metrics.descender) / 64.f; // make positive

	for (uint32_t cp : cps) {
		if (FT_Load_Char(face, cp, FT_LOAD_RENDER)) {
			continue; // skip if missing
		}
		FT_GlyphSlot g = face->glyph;
		if (g->bitmap.pixel_mode != FT_PIXEL_MODE_GRAY) {
			if (FT_Load_Char(face, cp, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL)) {
				continue;
			}
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
	if (raws.empty()) {
		throw std::runtime_error("Text: no glyphs baked");
	}

	// Shelf pack (height-sorted)
	const uint32_t pad = std::max<uint32_t>(1, textParams.padding);
	std::vector<size_t> order(raws.size());
	for (size_t i = 0; i < order.size(); ++i)
		order[i] = i;
	std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return raws[a].h > raws[b].h; });

	std::vector<glm::ivec2> pos(raws.size());
	uint32_t shelfX = pad, shelfY = pad, shelfH = 0, curW = std::max(256u, textParams.maxAtlasWidth);
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

	// Fill glyph table w/ UVs
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
	// CPU assembly of R8 atlas
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

	// Staging buffer
	VkBuffer staging{};
	VkDeviceMemory stagingMem{};
	VkDeviceSize size = VkDeviceSize(W) * H;
	Engine::createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging, stagingMem);
	void *mapped = nullptr;
	vkMapMemory(Engine::device, stagingMem, 0, size, 0, &mapped);
	std::memcpy(mapped, pixels.data(), size_t(size));
	vkUnmapMemory(Engine::device, stagingMem);

	// GPU image
	Engine::createImage(W, H, atlasFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, atlasImage, atlasMemory);
	Engine::transitionImageLayout(atlasImage, atlasFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// Copy buffer → image using Engine helper
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
		uint8_t c = uint8_t(s[i]);
		if (c < 0x80) {
			out.push_back(c);
			++i;
		} else if ((c >> 5) == 0x6 && i + 1 < n) {
			out.push_back(((c & 0x1F) << 6) | (uint8_t(s[i + 1]) & 0x3F));
			i += 2;
		} else if ((c >> 4) == 0xE && i + 2 < n) {
			out.push_back(((c & 0x0F) << 12) | ((uint8_t(s[i + 1]) & 0x3F) << 6) | (uint8_t(s[i + 2]) & 0x3F));
			i += 3;
		} else if ((c >> 3) == 0x1E && i + 3 < n) {
			out.push_back(((c & 0x07) << 18) | ((uint8_t(s[i + 1]) & 0x3F) << 12) | ((uint8_t(s[i + 2]) & 0x3F) << 6) | (uint8_t(s[i + 3]) & 0x3F));
			i += 4;
		} else {
			++i;
		}
	}
	return out;
}

float Text::kerning(uint32_t prev, uint32_t curr) const {
	if (!face || !FT_HAS_KERNING(face) || !prev || !curr)
		return 0.0f;
	FT_Vector d{};
	FT_UInt l = FT_Get_Char_Index(face, prev), r = FT_Get_Char_Index(face, curr);
	if (FT_Get_Kerning(face, l, r, FT_KERNING_DEFAULT, &d) == 0)
		return float(d.x) / 64.0f;
	return 0.0f;
}

// Helper: decode cp and byte-length at s[i]
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
		return ((c & 0x0Fu) << 12) | ((p[i + 1] & 0x3Fu) << 6) | (p[i + 2] & 0x3Fu);
	}
	if ((c >> 3) == 0x1Eu && i + 3 < n) {
		adv = 4;
		return ((c & 0x07u) << 18) | ((p[i + 1] & 0x3Fu) << 12) | ((p[i + 2] & 0x3Fu) << 6) | (p[i + 3] & 0x3Fu);
	}
	adv = 1;
	return 0xFFFD;
}

// test if [a,b) overlaps any selection interval
static inline bool overlapsAny(size_t a, size_t b, const std::vector<std::pair<size_t, size_t>> &ranges) {
	for (auto &r : ranges)
		if (std::max(a, r.first) < std::min(b, r.second))
			return true;
	return false;
}

void Text::emitCaretQuad(float caretX, const glm::vec3 &origin, float scale, float caretWidthPx, std::vector<GlyphVertex> &outVerts, std::vector<uint32_t> &outIdx) {
	const float y0 = origin.y - ascenderPx_ * scale;
	const float y1 = origin.y + descenderPx_ * scale;
	const float x0 = caretX;
	const float w = std::max(caretWidthPx, 1.0f);
	const float x1 = x0 + w;

	const uint32_t f = 8u; // bit3 = caret quad
	const uint32_t base = (uint32_t)outVerts.size();

	// UVs unused for caret; vXNorm 0..1 across the caret; vQuadW carries width
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

	// UVs unused; vXNorm 0..1; vQuadW carries width (not required for selection, but kept consistent)
	outVerts.push_back({{x0, y0, origin.z}, {0, 0}, 0.0f, f, w});
	outVerts.push_back({{x0, y1, origin.z}, {0, 0}, 0.0f, f, w});
	outVerts.push_back({{x1, y1, origin.z}, {0, 0}, 1.0f, f, w});
	outVerts.push_back({{x1, y0, origin.z}, {0, 0}, 1.0f, f, w});
	outIdx.insert(outIdx.end(), {base + 0, base + 1, base + 2, base + 2, base + 3, base + 0});
}

void Text::buildGeometryTaggedUTF8(const std::string &s, const glm::vec3 &origin, float scale, const std::vector<std::pair<size_t, size_t>> &selRanges, std::optional<size_t> caretByte, float caretWidthPx, std::vector<GlyphVertex> &outVerts, std::vector<uint32_t> &outIdx) {
	outVerts.clear();
	outIdx.clear();

	// We’ll build into three buckets to control z-order (index order):
	std::vector<GlyphVertex> selVerts, glyphVerts, caretVerts;
	std::vector<uint32_t> selIdx, glyphIdx, caretIdx;

	float x = origin.x, y = origin.y, z = origin.z;
	uint32_t prev = 0;
	float caretX = std::numeric_limits<float>::quiet_NaN();

	// Track a contiguous selection run (in byte indices)
	bool inSelRun = false;
	float selRunX0 = 0.0f;

	auto endSelRunIfAny = [&](float runX1) {
		if (inSelRun) {
			emitSelectionQuad(selRunX0, runX1, origin, scale, selVerts, selIdx);
			inSelRun = false;
		}
	};

	for (size_t i = 0; i < s.size();) {
		size_t adv = 0;
		const uint32_t cp = utf8_decode_at(s, i, adv);
		if (adv == 0)
			break;

		const size_t giStart = i;
		const size_t giEnd = i + adv;

		// caret between glyphs
		if (caretByte && *caretByte == giStart)
			caretX = x;

		auto it = glyphs.find(cp);
		if (it == glyphs.end()) {
			// advance by space width if missing
			auto sp = glyphs.find(uint32_t(' '));
			x += (sp != glyphs.end() ? sp->second.advance : textParams.pixelHeight * 0.5f) * scale;

			// selection run may end at this break if the range ends before/at giEnd
			const bool selectedNow = overlapsAny(giStart, giEnd, selRanges);
			if (!selectedNow)
				endSelRunIfAny(x);

			if (caretByte && *caretByte == giEnd)
				caretX = x;
			i = giEnd;
			prev = 0;
			continue;
		}

		const GlyphMeta &g = it->second;
		x += kerning(prev, cp) * scale;

		// Determine if this glyph is within any selection range (by bytes)
		const bool selectedNow = overlapsAny(giStart, giEnd, selRanges);

		// Start a selection run at the *pre-glyph* x
		if (selectedNow && !inSelRun) {
			inSelRun = true;
			selRunX0 = x; // left edge before drawing this glyph’s bitmap/advance
		}
		// If selection ends before this glyph, close the previous run.
		if (!selectedNow && inSelRun) {
			endSelRunIfAny(x); // close at current x (before placing this glyph)
		}

		// Emit the glyph quad into the glyph bucket
		const float w = g.size.x * scale;
		const float h = g.size.y * scale;
		const float x0 = x + g.bearing.x * scale;
		const float x1 = x0 + w;
		const float y0 = y - g.bearing.y * scale;
		const float y1 = y0 + h;

		const uint32_t flags = selectedNow ? 1u : 0u; // bit0 for glyphs is harmless now

		const uint32_t base = (uint32_t)glyphVerts.size();
		glyphVerts.push_back({{x0, y0, z}, {g.uvMin.x, g.uvMin.y}, 0.0f, flags, w});
		glyphVerts.push_back({{x0, y1, z}, {g.uvMin.x, g.uvMax.y}, 0.0f, flags, w});
		glyphVerts.push_back({{x1, y1, z}, {g.uvMax.x, g.uvMax.y}, 1.0f, flags, w});
		glyphVerts.push_back({{x1, y0, z}, {g.uvMax.x, g.uvMin.y}, 1.0f, flags, w});
		glyphIdx.insert(glyphIdx.end(), {base + 0, base + 1, base + 2, base + 2, base + 3, base + 0});

		x += g.advance * scale;
		prev = cp;
		i = giEnd;

		if (caretByte && *caretByte == giEnd)
			caretX = x;
	}

	// Close a trailing selection run at end of line
	endSelRunIfAny(x);

	// Caret quad (draw on top): same as before, bit3 set, bit0 clear
	if (caretByte) {
		float cx = std::isnan(caretX) ? origin.x : caretX;
		emitCaretQuad(cx, origin, scale, caretWidthPx, caretVerts, caretIdx);
	}

	// Compose final order: selection bg (under), then glyphs, then caret (over)
	auto append = [&](auto &V, auto &I) {
		const uint32_t base = (uint32_t)outVerts.size();
		outVerts.insert(outVerts.end(), V.begin(), V.end());
		for (uint32_t id : I)
			outIdx.push_back(base + id);
	};

	// We need to rebase indices, so rebuild indices relative to new bases:
	// Rebuild index arrays with 0..N-1 for each bucket first:
	auto reindex = [](const std::vector<GlyphVertex> &V) -> std::vector<uint32_t> {
		std::vector<uint32_t> I;
		I.reserve((V.size() / 4) * 6);
		for (uint32_t q = 0; q < V.size(); q += 4) {
			I.insert(I.end(), {q + 0, q + 1, q + 2, q + 2, q + 3, q + 0});
		}
		return I;
	};

	// (If you prefer, you can keep the original per-bucket indices; this keeps code compact.)
	selIdx = reindex(selVerts);
	glyphIdx = reindex(glyphVerts);
	caretIdx = reindex(caretVerts);

	append(selVerts, selIdx);
	append(glyphVerts, glyphIdx);
	append(caretVerts, caretIdx);
}

float Text::getPixelWidth(const std::string &s, float scale) const {
	std::u32string t = utf8ToUtf32(s);
	float x = 0;
	uint32_t prev = 0;
	for (char32_t cp : t) {
		auto it = glyphs.find(uint32_t(cp));
		if (it == glyphs.end())
			continue;
		x += kerning(prev, uint32_t(cp));
		x += it->second.advance;
		prev = uint32_t(cp);
	}
	return x * scale;
}

float Text::getPixelHeight() { return float(textParams.pixelHeight); }

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
	attributeDescriptions = std::vector<VkVertexInputAttributeDescription>(attrs.begin(), attrs.end());
}

void Text::setupGraphicsPipeline() {
	rasterizer.cullMode = VK_CULL_MODE_NONE;

	depthStencil.depthWriteEnable = VK_FALSE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

	pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pc.offset = 0;
	pc.size = sizeof(glm::vec4) * 7;

	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pc;
}

static std::vector<std::pair<size_t, size_t>> findAllMatches(const std::string &hay, const std::string &needle, bool caseSensitive) {
	std::vector<std::pair<size_t, size_t>> out;
	if (needle.empty())
		return out;
	if (caseSensitive) {
		size_t pos = 0;
		while ((pos = hay.find(needle, pos)) != std::string::npos) {
			out.emplace_back(pos, pos + needle.size());
			pos += needle.size();
		}
	} else {
		// simple ASCII fold; for full Unicode casing, integrate ICU later
		std::string H = hay, N = needle;
		std::transform(H.begin(), H.end(), H.begin(), ::tolower);
		std::transform(N.begin(), N.end(), N.begin(), ::tolower);
		size_t pos = 0;
		while ((pos = H.find(N, pos)) != std::string::npos) {
			out.emplace_back(pos, pos + N.size());
			pos += N.size();
		}
	}
	return out;
}

void Text::renderTextEx(const std::string &text, const std::optional<glm::vec3> &origin, float scale, const glm::vec4 &textColor, const std::vector<std::pair<size_t, size_t>> &selRanges, const glm::vec4 &selColor, const std::optional<Caret> &caretOpt, const BillboardParams *bbOpt) {
	copyUBO();

	const bool billboard = (bbOpt != nullptr);

	// Build tagged geometry
	std::vector<GlyphVertex> verts;
	std::vector<uint32_t> idx;
	std::optional<size_t> caretByte = caretOpt ? std::optional<size_t>(caretOpt->byte) : std::nullopt;
	float caretW = caretOpt ? caretOpt->px : 0.0f;

	if (!billboard) {
		if (origin.has_value()) {
			buildGeometryTaggedUTF8(text, origin.value(), scale, selRanges, caretByte, caretW, verts, idx);
		} else {
			buildGeometryTaggedUTF8(text, {-getPixelWidth(text) / 2.0f, getPixelHeight() / 3.3 * scale, 0.0f}, scale, selRanges, caretByte, caretW, verts, idx);
		}
	} else {
		buildGeometryTaggedUTF8(text, {/*origin*/ 0, 0, 0}, scale, selRanges, caretOpt ? std::optional<size_t>(caretOpt->byte) : std::nullopt, caretOpt ? caretOpt->px : 0.0f, verts, idx);
	}

	if (idx.empty())
		return;

	// (re)alloc + upload (same as before) ...
	const uint32_t fi = Engine::currentFrame;
	VkDeviceSize vSize = sizeof(verts[0]) * verts.size();
	VkDeviceSize iSize = sizeof(idx[0]) * idx.size();

	// --- (Re)create VB for this frame if needed ---
	if (frameVB[fi] == VK_NULL_HANDLE || frameVBSize[fi] < vSize) {
		if (frameVB[fi]) {
			vkDestroyBuffer(Engine::device, frameVB[fi], nullptr);
			frameVB[fi] = VK_NULL_HANDLE;
		}
		if (frameVBMem[fi]) {
			vkFreeMemory(Engine::device, frameVBMem[fi], nullptr);
			frameVBMem[fi] = VK_NULL_HANDLE;
		}
		Engine::createBuffer(vSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, frameVB[fi], frameVBMem[fi]);
		frameVBSize[fi] = vSize;
	}

	// --- (Re)create IB for this frame if needed ---
	if (frameIB[fi] == VK_NULL_HANDLE || frameIBSize[fi] < iSize) {
		if (frameIB[fi]) {
			vkDestroyBuffer(Engine::device, frameIB[fi], nullptr);
			frameIB[fi] = VK_NULL_HANDLE;
		}
		if (frameIBMem[fi]) {
			vkFreeMemory(Engine::device, frameIBMem[fi], nullptr);
			frameIBMem[fi] = VK_NULL_HANDLE;
		}
		Engine::createBuffer(iSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, frameIB[fi], frameIBMem[fi]);
		frameIBSize[fi] = iSize;
	}

	// --- Upload verts via staging ---
	{
		VkBuffer staging;
		VkDeviceMemory stagingMem;
		Engine::createBuffer(vSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging, stagingMem);
		void *mapped = nullptr;
		vkMapMemory(Engine::device, stagingMem, 0, vSize, 0, &mapped);
		std::memcpy(mapped, verts.data(), (size_t)vSize);
		vkUnmapMemory(Engine::device, stagingMem);
		Engine::copyBuffer(staging, frameVB[fi], vSize);
		vkDestroyBuffer(Engine::device, staging, nullptr);
		vkFreeMemory(Engine::device, stagingMem, nullptr);
	}

	// --- Upload indices via staging ---
	{
		VkBuffer staging;
		VkDeviceMemory stagingMem;
		Engine::createBuffer(iSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging, stagingMem);
		void *mapped = nullptr;
		vkMapMemory(Engine::device, stagingMem, 0, iSize, 0, &mapped);
		std::memcpy(mapped, idx.data(), (size_t)iSize);
		vkUnmapMemory(Engine::device, stagingMem);
		Engine::copyBuffer(staging, frameIB[fi], iSize);
		vkDestroyBuffer(Engine::device, staging, nullptr);
		vkFreeMemory(Engine::device, stagingMem, nullptr);
	}

	// --- Push constants (now include billboard data and push to VS|FS) ---
	struct PC {
		glm::vec4 textColor;
		glm::vec4 selectColor;
		glm::vec4 caretColor;
		glm::vec4 misc;			// x=caretPx, y=caretOn, z=time, w=mode (0=normal, 1=billboard)
		glm::vec4 bbCenter;		// xyz = world center (billboard only)
		glm::vec4 bbOffsetPx;	// xy = pixel offset
		glm::vec4 bbScreenSize; // xy = framebuffer size in px
	} pc{};

	pc.textColor = textColor;
	pc.selectColor = selColor;
	pc.caretColor = caretOpt ? caretOpt->color : glm::vec4(0, 0, 0, 0);
	pc.misc = glm::vec4(caretOpt ? caretOpt->px : 0.0f, (caretOpt && caretOpt->on) ? 1.0f : 0.0f, Engine::time,
						billboard ? 1.0f : 0.0f); // <- mode

	if (billboard) {
		pc.bbCenter = glm::vec4(bbOpt->centerWorld, 0.0f);
		pc.bbOffsetPx = glm::vec4(bbOpt->offsetPx, 0.0f, 0.0f);
		pc.bbScreenSize = glm::vec4(float(Engine::swapChainExtent.width), float(Engine::swapChainExtent.height), 0.f, 0.f);
	} else {
		pc.bbCenter = pc.bbOffsetPx = pc.bbScreenSize = glm::vec4(0);
	}

	vkCmdBindPipeline(Engine::currentCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
	vkCmdSetViewport(Engine::currentCommandBuffer(), 0, 1, &screenParams.viewport);
	vkCmdSetScissor(Engine::currentCommandBuffer(), 0, 1, &screenParams.scissor);

	VkBuffer vbs[]{frameVB[Engine::currentFrame]};
	VkDeviceSize offs[]{0};
	vkCmdBindVertexBuffers(Engine::currentCommandBuffer(), 0, 1, vbs, offs);
	vkCmdBindIndexBuffer(Engine::currentCommandBuffer(), frameIB[Engine::currentFrame], 0, VK_INDEX_TYPE_UINT32);

	vkCmdPushConstants(Engine::currentCommandBuffer(), pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PC), &pc);
	vkCmdBindDescriptorSets(Engine::currentCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[Engine::currentFrame], 0, nullptr);
	vkCmdDrawIndexed(Engine::currentCommandBuffer(), (uint32_t)idx.size(), 1, 0, 0, 0);
}

void Text::render() {
	static const std::vector<std::pair<size_t, size_t>> none;
	renderTextEx(text, std::nullopt, 1.0f, color, none, glm::vec4(0, 0, 0, 0), std::nullopt);
}

// 1) normal
void Text::renderText(const std::string &text, const glm::vec4 &color, float scale, std::optional<glm::vec3> origin) {
	static const std::vector<std::pair<size_t, size_t>> none;
	renderTextEx(text, origin, scale, color, none, glm::vec4(0, 0, 0, 0), std::nullopt);
}

// 2) caret only
void Text::renderText(const std::string &text, const Caret &caret, const glm::vec4 &color, float scale, std::optional<glm::vec3> origin) {
	static const std::vector<std::pair<size_t, size_t>> none;
	renderTextEx(text, origin, scale, color, none, glm::vec4(0, 0, 0, 0), caret);
}

// 3) explicit range
void Text::renderText(const std::string &text, const SelectionRange &sel, std::optional<Caret> caret, const glm::vec4 &color, float scale, std::optional<glm::vec3> origin) {
	std::vector<std::pair<size_t, size_t>> ranges = {{sel.start, sel.end}};
	renderTextEx(text, origin, scale, color, ranges, sel.color, caret);
}

// 4) match all substrings
void Text::renderText(const std::string &text, const SelectionMatches &matches, std::optional<Caret> caret, const glm::vec4 &color, float scale, std::optional<glm::vec3> origin) {
	auto ranges = findAllMatches(text, matches.needle, matches.caseSensitive);
	renderTextEx(text, origin, scale, color, ranges, matches.color, caret);
}

void Text::renderBillboard(const std::string &text, const BillboardParams &bb, const glm::vec4 &color, float scale) {
	static const std::vector<std::pair<size_t, size_t>> none;
	renderTextEx(text, std::nullopt, scale, color, none, glm::vec4(0), std::nullopt, &bb);
}
