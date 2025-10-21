#pragma once

#include "engine.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <vector>
#if ANDROID_VK
#include <unistd.h>
#endif

#if ANDROID_VK
constexpr bool enableValidationLayers = false; // always off on Android
struct android_app;
extern android_app *g_app;
#else
#ifndef NDEBUG
constexpr bool enableValidationLayers = true; // on in debug desktop
#else
constexpr bool enableValidationLayers = false; // off in release desktop
#endif
#endif

const uint32_t WIDTH = 1920;
const uint32_t HEIGHT = 1080;

const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};
const std::vector<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

using namespace Engine;

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *) {
	std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
	return VK_FALSE;
}

namespace Pipeline {

inline std::vector<VkSemaphore> imageAvailableSemaphores;
inline std::vector<VkSemaphore> renderFinishedSemaphores;
inline std::vector<VkSemaphore> computeFinishedSemaphores;
inline std::vector<VkSemaphore> renderFinishedSemaphoresPerImage;
inline std::vector<VkFence> inFlightFences;
inline std::vector<VkFence> computeInFlightFences;
inline std::vector<VkFence> imagesInFlight;

inline VkFormat sceneColorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
inline std::vector<VkImageView> sceneColorAttachmentViews;

struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsAndComputeFamily;
	std::optional<uint32_t> presentFamily;
	bool isComplete() const { return graphicsAndComputeFamily.has_value() && presentFamily.has_value(); }
};

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

inline uint32_t calcMipLevels(uint32_t w, uint32_t h) { return 1u + (uint32_t)std::floor(std::log2(std::max(w, h))); }

inline void createSceneColorTargets() {
	const uint32_t mips = calcMipLevels(swapChainExtent.width, swapChainExtent.height);

	sceneColorImages.resize(swapChainImages.size());
	sceneColorMemories.resize(swapChainImages.size());
	sceneColorViews.resize(swapChainImages.size());			  // all mips (for sampling)
	sceneColorAttachmentViews.resize(swapChainImages.size()); // level 0 only (for FB)

	for (size_t i = 0; i < swapChainImages.size(); ++i) {
		VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		createImage(swapChainExtent.width, swapChainExtent.height, sceneColorFormat, VK_IMAGE_TILING_OPTIMAL, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, sceneColorImages[i], sceneColorMemories[i],
					/*mipLevels*/ mips, VK_SAMPLE_COUNT_1_BIT);

		// View for sampling (all mips)
		sceneColorViews[i] = createImageView(sceneColorImages[i], sceneColorFormat, VK_IMAGE_ASPECT_COLOR_BIT,
											 /*baseMip*/ 0, /*levelCount*/ mips);

		// View for framebuffer attachment (exactly one mip!)
		sceneColorAttachmentViews[i] = createImageView(sceneColorImages[i], sceneColorFormat, VK_IMAGE_ASPECT_COLOR_BIT,
													   /*baseMip*/ 0, /*levelCount*/ 1);
	}
}

inline void destroySceneColorTargets() {
	for (size_t i = 0; i < sceneColorImages.size(); ++i) {
		if (sceneColorAttachmentViews.size() > i && sceneColorAttachmentViews[i])
			vkDestroyImageView(device, sceneColorAttachmentViews[i], nullptr);
		if (sceneColorViews.size() > i && sceneColorViews[i])
			vkDestroyImageView(device, sceneColorViews[i], nullptr);
		if (sceneColorImages[i])
			vkDestroyImage(device, sceneColorImages[i], nullptr);
		if (sceneColorMemories[i])
			vkFreeMemory(device, sceneColorMemories[i], nullptr);
	}
	sceneColorAttachmentViews.clear();
	sceneColorViews.clear();
	sceneColorImages.clear();
	sceneColorMemories.clear();
}

