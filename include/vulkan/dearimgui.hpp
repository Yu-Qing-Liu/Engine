#pragma once

#include <vulkan/vulkan.h>
struct GLFWwindow;

class DearImGui {
  public:
	DearImGui() = default;
	~DearImGui();

	void init(GLFWwindow *window, VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, uint32_t graphicsQueueFamily, VkQueue graphicsQueue, VkFormat swapchainFormat, uint32_t imageCount, uint32_t minImageCount);

	void onSwapchainRecreated(VkFormat newSwapchainFormat, uint32_t newImageCount, uint32_t newMinImageCount);

	void newFrame();
	void recordDraw(VkCommandBuffer cmd); // ImGui_ImplVulkan_RenderDrawData
	void shutdown();

  private:
	bool initialized = false;

	GLFWwindow *sWindow = nullptr;
	VkInstance sInstance = VK_NULL_HANDLE;
	VkPhysicalDevice sPhysicalDevice = VK_NULL_HANDLE;
	VkDevice sDevice = VK_NULL_HANDLE;
	uint32_t sGraphicsQueueFamily = 0;
	VkQueue sGraphicsQueue = VK_NULL_HANDLE;
	VkFormat sSwapchainFormat = VK_FORMAT_UNDEFINED;
	uint32_t sImageCount = 0;
	uint32_t sMinImageCount = 0;

	VkDescriptorPool imguiDescriptorPool = VK_NULL_HANDLE;

	void createDescriptorPool();
};
