#pragma once

#include "physicaldevice.hpp"
#include <vector>
#include <vulkan/vulkan.h>

class LogicalDevice {
  public:
	LogicalDevice(VkPhysicalDevice physicalDevice, const QueueFamilyIndices &families, const std::vector<const char *> &deviceExtensions, bool enableValidation);
	~LogicalDevice();

	VkDevice getDevice() const { return device; }
	VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }

	VkQueue getGraphicsQueue() const { return graphicsQueue; }
	VkQueue getComputeQueue() const { return computeQueue; }
	VkQueue getPresentQueue() const { return presentQueue; }

	uint32_t getGraphicsQueueFamily() const { return qGraphics; }
	uint32_t getPresentQueueFamily() const { return qPresent; }

	VkCommandBuffer beginSingleUseCmd() const;
	void endSingleUseCmdGraphics(VkCommandBuffer cmd) const;

  private:
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE; // borrowed
	VkDevice device = VK_NULL_HANDLE;

	VkQueue graphicsQueue = VK_NULL_HANDLE;
	VkQueue computeQueue = VK_NULL_HANDLE;
	VkQueue presentQueue = VK_NULL_HANDLE;

	uint32_t qGraphics = 0;
	uint32_t qPresent = 0;

	VkCommandPool uploadCmdPool = VK_NULL_HANDLE;

	void createLogicalDevice(const QueueFamilyIndices &families, const std::vector<const char *> &deviceExtensions, bool enableValidation);
};
