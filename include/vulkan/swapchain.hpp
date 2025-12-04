#pragma once

#include "physicaldevice.hpp"
#include <GLFW/glfw3.h>
#include <vector>
#include <vulkan/vulkan.h>

class Swapchain {
  public:
	Swapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, const QueueFamilyIndices &families, GLFWwindow* window);
	~Swapchain();

	VkSwapchainKHR getHandle() const { return swapchain; }
	VkFormat getImageFormat() const { return imageFormat; }
	VkExtent2D getExtent() const { return extent; }

	const std::vector<VkImage> &getImages() const { return images; }
	const std::vector<VkImageView> &getImageViews() const { return imageViews; }

	void recreate(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, const QueueFamilyIndices &families, GLFWwindow *window);

  private:
	VkDevice device = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;

	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	VkFormat imageFormat = VK_FORMAT_UNDEFINED;
	VkExtent2D extent{};

	std::vector<VkImage> images;
	std::vector<VkImageView> imageViews;

	void createInternal(VkPhysicalDevice physicalDevice, const QueueFamilyIndices &families, GLFWwindow *window);

	void destroyInternal();

	SwapchainSupportDetails querySupport(VkPhysicalDevice dev) const;
	VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &) const;
	VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR> &) const;
	VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR &, GLFWwindow *window) const;
	void createImageViews();
};
