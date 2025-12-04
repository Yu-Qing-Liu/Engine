#pragma once

#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>

class CommandBuffers {
  public:
	CommandBuffers() = default;
	~CommandBuffers();

	void create(VkDevice device, uint32_t graphicsQueueFamily, uint32_t frameOverlap);
	void destroy();

	VkCommandPool getPool() const { return commandPool; }
	VkCommandBuffer getGraphicsCmd(uint32_t idx) const { return graphicsCmd[idx]; }
	VkCommandBuffer getComputeCmd(uint32_t idx) const { return computeCmd[idx]; }

	VkCommandBuffer beginSingleTime(VkDevice device);
	void endSingleTime(VkDevice device, VkQueue queue, VkCommandBuffer cmd);

  private:
	VkDevice device = VK_NULL_HANDLE;
	VkCommandPool commandPool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> graphicsCmd;
	std::vector<VkCommandBuffer> computeCmd;
};