inline void createSceneSamplerAndSets() {
	VkSamplerCreateInfo sci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
	sci.magFilter = VK_FILTER_LINEAR;
	sci.minFilter = VK_FILTER_LINEAR;
	sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR; // <<< enable trilinear
	sci.addressModeU = sci.addressModeV = sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	sci.maxAnisotropy = 1.0f; // (enable and set >1 if you like)
	sci.anisotropyEnable = VK_FALSE;
	sci.mipLodBias = 0.0f;
	sci.minLod = 0.0f;
	sci.maxLod = (float)calcMipLevels(swapChainExtent.width, swapChainExtent.height) - 1.0f;

	if (vkCreateSampler(device, &sci, nullptr, &sceneSampler) != VK_SUCCESS)
		throw std::runtime_error("scene sampler create failed");

	// 2) SetLayout: set=1, binding=0 sampler2D uScene
	VkDescriptorSetLayoutBinding b{};
	b.binding = 0;
	b.descriptorCount = 1;
	b.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	b.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo lci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
	lci.bindingCount = 1;
	lci.pBindings = &b;
	if (vkCreateDescriptorSetLayout(device, &lci, nullptr, &sceneSetLayout) != VK_SUCCESS)
		throw std::runtime_error("scene set layout failed");

	// 3) Pool
	VkDescriptorPoolSize ps{};
	ps.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	ps.descriptorCount = (uint32_t)swapChainImages.size();

	VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
	pci.maxSets = (uint32_t)swapChainImages.size();
	pci.poolSizeCount = 1;
	pci.pPoolSizes = &ps;
	if (vkCreateDescriptorPool(device, &pci, nullptr, &sceneDescPool) != VK_SUCCESS)
		throw std::runtime_error("scene desc pool failed");

	// 4) Allocate + write per-image
	sceneSets.resize(swapChainImages.size());
	std::vector<VkDescriptorSetLayout> layouts(swapChainImages.size(), sceneSetLayout);
	VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
	ai.descriptorPool = sceneDescPool;
	ai.descriptorSetCount = (uint32_t)layouts.size();
	ai.pSetLayouts = layouts.data();
	if (vkAllocateDescriptorSets(device, &ai, sceneSets.data()) != VK_SUCCESS)
		throw std::runtime_error("scene desc alloc failed");

	for (size_t i = 0; i < sceneSets.size(); ++i) {
		VkDescriptorImageInfo ii{};
		ii.sampler = sceneSampler;
		ii.imageView = sceneColorViews[i];
		ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // subpass 1

		VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		w.dstSet = sceneSets[i];
		w.dstBinding = 0;
		w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w.descriptorCount = 1;
		w.pImageInfo = &ii;

		vkUpdateDescriptorSets(device, 1, &w, 0, nullptr);
	}
}

