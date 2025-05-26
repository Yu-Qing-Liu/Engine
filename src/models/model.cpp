#include "models/model.hpp"
#include <string>
#include <vulkan/vulkan_core.h>

Model::Model(VkDevice &device, const std::string &modelRootPath, VkRenderPass &renderPass, VkExtent2D &swapChainExtent) : device(device), modelRootPath(modelRootPath), renderPass(renderPass), swapChainExtent(swapChainExtent) {
	shader_utils = &ShaderUtils::getInstance(device);
	shader_program = shader_utils->compileShaderProgram(modelRootPath);
}

Model::~Model() {
    if (shader_program.computeShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, shader_program.computeShader, nullptr);
    }
    if (shader_program.fragmentShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, shader_program.fragmentShader, nullptr);
    }
    if (shader_program.geometryShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, shader_program.geometryShader, nullptr);
    }
    if (shader_program.tessellationControlShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, shader_program.tessellationControlShader, nullptr);
    }
    if (shader_program.tessellationEvaluationShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, shader_program.tessellationEvaluationShader, nullptr);
    }
    if (shader_program.vertexShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, shader_program.vertexShader, nullptr);
    }
    vkDestroyPipeline(device, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
}

void Model::createGraphicsPipeline(const std::vector<VkPipelineShaderStageCreateInfo> &shaderStages, VkPipelineVertexInputStateCreateInfo vertexInputInfo, VkPipelineInputAssemblyStateCreateInfo inputAssembly) {
	// Viewport and Scissor State (using dynamic states)
	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	// Rasterization State
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

	// Multisampling State
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	// Color Blending Attachment
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	// Color Blending State
	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	// Dynamic States
	std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	// Pipeline Layout
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 0;
	pipelineLayoutInfo.pushConstantRangeCount = 0;

	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create pipeline layout!");
	}

	// Graphics Pipeline Creation
	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineInfo.pStages = shaderStages.data();
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = nullptr;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create graphics pipeline!");
	}
}

void Model::draw(VkCommandBuffer &commandBuffer, const vec3 &position, const quat &rotation, const vec3 &scale, const vec3 &color) {}
