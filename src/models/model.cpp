#include "model.hpp"
#include "engine.hpp"
#include <cstring>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

Model::Model(const std::string &shaderPath, const std::vector<Vertex> &vertices, const std::vector<uint16_t> &indices) : vertices(vertices), indices(indices) {
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;

	shaderProgram = Engine::compileShaderProgram(shaderPath);
}

Model::Model(const std::string &shaderPath, const std::vector<TexVertex> &vertices, const std::vector<uint16_t> &indices) : texVertices(vertices), indices(indices) {
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;

	shaderProgram = Engine::compileShaderProgram(shaderPath);
}

Model::~Model() {
	if (shaderProgram.computeShader != VK_NULL_HANDLE) {
		vkDestroyShaderModule(Engine::device, shaderProgram.computeShader, nullptr);
	}
	if (shaderProgram.fragmentShader != VK_NULL_HANDLE) {
		vkDestroyShaderModule(Engine::device, shaderProgram.fragmentShader, nullptr);
	}
	if (shaderProgram.geometryShader != VK_NULL_HANDLE) {
		vkDestroyShaderModule(Engine::device, shaderProgram.geometryShader, nullptr);
	}
	if (shaderProgram.tessellationControlShader != VK_NULL_HANDLE) {
		vkDestroyShaderModule(Engine::device, shaderProgram.tessellationControlShader, nullptr);
	}
	if (shaderProgram.tessellationEvaluationShader != VK_NULL_HANDLE) {
		vkDestroyShaderModule(Engine::device, shaderProgram.tessellationEvaluationShader, nullptr);
	}
	if (shaderProgram.vertexShader != VK_NULL_HANDLE) {
		vkDestroyShaderModule(Engine::device, shaderProgram.vertexShader, nullptr);
	}

	for (size_t i = 0; i < Engine::MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroyBuffer(Engine::device, uniformBuffers[i], nullptr);
		vkFreeMemory(Engine::device, uniformBuffersMemory[i], nullptr);
	}

	vkDestroyBuffer(Engine::device, vertexBuffer, nullptr);
	vkFreeMemory(Engine::device, vertexBufferMemory, nullptr);

	vkDestroyBuffer(Engine::device, indexBuffer, nullptr);
	vkFreeMemory(Engine::device, indexBufferMemory, nullptr);

    vkDestroyDescriptorPool(Engine::device, descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(Engine::device, descriptorSetLayout, nullptr);

	vkDestroyPipeline(Engine::device, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(Engine::device, pipelineLayout, nullptr);
}

void Model::setUniformBuffer(const mat4 &model, const mat4 &view, const mat4 &proj) {
    ubo.model = model;
    ubo.view = view;
    ubo.proj = proj;
    ubo.proj[1][1] *= -1;
    memcpy(uniformBuffersMapped[Engine::currentFrame], &ubo, sizeof(ubo));
}

void Model::createBindingDescriptions() {
    bindingDescription = Vertex::getBindingDescription();
	auto attrs = Vertex::getAttributeDescriptions();
	attributeDescriptions = std::vector<VkVertexInputAttributeDescription>(attrs.begin(), attrs.end());
}

void Model::createGraphicsPipeline() {
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    shaderStages = {Engine::createShaderStageInfo(shaderProgram.vertexShader, VK_SHADER_STAGE_VERTEX_BIT), Engine::createShaderStageInfo(shaderProgram.fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT)};

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
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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
	pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0;

	if (vkCreatePipelineLayout(Engine::device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
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
	pipelineInfo.renderPass = Engine::renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	if (vkCreateGraphicsPipelines(Engine::device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create graphics pipeline!");
	}
}

void Model::createDescriptorSetLayout() {
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	uboLayoutBinding.pImmutableSamplers = nullptr;

	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &uboLayoutBinding;

	if (vkCreateDescriptorSetLayout(Engine::device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}
}

void Model::createVertexBuffer() {
    VkDeviceSize bufferSize;
    if (!vertices.empty()) {
        bufferSize = sizeof(vertices[0]) * vertices.size();
    } else if(!texVertices.empty()) {
        bufferSize = sizeof(texVertices[0]) * texVertices.size();
    } else {
        throw std::runtime_error("No vertices specified for Vertex Buffer");
    }

    Engine::createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingBufferMemory
    );

	void *data;
	vkMapMemory(Engine::device, stagingBufferMemory, 0, bufferSize, 0, &data);
    if (!vertices.empty()) {
        memcpy(data, vertices.data(), (size_t)bufferSize);
    } else if(!texVertices.empty()) {
        memcpy(data, texVertices.data(), (size_t)bufferSize);
    } else {
        throw std::runtime_error("No vertices specified for Vertex Buffer");
    }
	vkUnmapMemory(Engine::device, stagingBufferMemory);

    Engine::createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        vertexBuffer,
        vertexBufferMemory
    );

    Engine::copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

    vkDestroyBuffer(Engine::device, stagingBuffer, nullptr);
    vkFreeMemory(Engine::device, stagingBufferMemory, nullptr);
}

void Model::createIndexBuffer() {
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    Engine::createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingBufferMemory
    );

	void *data;
	vkMapMemory(Engine::device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, indices.data(), (size_t)bufferSize);
	vkUnmapMemory(Engine::device, stagingBufferMemory);

    Engine::createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        indexBuffer,
        indexBufferMemory
    );

    Engine::copyBuffer(stagingBuffer, indexBuffer, bufferSize);

    vkDestroyBuffer(Engine::device, stagingBuffer, nullptr);
    vkFreeMemory(Engine::device, stagingBufferMemory, nullptr);
}

void Model::createUniformBuffers() {
	VkDeviceSize bufferSize = sizeof(UBO);

	uniformBuffers.resize(Engine::MAX_FRAMES_IN_FLIGHT);
	uniformBuffersMemory.resize(Engine::MAX_FRAMES_IN_FLIGHT);
	uniformBuffersMapped.resize(Engine::MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < Engine::MAX_FRAMES_IN_FLIGHT; i++) {
		Engine::createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);
		vkMapMemory(Engine::device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
	}
}

void Model::createDescriptorPool() {
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = static_cast<uint32_t>(Engine::MAX_FRAMES_IN_FLIGHT);
    
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = static_cast<uint32_t>(Engine::MAX_FRAMES_IN_FLIGHT);

    if (vkCreateDescriptorPool(Engine::device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool");
    }
}

void Model::createDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(Engine::MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(Engine::MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();

    descriptorSets.resize(Engine::MAX_FRAMES_IN_FLIGHT);

    if (vkAllocateDescriptorSets(Engine::device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descritor set");
    }

    for (size_t i = 0; i < Engine::MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UBO);

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descriptorSets[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;
        
        vkUpdateDescriptorSets(Engine::device, 1, &descriptorWrite, 0, nullptr);
    }
}

void Model::render() {
	vkCmdBindPipeline(Engine::currentCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)Engine::swapChainExtent.width;
	viewport.height = (float)Engine::swapChainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(Engine::currentCommandBuffer(), 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = {0, 0};
	scissor.extent = Engine::swapChainExtent;
	vkCmdSetScissor(Engine::currentCommandBuffer(), 0, 1, &scissor);

	VkBuffer vertexBuffers[] = {vertexBuffer};
	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(Engine::currentCommandBuffer(), 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(Engine::currentCommandBuffer(), indexBuffer, 0, VK_INDEX_TYPE_UINT16);

    vkCmdBindDescriptorSets(Engine::currentCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[Engine::currentFrame], 0, nullptr);

	vkCmdDrawIndexed(Engine::currentCommandBuffer(), static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
}

void Model::draw() {
    if(onFrameUpdate) {
        onFrameUpdate(*this);
    }
}