inline void buildMipsForImage(VkCommandBuffer cmd, VkImage image, VkFormat /*format*/, uint32_t w, uint32_t h, uint32_t levels) {
	// Level 0 is already TRANSFER_SRC_OPTIMAL (scene pass finalLayout).
	// We still add a barrier to sync color writes -> transfer reads.
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;

	barrier.subresourceRange.baseMipLevel = 0;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	int32_t mipW = (int32_t)w;
	int32_t mipH = (int32_t)h;

	for (uint32_t i = 1; i < levels; ++i) {
		// Transition dst level i to TRANSFER_DST_OPTIMAL
		barrier.subresourceRange.baseMipLevel = i;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		// Blit i-1 -> i (linear)
		VkImageBlit blit{};
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		blit.srcOffsets[0] = {0, 0, 0};
		blit.srcOffsets[1] = {std::max(1, mipW), std::max(1, mipH), 1};

		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;
		blit.dstOffsets[0] = {0, 0, 0};
		blit.dstOffsets[1] = {std::max(1, mipW / 2), std::max(1, mipH / 2), 1};

		vkCmdBlitImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

		// Make level i src for next iteration (and later SHADER_READ)
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		mipW = std::max(1, mipW / 2);
		mipH = std::max(1, mipH / 2);
	}

	// Transition whole chain to SHADER_READ_ONLY_OPTIMAL
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = levels;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL; // level 0 is SRC, others are SRC now too
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

inline void destroySceneSamplerAndSets() {
	if (!sceneSets.empty()) {
		// descriptor sets are freed when pool is destroyed
		sceneSets.clear();
	}
	if (sceneDescPool) {
		vkDestroyDescriptorPool(device, sceneDescPool, nullptr);
		sceneDescPool = VK_NULL_HANDLE;
	}
	if (sceneSetLayout) {
		vkDestroyDescriptorSetLayout(device, sceneSetLayout, nullptr);
		sceneSetLayout = VK_NULL_HANDLE;
	}
	if (sceneSampler) {
		vkDestroySampler(device, sceneSampler, nullptr);
		sceneSampler = VK_NULL_HANDLE;
	}
}

inline bool checkValidationLayerSupport() {
	uint32_t layerCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	std::vector<VkLayerProperties> available(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, available.data());
	for (const char *name : validationLayers) {
		bool found = false;
		for (const auto &lp : available) {
			if (strcmp(name, lp.layerName) == 0) {
				found = true;
				break;
			}
		}
		if (!found)
			return false;
	}
	return true;
}

inline std::vector<const char *> getRequiredExtensions() {
	std::vector<const char *> exts;
#if !ANDROID_VK
	uint32_t count = 0;
	const char **glfwExts = glfwGetRequiredInstanceExtensions(&count);
	exts.assign(glfwExts, glfwExts + count);
#else
	exts.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
	exts.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#endif
	if (enableValidationLayers)
		exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	return exts;
}

inline void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &ci) {
	ci = {};
	ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	ci.pfnUserCallback = debugCallback;
}

inline void createInstance() {
	if (enableValidationLayers && !checkValidationLayerSupport()) {
		throw std::runtime_error("validation layers requested, but not available!");
	}

	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Vulkan Engine";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	auto exts = getRequiredExtensions();

	VkInstanceCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	ci.pApplicationInfo = &appInfo;
	ci.enabledExtensionCount = static_cast<uint32_t>(exts.size());
	ci.ppEnabledExtensionNames = exts.data();

	VkDebugUtilsMessengerCreateInfoEXT dbg{};
	if (enableValidationLayers) {
		ci.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		ci.ppEnabledLayerNames = validationLayers.data();
		populateDebugMessengerCreateInfo(dbg);
		ci.pNext = &dbg;
	}

	if (vkCreateInstance(&ci, nullptr, &instance) != VK_SUCCESS) {
		throw std::runtime_error("failed to create instance!");
	}
}

inline VkResult CreateDebugUtilsMessengerEXT(VkInstance inst, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pMessenger) {
	auto fn = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(inst, "vkCreateDebugUtilsMessengerEXT");
	return fn ? fn(inst, pCreateInfo, pAllocator, pMessenger) : VK_ERROR_EXTENSION_NOT_PRESENT;
}
inline void DestroyDebugUtilsMessengerEXT(VkInstance inst, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks *pAllocator) {
	auto fn = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(inst, "vkDestroyDebugUtilsMessengerEXT");
	if (fn)
		fn(inst, messenger, pAllocator);
}

inline void setupDebugMessenger() {
	if (!enableValidationLayers)
		return;
	VkDebugUtilsMessengerCreateInfoEXT ci;
	populateDebugMessengerCreateInfo(ci);
	if (CreateDebugUtilsMessengerEXT(instance, &ci, nullptr, &debugMessenger) != VK_SUCCESS) {
		throw std::runtime_error("failed to set up debug messenger!");
	}
}

inline void createSurface() {
#if !ANDROID_VK
	if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
		throw std::runtime_error("failed to create window surface!");
	}
