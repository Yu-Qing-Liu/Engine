#include "commandbuffers.hpp"
#include <stdexcept>

CommandBuffers::~CommandBuffers() { destroy(); }

void CommandBuffers::create(VkDevice devIn, uint32_t graphicsQueueFamily, uint32_t frameOverlap) {
	device = devIn;

	VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
	pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pci.queueFamilyIndex = graphicsQueueFamily;

	if (vkCreateCommandPool(device, &pci, nullptr, &commandPool) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create command pool");
	}

	graphicsCmd.resize(frameOverlap);
	computeCmd.resize(frameOverlap);

	VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
	cai.commandPool = commandPool;
	cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cai.commandBufferCount = frameOverlap;

	if (vkAllocateCommandBuffers(device, &cai, graphicsCmd.data()) != VK_SUCCESS) {
		throw std::runtime_error("Failed to alloc graphics command buffers");
	}

	if (vkAllocateCommandBuffers(device, &cai, computeCmd.data()) != VK_SUCCESS) {
		throw std::runtime_error("Failed to alloc compute command buffers");
	}
}

void CommandBuffers::destroy() {
	if (device == VK_NULL_HANDLE)
		return;

	if (!graphicsCmd.empty()) {
		vkFreeCommandBuffers(device, commandPool, (uint32_t)graphicsCmd.size(), graphicsCmd.data());
		graphicsCmd.clear();
	}

	if (!computeCmd.empty()) {
		vkFreeCommandBuffers(device, commandPool, (uint32_t)computeCmd.size(), computeCmd.data());
		computeCmd.clear();
	}

	if (commandPool != VK_NULL_HANDLE) {
		vkDestroyCommandPool(device, commandPool, nullptr);
		commandPool = VK_NULL_HANDLE;
	}

	device = VK_NULL_HANDLE;
}

VkCommandBuffer CommandBuffers::beginSingleTime(VkDevice devIn) {
	// You must have already created commandPool from the same device.
	VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
	ai.commandPool = commandPool;
	ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	ai.commandBufferCount = 1;

	VkCommandBuffer cmd = VK_NULL_HANDLE;
	if (vkAllocateCommandBuffers(devIn, &ai, &cmd) != VK_SUCCESS) {
		throw std::runtime_error("Failed to alloc single-time cmd buffer");
	}

	VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &bi);

	return cmd;
}

void CommandBuffers::endSingleTime(VkDevice devIn, VkQueue queue, VkCommandBuffer cmd) {
	vkEndCommandBuffer(cmd);

	VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
	si.commandBufferCount = 1;
	si.pCommandBuffers = &cmd;

	vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE);
	vkQueueWaitIdle(queue);

	vkFreeCommandBuffers(devIn, commandPool, 1, &cmd);
}
