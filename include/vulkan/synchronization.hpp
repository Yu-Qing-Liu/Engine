#pragma once

#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>

class Synchronization {
  public:
	Synchronization() = default;
	~Synchronization();

	void create(VkDevice device, uint32_t frameOverlap, uint32_t swapImageCount);
	void destroy();

	VkSemaphore imageAvailable(uint32_t frame) const { return imgAvailable[frame]; }
	VkSemaphore renderFinished(uint32_t frame) const { return renderFinishedSem[frame]; }
	VkSemaphore computeFinished(uint32_t frame) const { return computeFinishedSem[frame]; }
	VkFence inFlightFence(uint32_t frame) const { return inFlightFences[frame]; }
	VkFence computeFence(uint32_t frame) const { return computeFences[frame]; }

	VkSemaphore renderFinishedForImage(uint32_t imageIndex) const { return renderFinishedPerImage[imageIndex]; }

  private:
	VkDevice device = VK_NULL_HANDLE;

	std::vector<VkSemaphore> imgAvailable;
	std::vector<VkSemaphore> renderFinishedSem;
	std::vector<VkSemaphore> computeFinishedSem;
	std::vector<VkSemaphore> renderFinishedPerImage;
	std::vector<VkFence> inFlightFences;
	std::vector<VkFence> computeFences;
};
