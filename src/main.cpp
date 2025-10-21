#include "appdata.hpp"
#include "dimgui.hpp"
#include "engine.hpp"
#include "events.hpp"
#include "inventoryqueries.hpp"
#include "pipeline.hpp"
#include "recipesqueries.hpp"
#include "scenes.hpp"
#include "text.hpp"

#include <GLFW/glfw3.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan_core.h>

using namespace Engine;
using namespace Pipeline;

class Application {
  public:
	void run() {
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

  private:
	bool framebufferResized = false;

	std::unique_ptr<Scenes> scenes;

	void initWindow() {
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
		glfwSetWindowUserPointer(window, this);
		glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

		lastTime = glfwGetTime();
		startTime = glfwGetTime();

		glfwMakeContextCurrent(window);

		glfwSetInputMode(window, GLFW_STICKY_KEYS, GLFW_FALSE);
		glfwSetInputMode(window, GLFW_REPEAT, GLFW_TRUE);

		glfwSetMouseButtonCallback(window, Events::handleMouseCallbacks);
		glfwSetKeyCallback(window, Events::handleKeyboardCallbacks);
		glfwSetCharCallback(window, Events::handleCharacterInputCallbacks);
		glfwSetWindowFocusCallback(window, Events::handleWindowFocusedCallbacks);
		glfwSetScrollCallback(window, Events::handleScrollCallbacks);

		glfwSetCursorPosCallback(window, [](GLFWwindow *, double x, double y) {
			Events::pointerX = (float)x;
			Events::pointerY = (float)y;
			Events::dispatchCursorCallback(x, y);
		});
	}

	static void framebufferResizeCallback(GLFWwindow *window, int width, int height) {
		auto app = reinterpret_cast<Application *>(glfwGetWindowUserPointer(window));
		app->framebufferResized = true;
	}

	void initVulkan() {
		Pipeline::createInstance();
		Pipeline::setupDebugMessenger();
		Pipeline::createSurface();
		Pipeline::pickPhysicalDevice();
		Pipeline::createLogicalDevice();
		Pipeline::createSwapChain();
		Pipeline::createRenderPasses();
		Pipeline::createSwapchainDependent();
		Pipeline::createCommandPool();

		Pipeline::createCommandBuffers();
		Pipeline::createComputeCommandBuffers();
		Pipeline::createSyncObjects();

		DImGui::setup(window, instance, physicalDevice, device, Engine::graphicsQueueFamilyIndex, graphicsQueue, renderPass1, (uint32_t)swapChainImages.size(), MAX_FRAMES_IN_FLIGHT, VK_SAMPLE_COUNT_1_BIT, VK_NULL_HANDLE);
		scenes = std::make_unique<Scenes>();
	}

	void mainLoop() {
		double lastTime = glfwGetTime();
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();
			DImGui::newFrame();
			drawFrame();
			double currentTime = glfwGetTime();
			deltaTime = currentTime - lastTime;
			lastFrameTime = (currentTime - lastTime) * 1000.0;
			Engine::time = currentTime - startTime;
			lastTime = currentTime;
		}

		vkDeviceWaitIdle(device);
	}

	void cleanup() {
		Text::Text_ShutdownUploadRings();
		DImGui::shutdown(device);
		Pipeline::cleanupSwapChain();
		vkDestroyRenderPass(device, renderPass, nullptr);
		vkDestroyRenderPass(device, renderPass1, nullptr);

		// Destroy synchronization objects
		Pipeline::cleanupSyncObjects();

		vkDestroyCommandPool(device, commandPool, nullptr);

		scenes.reset();

		vkDestroyDevice(device, nullptr);

		if (enableValidationLayers) {
			Pipeline::DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
		}

		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);

		glfwDestroyWindow(window);

