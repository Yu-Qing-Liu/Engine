#include "swapchain.hpp"
#include <algorithm>
#include <stdexcept>

Swapchain::Swapchain(VkPhysicalDevice phys, VkDevice devIn, VkSurfaceKHR surf, const QueueFamilyIndices &fam, GLFWwindow *window) : device(devIn), surface(surf) { createInternal(phys, fam, window); }

Swapchain::~Swapchain() { destroyInternal(); }

SwapchainSupportDetails Swapchain::querySupport(VkPhysicalDevice dev) const {
	SwapchainSupportDetails d{};
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, surface, &d.capabilities);

	uint32_t fmtCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &fmtCount, nullptr);
	if (fmtCount) {
		d.formats.resize(fmtCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &fmtCount, d.formats.data());
	}

	uint32_t pmCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &pmCount, nullptr);
	if (pmCount) {
		d.presentModes.resize(pmCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &pmCount, d.presentModes.data());
	}

	return d;
}

VkSurfaceFormatKHR Swapchain::chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &fmts) const {
	for (auto &f : fmts) {
		// Common SRGB swapchain format on desktop
		if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return f;
		}
	}
	return fmts[0];
}

VkPresentModeKHR Swapchain::choosePresentMode(const std::vector<VkPresentModeKHR> &modes) const {
	// Hard vsync: always use FIFO if available, otherwise fall back to first mode.
	for (auto &m : modes) {
		if (m == VK_PRESENT_MODE_FIFO_KHR)
			return m;
	}
	// Spec guarantees FIFO is supported, but just in case:
	return modes.empty() ? VK_PRESENT_MODE_FIFO_KHR : modes[0];
}

static VkExtent2D clampExtentToCaps(const VkSurfaceCapabilitiesKHR &caps, VkExtent2D desired) {
	VkExtent2D actual = desired;
	actual.width = std::max(caps.minImageExtent.width, std::min(caps.maxImageExtent.width, actual.width));
	actual.height = std::max(caps.minImageExtent.height, std::min(caps.maxImageExtent.height, actual.height));
	return actual;
}

VkExtent2D Swapchain::chooseExtent(const VkSurfaceCapabilitiesKHR &caps, GLFWwindow *window) const {
	if (caps.currentExtent.width != 0xFFFFFFFFu) {
		// The window system has already decided — just use it.
		return caps.currentExtent;
	}

	// Vulkan wants YOU to pick the size.
	int width, height;
	glfwGetFramebufferSize(window, &width, &height);

	VkExtent2D desired = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

	return clampExtentToCaps(caps, desired);
}

void Swapchain::createInternal(VkPhysicalDevice phys, const QueueFamilyIndices &fam, GLFWwindow *window) {
	SwapchainSupportDetails support = querySupport(phys);
	if (support.formats.empty() || support.presentModes.empty()) {
		throw std::runtime_error("Swapchain support incomplete");
	}

	VkSurfaceFormatKHR surfFmt = chooseSurfaceFormat(support.formats);
	VkPresentModeKHR present = choosePresentMode(support.presentModes);
	VkExtent2D ext = chooseExtent(support.capabilities, window);

	uint32_t imageCount = support.capabilities.minImageCount + 1;
	if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount) {
		imageCount = support.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
	ci.surface = surface;
	ci.minImageCount = imageCount;
	ci.imageFormat = surfFmt.format;
	ci.imageColorSpace = surfFmt.colorSpace;
	ci.imageExtent = ext;
	ci.imageArrayLayers = 1;
	ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	uint32_t qIdx[2] = {fam.graphicsAndComputeFamily.value(), fam.presentFamily.value()};

	if (fam.graphicsAndComputeFamily != fam.presentFamily) {
		ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		ci.queueFamilyIndexCount = 2;
		ci.pQueueFamilyIndices = qIdx;
	} else {
		ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	ci.preTransform = support.capabilities.currentTransform;
	ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	ci.presentMode = present;
	ci.clipped = VK_TRUE;

	if (vkCreateSwapchainKHR(device, &ci, nullptr, &swapchain) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create swapchain");
	}

	uint32_t actualCount = 0;
	vkGetSwapchainImagesKHR(device, swapchain, &actualCount, nullptr);
	images.resize(actualCount);
	vkGetSwapchainImagesKHR(device, swapchain, &actualCount, images.data());

	imageFormat = surfFmt.format;
	extent = ext;

	createImageViews();
}

void Swapchain::createImageViews() {
	imageViews.resize(images.size());
	for (size_t i = 0; i < images.size(); ++i) {
		VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
		vi.image = images[i];
		vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
		vi.format = imageFormat;
		vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		vi.subresourceRange.baseMipLevel = 0;
		vi.subresourceRange.levelCount = 1;
		vi.subresourceRange.baseArrayLayer = 0;
		vi.subresourceRange.layerCount = 1;

		if (vkCreateImageView(device, &vi, nullptr, &imageViews[i]) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create swapchain image view");
		}
	}
}

void Swapchain::destroyInternal() {
	if (device == VK_NULL_HANDLE)
		return;

	for (VkImageView v : imageViews) {
		if (v)
			vkDestroyImageView(device, v, nullptr);
	}
	imageViews.clear();

	if (swapchain != VK_NULL_HANDLE) {
		vkDestroySwapchainKHR(device, swapchain, nullptr);
		swapchain = VK_NULL_HANDLE;
	}
}

void Swapchain::recreate(VkPhysicalDevice phys, VkDevice devIn, VkSurfaceKHR surf, const QueueFamilyIndices &fam, GLFWwindow *window) {
	// NOTE: caller should vkDeviceWaitIdle() first.
	destroyInternal();
	device = devIn;
	surface = surf;
	createInternal(phys, fam, window);
}
