#pragma once

#include "debug.hpp"

#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace Memory {

inline static uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t typeBits, VkMemoryPropertyFlags required) {
	VkPhysicalDeviceMemoryProperties mp{};
	vkGetPhysicalDeviceMemoryProperties(phys, &mp);
	for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
		const bool typeOk = (typeBits & (1u << i)) != 0;
		const bool flagsOk = (mp.memoryTypes[i].propertyFlags & required) == required;
		if (typeOk && flagsOk)
			return i;
	}
	throw std::runtime_error("No suitable memory type");
}

} // namespace Memory