#else
	windowAndroid = windowAndroid ? windowAndroid : (g_app ? g_app->window : nullptr);
	if (!windowAndroid)
		throw std::runtime_error("ANativeWindow not ready");
	VkAndroidSurfaceCreateInfoKHR sci{VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR};
	sci.window = windowAndroid;
	if (vkCreateAndroidSurfaceKHR(instance, &sci, nullptr, &surface) != VK_SUCCESS) {
		throw std::runtime_error("failed to create android surface!");
	}
#endif
}

inline QueueFamilyIndices findQueueFamilies(VkPhysicalDevice dev) {
	QueueFamilyIndices indices{};
	uint32_t count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
	std::vector<VkQueueFamilyProperties> props(count);
	vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, props.data());

	for (uint32_t i = 0; i < count; ++i) {
		if (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphicsAndComputeFamily = i;
		}
		VkBool32 presentSupport = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &presentSupport);
		if (presentSupport) {
			indices.presentFamily = i;
		}
		if (indices.isComplete())
			break;
	}
	Engine::graphicsQueueFamilyIndex = indices.graphicsAndComputeFamily.value();
	Engine::presentQueueFamilyIndex = indices.presentFamily.value();
	Engine::computeQueueFamilyIndex = indices.graphicsAndComputeFamily.value();
	return indices;
}

inline bool checkDeviceExtensionSupport(VkPhysicalDevice dev) {
	uint32_t count = 0;
	vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, nullptr);
	std::vector<VkExtensionProperties> available(count);
	vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, available.data());
	std::set<std::string> required(deviceExtensions.begin(), deviceExtensions.end());
	for (const auto &e : available)
		required.erase(e.extensionName);
	return required.empty();
}

inline SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice dev) {
	SwapChainSupportDetails d{};
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, surface, &d.capabilities);

	uint32_t fCount = 0, pCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &fCount, nullptr);
	if (fCount) {
		d.formats.resize(fCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &fCount, d.formats.data());
	}
	vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &pCount, nullptr);
	if (pCount) {
		d.presentModes.resize(pCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &pCount, d.presentModes.data());
	}
	return d;
}

inline bool isDeviceSuitable(VkPhysicalDevice dev) {
	auto indices = findQueueFamilies(dev);
	bool extOK = checkDeviceExtensionSupport(dev);
	bool swapOK = false;
	if (extOK) {
		auto sup = querySwapChainSupport(dev);
		swapOK = !sup.formats.empty() && !sup.presentModes.empty();
	}
	VkPhysicalDeviceFeatures feat{};
	vkGetPhysicalDeviceFeatures(dev, &feat);
	return indices.isComplete() && extOK && swapOK && feat.samplerAnisotropy;
}

inline void pickPhysicalDevice() {
	uint32_t count = 0;
	vkEnumeratePhysicalDevices(instance, &count, nullptr);
	if (!count)
		throw std::runtime_error("failed to find GPUs with Vulkan support!");
	std::vector<VkPhysicalDevice> devs(count);
	vkEnumeratePhysicalDevices(instance, &count, devs.data());

	for (auto d : devs) {
		if (isDeviceSuitable(d)) {
			physicalDevice = d;
			break;
		}
	}
	if (physicalDevice == VK_NULL_HANDLE)
		throw std::runtime_error("failed to find a suitable GPU!");
}

inline void createLogicalDevice() {
	auto indices = findQueueFamilies(physicalDevice);

	std::vector<VkDeviceQueueCreateInfo> queues;
	std::set<uint32_t> unique = {indices.graphicsAndComputeFamily.value(), indices.presentFamily.value()};
	float priority = 1.0f;
	for (auto qf : unique) {
		VkDeviceQueueCreateInfo qi{};
		qi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		qi.queueFamilyIndex = qf;
		qi.queueCount = 1;
		qi.pQueuePriorities = &priority;
		queues.push_back(qi);
	}

	VkPhysicalDeviceFeatures feat{};
	feat.samplerAnisotropy = VK_TRUE;

	VkDeviceCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	ci.queueCreateInfoCount = static_cast<uint32_t>(queues.size());
	ci.pQueueCreateInfos = queues.data();
	ci.pEnabledFeatures = &feat;
	ci.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	ci.ppEnabledExtensionNames = deviceExtensions.data();
	if (enableValidationLayers) {
		ci.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		ci.ppEnabledLayerNames = validationLayers.data();
	}

	if (vkCreateDevice(physicalDevice, &ci, nullptr, &device) != VK_SUCCESS) {
		throw std::runtime_error("failed to create logical device!");
	}

	vkGetDeviceQueue(device, indices.graphicsAndComputeFamily.value(), 0, &graphicsQueue);
	vkGetDeviceQueue(device, indices.graphicsAndComputeFamily.value(), 0, &computeQueue);
	vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}

