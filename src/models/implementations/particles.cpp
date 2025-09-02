#include "particles.hpp"
#include "engine.hpp"
#include <cstring>
#include <random>
#include <vulkan/vulkan_core.h>

Particles::Particles(uint32_t particleCount, uint32_t width, uint32_t height) : particleCount(particleCount), width(width), height(height), Model(Engine::shaderRootPath + "/particle") {
	createComputeDescriptorSetLayout();
	createShaderStorageBuffers();
	createUniformBuffers();
	createDescriptorPool();
	createComputeDescriptorSets();
	createBindingDescriptions();
	createGraphicsPipeline();
	createComputePipeline();
}

Particles::~Particles() {
	if (computePipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(Engine::device, computePipeline, nullptr);
	}
	if (computePipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(Engine::device, computePipelineLayout, nullptr);
	}
	if (computeDescriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(Engine::device, computeDescriptorSetLayout, nullptr);
	}

	for (size_t i = 0; i < Engine::MAX_FRAMES_IN_FLIGHT; ++i) {
		if (shaderStorageBuffers[i] != VK_NULL_HANDLE) {
			vkDestroyBuffer(Engine::device, shaderStorageBuffers[i], nullptr);
			shaderStorageBuffers[i] = VK_NULL_HANDLE;
		}
		if (shaderStorageBuffersMemory[i] != VK_NULL_HANDLE) {
			vkFreeMemory(Engine::device, shaderStorageBuffersMemory[i], nullptr);
			shaderStorageBuffersMemory[i] = VK_NULL_HANDLE;
		}
	}
}

