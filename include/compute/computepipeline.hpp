#pragma once

#include <vulkan/vulkan_core.h>

class ComputePipeline {
  public:
	ComputePipeline() = default;
	ComputePipeline(ComputePipeline &&) = default;
	ComputePipeline(const ComputePipeline &) = default;
	ComputePipeline &operator=(ComputePipeline &&) = default;
	ComputePipeline &operator=(const ComputePipeline &) = default;
	~ComputePipeline();

	virtual void initialize() {};
	virtual void updateComputeUniformBuffer() {};
	virtual void compute() {};

	bool initialized = false;

  protected:
	VkDescriptorSetLayout computeDescriptorSetLayout = VK_NULL_HANDLE;
	VkPipelineLayout computePipelineLayout = VK_NULL_HANDLE;
	VkPipeline computePipeline = VK_NULL_HANDLE;
	VkDescriptorPool computePool = VK_NULL_HANDLE;
	VkDescriptorSet computeDescriptorSet = VK_NULL_HANDLE;

	virtual void createComputeDescriptorSetLayout() {};
	virtual void createComputePipeline() {};
	virtual void createShaderStorageBuffers() {};
	virtual void createComputeDescriptorSets() {};
};
