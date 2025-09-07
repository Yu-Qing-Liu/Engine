#pragma once

#include "engine.hpp"
#include <algorithm>
#include <array>
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
constexpr bool enableValidationLayers = false;   // always off on Android
struct android_app;
extern android_app *g_app;
#else
#  ifndef NDEBUG
constexpr bool enableValidationLayers = true;    // on in debug desktop
#  else
constexpr bool enableValidationLayers = false;   // off in release desktop
#  endif
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
	vkDestroyImageView(device, depthImageView, nullptr);
	vkDestroyImage(device, depthImage, nullptr);
	vkFreeMemory(device, depthImageMemory, nullptr);

	for (auto fb : swapChainFramebuffers)
		vkDestroyFramebuffer(device, fb, nullptr);
	for (auto iv : swapChainImageViews)
		vkDestroyImageView(device, iv, nullptr);

	vkDestroySwapchainKHR(device, swapChain, nullptr);
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

inline void createRenderPass() {
	VkAttachmentDescription color{};
	color.format = swapChainImageFormat;
	color.samples = VK_SAMPLE_COUNT_1_BIT;
	color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentDescription depth{};
	depth.format = findDepthFormat();
	depth.samples = VK_SAMPLE_COUNT_1_BIT;
	depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
	VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef;
	subpass.pDepthStencilAttachment = &depthRef;

	VkSubpassDependency dep{};
	dep.srcSubpass = VK_SUBPASS_EXTERNAL;
	dep.dstSubpass = 0;
	dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dep.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	std::array<VkAttachmentDescription, 2> att{color, depth};
	VkRenderPassCreateInfo rpci{};
	rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	rpci.attachmentCount = static_cast<uint32_t>(att.size());
	rpci.pAttachments = att.data();
	rpci.subpassCount = 1;
	rpci.pSubpasses = &subpass;
	rpci.dependencyCount = 1;
	rpci.pDependencies = &dep;

	if (vkCreateRenderPass(device, &rpci, nullptr, &renderPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create render pass!");
	}
}

inline void createFramebuffers() {
	swapChainFramebuffers.resize(swapChainImageViews.size());
	for (size_t i = 0; i < swapChainImageViews.size(); ++i) {
		std::array<VkImageView, 2> atts = {swapChainImageViews[i], depthImageView};
		VkFramebufferCreateInfo ci{};
		ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		ci.renderPass = renderPass;
		ci.attachmentCount = static_cast<uint32_t>(atts.size());
		ci.pAttachments = atts.data();
		ci.width = swapChainExtent.width;
		ci.height = swapChainExtent.height;
		ci.layers = 1;
		if (vkCreateFramebuffer(device, &ci, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create framebuffer!");
		}
	}
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

	createSwapChain();
	createImageViews();
	createDepthResources();
	createFramebuffers();

	createSyncObjects();
	currentFrame = 0;
}

} // namespace Pipeline