void Particles::createComputeDescriptorSetLayout() {
	std::array<VkDescriptorSetLayoutBinding, 3> layoutBindings{};
	layoutBindings[0].binding = 0;
	layoutBindings[0].descriptorCount = 1;
	layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	layoutBindings[0].pImmutableSamplers = nullptr;
	layoutBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	layoutBindings[1].binding = 1;
	layoutBindings[1].descriptorCount = 1;
	layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	layoutBindings[1].pImmutableSamplers = nullptr;
	layoutBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	layoutBindings[2].binding = 2;
	layoutBindings[2].descriptorCount = 1;
	layoutBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	layoutBindings[2].pImmutableSamplers = nullptr;
	layoutBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 3;
	layoutInfo.pBindings = layoutBindings.data();

	if (vkCreateDescriptorSetLayout(Engine::device, &layoutInfo, nullptr, &computeDescriptorSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create compute descriptor set layout!");
	}
}

void Particles::createBindingDescriptions() {
	bindingDescription = Particle::getBindingDescription();
	auto attrs = Particle::getAttributeDescriptions();
	attributeDescriptions = vector<VkVertexInputAttributeDescription>(attrs.begin(), attrs.end());
}

void Particles::createGraphicsPipeline() {
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	shaderProgram = Engine::compileShaderProgram(shaderPath);
	shaderStages = {Engine::createShaderStageInfo(shaderProgram.vertexShader, VK_SHADER_STAGE_VERTEX_BIT), Engine::createShaderStageInfo(shaderProgram.fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT)};

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_FALSE;
	depthStencil.depthWriteEnable = VK_FALSE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

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

	std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 0;
	pipelineLayoutInfo.pSetLayouts = nullptr;

	if (vkCreatePipelineLayout(Engine::device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create pipeline layout!");
	}

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages.data();
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.renderPass = Engine::renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.pDepthStencilState = &depthStencil;

	if (vkCreateGraphicsPipelines(Engine::device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
		throw std::runtime_error("failed to create graphics pipeline!");
	}
}

void Particles::createComputePipeline() {
	VkShaderModule computeShaderModule = shaderProgram.computeShader;

	VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
	computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	computeShaderStageInfo.module = computeShaderModule;
	computeShaderStageInfo.pName = "main";

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &computeDescriptorSetLayout;

	if (vkCreatePipelineLayout(Engine::device, &pipelineLayoutInfo, nullptr, &computePipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create compute pipeline layout!");
	}

	VkComputePipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.layout = computePipelineLayout;
	pipelineInfo.stage = computeShaderStageInfo;

	if (vkCreateComputePipelines(Engine::device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline) != VK_SUCCESS) {
		throw std::runtime_error("failed to create compute pipeline!");
	}
}

void Particles::createShaderStorageBuffers() {
	std::default_random_engine rndEngine((unsigned)time(nullptr));
	std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);

	// Initial particle positions on a circle
	std::vector<Particle> particles(particleCount);
	for (auto &particle : particles) {
		float r = 0.25f * sqrt(rndDist(rndEngine));
		float theta = rndDist(rndEngine) * 2.0f * 3.14159265358979323846f;
		float x = r * cos(theta) * height / width;
		float y = r * sin(theta);
		particle.position = glm::vec2(x, y);
		particle.velocity = glm::normalize(glm::vec2(x, y)) * 0.00025f;
		particle.color = glm::vec4(rndDist(rndEngine), rndDist(rndEngine), rndDist(rndEngine), 1.0f);
	}

	VkDeviceSize bufferSize = sizeof(Particle) * particleCount;

	// Create a staging buffer used to upload data to the gpu
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	Engine::createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	void *data;
	vkMapMemory(Engine::device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, particles.data(), (size_t)bufferSize);
	vkUnmapMemory(Engine::device, stagingBufferMemory);

	shaderStorageBuffers.resize(Engine::MAX_FRAMES_IN_FLIGHT);
	shaderStorageBuffersMemory.resize(Engine::MAX_FRAMES_IN_FLIGHT);

	// Copy initial particle data to all storage buffers
	for (size_t i = 0; i < Engine::MAX_FRAMES_IN_FLIGHT; i++) {
		Engine::createBuffer(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, shaderStorageBuffers[i], shaderStorageBuffersMemory[i]);
		Engine::copyBuffer(stagingBuffer, shaderStorageBuffers[i], bufferSize);
	}

	vkDestroyBuffer(Engine::device, stagingBuffer, nullptr);
	vkFreeMemory(Engine::device, stagingBufferMemory, nullptr);
}

void Particles::createUniformBuffers() {
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);

	uniformBuffers.resize(Engine::MAX_FRAMES_IN_FLIGHT);
	uniformBuffersMemory.resize(Engine::MAX_FRAMES_IN_FLIGHT);
	uniformBuffersMapped.resize(Engine::MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < Engine::MAX_FRAMES_IN_FLIGHT; i++) {
		Engine::createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);
		vkMapMemory(Engine::device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
	}
}

void Particles::createDescriptorPool() {
	std::array<VkDescriptorPoolSize, 2> poolSizes{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = static_cast<uint32_t>(Engine::MAX_FRAMES_IN_FLIGHT);

	poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSizes[1].descriptorCount = static_cast<uint32_t>(Engine::MAX_FRAMES_IN_FLIGHT) * 2;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 2;
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = static_cast<uint32_t>(Engine::MAX_FRAMES_IN_FLIGHT);

	if (vkCreateDescriptorPool(Engine::device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor pool!");
	}
}

void Particles::createComputeDescriptorSets() {
	std::vector<VkDescriptorSetLayout> layouts(Engine::MAX_FRAMES_IN_FLIGHT, computeDescriptorSetLayout);
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(Engine::MAX_FRAMES_IN_FLIGHT);
	allocInfo.pSetLayouts = layouts.data();

	computeDescriptorSets.resize(Engine::MAX_FRAMES_IN_FLIGHT);
	if (vkAllocateDescriptorSets(Engine::device, &allocInfo, computeDescriptorSets.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate descriptor sets!");
	}

	for (size_t i = 0; i < Engine::MAX_FRAMES_IN_FLIGHT; i++) {
		VkDescriptorBufferInfo uniformBufferInfo{};
		uniformBufferInfo.buffer = uniformBuffers[i];
		uniformBufferInfo.offset = 0;
		uniformBufferInfo.range = sizeof(UniformBufferObject);

		std::array<VkWriteDescriptorSet, 3> descriptorWrites{};
		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = computeDescriptorSets[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &uniformBufferInfo;

		VkDescriptorBufferInfo storageBufferInfoLastFrame{};
		storageBufferInfoLastFrame.buffer = shaderStorageBuffers[(i - 1) % Engine::MAX_FRAMES_IN_FLIGHT];
		storageBufferInfoLastFrame.offset = 0;
		storageBufferInfoLastFrame.range = sizeof(Particle) * particleCount;

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = computeDescriptorSets[i];
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pBufferInfo = &storageBufferInfoLastFrame;

		VkDescriptorBufferInfo storageBufferInfoCurrentFrame{};
		storageBufferInfoCurrentFrame.buffer = shaderStorageBuffers[i];
		storageBufferInfoCurrentFrame.offset = 0;
		storageBufferInfoCurrentFrame.range = sizeof(Particle) * particleCount;

		descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[2].dstSet = computeDescriptorSets[i];
		descriptorWrites[2].dstBinding = 2;
		descriptorWrites[2].dstArrayElement = 0;
		descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrites[2].descriptorCount = 1;
		descriptorWrites[2].pBufferInfo = &storageBufferInfoCurrentFrame;

		vkUpdateDescriptorSets(Engine::device, 3, descriptorWrites.data(), 0, nullptr);
	}
}

void Particles::updateComputeUniformBuffer() {
	UniformBufferObject ubo{};
	ubo.deltatime = Engine::lastFrameTime * 2.0f;
	memcpy(uniformBuffersMapped[Engine::currentFrame], &ubo, sizeof(ubo));
}

void Particles::compute() {
	vkCmdBindPipeline(Engine::currentComputeCommandBuffer(), VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);

	vkCmdBindDescriptorSets(Engine::currentComputeCommandBuffer(), VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &computeDescriptorSets[Engine::currentFrame], 0, nullptr);

	vkCmdDispatch(Engine::currentComputeCommandBuffer(), particleCount / 256, 1, 1);
}

void Particles::render(const UBO &ubo, const ScreenParams &screenParams) {
	vkCmdBindPipeline(Engine::currentCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

	vkCmdSetViewport(Engine::currentCommandBuffer(), 0, 1, &screenParams.viewport);

	vkCmdSetScissor(Engine::currentCommandBuffer(), 0, 1, &screenParams.scissor);

	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(Engine::currentCommandBuffer(), 0, 1, &shaderStorageBuffers[Engine::currentFrame], offsets);

	vkCmdDraw(Engine::currentCommandBuffer(), particleCount, 1, 0, 0);
}
