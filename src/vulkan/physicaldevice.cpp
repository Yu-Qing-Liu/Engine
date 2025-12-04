#include "physicaldevice.hpp"

#include <cstring>
#include <set>
#include <stdexcept>

PhysicalDevice::PhysicalDevice(VkInstance inst, VkSurfaceKHR surf) : instance(inst), surface(surf) { pickPhysicalDevice(); }

PhysicalDevice::~PhysicalDevice() {
	// nothing to destroy; VkPhysicalDevice is "borrowed"
}

void PhysicalDevice::pickPhysicalDevice() {
	uint32_t count = 0;
	vkEnumeratePhysicalDevices(instance, &count, nullptr);
	if (count == 0) {
		throw std::runtime_error("No Vulkan-capable GPU found");
	}

	std::vector<VkPhysicalDevice> devs(count);
	vkEnumeratePhysicalDevices(instance, &count, devs.data());

	VkPhysicalDevice best = VK_NULL_HANDLE;
	for (auto d : devs) {
		VkPhysicalDeviceProperties props{};
		vkGetPhysicalDeviceProperties(d, &props);

		// Prefer discrete GPU
		if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			best = d;
			break;
		}
	}

	if (best == VK_NULL_HANDLE) {
		for (VkPhysicalDevice d : devs) {
			if (isDeviceSuitable(d)) {
				physicalDevice = d;
				break;
			}
		}
	} else {
        physicalDevice = best;
    }

	if (physicalDevice == VK_NULL_HANDLE) {
		throw std::runtime_error("No suitable GPU found");
	}

	families = findQueueFamilies(physicalDevice);
}

bool PhysicalDevice::checkDeviceExtensionSupport(VkPhysicalDevice dev) const {
	uint32_t extCount = 0;
	vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, nullptr);

	std::vector<VkExtensionProperties> exts(extCount);
	vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, exts.data());

	std::set<std::string> required(requiredDeviceExtensions.begin(), requiredDeviceExtensions.end());
	for (auto &e : exts) {
		required.erase(e.extensionName);
	}
	return required.empty();
}

QueueFamilyIndices PhysicalDevice::findQueueFamilies(VkPhysicalDevice dev) const {
	QueueFamilyIndices out{};

	uint32_t familyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(dev, &familyCount, nullptr);

	std::vector<VkQueueFamilyProperties> props(familyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(dev, &familyCount, props.data());

	for (uint32_t i = 0; i < familyCount; ++i) {
		if (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			out.graphicsAndComputeFamily = i;
		}

		VkBool32 presentSupport = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &presentSupport);
		if (presentSupport) {
			out.presentFamily = i;
		}

		if (out.isComplete())
			break;
	}

	return out;
}

SwapchainSupportDetails PhysicalDevice::querySwapchainSupport(VkPhysicalDevice dev) const {
	SwapchainSupportDetails s{};

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, surface, &s.capabilities);

	uint32_t fmtCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &fmtCount, nullptr);
	if (fmtCount) {
		s.formats.resize(fmtCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &fmtCount, s.formats.data());
	}

	uint32_t pmCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &pmCount, nullptr);
	if (pmCount) {
		s.presentModes.resize(pmCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &pmCount, s.presentModes.data());
	}

	return s;
}

bool PhysicalDevice::isDeviceSuitable(VkPhysicalDevice dev) const {
	QueueFamilyIndices f = findQueueFamilies(dev);
	if (!f.isComplete())
		return false;

	bool extOK = checkDeviceExtensionSupport(dev);
	if (!extOK)
		return false;

	auto sup = querySwapchainSupport(dev);
	bool swapOK = !sup.formats.empty() && !sup.presentModes.empty();
	if (!swapOK)
		return false;

	VkPhysicalDeviceFeatures feats{};
	vkGetPhysicalDeviceFeatures(dev, &feats);
	if (feats.samplerAnisotropy != VK_TRUE)
		return false;

	return true;
}
