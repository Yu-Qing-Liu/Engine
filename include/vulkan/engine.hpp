#pragma once

#include "commandbuffers.hpp"
#include "dearimgui.hpp"
#include "debug.hpp"
#include "graphicsbuffers.hpp"
#include "logicaldevice.hpp"
#include "physicaldevice.hpp"
#include "scenes.hpp"
#include "surface.hpp"
#include "swapchain.hpp"
#include "synchronization.hpp"

#include <cstdint>
#include <memory>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

struct GLFWwindow;

class Engine {
  public:
	Engine() = default;
	~Engine() = default;

	void init(GLFWwindow *window);
	void drawFrame(Scenes &scenes, bool framebufferResizedFlag);
	void recreateSwapchain(Scenes &scenes);
	void beginImGuiFrame();

	VkDevice getDevice() const { return logicalDevice ? logicalDevice->getDevice() : VK_NULL_HANDLE; }
	VkPhysicalDevice getPhysicalDevice() const { return physicalDevice ? physicalDevice->getPhysicalDevice() : VK_NULL_HANDLE; }
	const GraphicsBuffers &getGraphicsBuffer() const { return *graphicsBuffers; }
	const LogicalDevice &getLogicalDevice() const { return *logicalDevice; }
	const Swapchain &getSwapchain() const { return *swapchain; }
	GLFWwindow *getWindow() const { return window; }

  private:
	GLFWwindow *window = nullptr;

	std::unique_ptr<Debug> debug;
	std::unique_ptr<Surface> surface;
	std::unique_ptr<PhysicalDevice> physicalDevice;
	std::unique_ptr<LogicalDevice> logicalDevice;
	std::unique_ptr<Swapchain> swapchain;
	std::unique_ptr<GraphicsBuffers> graphicsBuffers;
	std::unique_ptr<CommandBuffers> commandBuffers;
	std::unique_ptr<Synchronization> synchronization;
	std::unique_ptr<DearImGui> imgui;

	uint32_t currentFrameIndex = 0;
	uint32_t swapImageCount = 2;
	const uint32_t blurLayerCount = 4;
};
