#include "logicaldevice.hpp"
#include "debug.hpp"
#include <cstring>

LogicalDevice::LogicalDevice(VkPhysicalDevice phys, const QueueFamilyIndices &fam, const std::vector<const char *> &deviceExtensions, bool enableValidation) : physicalDevice(phys) {
	createLogicalDevice(fam, deviceExtensions, enableValidation);

	qGraphics = fam.graphicsAndComputeFamily.value();
	qPresent = fam.presentFamily.value();

	vkGetDeviceQueue(device, qGraphics, 0, &graphicsQueue);
	vkGetDeviceQueue(device, qGraphics, 0, &computeQueue);
	vkGetDeviceQueue(device, qPresent, 0, &presentQueue);

	VkCommandPoolCreateInfo ci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
	ci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	ci.queueFamilyIndex = qGraphics;
	VK_CHECK(vkCreateCommandPool(device, &ci, nullptr, &uploadCmdPool));
}

LogicalDevice::~LogicalDevice() {
	if (uploadCmdPool) {
		vkDestroyCommandPool(device, uploadCmdPool, nullptr);
		uploadCmdPool = VK_NULL_HANDLE;
	}
	if (device != VK_NULL_HANDLE) {
		vkDeviceWaitIdle(device);
		vkDestroyDevice(device, nullptr);
		device = VK_NULL_HANDLE;
	}
}

void LogicalDevice::createLogicalDevice(const QueueFamilyIndices &fam, const std::vector<const char *> &deviceExtensions, bool enableValidation) {
	// Unique queue families
	std::vector<uint32_t> uniqueFamilies;
	uniqueFamilies.push_back(fam.graphicsAndComputeFamily.value());
	if (fam.presentFamily.value() != fam.graphicsAndComputeFamily.value()) {
		uniqueFamilies.push_back(fam.presentFamily.value());
	}

	float priority = 1.0f;
	std::vector<VkDeviceQueueCreateInfo> qinfos;
	qinfos.reserve(uniqueFamilies.size());
	for (uint32_t qf : uniqueFamilies) {
		VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
		qi.queueFamilyIndex = qf;
		qi.queueCount = 1;
		qi.pQueuePriorities = &priority;
		qinfos.push_back(qi);
	}

	VkPhysicalDeviceFeatures feats{};
	feats.samplerAnisotropy = VK_TRUE;

	// Dynamic rendering
	VkPhysicalDeviceDynamicRenderingFeatures dynFeat{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES};
	dynFeat.dynamicRendering = VK_TRUE;

	// Synchronization2
	VkPhysicalDeviceSynchronization2Features sync2Feat{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES};
	sync2Feat.synchronization2 = VK_TRUE;

	// Descriptor Indexing (use the EXT struct; valid on 1.2/1.3)
	VkPhysicalDeviceDescriptorIndexingFeatures indexingFeat{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES};
	// Enable the bits you plan to use; the “safe defaults” below cover runtime arrays + UAB:
	indexingFeat.runtimeDescriptorArray = VK_TRUE;
	indexingFeat.descriptorBindingPartiallyBound = VK_TRUE;
	indexingFeat.descriptorBindingVariableDescriptorCount = VK_TRUE;
	indexingFeat.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
	indexingFeat.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
	indexingFeat.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
	indexingFeat.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
	indexingFeat.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;
	indexingFeat.descriptorBindingUniformTexelBufferUpdateAfterBind = VK_TRUE;
	indexingFeat.descriptorBindingStorageTexelBufferUpdateAfterBind = VK_TRUE;
	// non-uniform indexing support in shaders:
	indexingFeat.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
	indexingFeat.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
	indexingFeat.shaderStorageImageArrayNonUniformIndexing = VK_TRUE;
	indexingFeat.shaderUniformBufferArrayNonUniformIndexing = VK_TRUE;
	indexingFeat.shaderUniformTexelBufferArrayNonUniformIndexing = VK_TRUE;
	indexingFeat.shaderStorageTexelBufferArrayNonUniformIndexing = VK_TRUE;

	dynFeat.pNext = &sync2Feat;
	sync2Feat.pNext = &indexingFeat;

	VkDeviceCreateInfo ci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
	ci.queueCreateInfoCount = (uint32_t)qinfos.size();
	ci.pQueueCreateInfos = qinfos.data();
	ci.pEnabledFeatures = &feats;
	ci.enabledExtensionCount = (uint32_t)deviceExtensions.size();
	ci.ppEnabledExtensionNames = deviceExtensions.data();

	if (enableValidation) {
		ci.enabledLayerCount = (uint32_t)validationLayers.size();
		ci.ppEnabledLayerNames = validationLayers.data();
	} else {
		ci.enabledLayerCount = 0;
		ci.ppEnabledLayerNames = nullptr;
	}

	// Hook the feature chain here
	ci.pNext = &dynFeat;

	VK_CHECK(vkCreateDevice(physicalDevice, &ci, nullptr, &device));
}

VkCommandBuffer LogicalDevice::beginSingleUseCmd() const {
	VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
	ai.commandPool = uploadCmdPool;
	ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	ai.commandBufferCount = 1;

	VkCommandBuffer cmd{};
	VK_CHECK(vkAllocateCommandBuffers(device, &ai, &cmd));

	VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VK_CHECK(vkBeginCommandBuffer(cmd, &bi));
	return cmd;
}

void LogicalDevice::endSingleUseCmdGraphics(VkCommandBuffer cmd) const {
	VK_CHECK(vkEndCommandBuffer(cmd));
	VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
	si.commandBufferCount = 1;
	si.pCommandBuffers = &cmd;
	VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &si, VK_NULL_HANDLE));
	VK_CHECK(vkQueueWaitIdle(graphicsQueue));
	vkFreeCommandBuffers(device, uploadCmdPool, 1, &cmd);
}
