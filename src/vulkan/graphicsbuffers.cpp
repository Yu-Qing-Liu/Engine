#include "graphicsbuffers.hpp"
#include "memory.hpp"
#include <cstring>
#include <stdexcept>

GraphicsBuffers::~GraphicsBuffers() { destroy(); }

void GraphicsBuffers::destroy() {
	if (dev == VK_NULL_HANDLE)
		return;

	destroySamplerAndDescriptors();
	destroyDepthTarget();
	destroySceneColorTargets();

	dev = VK_NULL_HANDLE;
	phys = VK_NULL_HANDLE;
	swapCount = 0;
}

uint32_t GraphicsBuffers::calcMipLevels(uint32_t w, uint32_t h) const {
	uint32_t larger = (w > h ? w : h);
	uint32_t levels = 1;
	while (larger > 1) {
		larger >>= 1;
		levels++;
	}
	return levels;
}

VkFormat GraphicsBuffers::findSupportedDepthFormat(const std::vector<VkFormat> &cands) const {
	for (VkFormat f : cands) {
		VkFormatProperties props{};
		vkGetPhysicalDeviceFormatProperties(phys, f, &props);
		if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
			return f;
		}
	}
	throw std::runtime_error("GraphicsBuffers: No supported depth format");
}

VkFormat GraphicsBuffers::chooseDepthFormat() const {
	static const std::vector<VkFormat> candidates = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
	return findSupportedDepthFormat(candidates);
}

void GraphicsBuffers::create(VkPhysicalDevice physicalDevice, VkDevice deviceIn, VkExtent2D extent, VkFormat colorFormat, uint32_t swapImageCount) {

	phys = physicalDevice;
	dev = deviceIn;
	size = extent;
	sceneColorFmt = colorFormat;
	swapCount = swapImageCount;

	mipLevels = calcMipLevels(size.width, size.height);
	depthFmt = chooseDepthFormat();

	createSceneColorTargets();
	createDepthTarget();
	createSamplerAndDescriptors();
}

void GraphicsBuffers::createImage(uint32_t w, uint32_t h, VkFormat format, VkImageUsageFlags usage, VkImage &outImage, VkDeviceMemory &outMem, uint32_t mipLvls, VkSampleCountFlagBits samples) {
	VkImageCreateInfo ici{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
	ici.imageType = VK_IMAGE_TYPE_2D;
	ici.extent = {w, h, 1};
	ici.mipLevels = mipLvls;
	ici.arrayLayers = 1;
	ici.format = format;
	ici.tiling = VK_IMAGE_TILING_OPTIMAL;
	ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	ici.usage = usage;
	ici.samples = samples;
	ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateImage(dev, &ici, nullptr, &outImage) != VK_SUCCESS) {
		throw std::runtime_error("GraphicsBuffers: Failed to create image");
	}

	VkMemoryRequirements memReq{};
	vkGetImageMemoryRequirements(dev, outImage, &memReq);

	VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	mai.allocationSize = memReq.size;
	mai.memoryTypeIndex = Memory::findMemoryType(phys, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (vkAllocateMemory(dev, &mai, nullptr, &outMem) != VK_SUCCESS) {
		throw std::runtime_error("GraphicsBuffers: Failed to allocate image memory");
	}

	vkBindImageMemory(dev, outImage, outMem, 0);
}

VkImageView GraphicsBuffers::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect, uint32_t baseMip, uint32_t levelCount) {
	VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
	vi.image = image;
	vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
	vi.format = format;
	vi.subresourceRange.aspectMask = aspect;
	vi.subresourceRange.baseMipLevel = baseMip;
	vi.subresourceRange.levelCount = levelCount;
	vi.subresourceRange.baseArrayLayer = 0;
	vi.subresourceRange.layerCount = 1;

	VkImageView view = VK_NULL_HANDLE;
	if (vkCreateImageView(dev, &vi, nullptr, &view) != VK_SUCCESS) {
		throw std::runtime_error("GraphicsBuffers: Failed to create image view");
	}
	return view;
}

