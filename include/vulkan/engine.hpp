#pragma once

#if !ANDROID_VK
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#else
#include <android/native_window.h>	   // ANativeWindow
#include <android/native_window_jni.h> // (optional; if you pass from Java)
#include <vulkan/vulkan_core.h>
#endif

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace Engine {

inline const int MAX_FRAMES_IN_FLIGHT = 2;

inline VkDebugUtilsMessengerEXT debugMessenger;

#if !ANDROID_VK
inline GLFWwindow *window = nullptr;
#else
inline ANativeWindow *windowAndroid = nullptr;
// Call this early in android_main() when app->window becomes non-null or changes.
inline void setAndroidWindow(ANativeWindow *w) { windowAndroid = w; }
#endif

inline VkSurfaceKHR surface = VK_NULL_HANDLE;

inline VkInstance instance = VK_NULL_HANDLE;

inline VkQueue graphicsQueue = VK_NULL_HANDLE;
inline VkQueue computeQueue = VK_NULL_HANDLE;
inline VkQueue presentQueue = VK_NULL_HANDLE;

inline uint32_t graphicsQueueFamilyIndex = UINT32_MAX;
inline uint32_t presentQueueFamilyIndex  = UINT32_MAX;
inline uint32_t computeQueueFamilyIndex  = UINT32_MAX;

inline VkDevice device = VK_NULL_HANDLE;
inline VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

inline VkRenderPass renderPass  = VK_NULL_HANDLE;
inline VkRenderPass renderPass1     = VK_NULL_HANDLE;
inline VkExtent2D swapChainExtent{};

inline VkCommandPool commandPool = VK_NULL_HANDLE;

inline VkFormat swapChainImageFormat = VK_FORMAT_UNDEFINED;
inline std::vector<VkImageView> swapChainImageViews;
inline std::vector<VkImage> swapChainImages;

inline std::vector<VkCommandBuffer> commandBuffers;
inline std::vector<VkCommandBuffer> computeCommandBuffers;
inline uint32_t currentFrame = 0;

inline uint32_t currentImageIndex = 0;
inline std::vector<VkImage> sceneColorImages;
inline std::vector<VkDeviceMemory> sceneColorMemories;
inline std::vector<VkImageView> sceneColorViews;

inline VkSampler sceneSampler = VK_NULL_HANDLE;
inline VkDescriptorSetLayout sceneSetLayout = VK_NULL_HANDLE;
inline VkDescriptorPool sceneDescPool = VK_NULL_HANDLE;
inline std::vector<VkDescriptorSet> sceneSets; // size == swapChainImages.size()

inline VkSwapchainKHR swapChain = VK_NULL_HANDLE;
inline std::vector<VkFramebuffer> sceneFramebuffers;
inline std::vector<VkFramebuffer> uiFramebuffers;

inline VkImage depthImage = VK_NULL_HANDLE;
inline VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
inline VkImageView depthImageView = VK_NULL_HANDLE;

inline float startTime = 0.0f;
inline float deltaTime = 0.0f;
inline float lastFrameTime = 0.0f;
inline double lastTime = 0.0;
inline float time = 0.0f;

inline VkCommandBuffer currentCommandBuffer() { return commandBuffers[currentFrame]; }
inline VkCommandBuffer currentComputeCommandBuffer() { return computeCommandBuffers[currentFrame]; }

inline VkPipelineShaderStageCreateInfo createShaderStageInfo(VkShaderModule &shaderModule, VkShaderStageFlagBits stage) {
	VkPipelineShaderStageCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	info.stage = stage;
	info.module = shaderModule;
	info.pName = "main";
	return info;
}

inline uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties memProperties{};
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1u << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}
	throw std::runtime_error("failed to find suitable memory type!");
}

inline void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer &buffer, VkDeviceMemory &bufferMemory) {
	VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
		throw std::runtime_error("failed to create buffer!");

	VkMemoryRequirements memReq{};
	vkGetBufferMemoryRequirements(device, buffer, &memReq);

	VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	allocInfo.allocationSize = memReq.size;
	allocInfo.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, properties);

	if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
		throw std::runtime_error("failed to allocate buffer memory!");

	vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

