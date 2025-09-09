#include <android/input.h> // <<< ADD
#include <android/log.h>
#include <android/looper.h>
#include <android_native_app_glue.h>
#include <chrono>
#include <unistd.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>

#include "assets.hpp"
#include "engine.hpp"
#include "events.hpp"
#include "pipeline.hpp"
#include "scenes.hpp"

using namespace Engine;
using namespace Pipeline;

static const char *TAG = "VulkanApp";

static inline void vk_check(VkResult r, const char *what) {
	if (r != VK_SUCCESS) {
		__android_log_print(ANDROID_LOG_ERROR, TAG, "Vulkan error %d: %s", r, what);
		std::abort();
	}
}

// <<< ADD: define the global used by platform.hpp (NativeWin / files path helpers)
android_app *g_app = nullptr;

struct AppState {
	bool animating = false;
	bool ready = false; // swapchain available
	std::unique_ptr<Scenes> scenes;
};

// --- Record commands just like your desktop code does ---
static void recordComputeCommandBuffer(VkCommandBuffer commandBuffer, AppState &S) {
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	vk_check(vkBeginCommandBuffer(commandBuffer, &beginInfo), "begin compute cmd");

	S.scenes->computePass();

	vk_check(vkEndCommandBuffer(commandBuffer), "end compute cmd");
}

static void recordGraphicsCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, AppState &S) {
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	vk_check(vkBeginCommandBuffer(commandBuffer, &beginInfo), "begin gfx cmd");

	VkRenderPassBeginInfo rp{};
	rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rp.renderPass = renderPass;
	rp.framebuffer = swapChainFramebuffers[imageIndex];
	rp.renderArea.offset = {0, 0};
	rp.renderArea.extent = swapChainExtent;

	std::array<VkClearValue, 2> clearValues{};
	clearValues[0].color = {{0.f, 0.f, 0.f, 1.f}};
	clearValues[1].depthStencil = {1.f, 0};
	rp.clearValueCount = (uint32_t)clearValues.size();
	rp.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(commandBuffer, &rp, VK_SUBPASS_CONTENTS_INLINE);

	S.scenes->renderPass();

	vkCmdEndRenderPass(commandBuffer);
	vk_check(vkEndCommandBuffer(commandBuffer), "end gfx cmd");
}

// --- One frame (compute -> graphics -> present), mirrors your desktop drawFrame() ---
static bool drawFrameOnce(AppState &S) {
	// Compute submission
	vk_check(vkWaitForFences(device, 1, &computeInFlightFences[currentFrame], VK_TRUE, UINT64_MAX), "wait compute fence");

	S.scenes->updateComputeUniformBuffers();

	vk_check(vkResetFences(device, 1, &computeInFlightFences[currentFrame]), "reset compute fence");
	vk_check(vkResetCommandBuffer(computeCommandBuffers[currentFrame], 0), "reset compute cb");
	recordComputeCommandBuffer(computeCommandBuffers[currentFrame], S);

	VkSubmitInfo cSubmit{};
	cSubmit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	cSubmit.commandBufferCount = 1;
	cSubmit.pCommandBuffers = &computeCommandBuffers[currentFrame];
	cSubmit.signalSemaphoreCount = 1;
	cSubmit.pSignalSemaphores = &computeFinishedSemaphores[currentFrame];

	vk_check(vkQueueSubmit(computeQueue, 1, &cSubmit, computeInFlightFences[currentFrame]), "submit compute");

	// Graphics acquire
	vk_check(vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX), "wait gfx fence");

	uint32_t imageIndex = 0;
	VkResult acq = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

	if (acq == VK_ERROR_OUT_OF_DATE_KHR) {
		recreateSwapChain();
		S.scenes->swapChainUpdate();
		return false;
	}
	if (acq != VK_SUCCESS && acq != VK_SUBOPTIMAL_KHR)
		throw std::runtime_error("failed to acquire swap chain image!");

	if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
		vk_check(vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX), "wait image fence");
	}

	S.scenes->updateUniformBuffers();

	vk_check(vkResetFences(device, 1, &inFlightFences[currentFrame]), "reset gfx fence");
	imagesInFlight[imageIndex] = inFlightFences[currentFrame];

	// Record graphics for this frame
	vk_check(vkResetCommandBuffer(commandBuffers[currentFrame], 0), "reset gfx cb");
	recordGraphicsCommandBuffer(commandBuffers[currentFrame], imageIndex, S);

	// Queue submit for graphics
	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	VkSemaphore waitSems[] = {computeFinishedSemaphores[currentFrame], imageAvailableSemaphores[currentFrame]};

	VkSubmitInfo gSubmit{};
	gSubmit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	gSubmit.waitSemaphoreCount = 2;
	gSubmit.pWaitSemaphores = waitSems;
	gSubmit.pWaitDstStageMask = waitStages;
	gSubmit.commandBufferCount = 1;
	gSubmit.pCommandBuffers = &commandBuffers[currentFrame];

	VkSemaphore sigSems[] = {renderFinishedSemaphoresPerImage[imageIndex]};
	gSubmit.signalSemaphoreCount = 1;
	gSubmit.pSignalSemaphores = sigSems;

	vk_check(vkQueueSubmit(graphicsQueue, 1, &gSubmit, inFlightFences[currentFrame]), "submit gfx");

	// Present (wait on per-image semaphore)
	VkPresentInfoKHR present{};
	present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present.waitSemaphoreCount = 1;
	present.pWaitSemaphores = sigSems;
	present.swapchainCount = 1;
	present.pSwapchains = &swapChain;
	present.pImageIndices = &imageIndex;

	VkResult pres = vkQueuePresentKHR(presentQueue, &present);
	if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR) {
		recreateSwapChain();
		S.scenes->swapChainUpdate();
		return false;
	}
	if (pres != VK_SUCCESS)
		throw std::runtime_error("failed to present swap chain image!");

	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	return true;
}