inline VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &formats) {
	for (const auto &f : formats) {
		if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			return f;
	}
	return formats[0];
}

inline VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &modes) {
	for (const auto &m : modes) {
		if (m == VK_PRESENT_MODE_MAILBOX_KHR)
			return m;
	}
	// FIFO is always supported (and commonly the only one on Android).
	return VK_PRESENT_MODE_FIFO_KHR;
}

inline VkExtent2D currentFramebufferExtent() {
#if !ANDROID_VK
	int w = 0, h = 0;
	glfwGetFramebufferSize(window, &w, &h);
	if (w <= 0)
		w = 1;
	if (h <= 0)
		h = 1;
	return {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
#else
	int w = windowAndroid ? ANativeWindow_getWidth(windowAndroid) : 0;
	int h = windowAndroid ? ANativeWindow_getHeight(windowAndroid) : 0;
	if (w <= 0)
		w = 1;
	if (h <= 0)
		h = 1;
	return {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
#endif
}

inline VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &caps) {
	if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return caps.currentExtent; // surface size dictated by platform
	} else {
		VkExtent2D actual = currentFramebufferExtent();
		actual.width = std::clamp(actual.width, caps.minImageExtent.width, caps.maxImageExtent.width);
		actual.height = std::clamp(actual.height, caps.minImageExtent.height, caps.maxImageExtent.height);
		return actual;
	}
}

