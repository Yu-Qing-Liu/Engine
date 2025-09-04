#include "texture.hpp"
#include <stdexcept>
#include <vulkan/vulkan_core.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.hpp>

Texture::Texture(const string &texturePath, const vector<Vertex> &vertices, const vector<uint16_t> &indices) : texturePath(texturePath), vertices(vertices), Model(Engine::shaderRootPath + "/texture", indices) {
	createDescriptorSetLayout();

	createTextureImageFromFile();
	createTextureImageView();
	createTextureSampler();

	createUniformBuffers();
	createDescriptorPool();
	createDescriptorSets();
	createVertexBuffer<Vertex>(vertices);
	createIndexBuffer();
	createBindingDescriptions();
	createGraphicsPipeline();

    createComputeDescriptorSetLayout();
    createShaderStorageBuffers();
    createComputeDescriptorSets();
    createComputePipeline();
}

Texture::Texture(const aiTexture &embeddedTex, const vector<Vertex> &vertices, const vector<uint16_t> &indices) : embeddedTex(embeddedTex), vertices(vertices), Model(Engine::shaderRootPath + "/texture", indices) {
	createDescriptorSetLayout();

	createTextureImageFromEmbedded();
	createTextureImageView();
	createTextureSampler();

	createUniformBuffers();
	createDescriptorPool();
	createDescriptorSets();
	createVertexBuffer<Vertex>(vertices);
	createIndexBuffer();
	createBindingDescriptions();
	createGraphicsPipeline();

    createComputeDescriptorSetLayout();
    createShaderStorageBuffers();
    createComputeDescriptorSets();
    createComputePipeline();
}

Texture::~Texture() {
    if (textureImage != VK_NULL_HANDLE) {
        vkDestroyImage(Engine::device, textureImage, nullptr);
    }
    if (textureImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(Engine::device, textureImageMemory, nullptr);
    }
    if (textureSampler != VK_NULL_HANDLE) {
        vkDestroySampler(Engine::device, textureSampler, nullptr);
    }
    if (textureImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(Engine::device, textureImageView, nullptr);
    }
}

void Texture::buildBVH() {
	// Gather positions and triangles from current mesh
	posGPU.clear();
	triGPU.clear();
	if (!vertices.empty()) {
		posGPU.reserve(vertices.size());
		for (auto &v : vertices) {
			posGPU.push_back(v.pos);
        }
	} else {
		throw std::runtime_error("BVH build: no vertices");
	}

	std::vector<BuildTri> tris;
	tris.reserve(indices.size() / 3);
	for (size_t t = 0; t < indices.size(); t += 3) {
		uint32_t i0 = indices[t + 0], i1 = indices[t + 1], i2 = indices[t + 2];
		const vec3 &A = posGPU[i0];
		const vec3 &B = posGPU[i1];
		const vec3 &C = posGPU[i2];
		BuildTri bt;
		bt.i0 = i0;
		bt.i1 = i1;
		bt.i2 = i2;
		bt.b = triAabb(A, B, C);
		bt.centroid = (A + B + C) * (1.0f / 3.0f);
		tris.push_back(bt);

		triGPU.push_back({i0, i1, i2, 0});
	}

	// Build tree into BuildNode list (temporary)
	std::vector<BuildNode> tmp;
	tmp.reserve(tris.size() * 2);
	int root = buildNode(tris, 0, (int)tris.size(), 0, tmp);

	// Rebuild GPU triangles in the final order used by leaves
	triGPU.clear();
	triGPU.reserve(tris.size());
	for (const auto &t : tris) {
		triGPU.push_back({t.i0, t.i1, t.i2, 0u});
	}

	// Flatten to GPU nodes (depth-first, implicit right=left+1 for internal nodes)
	bvhNodes.clear();
	bvhNodes.resize(tmp.size());
	// Map temp indices to linear DFS order
	std::vector<int> map(tmp.size(), -1);
	std::function<void(int, int &)> dfs = [&](int ni, int &outIdx) {
		int my = outIdx++;
		map[ni] = my;
		if (tmp[ni].triCount == 0) {
			dfs(tmp[ni].left, outIdx);
			dfs(tmp[ni].right, outIdx);
		}
	};
	int counter = 0;
	dfs(root, counter);

	// Fill nodes in DFS order
	std::function<void(int)> emit = [&](int ni) {
		int me = map[ni];
		const BuildNode &n = tmp[ni];
		BVHNodeGPU gn;
		gn.bmin = vec4(n.b.bmin, 0.0f);
		gn.bmax = vec4(n.b.bmax, 0.0f);

		if (n.triCount == 0) {
			gn.leftFirst = map[n.left];
			gn.rightOrCount = (uint32_t(map[n.right]) | 0x80000000u); // INTERNAL
			bvhNodes[me] = gn;
			emit(n.left);
			emit(n.right);
		} else {
			gn.leftFirst = n.firstTri;
			gn.rightOrCount = n.triCount; // leaf => count, no high bit
			bvhNodes[me] = gn;
		}
	};
	emit(root);
}

