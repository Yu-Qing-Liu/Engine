#include "image.hpp"
#include <stb_image.hpp>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

Image::Image(Scene *scene, const MVP &ubo, ScreenParams &screenParams, const vector<Vertex> &vertices, const vector<uint32_t> &indices, const VkRenderPass &renderPass) : vertices(vertices), Model(scene, ubo, screenParams, Assets::shaderRootPath + "/unique/image", renderPass) {
	this->indices = indices;

	createDescriptorSetLayout();

	createTextureImageFromFile(Assets::textureRootPath + "/icons/image.png");

	computeAspectUV();

	createUniformBuffers();
	createDescriptorPool();
	createDescriptorSets();
	createVertexBuffer<Vertex>(vertices);
	createIndexBuffer();
	createBindingDescriptions();
	createGraphicsPipeline();
}

Image::~Image() {
	for (size_t i = 0; i < paramsBuffers.size(); ++i) {
		if (paramsBuffersMemory[i]) {
			if (paramsBuffersMapped[i]) {
				vkUnmapMemory(Engine::device, paramsBuffersMemory[i]);
			}
			vkFreeMemory(Engine::device, paramsBuffersMemory[i], nullptr);
		}
		if (paramsBuffers[i]) {
			vkDestroyBuffer(Engine::device, paramsBuffers[i], nullptr);
		}
	}

	for (size_t i = 0; i < texImages.size(); ++i) {
		if (texImages[i] != VK_NULL_HANDLE) {
			vkDestroyImage(Engine::device, texImages[i], nullptr);
		}
		if (texImageMemory[i] != VK_NULL_HANDLE) {
			vkFreeMemory(Engine::device, texImageMemory[i], nullptr);
		}
		if (texSamplers[i] != VK_NULL_HANDLE) {
			vkDestroySampler(Engine::device, texSamplers[i], nullptr);
		}
		if (texImageViews[i] != VK_NULL_HANDLE) {
			vkDestroyImageView(Engine::device, texImageViews[i], nullptr);
		}
	}
}

void Image::buildBVH() { rayTracing->buildBVH<Vertex>(vertices, indices); }

void Image::copyParams() {
	for (void *dst : paramsBuffersMapped) {
		std::memcpy(dst, &params, sizeof(Params));
	}
}

void Image::createUniformBuffers() {
	Model::createUniformBuffers();
	Model::createUniformBuffers<Params>(paramsBuffers, paramsBuffersMemory, paramsBuffersMapped);
}

void Image::createDescriptorSetLayout() {
	mvpLayoutBinding.binding = 0;
	mvpLayoutBinding.descriptorCount = 1;
	mvpLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	mvpLayoutBinding.pImmutableSamplers = nullptr;
	mvpLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	samplerLayoutBinding.binding = 1;
	samplerLayoutBinding.descriptorCount = MAX_TEXTURES;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.pImmutableSamplers = nullptr;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	paramsBinding.binding = 2;
	paramsBinding.descriptorCount = 1;
	paramsBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	paramsBinding.pImmutableSamplers = nullptr;
	paramsBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	std::array<VkDescriptorSetLayoutBinding, 3> bindings = {mvpLayoutBinding, samplerLayoutBinding, paramsBinding};

	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(Engine::device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}
}