inline void createSwapChain() {
	auto sup = querySwapChainSupport(physicalDevice);
	auto format = chooseSwapSurfaceFormat(sup.formats);
	auto mode = chooseSwapPresentMode(sup.presentModes);
	auto extent = chooseSwapExtent(sup.capabilities);

	uint32_t imageCount = sup.capabilities.minImageCount + 1;
	if (sup.capabilities.maxImageCount > 0 && imageCount > sup.capabilities.maxImageCount)
		imageCount = sup.capabilities.maxImageCount;

	VkSwapchainCreateInfoKHR ci{};
	ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	ci.surface = surface;
	ci.minImageCount = imageCount;
	ci.imageFormat = format.format;
	ci.imageColorSpace = format.colorSpace;
	ci.imageExtent = extent;
	ci.imageArrayLayers = 1;
	ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	auto indices = findQueueFamilies(physicalDevice);
	uint32_t qIdx[] = {indices.graphicsAndComputeFamily.value(), indices.presentFamily.value()};
	if (indices.graphicsAndComputeFamily != indices.presentFamily) {
		ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		ci.queueFamilyIndexCount = 2;
		ci.pQueueFamilyIndices = qIdx;
	} else {
		ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	ci.preTransform = sup.capabilities.currentTransform;
	ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	ci.presentMode = mode;
	ci.clipped = VK_TRUE;

	if (vkCreateSwapchainKHR(device, &ci, nullptr, &swapChain) != VK_SUCCESS) {
		throw std::runtime_error("failed to create swap chain!");
	}

	vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
	swapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

	swapChainImageFormat = format.format;
	swapChainExtent = extent;
}

inline void cleanupSwapChain() {
	for (auto fb : sceneFramebuffers)
		vkDestroyFramebuffer(device, fb, nullptr);
	sceneFramebuffers.clear();

	for (auto fb : uiFramebuffers)
		vkDestroyFramebuffer(device, fb, nullptr);
	uiFramebuffers.clear();

	destroySceneColorTargets();

	vkDestroyImageView(device, depthImageView, nullptr);
	if (depthImage)
		vkDestroyImage(device, depthImage, nullptr);
	if (depthImageMemory)
		vkFreeMemory(device, depthImageMemory, nullptr);

	for (auto iv : swapChainImageViews)
		vkDestroyImageView(device, iv, nullptr);
	swapChainImageViews.clear();

	if (swapChain)
		vkDestroySwapchainKHR(device, swapChain, nullptr);

	destroySceneSamplerAndSets();
}

inline void cleanupSyncObjects() {
	for (auto s : imageAvailableSemaphores)
		if (s)
			vkDestroySemaphore(device, s, nullptr);
	for (auto s : renderFinishedSemaphores)
		if (s)
			vkDestroySemaphore(device, s, nullptr);
	for (auto s : computeFinishedSemaphores)
		if (s)
			vkDestroySemaphore(device, s, nullptr);
	for (auto s : renderFinishedSemaphoresPerImage)
		if (s)
			vkDestroySemaphore(device, s, nullptr);
	for (auto f : inFlightFences)
		if (f)
			vkDestroyFence(device, f, nullptr);
	for (auto f : computeInFlightFences)
		if (f)
			vkDestroyFence(device, f, nullptr);

	imageAvailableSemaphores.clear();
	renderFinishedSemaphores.clear();
	computeFinishedSemaphores.clear();
	renderFinishedSemaphoresPerImage.clear();
	inFlightFences.clear();
	computeInFlightFences.clear();
	imagesInFlight.clear();
}

inline void createImageViews() {
	swapChainImageViews.resize(swapChainImages.size());
	for (uint32_t i = 0; i < swapChainImages.size(); ++i) {
		swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
	}
}

inline VkFormat findSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
	for (VkFormat f : candidates) {
		VkFormatProperties p{};
		vkGetPhysicalDeviceFormatProperties(physicalDevice, f, &p);
		if (tiling == VK_IMAGE_TILING_LINEAR && (p.linearTilingFeatures & features) == features)
			return f;
		if (tiling == VK_IMAGE_TILING_OPTIMAL && (p.optimalTilingFeatures & features) == features)
			return f;
	}
	throw std::runtime_error("failed to find supported format!");
}

