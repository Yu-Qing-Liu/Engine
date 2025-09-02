#include "text.hpp"
#include "engine.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

Text::Text(const TextParams &textParams) : textParams(textParams), Model(Engine::shaderRootPath + "/text") {
	if (FT_Init_FreeType(&ft)) {
		throw std::runtime_error("FREETYPE: Could not init library");
	}
	if (FT_New_Face(ft, textParams.fontPath.c_str(), 0, &face)) {
		throw std::runtime_error("FREETYPE: Failed to load font");
	}
	FT_Set_Pixel_Sizes(face, 0, textParams.pixelHeight);
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

void Text::buildGeometryUTF8(const std::string &utf8, const glm::vec3 &origin, float scale, std::vector<GlyphVertex> &outVerts, std::vector<uint32_t> &outIdx) const {
	outVerts.clear();
	outIdx.clear();
	std::u32string text = utf8ToUtf32(utf8);
	float x = origin.x, y = origin.y, z = origin.z;
	uint32_t prev = 0;
	for (char32_t cp : text) {
		auto it = glyphs.find(uint32_t(cp));
		if (it == glyphs.end()) {
			auto sp = glyphs.find(uint32_t(' '));
			x += (sp != glyphs.end() ? sp->second.advance : textParams.pixelHeight / 2) * scale;
			prev = 0;
			continue;
		}
		const GlyphMeta &g = it->second;
		x += kerning(prev, uint32_t(cp)) * scale;
		float w = g.size.x * scale, h = g.size.y * scale;
		float x0 = x + g.bearing.x * scale;
		float x1 = x0 + w;
		float y0 = y - g.bearing.y * scale;
		float y1 = y0 + h;
		uint32_t base = uint32_t(outVerts.size());
		outVerts.push_back({{x0, y0, z}, {g.uvMin.x, g.uvMin.y}});
		outVerts.push_back({{x1, y0, z}, {g.uvMax.x, g.uvMin.y}});
		outVerts.push_back({{x1, y1, z}, {g.uvMax.x, g.uvMax.y}});
		outVerts.push_back({{x0, y1, z}, {g.uvMin.x, g.uvMax.y}});
		outIdx.insert(outIdx.end(), {base + 0, base + 1, base + 2, base + 2, base + 3, base + 0});
		x += g.advance * scale;
		prev = uint32_t(cp);
	}
}

float Text::measureUTF8(const std::string &s, float scale) const {
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
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.pImmutableSamplers = nullptr;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	samplerLayoutBinding.binding = 1;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.pImmutableSamplers = nullptr;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	std::array<VkDescriptorSetLayoutBinding, 2> bindings{uboLayoutBinding, samplerLayoutBinding};
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
		buf.buffer = uniformBuffers[i];
		buf.offset = 0;
		buf.range = sizeof(UBO);
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

void Text::createGraphicsPipeline() {
	inputAssembly = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	vertexInputInfo = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	shaderProgram = Engine::compileShaderProgram(shaderPath);
	shaderStages = {Engine::createShaderStageInfo(shaderProgram.vertexShader, VK_SHADER_STAGE_VERTEX_BIT), Engine::createShaderStageInfo(shaderProgram.fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT)};

	VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
	vp.viewportCount = 1;
	vp.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = VK_CULL_MODE_NONE;
	rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rs.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
	ds.depthTestEnable = VK_TRUE;
	ds.depthWriteEnable = VK_FALSE;
	ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

	VkPipelineColorBlendAttachmentState cbAtt{};
	cbAtt.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	cbAtt.blendEnable = VK_TRUE;
	cbAtt.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	cbAtt.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	cbAtt.colorBlendOp = VK_BLEND_OP_ADD;
	cbAtt.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	cbAtt.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	cbAtt.alphaBlendOp = VK_BLEND_OP_ADD;
	VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
	cb.attachmentCount = 1;
	cb.pAttachments = &cbAtt;

	std::vector<VkDynamicState> dyn{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo dynInfo{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
	dynInfo.dynamicStateCount = static_cast<uint32_t>(dyn.size());
	dynInfo.pDynamicStates = dyn.data();

	VkPushConstantRange pc{};
	pc.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	pc.offset = 0;
	pc.size = sizeof(glm::vec4);
	VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	pl.setLayoutCount = 1;
	pl.pSetLayouts = &descriptorSetLayout;
	pl.pushConstantRangeCount = 1;
	pl.pPushConstantRanges = &pc;
	if (vkCreatePipelineLayout(Engine::device, &pl, nullptr, &pipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("Text: pipeline layout failed");
	}

	VkGraphicsPipelineCreateInfo gp{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
	gp.stageCount = static_cast<uint32_t>(shaderStages.size());
	gp.pStages = shaderStages.data();
	gp.pVertexInputState = &vertexInputInfo;
	gp.pInputAssemblyState = &inputAssembly;
	gp.pViewportState = &vp;
	gp.pRasterizationState = &rs;
	gp.pMultisampleState = &ms;
	gp.pDepthStencilState = &ds;
	gp.pColorBlendState = &cb;
	gp.pDynamicState = &dynInfo;
	gp.layout = pipelineLayout;
	gp.renderPass = Engine::renderPass;
	gp.subpass = 0;
	if (vkCreateGraphicsPipelines(Engine::device, VK_NULL_HANDLE, 1, &gp, nullptr, &graphicsPipeline) != VK_SUCCESS) {
		throw std::runtime_error("Text: create pipeline failed");
	}
}

// ---------- Draw one string ----------
void Text::renderText(const UBO &ubo, const ScreenParams &screenParams, const std::string &utf8, const vec3 &origin, float scale, const vec4 &color) {
	// Build CPU geometry
	std::vector<GlyphVertex> verts;
	std::vector<uint32_t> idx;
	buildGeometryUTF8(utf8, origin, scale, verts, idx);
	if (idx.empty()) {
		return;
	}

	const uint32_t fi = Engine::currentFrame; // slot for this in-flight frame
	VkDeviceSize vSize = sizeof(verts[0]) * verts.size();
	VkDeviceSize iSize = sizeof(idx[0]) * idx.size();

	// --- (Re)create VB for this frame if needed ---
	if (frameVB[fi] == VK_NULL_HANDLE || frameVBSize[fi] < vSize) {
		// safe to destroy: fence for this frame should have already been waited on by the app
		if (frameVB[fi]) {
			vkDestroyBuffer(Engine::device, frameVB[fi], nullptr), frameVB[fi] = VK_NULL_HANDLE;
		}
		if (frameVBMem[fi]) {
			vkFreeMemory(Engine::device, frameVBMem[fi], nullptr), frameVBMem[fi] = VK_NULL_HANDLE;
		}
		Engine::createBuffer(vSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, frameVB[fi], frameVBMem[fi]);
		frameVBSize[fi] = vSize;
	}

	// --- (Re)create IB for this frame if needed ---
	if (frameIB[fi] == VK_NULL_HANDLE || frameIBSize[fi] < iSize) {
		if (frameIB[fi]) {
			vkDestroyBuffer(Engine::device, frameIB[fi], nullptr), frameIB[fi] = VK_NULL_HANDLE;
		}
		if (frameIBMem[fi]) {
			vkFreeMemory(Engine::device, frameIBMem[fi], nullptr), frameIBMem[fi] = VK_NULL_HANDLE;
		}
		Engine::createBuffer(iSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, frameIB[fi], frameIBMem[fi]);
		frameIBSize[fi] = iSize;
	}

	// --- Upload via staging each frame (safe: Engine::endSingleTimeCommands waits queue idle) ---
	{
		VkBuffer staging;
		VkDeviceMemory stagingMem;
		Engine::createBuffer(vSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging, stagingMem);

		void *mapped = nullptr;
		vkMapMemory(Engine::device, stagingMem, 0, vSize, 0, &mapped);
		std::memcpy(mapped, verts.data(), size_t(vSize));
		vkUnmapMemory(Engine::device, stagingMem);

		Engine::copyBuffer(staging, frameVB[fi], vSize);
		vkDestroyBuffer(Engine::device, staging, nullptr);
		vkFreeMemory(Engine::device, stagingMem, nullptr);
	}
	{
		VkBuffer staging;
		VkDeviceMemory stagingMem;
		Engine::createBuffer(iSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging, stagingMem);

		void *mapped = nullptr;
		vkMapMemory(Engine::device, stagingMem, 0, iSize, 0, &mapped);
		std::memcpy(mapped, idx.data(), size_t(iSize));
		vkUnmapMemory(Engine::device, stagingMem);

		Engine::copyBuffer(staging, frameIB[fi], iSize);
		vkDestroyBuffer(Engine::device, staging, nullptr);
		vkFreeMemory(Engine::device, stagingMem, nullptr);
	}

	// Update UBO & draw
	if (!this->ubo.has_value()) {
		this->ubo = ubo;
		this->ubo.value().proj[1][1] *= -1;
	}

	vkCmdBindPipeline(Engine::currentCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

	vkCmdSetViewport(Engine::currentCommandBuffer(), 0, 1, &screenParams.viewport);

	VkRect2D sc{};
	sc.offset = {0, 0};
	sc.extent = Engine::swapChainExtent;
	vkCmdSetScissor(Engine::currentCommandBuffer(), 0, 1, &screenParams.scissor);

	VkBuffer vbs[]{frameVB[fi]};
	VkDeviceSize offs[]{0};
	vkCmdBindVertexBuffers(Engine::currentCommandBuffer(), 0, 1, vbs, offs);
	vkCmdBindIndexBuffer(Engine::currentCommandBuffer(), frameIB[fi], 0, VK_INDEX_TYPE_UINT32);

	vkCmdPushConstants(Engine::currentCommandBuffer(), pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec4), &color);

	vkCmdBindDescriptorSets(Engine::currentCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[Engine::currentFrame], 0, nullptr);

	vkCmdDrawIndexed(Engine::currentCommandBuffer(), static_cast<uint32_t>(idx.size()), 1, 0, 0, 0);
}

void Text::renderText(const UBO &ubo, const ScreenParams &screenParams, const std::string &utf8, float scale, const vec4 &color) { renderText(ubo, screenParams, utf8, {-measureUTF8(utf8) / 2.0f, -getPixelHeight() * scale / 2.0f, 0.0f}, scale, color); }