// --- Create the full Vulkan stack and your scenes (first window ready) ---
static void initVulkanAndScenes(android_app *app, AppState &S) {
	// Point Assets to <files>/... and make sure directories exist
	Assets::initializeAndroid(app);

	// Base timing vars like your desktop init
	lastTime = 0.0;
	startTime = 0.0f;
	Engine::time = 0.0f;

	// Vulkan core
	createInstance();
	setupDebugMessenger();
	createSurface(); // Android path inside Pipeline::createSurface()
	pickPhysicalDevice();
	createLogicalDevice();
	createSwapChain();
	createImageViews();
	createRenderPass();
	createDepthResources();
	createFramebuffers();
	createCommandPool();
	createCommandBuffers();
	createComputeCommandBuffers();
	createSyncObjects();

	currentFrame = 0;

	// Scenes like desktop
	S.scenes = std::make_unique<Scenes>();
}

// --- Free only swapchain + sync on window loss (keep device/instance) ---
static void destroySwapchainOnly() {
	vkDeviceWaitIdle(device);
	cleanupSwapChain();
	cleanupSyncObjects();
}

// --- Free everything on app destroy ---
static void destroyAll(AppState &S) {
	if (device == VK_NULL_HANDLE)
		return;

	vkDeviceWaitIdle(device);

	cleanupSwapChain();
	cleanupSyncObjects();

	if (commandPool) {
		vkDestroyCommandPool(device, commandPool, nullptr);
	}
	commandPool = VK_NULL_HANDLE;

	S.scenes.reset();

	if (renderPass) {
		vkDestroyRenderPass(device, renderPass, nullptr);
		renderPass = VK_NULL_HANDLE;
	}

	vkDestroyDevice(device, nullptr);
	device = VK_NULL_HANDLE;

	if (surface) {
		vkDestroySurfaceKHR(instance, surface, nullptr);
		surface = VK_NULL_HANDLE;
	}

	if (enableValidationLayers && debugMessenger) {
		DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
		debugMessenger = VK_NULL_HANDLE;
	}

	if (instance) {
		vkDestroyInstance(instance, nullptr);
		instance = VK_NULL_HANDLE;
	}
}

// --- Android lifecycle commands ---
static void handle_cmd(android_app *app, int32_t cmd) {
	auto *S = reinterpret_cast<AppState *>(app->userData);

	switch (cmd) {
	case APP_CMD_INIT_WINDOW:
		if (app->window) {
			if (instance == VK_NULL_HANDLE) {
				initVulkanAndScenes(app, *S);
			} else {
				createSurface();
				createSwapChain();
				createImageViews();
				createDepthResources();
				createFramebuffers();
				createSyncObjects();
				currentFrame = 0;
				if (S->scenes)
					S->scenes->swapChainUpdate();
			}
			S->ready = true;
			S->animating = true;
		}
		break;

	case APP_CMD_TERM_WINDOW:
		S->animating = false;
		S->ready = false;
		destroySwapchainOnly();
		if (surface) {
			vkDestroySurfaceKHR(instance, surface, nullptr);
			surface = VK_NULL_HANDLE;
		}
		break;

	case APP_CMD_GAINED_FOCUS:
		S->animating = true;
		break;

	case APP_CMD_LOST_FOCUS:
		S->animating = false;
		break;

	case APP_CMD_CONFIG_CHANGED:
	case APP_CMD_WINDOW_RESIZED:
		if (S->ready && surface != VK_NULL_HANDLE) {
			recreateSwapChain();
			if (S->scenes)
				S->scenes->swapChainUpdate();
		}
		break;

	default:
		break;
	}
}

// --- Entry point for NativeActivity ---
void android_main(android_app *app) {
	app_dummy();

	AppState state{};
	app->userData = &state;
	app->onAppCmd = handle_cmd;
	app->onInputEvent = Events::handleAndroidInput; // <<< ADD: hook input so pointer is updated
	g_app = app;									// <<< ADD: set global for platform helpers

	using clock = std::chrono::steady_clock;
	auto t0 = clock::now();

	// Event/render loop
	while (true) {
		int events;
		android_poll_source *source;

		int timeout = (state.animating ? 0 : -1);
		int id = 0;
		while ((id = ALooper_pollOnce(timeout, nullptr, &events, (void **)&source)) >= 0) {
			if (source)
				source->process(app, source);
			if (app->destroyRequested) {
				destroyAll(state);
				return;
			}
			timeout = 0;
		}

		if (state.animating && state.ready && device != VK_NULL_HANDLE && surface != VK_NULL_HANDLE) {
			auto now = clock::now();
			double t = std::chrono::duration<double>(now - t0).count();
			lastFrameTime = float((t - lastTime) * 1000.0);
			Engine::time = float(t);
			lastTime = t;

			try {
				drawFrameOnce(state);
			} catch (const std::exception &e) {
				__android_log_print(ANDROID_LOG_ERROR, TAG, "drawFrame exception: %s", e.what());
			}
		} else {
			usleep(5 * 1000);
		}
	}
}
