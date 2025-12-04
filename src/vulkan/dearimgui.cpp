#include "dearimgui.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include <stdexcept>

DearImGui::~DearImGui() { shutdown(); }

void DearImGui::createDescriptorPool() {
	if (imguiDescriptorPool != VK_NULL_HANDLE)
		return;

	// Big general-purpose pool like you had before
	VkDescriptorPoolSize pool_sizes[] = {
		{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},		   {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000}, {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},			 {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},			{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000}, {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},		  {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000}, {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000},
	};

	VkDescriptorPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000u * (uint32_t)(sizeof(pool_sizes) / sizeof(pool_sizes[0]));
	pool_info.poolSizeCount = (uint32_t)(sizeof(pool_sizes) / sizeof(pool_sizes[0]));
	pool_info.pPoolSizes = pool_sizes;

	if (vkCreateDescriptorPool(sDevice, &pool_info, nullptr, &imguiDescriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("ImGui: failed to create descriptor pool");
	}
}

void DearImGui::init(GLFWwindow *window, VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, uint32_t graphicsQueueFamily, VkQueue graphicsQueue, VkFormat swapchainFormat, uint32_t imageCount, uint32_t minImageCount) {
	if (initialized)
		return;

	sWindow = window;
	sInstance = instance;
	sPhysicalDevice = physicalDevice;
	sDevice = device;
	sGraphicsQueueFamily = graphicsQueueFamily;
	sGraphicsQueue = graphicsQueue;
	sSwapchainFormat = swapchainFormat;
	sImageCount = imageCount;
	sMinImageCount = minImageCount;

	// Create ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	(void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	ImGui::StyleColorsDark();

	// Platform backend
	ImGui_ImplGlfw_InitForVulkan(window, false);

	// Renderer backend init info
	createDescriptorPool();

	ImGui_ImplVulkan_InitInfo init{};
	init.Instance = sInstance;
	init.PhysicalDevice = sPhysicalDevice;
	init.Device = sDevice;
	init.QueueFamily = sGraphicsQueueFamily;
	init.Queue = sGraphicsQueue;
	init.PipelineCache = VK_NULL_HANDLE;
	init.DescriptorPool = imguiDescriptorPool;
	init.Subpass = 0;
	init.MinImageCount = sMinImageCount;
	init.ImageCount = sImageCount;
	init.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	// Dynamic rendering mode instead of a render pass
	VkPipelineRenderingCreateInfoKHR pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
	pipelineInfo.colorAttachmentCount = 1;
	pipelineInfo.pColorAttachmentFormats = &sSwapchainFormat;
	pipelineInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
	pipelineInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	init.UseDynamicRendering = VK_TRUE;
	init.RenderPass = VK_NULL_HANDLE; // ignored in dynamic mode
	init.PipelineRenderingCreateInfo = pipelineInfo;

	if (!ImGui_ImplVulkan_Init(&init)) {
		throw std::runtime_error("ImGui_ImplVulkan_Init failed");
	}

	initialized = true;
}

void DearImGui::onSwapchainRecreated(VkFormat newSwapchainFormat, uint32_t newImageCount, uint32_t newMinImageCount) {
	if (!initialized)
		return;

	ImGui_ImplVulkan_Shutdown();

	sSwapchainFormat = newSwapchainFormat;
	sImageCount = newImageCount;
	sMinImageCount = newMinImageCount;

	// Recreate backend with updated info
	ImGui_ImplVulkan_InitInfo init{};
	init.Instance = sInstance;
	init.PhysicalDevice = sPhysicalDevice;
	init.Device = sDevice;
	init.QueueFamily = sGraphicsQueueFamily;
	init.Queue = sGraphicsQueue;
	init.PipelineCache = VK_NULL_HANDLE;
	init.DescriptorPool = imguiDescriptorPool; // reuse
	init.Subpass = 0;
	init.MinImageCount = sMinImageCount;
	init.ImageCount = sImageCount;
	init.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineRenderingCreateInfoKHR pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    pipelineInfo.colorAttachmentCount = 1;
    pipelineInfo.pColorAttachmentFormats = &sSwapchainFormat;
    pipelineInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    pipelineInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    init.UseDynamicRendering = VK_TRUE;
    init.RenderPass = VK_NULL_HANDLE;
    init.PipelineRenderingCreateInfo = pipelineInfo;

	if (!ImGui_ImplVulkan_Init(&init)) {
		throw std::runtime_error("ImGui_ImplVulkan_Init (reinit) failed");
	}
}

void DearImGui::newFrame() {
	ImGui_ImplGlfw_NewFrame();
	ImGui_ImplVulkan_NewFrame();
	ImGui::NewFrame();
}

void DearImGui::recordDraw(VkCommandBuffer cmd) {
	ImGui::Render();
	ImDrawData *dd = ImGui::GetDrawData();
	ImGui_ImplVulkan_RenderDrawData(dd, cmd, VK_NULL_HANDLE);
}

void DearImGui::shutdown() {
	if (!initialized)
		return;

	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();

	if (imguiDescriptorPool) {
		vkDestroyDescriptorPool(sDevice, imguiDescriptorPool, nullptr);
		imguiDescriptorPool = VK_NULL_HANDLE;
	}

	ImGui::DestroyContext();
	initialized = false;

	sWindow = nullptr;
	sInstance = VK_NULL_HANDLE;
	sPhysicalDevice = VK_NULL_HANDLE;
	sDevice = VK_NULL_HANDLE;
	sGraphicsQueueFamily = 0;
	sGraphicsQueue = VK_NULL_HANDLE;
	sSwapchainFormat = VK_FORMAT_UNDEFINED;
	sImageCount = 0;
	sMinImageCount = 0;
}
