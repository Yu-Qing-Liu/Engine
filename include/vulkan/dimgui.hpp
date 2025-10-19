#pragma once

#include <stdexcept>
#include <vulkan/vulkan_core.h>

#if !ANDROID_VK
#include <GLFW/glfw3.h>
#endif

// Dear ImGui
#include "imgui.h"
#if !ANDROID_VK
#include "imgui_impl_glfw.h"
#endif
#include "imgui_impl_vulkan.h"

namespace DImGui {

// -----------------------------
// Persistent backend state
// -----------------------------
inline bool imguiInitialized = false;
inline VkDescriptorPool imguiDescriptorPool = VK_NULL_HANDLE;

// Saved so we can re-init the Vulkan backend on swapchain/render-pass rebuilds
inline GLFWwindow *sWindow = nullptr;
inline VkInstance sInstance = VK_NULL_HANDLE;
inline VkPhysicalDevice sPhysicalDevice = VK_NULL_HANDLE;
inline VkDevice sDevice = VK_NULL_HANDLE;
inline uint32_t sGraphicsQueueFamily = 0;
inline VkQueue sGraphicsQueue = VK_NULL_HANDLE;
inline VkRenderPass sUiRenderPass = VK_NULL_HANDLE;
inline uint32_t sImageCount = 0;
inline uint32_t sMinImageCount = 0;
inline VkSampleCountFlagBits sMsaaSamples = VK_SAMPLE_COUNT_1_BIT;
inline VkPipelineCache sPipelineCache = VK_NULL_HANDLE;

// -----------------------------------------
// A big, general-purpose descriptor pool
// -----------------------------------------
inline void createDescriptorPool(VkDevice device) {
	if (imguiDescriptorPool != VK_NULL_HANDLE)
		return;

	const VkDescriptorPoolSize pool_sizes[] = {
		{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},		   {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000}, {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},			 {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},			{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000}, {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},		  {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000}, {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000},
	};

	VkDescriptorPoolCreateInfo pool_info{};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000u * (uint32_t)(sizeof(pool_sizes) / sizeof(pool_sizes[0]));
	pool_info.poolSizeCount = (uint32_t)(sizeof(pool_sizes) / sizeof(pool_sizes[0]));
	pool_info.pPoolSizes = pool_sizes;

	if (vkCreateDescriptorPool(device, &pool_info, nullptr, &imguiDescriptorPool) != VK_SUCCESS)
		throw std::runtime_error("ImGui: failed to create descriptor pool");
}

// ---------------------------------------------------------
// Initialize ImGui (context + GLFW platform + Vulkan RD)
// ---------------------------------------------------------
inline void setup(GLFWwindow *window, VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, uint32_t graphicsQueueFamily, VkQueue graphicsQueue,
				  VkRenderPass uiRenderPass, // your UI render pass
				  uint32_t imageCount,		 // swapchain images
				  uint32_t minImageCount,	 // usually >= 2
				  VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT, VkPipelineCache pipelineCache = VK_NULL_HANDLE) {
	if (imguiInitialized)
		return;

	// Save for rebuilds
	sWindow = window;
	sInstance = instance;
	sPhysicalDevice = physicalDevice;
	sDevice = device;
	sGraphicsQueueFamily = graphicsQueueFamily;
	sGraphicsQueue = graphicsQueue;
	sUiRenderPass = uiRenderPass;
	sImageCount = imageCount;
	sMinImageCount = minImageCount;
	sMsaaSamples = msaaSamples;
	sPipelineCache = pipelineCache;

	// Core context + style
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	(void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	ImGui::StyleColorsDark();

#if !ANDROID_VK
	// Platform backend
	ImGui_ImplGlfw_InitForVulkan(window, false);
#endif

	// Renderer backend
	createDescriptorPool(device);

	ImGui_ImplVulkan_InitInfo init{};
	init.Instance = instance;
	init.PhysicalDevice = physicalDevice;
	init.Device = device;
	init.QueueFamily = graphicsQueueFamily;
	init.Queue = graphicsQueue;
	init.PipelineCache = pipelineCache;
	init.DescriptorPool = imguiDescriptorPool;
	init.Subpass = 0;
	init.MinImageCount = minImageCount;
	init.ImageCount = imageCount;
	init.MSAASamples = msaaSamples;
	init.RenderPass = uiRenderPass; // we use a real RP
	init.UseDynamicRendering = false;

	if (!ImGui_ImplVulkan_Init(&init))
		throw std::runtime_error("ImGui_ImplVulkan_Init failed");

	// Optional (fonts upload now; backend can also lazy-upload on first frame)
	// ImGui_ImplVulkan_CreateFontsTexture(); // since 2023 backend records its own buffer
	imguiInitialized = true;
}

// ----------------------------------------------------
// Must be called when swapchain/render pass changes.
// We re-init the Vulkan renderer backend to the new RP.
// ----------------------------------------------------
inline void onSwapchainRecreated(VkRenderPass newUiRenderPass, uint32_t newImageCount, uint32_t newMinImageCount) {
	IM_ASSERT(imguiInitialized);

	// If only min-image-count changed, we can just set that.
	const bool onlyMinChanged = (newUiRenderPass == sUiRenderPass) && (newImageCount == sImageCount) && (newMinImageCount != sMinImageCount);

	if (onlyMinChanged) {
		ImGui_ImplVulkan_SetMinImageCount(newMinImageCount);
		sMinImageCount = newMinImageCount;
		return;
	}

	// Re-init Vulkan backend if RP or actual image count changed.
	sUiRenderPass = newUiRenderPass;
	sImageCount = newImageCount;
	sMinImageCount = newMinImageCount;

	// NOTE: Keep the ImGui context and GLFW platform backend.
	ImGui_ImplVulkan_Shutdown();

	ImGui_ImplVulkan_InitInfo init{};
	init.Instance = sInstance;
	init.PhysicalDevice = sPhysicalDevice;
	init.Device = sDevice;
	init.QueueFamily = sGraphicsQueueFamily;
	init.Queue = sGraphicsQueue;
	init.PipelineCache = sPipelineCache;
	init.DescriptorPool = imguiDescriptorPool; // keep same pool
	init.Subpass = 0;
	init.MinImageCount = sMinImageCount;
	init.ImageCount = sImageCount;
	init.MSAASamples = sMsaaSamples;
	init.RenderPass = sUiRenderPass;
	init.UseDynamicRendering = false;

	if (!ImGui_ImplVulkan_Init(&init))
		throw std::runtime_error("ImGui_ImplVulkan_Init (re-init) failed");
}

// --------------------------
// Per-frame begin
// --------------------------
inline void newFrame() {
#if !ANDROID_VK
	ImGui_ImplGlfw_NewFrame();
#endif
	ImGui_ImplVulkan_NewFrame(); // <- IMPORTANT (was missing)
	ImGui::NewFrame();
}

// -------------------------------------------------
// Record draw data into the current UI subpass
// -------------------------------------------------
inline void recordDraw(VkCommandBuffer cmd, VkPipeline pipeline = VK_NULL_HANDLE) {
	ImGui::Render();
	ImDrawData *dd = ImGui::GetDrawData();
	ImGui_ImplVulkan_RenderDrawData(dd, cmd, pipeline); // pipeline can be VK_NULL_HANDLE
}

// ---------------
// Shutdown
// ---------------
inline void shutdown(VkDevice device) {
	if (!imguiInitialized)
		return;

	ImGui_ImplVulkan_Shutdown();
#if !ANDROID_VK
	ImGui_ImplGlfw_Shutdown();
#endif
	if (imguiDescriptorPool) {
		vkDestroyDescriptorPool(device, imguiDescriptorPool, nullptr);
		imguiDescriptorPool = VK_NULL_HANDLE;
	}
	ImGui::DestroyContext();
	imguiInitialized = false;

	// Clear saved state
	sWindow = nullptr;
	sInstance = VK_NULL_HANDLE;
	sPhysicalDevice = VK_NULL_HANDLE;
	sDevice = VK_NULL_HANDLE;
	sGraphicsQueueFamily = 0;
	sGraphicsQueue = VK_NULL_HANDLE;
	sUiRenderPass = VK_NULL_HANDLE;
	sImageCount = sMinImageCount = 0;
	sMsaaSamples = VK_SAMPLE_COUNT_1_BIT;
	sPipelineCache = VK_NULL_HANDLE;
}

} // namespace DImGui