inline VkFormat findDepthFormat() { return findSupportedFormat({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT); }

inline void createDepthResources() {
	VkFormat depthFormat = findDepthFormat();
	createImage(swapChainExtent.width, swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);
	depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

inline void createRenderPassScene() {
	// A0: scene color
	VkAttachmentDescription aScene{};
	aScene.format = sceneColorFormat;
	aScene.samples = VK_SAMPLE_COUNT_1_BIT;
	aScene.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	aScene.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	aScene.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	aScene.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	aScene.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	// We’ll blit right after; make subpass transition straight to TRANSFER_SRC
	aScene.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

	// A1: depth
	VkAttachmentDescription aDepth{};
	aDepth.format = findDepthFormat();
	aDepth.samples = VK_SAMPLE_COUNT_1_BIT;
	aDepth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	aDepth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	aDepth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	aDepth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	aDepth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	aDepth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
	VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

	VkSubpassDescription sp{};
	sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	sp.colorAttachmentCount = 1;
	sp.pColorAttachments = &colorRef;
	sp.pDepthStencilAttachment = &depthRef;

	// External -> Subpass 0
	VkSubpassDependency dep{};
	dep.srcSubpass = VK_SUBPASS_EXTERNAL;
	dep.dstSubpass = 0;
	dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dep.srcAccessMask = 0;
	dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	std::array<VkAttachmentDescription, 2> atts{aScene, aDepth};
	VkRenderPassCreateInfo rpci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
	rpci.attachmentCount = (uint32_t)atts.size();
	rpci.pAttachments = atts.data();
	rpci.subpassCount = 1;
	rpci.pSubpasses = &sp;
	rpci.dependencyCount = 1;
	rpci.pDependencies = &dep;

	if (vkCreateRenderPass(device, &rpci, nullptr, &renderPass) != VK_SUCCESS)
		throw std::runtime_error("failed to create scene render pass");
}

inline void createRenderPassUI() {
	// A0: swapchain
	VkAttachmentDescription aSwap{};
	aSwap.format = swapChainImageFormat;
	aSwap.samples = VK_SAMPLE_COUNT_1_BIT;
	aSwap.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	aSwap.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	aSwap.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	aSwap.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	aSwap.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	aSwap.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

	VkSubpassDescription sp{};
	sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	sp.colorAttachmentCount = 1;
	sp.pColorAttachments = &colorRef;

	VkSubpassDependency dep{};
	dep.srcSubpass = VK_SUBPASS_EXTERNAL;
	dep.dstSubpass = 0;
	dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dep.srcAccessMask = 0;
	dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo rpci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
	rpci.attachmentCount = 1;
	rpci.pAttachments = &aSwap;
	rpci.subpassCount = 1;
	rpci.pSubpasses = &sp;
	rpci.dependencyCount = 1;
	rpci.pDependencies = &dep;

	if (vkCreateRenderPass(device, &rpci, nullptr, &renderPass1) != VK_SUCCESS)
		throw std::runtime_error("failed to create UI render pass");
}

inline void createRenderPasses() {
	createRenderPassScene();
	createRenderPassUI();
}

inline void createFramebuffersScene() {
	sceneFramebuffers.resize(swapChainImageViews.size());
	for (size_t i = 0; i < swapChainImageViews.size(); ++i) {
		// IMPORTANT: attachment view must expose ONE mip level (level 0)
		// Make sure you created sceneColorAttachmentViews[i] (levelCount=1)
		std::array<VkImageView, 2> atts = {sceneColorAttachmentViews[i], // levelCount=1 view
										   depthImageView};
		VkFramebufferCreateInfo ci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
		ci.renderPass = renderPass;
		ci.attachmentCount = (uint32_t)atts.size();
		ci.pAttachments = atts.data();
		ci.width = swapChainExtent.width;
		ci.height = swapChainExtent.height;
		ci.layers = 1;
		if (vkCreateFramebuffer(device, &ci, nullptr, &sceneFramebuffers[i]) != VK_SUCCESS)
			throw std::runtime_error("failed to create scene framebuffer");
	}
}

inline void createFramebuffersUI() {
	uiFramebuffers.resize(swapChainImageViews.size());
	for (size_t i = 0; i < swapChainImageViews.size(); ++i) {
		VkImageView att = swapChainImageViews[i];
		VkFramebufferCreateInfo ci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
		ci.renderPass = renderPass1;
		ci.attachmentCount = 1;
		ci.pAttachments = &att;
		ci.width = swapChainExtent.width;
		ci.height = swapChainExtent.height;
		ci.layers = 1;
		if (vkCreateFramebuffer(device, &ci, nullptr, &uiFramebuffers[i]) != VK_SUCCESS)
			throw std::runtime_error("failed to create UI framebuffer");
	}
}

inline void createFramebuffers() {
	createFramebuffersScene();
	createFramebuffersUI();
}

inline void createCommandPool() {
	auto q = findQueueFamilies(physicalDevice);
	VkCommandPoolCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	ci.queueFamilyIndex = q.graphicsAndComputeFamily.value();
	if (vkCreateCommandPool(device, &ci, nullptr, &commandPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create command pool!");
	}
}

inline void createCommandBuffers() {
	commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	VkCommandBufferAllocateInfo ai{};
	ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	ai.commandPool = commandPool;
	ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	ai.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
	if (vkAllocateCommandBuffers(device, &ai, commandBuffers.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate command buffers!");
	}
}

inline void createComputeCommandBuffers() {
	computeCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	VkCommandBufferAllocateInfo ai{};
	ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	ai.commandPool = commandPool;
	ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	ai.commandBufferCount = static_cast<uint32_t>(computeCommandBuffers.size());
	if (vkAllocateCommandBuffers(device, &ai, computeCommandBuffers.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate compute command buffers!");
	}
}

inline void createSyncObjects() {
	imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT, VK_NULL_HANDLE);
	renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT, VK_NULL_HANDLE);
	computeFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT, VK_NULL_HANDLE);
	inFlightFences.resize(MAX_FRAMES_IN_FLIGHT, VK_NULL_HANDLE);
	computeInFlightFences.resize(MAX_FRAMES_IN_FLIGHT, VK_NULL_HANDLE);

	const size_t imageCount = swapChainImages.size();
	renderFinishedSemaphoresPerImage.resize(imageCount, VK_NULL_HANDLE);
	imagesInFlight.resize(imageCount, VK_NULL_HANDLE);

	VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
	VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
	fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		if (!imageAvailableSemaphores[i] && vkCreateSemaphore(device, &sci, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS)
			throw std::runtime_error("failed to create imageAvailable semaphore!");
		if (!renderFinishedSemaphores[i] && vkCreateSemaphore(device, &sci, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS)
			throw std::runtime_error("failed to create renderFinished semaphore!");
		if (!inFlightFences[i] && vkCreateFence(device, &fci, nullptr, &inFlightFences[i]) != VK_SUCCESS)
			throw std::runtime_error("failed to create inFlight fence!");
		if (!computeFinishedSemaphores[i] && vkCreateSemaphore(device, &sci, nullptr, &computeFinishedSemaphores[i]) != VK_SUCCESS)
			throw std::runtime_error("failed to create computeFinished semaphore!");
		if (!computeInFlightFences[i] && vkCreateFence(device, &fci, nullptr, &computeInFlightFences[i]) != VK_SUCCESS)
			throw std::runtime_error("failed to create computeInFlight fence!");
	}

	for (size_t i = 0; i < imageCount; ++i) {
		if (!renderFinishedSemaphoresPerImage[i] && vkCreateSemaphore(device, &sci, nullptr, &renderFinishedSemaphoresPerImage[i]) != VK_SUCCESS)
			throw std::runtime_error("failed to create per-image renderFinished semaphore!");
		imagesInFlight[i] = VK_NULL_HANDLE;
	}
}

inline void createSwapchainDependent() {
	createImageViews();
	createSceneColorTargets();
	createDepthResources();
	createFramebuffers();
	createSceneSamplerAndSets();
}

inline void recreateSwapChain() {
	// Wait for a valid (non-zero) framebuffer size
#if !ANDROID_VK
	int w = 0, h = 0;
	glfwGetFramebufferSize(window, &w, &h);
	while (w == 0 || h == 0) {
		glfwGetFramebufferSize(window, &w, &h);
		glfwWaitEvents();
	}
#else
	int w = windowAndroid ? ANativeWindow_getWidth(windowAndroid) : 0;
	int h = windowAndroid ? ANativeWindow_getHeight(windowAndroid) : 0;
	while (w == 0 || h == 0) {
		usleep(16 * 1000); // ~1 frame
		if (windowAndroid) {
			w = ANativeWindow_getWidth(windowAndroid);
			h = ANativeWindow_getHeight(windowAndroid);
		}
	}
#endif

	vkDeviceWaitIdle(device);

	cleanupSwapChain();
	cleanupSyncObjects();
	if (renderPass) {
		vkDestroyRenderPass(device, renderPass, nullptr);
	}
	if (renderPass1) {
		vkDestroyRenderPass(device, renderPass1, nullptr);
	}

	createSwapChain();
	createRenderPasses();
	createSwapchainDependent();

	createSyncObjects();
	currentFrame = 0;
}

} // namespace Pipeline
