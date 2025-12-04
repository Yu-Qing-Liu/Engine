#include "synchronization.hpp"
#include <stdexcept>

Synchronization::~Synchronization() { destroy(); }

void Synchronization::create(VkDevice devIn, uint32_t frameOverlap, uint32_t swapImageCount) {
	device = devIn;

	imgAvailable.resize(frameOverlap, VK_NULL_HANDLE);
	renderFinishedSem.resize(frameOverlap, VK_NULL_HANDLE);
	computeFinishedSem.resize(frameOverlap, VK_NULL_HANDLE);
	inFlightFences.resize(frameOverlap, VK_NULL_HANDLE);
	computeFences.resize(frameOverlap, VK_NULL_HANDLE);

	renderFinishedPerImage.resize(swapImageCount, VK_NULL_HANDLE);

	VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
	VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
	fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (uint32_t i = 0; i < frameOverlap; ++i) {
		if (vkCreateSemaphore(device, &sci, nullptr, &imgAvailable[i]) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create imgAvailable semaphore");
		}
		if (vkCreateSemaphore(device, &sci, nullptr, &renderFinishedSem[i]) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create renderFinished semaphore");
		}
		if (vkCreateSemaphore(device, &sci, nullptr, &computeFinishedSem[i]) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create computeFinished semaphore");
		}
		if (vkCreateFence(device, &fci, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create inFlight fence");
		}
		if (vkCreateFence(device, &fci, nullptr, &computeFences[i]) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create compute fence");
		}
	}

	for (uint32_t i = 0; i < swapImageCount; ++i) {
		if (vkCreateSemaphore(device, &sci, nullptr, &renderFinishedPerImage[i]) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create per-image renderFinished semaphore");
		}
	}
}

void Synchronization::destroy() {
	if (device == VK_NULL_HANDLE)
		return;

	for (auto s : imgAvailable)
		if (s)
			vkDestroySemaphore(device, s, nullptr);
	for (auto s : renderFinishedSem)
		if (s)
			vkDestroySemaphore(device, s, nullptr);
	for (auto s : computeFinishedSem)
		if (s)
			vkDestroySemaphore(device, s, nullptr);
	for (auto s : renderFinishedPerImage)
		if (s)
			vkDestroySemaphore(device, s, nullptr);
	for (auto f : inFlightFences)
		if (f)
			vkDestroyFence(device, f, nullptr);
	for (auto f : computeFences)
		if (f)
			vkDestroyFence(device, f, nullptr);

	imgAvailable.clear();
	renderFinishedSem.clear();
	computeFinishedSem.clear();
	renderFinishedPerImage.clear();
	inFlightFences.clear();
	computeFences.clear();

	device = VK_NULL_HANDLE;
}