inline VkCommandBuffer beginSingleTimeCommands() {
	VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer cmd{};
	vkAllocateCommandBuffers(device, &allocInfo, &cmd);

	VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &beginInfo);

	return cmd;
}

inline void endSingleTimeCommands(VkCommandBuffer cmd) {
	vkEndCommandBuffer(cmd);

	VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &cmd;

	vkQueueSubmit(graphicsQueue, 1, &submit, VK_NULL_HANDLE);
	vkQueueWaitIdle(graphicsQueue);

	vkFreeCommandBuffers(device, commandPool, 1, &cmd);
}

inline void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) {
	VkCommandBuffer cmd = beginSingleTimeCommands();
	VkBufferCopy region{};
	region.size = size;
	vkCmdCopyBuffer(cmd, src, dst, 1, &region);
	endSingleTimeCommands(cmd);
}

inline void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage &image, VkDeviceMemory &imageMemory, uint32_t mipLevels, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT) {
	// NOTE: Vulkan disallows mipmaps for MSAA images. Keep samples=1 when mipLevels>1.
	if (mipLevels > 1 && samples != VK_SAMPLE_COUNT_1_BIT) {
		throw std::runtime_error("createImage: mipmapped images must use SAMPLE_COUNT_1_BIT");
	}

	VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
	ci.imageType = VK_IMAGE_TYPE_2D;
	ci.extent = {width, height, 1};
	ci.mipLevels = mipLevels;
	ci.arrayLayers = 1;
	ci.format = format;
	ci.tiling = tiling;
	ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	ci.usage = usage;
	ci.samples = samples;
	ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateImage(device, &ci, nullptr, &image) != VK_SUCCESS)
		throw std::runtime_error("failed to create image!");

	VkMemoryRequirements memReq{};
	vkGetImageMemoryRequirements(device, image, &memReq);

	VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	ai.allocationSize = memReq.size;
	ai.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, properties);

	if (vkAllocateMemory(device, &ai, nullptr, &imageMemory) != VK_SUCCESS)
		throw std::runtime_error("failed to allocate image memory!");

	vkBindImageMemory(device, image, imageMemory, 0);
}

inline void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage &image, VkDeviceMemory &imageMemory) { createImage(width, height, format, tiling, usage, properties, image, imageMemory, /*mipLevels=*/1, VK_SAMPLE_COUNT_1_BIT); }

inline VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect, uint32_t baseMip, uint32_t levelCount, uint32_t baseArrayLayer = 0, uint32_t layerCount = 1, VkImageViewType type = VK_IMAGE_VIEW_TYPE_2D) {
	VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
	vi.image = image;
	vi.viewType = type;
	vi.format = format;
	vi.subresourceRange.aspectMask = aspect;
	vi.subresourceRange.baseMipLevel = baseMip;
	vi.subresourceRange.levelCount = levelCount;
	vi.subresourceRange.baseArrayLayer = baseArrayLayer;
	vi.subresourceRange.layerCount = layerCount;

	VkImageView view{};
	if (vkCreateImageView(device, &vi, nullptr, &view) != VK_SUCCESS)
		throw std::runtime_error("failed to create image view!");
	return view;
}

inline VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect) { return createImageView(image, format, aspect, 0, 1); }

inline void transitionImageLayout(VkImage image, VkFormat /*format*/, VkImageLayout oldLayout, VkImageLayout newLayout) {
	VkCommandBuffer cmd = beginSingleTimeCommands();

	VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags srcStage{}, dstStage{};

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	} else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	} else {
		throw std::invalid_argument("unsupported layout transition!");
	}

	vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
	endSingleTimeCommands(cmd);
}

inline void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t w, uint32_t h) {
	VkCommandBuffer cmd = beginSingleTimeCommands();

	VkBufferImageCopy region{};
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = {0, 0, 0};
	region.imageExtent = {w, h, 1};

	vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	endSingleTimeCommands(cmd);
}

} // namespace Engine