void Image::createDescriptorPool() {
	array<VkDescriptorPoolSize, 2> poolSizes{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = static_cast<uint32_t>(Engine::MAX_FRAMES_IN_FLIGHT * 2);
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = static_cast<uint32_t>(Engine::MAX_FRAMES_IN_FLIGHT * MAX_TEXTURES);

	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = static_cast<uint32_t>(Engine::MAX_FRAMES_IN_FLIGHT);

	if (vkCreateDescriptorPool(Engine::device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor pool!");
	}
}

void Image::createDescriptorSets() {
	vector<VkDescriptorSetLayout> layouts(Engine::MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(Engine::MAX_FRAMES_IN_FLIGHT);
	allocInfo.pSetLayouts = layouts.data();
	allocInfo.pNext = nullptr;

	descriptorSets.resize(Engine::MAX_FRAMES_IN_FLIGHT);
	if (vkAllocateDescriptorSets(Engine::device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate descriptor sets!");
	}

	writeTextureArrayDescriptors();
}

void Image::writeTextureArrayDescriptors() {
	for (size_t i = 0; i < Engine::MAX_FRAMES_IN_FLIGHT; i++) {
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = mvpBuffers[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(MVP);

		// Build exactly MAX_TEXTURES image infos, padding with slot 0
		std::vector<VkDescriptorImageInfo> imageInfos;
		imageInfos.reserve(MAX_TEXTURES);

		// Ensure we have at least slot 0
		if (texImageViews.empty() || texSamplers.empty() || texImageViews[0] == VK_NULL_HANDLE || texSamplers[0] == VK_NULL_HANDLE) {
			throw std::runtime_error("writeTextureArrayDescriptors: slot 0 is not valid");
		}

		const VkImageView padView = texImageViews[0];
		const VkSampler padSampler = texSamplers[0];

		// push actual textures first
		for (size_t t = 0; t < texImageViews.size() && t < MAX_TEXTURES; ++t) {
			VkDescriptorImageInfo ii{};
			ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			ii.imageView = texImageViews[t] ? texImageViews[t] : padView;
			ii.sampler = texSamplers[t] ? texSamplers[t] : padSampler;
			imageInfos.push_back(ii);
		}
		// pad the rest with slot 0
		while (imageInfos.size() < MAX_TEXTURES) {
			VkDescriptorImageInfo ii{};
			ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			ii.imageView = padView;
			ii.sampler = padSampler;
			imageInfos.push_back(ii);
		}

		// Clamp texIndex to last valid slot
		params.texIndex = std::min(params.texIndex, int(MAX_TEXTURES - 1));
		VkDescriptorBufferInfo paramsInfo{};
		paramsInfo.buffer = paramsBuffers[i];
		paramsInfo.offset = 0;
		paramsInfo.range = sizeof(Params);

		std::array<VkWriteDescriptorSet, 3> writes{};

		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = descriptorSets[i];
		writes[0].dstBinding = 0;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[0].descriptorCount = 1;
		writes[0].pBufferInfo = &bufferInfo;

		writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].dstSet = descriptorSets[i];
		writes[1].dstBinding = 1; // sampler array
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[1].descriptorCount = MAX_TEXTURES; // <-- fixed size
		writes[1].pImageInfo = imageInfos.data(); // <-- padded to MAX_TEXTURES

		writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[2].dstSet = descriptorSets[i];
		writes[2].dstBinding = 2;
		writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[2].descriptorCount = 1;
		writes[2].pBufferInfo = &paramsInfo;

		vkUpdateDescriptorSets(Engine::device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
	}
}

void Image::setupGraphicsPipeline() {
	rasterizer.cullMode = VK_CULL_MODE_NONE;

	depthStencil.depthTestEnable = isOrtho() ? VK_FALSE : VK_TRUE;
	depthStencil.depthWriteEnable = isOrtho() ? VK_FALSE : VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; // or ZERO
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void Image::createBindingDescriptions() {
	bindingDescription = Vertex::getBindingDescription();
	auto attrs = Vertex::getAttributeDescriptions();
	attributeDescriptions = vector<VkVertexInputAttributeDescription>(attrs.begin(), attrs.end());
}

void Image::computeAspectUV() {
	if (texW <= 0 || texH <= 0)
		return;

	// Extract quad scale from model matrix columns (assumes no shear)
	const glm::vec3 col0 = glm::vec3(mvp.model[0]); // X axis
	const glm::vec3 col1 = glm::vec3(mvp.model[1]); // Y axis
	float quadW = glm::length(col0);
	float quadH = glm::length(col1);
	if (quadW <= 0.f || quadH <= 0.f)
		return;

	const float quadAspect = quadW / quadH;
	const float texAspect = float(texW) / float(texH);
	const float r = quadAspect / texAspect;

	// FIT (letter/pillar-box) + center; UV stays in [0,1]
	if (r > 1.0f) {
		// quad wider than texture -> shrink U
		params.uvScale = vec2(1.0f / r, 1.0f);
	} else {
		// quad taller/narrower -> shrink V
		params.uvScale = vec2(1.0f, r);
	}
	params.uvOffset = 0.5f * (vec2(1.0f) - params.uvScale);

	copyParams(); // memcpy to all per-frame UBOs
}

void Image::createTextureImageFromFile(const std::string &path) {
	int w = 0, h = 0, c = 0;
	stbi_uc *pixels = nullptr;
	auto bytes = Assets::loadBytes(path);
	if (!bytes.empty()) {
		pixels = stbi_load_from_memory(bytes.data(), (int)bytes.size(), &w, &h, &c, STBI_rgb_alpha);
	} else {
		pixels = stbi_load(path.c_str(), &w, &h, &c, STBI_rgb_alpha);
	}
	if (!pixels) {
		throw std::runtime_error("failed to load texture image!");
	}

	// create slot 0 containers
	texW = w;
	texH = h; // keep current active size
	texImages.resize(1, VK_NULL_HANDLE);
	texImageMemory.resize(1, VK_NULL_HANDLE);
	texImageViews.resize(1, VK_NULL_HANDLE);
	texSamplers.resize(1, VK_NULL_HANDLE);

	createOrResizeImage(0, w, h);
	uploadPixelsToImage(0, pixels, w, h);
	createTextureImageView(0);
	createTextureSampler(0);

	stbi_image_free(pixels);

	params.texIndex = 0;
	computeAspectUV();
}

void Image::createTextureImageView(uint32_t slot) {
	if (slot >= texImages.size())
		return;
	if (texImageViews[slot] != VK_NULL_HANDLE) {
		vkDestroyImageView(Engine::device, texImageViews[slot], nullptr);
	}
	texImageViews[slot] = Engine::createImageView(texImages[slot], VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
}

void Image::createTextureSampler(uint32_t slot) {
	if (slot >= texSamplers.size())
		return;
	if (texSamplers[slot] != VK_NULL_HANDLE) {
		vkDestroySampler(Engine::device, texSamplers[slot], nullptr);
	}
	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(Engine::physicalDevice, &properties);
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	if (vkCreateSampler(Engine::device, &samplerInfo, nullptr, &texSamplers[slot]) != VK_SUCCESS) {
		throw std::runtime_error("failed to create texture sampler!");
	}
}

void Image::createOrResizeImage(uint32_t slot, int width, int height) {
	if (slot >= texImages.size()) {
		texImages.resize(slot + 1, VK_NULL_HANDLE);
		texImageMemory.resize(slot + 1, VK_NULL_HANDLE);
		texImageViews.resize(slot + 1, VK_NULL_HANDLE);
		texSamplers.resize(slot + 1, VK_NULL_HANDLE);
	}

	// destroy prior image if exists
	if (texImages[slot] != VK_NULL_HANDLE) {
		vkDestroyImage(Engine::device, texImages[slot], nullptr);
		vkFreeMemory(Engine::device, texImageMemory[slot], nullptr);
		texImages[slot] = VK_NULL_HANDLE;
		texImageMemory[slot] = VK_NULL_HANDLE;
	}

	Engine::createImage(width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texImages[slot], texImageMemory[slot]);
}

void Image::uploadPixelsToImage(uint32_t slot, const void *rgba8Pixels, int width, int height) {
	const VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * static_cast<VkDeviceSize>(height) * 4;
	VkBuffer staging{};
	VkDeviceMemory stagingMem{};
	Engine::createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging, stagingMem);
	void *data = nullptr;
	vkMapMemory(Engine::device, stagingMem, 0, imageSize, 0, &data);
	std::memcpy(data, rgba8Pixels, static_cast<size_t>(imageSize));
	vkUnmapMemory(Engine::device, stagingMem);

	Engine::transitionImageLayout(texImages[slot], VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	Engine::copyBufferToImage(staging, texImages[slot], (uint32_t)width, (uint32_t)height);
	Engine::transitionImageLayout(texImages[slot], VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkDestroyBuffer(Engine::device, staging, nullptr);
	vkFreeMemory(Engine::device, stagingMem, nullptr);
}

void Image::setActiveTextureIndex(int idx) {
	if (idx < 0 || (size_t)idx >= texImages.size())
		return;
	params.texIndex = idx;
	for (size_t i = 0; i < paramsBuffersMapped.size(); ++i) {
		std::memcpy(paramsBuffersMapped[i], &params, sizeof(Params));
	}
}

void Image::updateImage(const void *rgba8Pixels, int width, int height, int channels, int slot) {
	if (channels != 4) {
		throw std::runtime_error("updateImage expects RGBA8 data (channels=4)");
	}
	uint32_t target = (slot >= 0) ? (uint32_t)slot : (uint32_t)params.texIndex;
	bool newSlot = target >= textureCount();

	if (newSlot) {
		if (target >= MAX_TEXTURES) {
			throw std::runtime_error("Exceeded MAX_TEXTURES in updateImage");
		}
		createOrResizeImage(target, width, height);
		uploadPixelsToImage(target, rgba8Pixels, width, height);
		createTextureImageView(target);
		createTextureSampler(target);
		writeTextureArrayDescriptors(); // descriptor array grew
		setActiveTextureIndex((int)target);
	} else {
		// Recreate if size changed? We cannot query size easily here; track it if needed.
		// For simplicity, recreate image every time size differs from current active (texW/texH)
		bool sizeChanged = (width != texW || height != texH) && (target == (uint32_t)params.texIndex);
		if (sizeChanged) {
			createOrResizeImage(target, width, height);
			createTextureImageView(target);
			createTextureSampler(target);
		}
		uploadPixelsToImage(target, rgba8Pixels, width, height);
	}

	// Update aspect ratio if this is the active slot
	if (target == (uint32_t)params.texIndex) {
		texW = width;
		texH = height;
		computeAspectUV();
	}
}

void Image::updateImage(const std::string &path, int slot) {
	int w = 0, h = 0, c = 0;
	stbi_uc *pixels = nullptr;
	auto bytes = Assets::loadBytes(path);
	if (!bytes.empty()) {
		pixels = stbi_load_from_memory(bytes.data(), (int)bytes.size(), &w, &h, &c, STBI_rgb_alpha);
	} else {
		pixels = stbi_load(path.c_str(), &w, &h, &c, STBI_rgb_alpha);
	}
	if (!pixels) {
		throw std::runtime_error("updateImage: failed to load image file");
	}
	updateImage(pixels, w, h, 4, slot);
	stbi_image_free(pixels);
}

void Image::render() {
	copyParams();
	Model::render();
}
