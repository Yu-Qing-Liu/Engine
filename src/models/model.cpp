#include "models/model.hpp"
#include <string>
#include <vulkan/vulkan_core.h>

Model::Model(VkDevice &device, std::string &modelRootPath, VkRenderPass &renderPass, VkExtent2D &swapChainExtent) : device(device), modelRootPath(modelRootPath), renderPass(renderPass), swapChainExtent(swapChainExtent) {
	shader_utils = &ShaderUtils::getInstance(device);
	shader_program = shader_utils->compileShaderProgram(modelRootPath);
}

void Model::createGraphicsPipeline(const std::vector<VkPipelineShaderStageCreateInfo> &shaderStages, VkPipelineVertexInputStateCreateInfo vertexInputInfo, VkPipelineInputAssemblyStateCreateInfo inputAssembly) {
	// Fixed function states
	VkPipelineViewportStateCreateInfo viewportState{};
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	VkPipelineMultisampleStateCreateInfo multisampling{};
	VkPipelineColorBlendStateCreateInfo colorBlending{};

	// Dynamic states
	std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

	// Pipeline layout
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);

	// Graphics pipeline creation
	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;
	// ... configure remaining parameters ...

	vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline);
}

void Model::draw(const vec3 &position, const quat &rotation, const vec3 &scale, const vec3 &color) {}
