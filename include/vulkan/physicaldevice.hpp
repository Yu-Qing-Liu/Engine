#pragma once

#include <optional>
#include <vector>
#include <vulkan/vulkan.h>

struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsAndComputeFamily;
	std::optional<uint32_t> presentFamily;
	bool isComplete() const { return graphicsAndComputeFamily.has_value() && presentFamily.has_value(); }
};

struct SwapchainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

static const std::vector<const char *> requiredDeviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	// dynamic rendering and sync2 are core in 1.3, but older (1.2) drivers may expose them as extensions.
};

class PhysicalDevice {
  public:
	PhysicalDevice(VkInstance instance, VkSurfaceKHR surface);
	~PhysicalDevice();

	VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }
	const QueueFamilyIndices &getQueueFamilies() const { return families; }

	SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice dev) const;

  private:
	VkInstance instance = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	QueueFamilyIndices families{};

	void pickPhysicalDevice();
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice dev) const;
	bool isDeviceSuitable(VkPhysicalDevice dev) const;
	bool checkDeviceExtensionSupport(VkPhysicalDevice dev) const;
};