		glfwTerminate();
	}

	void recordComputeCommandBuffer(VkCommandBuffer commandBuffer) {
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
			throw std::runtime_error("failed to begin recording compute command buffer!");
		}

		scenes->computePass();

		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
			throw std::runtime_error("failed to record compute command buffer!");
		}
	}

	void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex) {
		Engine::currentImageIndex = imageIndex;

		VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
		if (vkBeginCommandBuffer(cmd, &bi) != VK_SUCCESS)
			throw std::runtime_error("begin cmd");

		// --- PASS A: Scene (sceneColor + depth) ---
		std::array<VkClearValue, 2> clearA{};
		clearA[0].color = {{0, 0, 0, 0}};
		clearA[1].depthStencil = {1.0f, 0};

		VkRenderPassBeginInfo rpA{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
		rpA.renderPass = renderPass;
		rpA.framebuffer = sceneFramebuffers[imageIndex];
		rpA.renderArea.offset = {0, 0};
		rpA.renderArea.extent = swapChainExtent;
		rpA.clearValueCount = (uint32_t)clearA.size();
		rpA.pClearValues = clearA.data();

		vkCmdBeginRenderPass(cmd, &rpA, VK_SUBPASS_CONTENTS_INLINE);

		scenes->renderPass();
		vkCmdEndRenderPass(cmd);

		// --- Build mip chain for sceneColor[imageIndex] ---
		const uint32_t mips = calcMipLevels(swapChainExtent.width, swapChainExtent.height);
		buildMipsForImage(cmd, sceneColorImages[imageIndex], sceneColorFormat, swapChainExtent.width, swapChainExtent.height, mips);

		// --- PASS B: UI (swapchain), sampling uScene with mips ---
		VkClearValue clearB{};
		clearB.color = {{0, 0, 0, 1}};

		VkRenderPassBeginInfo rpB{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
		rpB.renderPass = renderPass1;
		rpB.framebuffer = uiFramebuffers[imageIndex];
		rpB.renderArea.offset = {0, 0};
		rpB.renderArea.extent = swapChainExtent;
		rpB.clearValueCount = 1;
		rpB.pClearValues = &clearB;

		vkCmdBeginRenderPass(cmd, &rpB, VK_SUBPASS_CONTENTS_INLINE);

		// (the descriptor view is all-mips; sampler uses mipmapMode=LINEAR)
		scenes->renderPass1();

		DImGui::recordDraw(cmd /*, VK_NULL_HANDLE*/);

		vkCmdEndRenderPass(cmd);

		if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
			throw std::runtime_error("end cmd");
	}

	void drawFrame() {
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		// Compute submission
		vkWaitForFences(device, 1, &computeInFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

		scenes->updateComputeUniformBuffers();

		vkResetFences(device, 1, &computeInFlightFences[currentFrame]);

		vkResetCommandBuffer(computeCommandBuffers[currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
		recordComputeCommandBuffer(computeCommandBuffers[currentFrame]);

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &computeCommandBuffers[currentFrame];
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &computeFinishedSemaphores[currentFrame];

		if (vkQueueSubmit(computeQueue, 1, &submitInfo, computeInFlightFences[currentFrame]) != VK_SUCCESS) {
			throw std::runtime_error("failed to submit compute command buffer!");
		};

		// Graphics submission
		vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			Pipeline::recreateSwapChain();
			scenes->swapChainUpdate();
			DImGui::onSwapchainRecreated(renderPass1, (uint32_t)swapChainImages.size(), MAX_FRAMES_IN_FLIGHT);
			return;
		} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			throw std::runtime_error("failed to acquire swap chain image!");
		}

		if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
			vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
		}

		scenes->updateUniformBuffers();

		vkResetFences(device, 1, &inFlightFences[currentFrame]);
		imagesInFlight[imageIndex] = inFlightFences[currentFrame];

		vkResetCommandBuffer(commandBuffers[currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
		recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

		submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = {computeFinishedSemaphores[currentFrame], imageAvailableSemaphores[currentFrame]};
		VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
		submitInfo.waitSemaphoreCount = 2;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

		VkSemaphore signalSemaphores[] = {renderFinishedSemaphoresPerImage[imageIndex]};
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
			throw std::runtime_error("failed to submit draw command buffer!");
		}

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		// Present must wait on the *per-image* semaphore signaled by the graphics submit
		VkSemaphore presentWaitSemaphores[] = {renderFinishedSemaphoresPerImage[imageIndex]};
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = presentWaitSemaphores;

		VkSwapchainKHR swapChains[] = {swapChain};
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;

		result = vkQueuePresentKHR(presentQueue, &presentInfo);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
			framebufferResized = false;
			Pipeline::recreateSwapChain();
			scenes->swapChainUpdate();
			DImGui::onSwapchainRecreated(renderPass1, (uint32_t)swapChainImages.size(), MAX_FRAMES_IN_FLIGHT);
		} else if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to present swap chain image!");
		}

		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}
};

int main() {
	Assets::initialize();
	AppData::openDB();
	AppData::printTables();
	RecipesQueries::createTable();
	InventoryQueries::createTable();
	Application app;
	try {
		app.run();
	} catch (const std::exception &e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