void GraphicsBuffers::createSceneColorTargets() {
	// create ping-pong A/B
	sceneColorAImages.resize(swapCount);
	sceneColorAMemory.resize(swapCount);
	sceneColorAAttViews.resize(swapCount);
	sceneColorASampleViews.resize(swapCount);

	sceneColorBImages.resize(swapCount);
	sceneColorBMemory.resize(swapCount);
	sceneColorBAttViews.resize(swapCount);
	sceneColorBSampleViews.resize(swapCount);

	const VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	for (uint32_t i = 0; i < swapCount; ++i) {
		// A
		createImage(size.width, size.height, sceneColorFmt, usage, sceneColorAImages[i], sceneColorAMemory[i], mipLevels, VK_SAMPLE_COUNT_1_BIT);

		sceneColorASampleViews[i] = createImageView(sceneColorAImages[i], sceneColorFmt, VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels);

		sceneColorAAttViews[i] = createImageView(sceneColorAImages[i], sceneColorFmt, VK_IMAGE_ASPECT_COLOR_BIT, 0,
												 1); // mip0 only

		// B
		createImage(size.width, size.height, sceneColorFmt, usage, sceneColorBImages[i], sceneColorBMemory[i], mipLevels, VK_SAMPLE_COUNT_1_BIT);

		sceneColorBSampleViews[i] = createImageView(sceneColorBImages[i], sceneColorFmt, VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels);

		sceneColorBAttViews[i] = createImageView(sceneColorBImages[i], sceneColorFmt, VK_IMAGE_ASPECT_COLOR_BIT, 0,
												 1); // mip0 only
	}
}

void GraphicsBuffers::destroySceneColorTargets() {
	for (size_t i = 0; i < sceneColorAImages.size(); ++i) {
		if (sceneColorAAttViews[i]) {
			vkDestroyImageView(dev, sceneColorAAttViews[i], nullptr);
		}
		if (sceneColorASampleViews[i]) {
			vkDestroyImageView(dev, sceneColorASampleViews[i], nullptr);
		}
		if (sceneColorAImages[i]) {
			vkDestroyImage(dev, sceneColorAImages[i], nullptr);
		}
		if (sceneColorAMemory[i]) {
			vkFreeMemory(dev, sceneColorAMemory[i], nullptr);
		}
	}

	for (size_t i = 0; i < sceneColorBImages.size(); ++i) {
		if (sceneColorBAttViews[i]) {
			vkDestroyImageView(dev, sceneColorBAttViews[i], nullptr);
		}
		if (sceneColorBSampleViews[i]) {
			vkDestroyImageView(dev, sceneColorBSampleViews[i], nullptr);
		}
		if (sceneColorBImages[i]) {
			vkDestroyImage(dev, sceneColorBImages[i], nullptr);
		}
		if (sceneColorBMemory[i]) {
			vkFreeMemory(dev, sceneColorBMemory[i], nullptr);
		}
	}

	sceneColorAAttViews.clear();
	sceneColorASampleViews.clear();
	sceneColorAImages.clear();
	sceneColorAMemory.clear();

	sceneColorBAttViews.clear();
	sceneColorBSampleViews.clear();
	sceneColorBImages.clear();
	sceneColorBMemory.clear();
}

void GraphicsBuffers::createDepthTarget() {
	depthFmt = chooseDepthFormat();

	const VkImageUsageFlags usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	createImage(size.width, size.height, depthFmt, usage, depthImage, depthMemory,
				/*mipLevels*/ 1, VK_SAMPLE_COUNT_1_BIT);

	depthImageView = createImageView(depthImage, depthFmt, VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1);
}

void GraphicsBuffers::destroyDepthTarget() {
	if (depthImageView) {
		vkDestroyImageView(dev, depthImageView, nullptr);
		depthImageView = VK_NULL_HANDLE;
	}
	if (depthImage) {
		vkDestroyImage(dev, depthImage, nullptr);
		depthImage = VK_NULL_HANDLE;
	}
	if (depthMemory) {
		vkFreeMemory(dev, depthMemory, nullptr);
		depthMemory = VK_NULL_HANDLE;
	}
}