void Texture::createDescriptorSetLayout() {
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

	array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(Engine::device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}
}

void Texture::createDescriptorPool() {
	array<VkDescriptorPoolSize, 2> poolSizes{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = static_cast<uint32_t>(Engine::MAX_FRAMES_IN_FLIGHT);
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = static_cast<uint32_t>(Engine::MAX_FRAMES_IN_FLIGHT);

	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = static_cast<uint32_t>(Engine::MAX_FRAMES_IN_FLIGHT);

	if (vkCreateDescriptorPool(Engine::device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor pool!");
	}
}

void Texture::createDescriptorSets() {
	vector<VkDescriptorSetLayout> layouts(Engine::MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(Engine::MAX_FRAMES_IN_FLIGHT);
	allocInfo.pSetLayouts = layouts.data();

	descriptorSets.resize(Engine::MAX_FRAMES_IN_FLIGHT);

	if (vkAllocateDescriptorSets(Engine::device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate descriptor sets!");
	}

	for (size_t i = 0; i < Engine::MAX_FRAMES_IN_FLIGHT; i++) {
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = uniformBuffers[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UBO);

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = textureImageView;
		imageInfo.sampler = textureSampler;

		array<VkWriteDescriptorSet, 2> descriptorWrites{};

		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = descriptorSets[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &bufferInfo;

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = descriptorSets[i];
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(Engine::device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}
}

void Texture::createBindingDescriptions() {
	bindingDescription = Vertex::getBindingDescription();
	auto attrs = Vertex::getAttributeDescriptions();
	attributeDescriptions = vector<VkVertexInputAttributeDescription>(attrs.begin(), attrs.end());
}

void Texture::createTextureImageFromFile() {
	int texWidth, texHeight, texChannels;
	stbi_uc *pixels = stbi_load(texturePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	VkDeviceSize imageSize = texWidth * texHeight * 4;

	if (!pixels) {
		throw std::runtime_error("failed to load texture image!");
	}

	Engine::createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
	void *data;
	vkMapMemory(Engine::device, stagingBufferMemory, 0, imageSize, 0, &data);
	memcpy(data, pixels, static_cast<size_t>(imageSize));
	vkUnmapMemory(Engine::device, stagingBufferMemory);

	stbi_image_free(pixels);

	Engine::createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);

    Engine::transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    Engine::copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
    Engine::transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkDestroyBuffer(Engine::device, stagingBuffer, nullptr);
	vkFreeMemory(Engine::device, stagingBufferMemory, nullptr);
}

void Texture::createTextureImageFromEmbedded() {
	int texWidth = 0, texHeight = 0;
	std::vector<unsigned char> rgba;

	if (embeddedTex.mHeight == 0) {
		// Compressed (PNG/JPEG/etc.)
		const unsigned char *bytes = reinterpret_cast<const unsigned char *>(embeddedTex.pcData);
		int w, h, ch;
		stbi_uc *data = stbi_load_from_memory(bytes, embeddedTex.mWidth, &w, &h, &ch, STBI_rgb_alpha);
		if (!data)
			throw std::runtime_error("failed to decode embedded image!");
		rgba.assign(data, data + (w * h * 4));
		texWidth = w;
		texHeight = h;
		stbi_image_free(data);
	} else {
		// Uncompressed raw (aiTexel 4 bytes/texel). If colors look swapped, swap r/b below.
		texWidth = static_cast<int>(embeddedTex.mWidth);
		texHeight = static_cast<int>(embeddedTex.mHeight);
		rgba.resize(texWidth * texHeight * 4);
		for (int i = 0; i < texWidth * texHeight; ++i) {
			rgba[i * 4 + 0] = embeddedTex.pcData[i].r; // swap with b if needed
			rgba[i * 4 + 1] = embeddedTex.pcData[i].g;
			rgba[i * 4 + 2] = embeddedTex.pcData[i].b; // swap with r if needed
			rgba[i * 4 + 3] = embeddedTex.pcData[i].a;
		}
	}

	if (rgba.empty() || texWidth <= 0 || texHeight <= 0) {
		throw std::runtime_error("embedded texture produced no data");
	}

	VkDeviceSize imageSize = static_cast<VkDeviceSize>(texWidth) * texHeight * 4;

	Engine::createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	void *data = nullptr;
	vkMapMemory(Engine::device, stagingBufferMemory, 0, imageSize, 0, &data);
	std::memcpy(data, rgba.data(), static_cast<size_t>(imageSize));
	vkUnmapMemory(Engine::device, stagingBufferMemory);

	Engine::createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);

    Engine::transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    Engine::copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

    Engine::transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkDestroyBuffer(Engine::device, stagingBuffer, nullptr);
	vkFreeMemory(Engine::device, stagingBufferMemory, nullptr);
}

void Texture::createTextureImageView() { textureImageView = Engine::createImageView(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT); }

void Texture::createTextureSampler() {
	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(Engine::physicalDevice, &properties);

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

	if (vkCreateSampler(Engine::device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
		throw std::runtime_error("failed to create texture sampler!");
	}
}
