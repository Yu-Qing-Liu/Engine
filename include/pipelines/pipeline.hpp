#pragma once

#include "assets.hpp"
#include <vector>
#include <vulkan/vulkan_core.h>

using std::vector;

class Pipeline {
  public:
	Pipeline() = default;
	~Pipeline();

	struct GraphicsPipeline {
		VkPipelineLayoutCreateInfo pipelineLayoutCI{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
		uint32_t pushConstantRangeCount;
		VkPushConstantRange pushContantRanges{};
		vector<VkVertexInputAttributeDescription> vertexInputAttributeDescription{};
		vector<VkVertexInputBindingDescription> vertexInputBindingDescriptions{};
		VkPipelineVertexInputStateCreateInfo vertexInputStateCI{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
		VkDynamicState dynamicStates[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
		VkPipelineDynamicStateCreateInfo dynamicStateCI{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
		VkPipelineViewportStateCreateInfo viewportStateCI{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
		VkPipelineRasterizationStateCreateInfo rasterizationStateCI{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
		VkPipelineMultisampleStateCreateInfo multisamplingStateCI{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
		VkPipelineDepthStencilStateCreateInfo depthStencilStateCI{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
		VkPipelineColorBlendAttachmentState colorBlendAttachmentState{};
		VkPipelineColorBlendStateCreateInfo colorBlendStateCI{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
		vector<VkPipelineShaderStageCreateInfo> pipelineShaderStageCI{};
		VkFormat colorFormat{};
		VkFormat depthFormat{};
		VkPipelineRenderingCreateInfo pipelineRenderingCI{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
		VkGraphicsPipelineCreateInfo graphicsPipelineCI{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
	};

	struct ComputePipeline {
		VkPipelineLayoutCreateInfo pipelineLayoutCI{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
		VkComputePipelineCreateInfo computePipelineCI{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
	};

	struct DescriptorSets {
		vector<vector<VkDescriptorSetLayoutBinding>> descriptorSetLayoutBindings{};
		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
		VkPipelineLayoutCreateInfo pipelineLayoutCI{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
		vector<VkDescriptorBufferInfo> descriptorBuffersInfo{};
		vector<VkDescriptorImageInfo> descriptorImagesInfo{}; // (optional, for samplers/images)
		vector<vector<VkWriteDescriptorSet>> writeDescriptorSets{};
		vector<VkDescriptorSetLayout> descriptorSetsLayout{};
		vector<VkDescriptorSet> descriptorSets{};
		std::vector<std::vector<uint32_t>> writeDescriptorBufferInfoIndex{};
		struct DynamicRef {
			uint32_t setIndex;
			uint32_t binding;
			uint32_t arrayElement;
		};
		std::vector<DynamicRef> dynamicOrder; // order required by vkCmdBindDescriptorSets
		std::vector<uint32_t> dynamicOffsets; // same length as dynamicOrder
	};

	VkDevice device{};
	VkPhysicalDevice physicalDevice{};

	VkDescriptorPool descriptorPool{};
	vector<VkDescriptorPoolSize> descriptorPoolSizes;

	VkSampleCountFlagBits samplesCountFlagBits{};

	VkPipelineLayout pipelineLayout{};
	VkPipeline pipeline{};

	Assets::ShaderModules shaders{};

	GraphicsPipeline graphicsPipeline{};
	ComputePipeline computePipeline{};
	DescriptorSets descriptorSets{};

	void createBuffer(VkDeviceSize sz, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer &buf, VkDeviceMemory &mem);

	void createWriteDescriptorSet(uint32_t dstBinding, VkDescriptorType descriptorType, const VkDescriptorBufferInfo &bufInfo, uint32_t descriptorCount = 1, uint32_t setIndex = 0);
	void createDescriptorSetLayoutBinding(uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags flags, uint32_t descriptorCount = 1, uint32_t setIndex = 0);
	void setDynamicOffset(uint32_t setIndex, uint32_t binding, uint32_t offsetBytes, uint32_t arrayElement = 0);

	void createVertexInputBindingDescription(uint32_t binding, uint32_t stride, VkVertexInputRate inputRate);

  public:
	void createDescriptorPool(uint32_t setCount);
	void createDescriptors();
	void createGraphicsPipeline();

	void createComputePipeline();
};