void GraphicsBuffers::createSamplerAndDescriptors() {
	// 1. sampler
	VkSamplerCreateInfo sci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
	sci.magFilter = VK_FILTER_LINEAR;
	sci.minFilter = VK_FILTER_LINEAR;
	sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sci.anisotropyEnable = VK_FALSE;
	sci.maxAnisotropy = 1.0f;
	sci.minLod = 0.0f;
	sci.maxLod = (float)mipLevels - 1.0f;
	sci.mipLodBias = 0.0f;

	if (vkCreateSampler(dev, &sci, nullptr, &sceneSampler) != VK_SUCCESS) {
		throw std::runtime_error("GraphicsBuffers: Failed to create sampler");
	}

	// 2. descriptor set layout
	VkDescriptorSetLayoutBinding b{};
	b.binding = 0;
	b.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	b.descriptorCount = 1;
	b.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo lci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
	lci.bindingCount = 1;
	lci.pBindings = &b;

	if (vkCreateDescriptorSetLayout(dev, &lci, nullptr, &sceneSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("GraphicsBuffers: Failed to create descriptor set layout");
	}

	// 3. descriptor pool
	// We need 2 descriptor sets per swap image (A and B).
	const uint32_t totalSets = swapCount * 2;

	VkDescriptorPoolSize ps{};
	ps.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	ps.descriptorCount = totalSets;

	VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
	pci.poolSizeCount = 1;
	pci.pPoolSizes = &ps;
	pci.maxSets = totalSets;
	pci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

	if (vkCreateDescriptorPool(dev, &pci, nullptr, &sceneDescPool) != VK_SUCCESS) {
		throw std::runtime_error("GraphicsBuffers: Failed to create descriptor pool");
	}

	// 4. allocate
	sceneSetsA.resize(swapCount);
	sceneSetsB.resize(swapCount);

	std::vector<VkDescriptorSetLayout> layouts;
	layouts.reserve(totalSets);
	for (uint32_t i = 0; i < swapCount; ++i) {
		layouts.push_back(sceneSetLayout); // A
		layouts.push_back(sceneSetLayout); // B
	}

	VkDescriptorSetAllocateInfo dai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
	dai.descriptorPool = sceneDescPool;
	dai.descriptorSetCount = totalSets;
	dai.pSetLayouts = layouts.data();

	std::vector<VkDescriptorSet> tmp(totalSets);
	if (vkAllocateDescriptorSets(dev, &dai, tmp.data()) != VK_SUCCESS) {
		throw std::runtime_error("GraphicsBuffers: Failed to allocate descriptor sets");
	}

	// scatter tmp -> A,B
	for (uint32_t i = 0; i < swapCount; ++i) {
		sceneSetsA[i] = tmp[i * 2 + 0];
		sceneSetsB[i] = tmp[i * 2 + 1];
	}

	// 5. write descriptors
	for (uint32_t i = 0; i < swapCount; ++i) {
		// A set
		{
			VkDescriptorImageInfo ii{};
			ii.sampler = sceneSampler;
			ii.imageView = sceneColorASampleViews[i];
			// NOTE: runtime will move image layouts, but descriptor only stores layout expectation.
			ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
			w.dstSet = sceneSetsA[i];
			w.dstBinding = 0;
			w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			w.descriptorCount = 1;
			w.pImageInfo = &ii;

			vkUpdateDescriptorSets(dev, 1, &w, 0, nullptr);
		}
		// B set
		{
			VkDescriptorImageInfo ii{};
			ii.sampler = sceneSampler;
			ii.imageView = sceneColorBSampleViews[i];
			ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
			w.dstSet = sceneSetsB[i];
			w.dstBinding = 0;
			w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			w.descriptorCount = 1;
			w.pImageInfo = &ii;

			vkUpdateDescriptorSets(dev, 1, &w, 0, nullptr);
		}
	}
}

void GraphicsBuffers::destroySamplerAndDescriptors() {
	sceneSetsA.clear();
	sceneSetsB.clear();

	if (sceneDescPool) {
		vkDestroyDescriptorPool(dev, sceneDescPool, nullptr);
		sceneDescPool = VK_NULL_HANDLE;
	}
	if (sceneSetLayout) {
		vkDestroyDescriptorSetLayout(dev, sceneSetLayout, nullptr);
		sceneSetLayout = VK_NULL_HANDLE;
	}
	if (sceneSampler) {
		vkDestroySampler(dev, sceneSampler, nullptr);
		sceneSampler = VK_NULL_HANDLE;
	}
}
