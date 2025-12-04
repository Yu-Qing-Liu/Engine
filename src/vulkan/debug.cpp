#include "debug.hpp"

#include <GLFW/glfw3.h>
#include <cstring>
#include <stdexcept>

Debug::Debug() {
	validationEnabled = enableValidationLayers;
	createInstance();
	setupDebugMessenger();
}

Debug::~Debug() {
	if (debugMessenger != VK_NULL_HANDLE) {
		destroyDebugUtilsMessengerEXT(debugMessenger, nullptr);
		debugMessenger = VK_NULL_HANDLE;
	}
	if (instance != VK_NULL_HANDLE) {
		vkDestroyInstance(instance, nullptr);
		instance = VK_NULL_HANDLE;
	}
}

bool Debug::checkValidationLayerSupport() const {
	uint32_t layerCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	std::vector<VkLayerProperties> layers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

	for (auto name : validationLayers) {
		bool found = false;
		for (auto &lp : layers) {
			if (strcmp(lp.layerName, name) == 0) {
				found = true;
				break;
			}
		}
		if (!found)
			return false;
	}
	return true;
}

std::vector<const char *> Debug::getRequiredExtensions() const {
    std::vector<const char *> exts;
    
    uint32_t glfwCount = 0;
    const char **glfwExts = glfwGetRequiredInstanceExtensions(&glfwCount);

    if (!glfwExts || glfwCount == 0) {
        std::fprintf(stderr, "[Vulkan] FATAL: glfwGetRequiredInstanceExtensions() returned null/empty.\n");
        throw std::runtime_error("GLFW did not provide required Vulkan instance extensions");
    }

	exts.assign(glfwExts, glfwExts + glfwCount);

    if (validationEnabled) {
        exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return exts;
}

void Debug::populateDebugCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &ci) const {
	ci = {};
	ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	ci.pfnUserCallback = vkDebugCallback;
}

void Debug::createInstance() {
	if (validationEnabled && !checkValidationLayerSupport()) {
		throw std::runtime_error("Validation layers requested but not available");
	}

	VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
	appInfo.pApplicationName = "Vulkan Engine";
	appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
	appInfo.pEngineName = "Engine";
	appInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
	// Ask for Vulkan 1.3 instance (safe to request; loader will negotiate down if needed)
	appInfo.apiVersion = VK_API_VERSION_1_3;

	auto exts = getRequiredExtensions();

	VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
	ci.pApplicationInfo = &appInfo;
	ci.enabledExtensionCount = (uint32_t)exts.size();
	ci.ppEnabledExtensionNames = exts.data();

	VkDebugUtilsMessengerCreateInfoEXT dbgInfo{};
	if (validationEnabled) {
		ci.enabledLayerCount = (uint32_t)validationLayers.size();
		ci.ppEnabledLayerNames = validationLayers.data();

		populateDebugCreateInfo(dbgInfo);
		ci.pNext = &dbgInfo;
	} else {
		ci.enabledLayerCount = 0;
		ci.ppEnabledLayerNames = nullptr;
	}

	if (vkCreateInstance(&ci, nullptr, &instance) != VK_SUCCESS) {
		throw std::runtime_error("failed to create VkInstance");
	}
}

VkResult Debug::createDebugUtilsMessengerEXT(const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAlloc, VkDebugUtilsMessengerEXT *pOut) {
	auto fn = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

	if (!fn)
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	return fn(instance, pCreateInfo, pAlloc, pOut);
}

void Debug::destroyDebugUtilsMessengerEXT(VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks *pAlloc) {
	auto fn = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (fn)
		fn(instance, messenger, pAlloc);
}

void Debug::setupDebugMessenger() {
	if (!validationEnabled)
		return;

	VkDebugUtilsMessengerCreateInfoEXT ci{};
	populateDebugCreateInfo(ci);

	if (createDebugUtilsMessengerEXT(&ci, nullptr, &debugMessenger) != VK_SUCCESS) {
		throw std::runtime_error("failed to set up debug messenger");
	}
}
