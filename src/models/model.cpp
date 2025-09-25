#include "model.hpp"
#include "engine.hpp"
#include "events.hpp"
#include "scene.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

Model::Model(Scene *scene, const MVP &ubo, ScreenParams &screenParams, const string &shaderPath, const VkRenderPass &renderPass) : scene(scene), ubo(ubo), screenParams(screenParams), shaderPath(shaderPath), renderPass(renderPass) {
	rayTracing = std::make_unique<RayTracingPipeline>(this);
	this->ubo.proj[1][1] *= -1;
	if (scene) {
		scene->models.emplace_back(this);
	}
}

Model::~Model() {
	if (scene) {
		auto &v = scene->models;
		v.erase(std::remove(v.begin(), v.end(), this), v.end());
		scene = nullptr;
	}

	onMouseEnter = nullptr;
	onMouseExit = nullptr;
	{
		if (watcher.joinable()) {
			watcher.request_stop();
			cv.notify_all();
			watcher.join();
		}
	}

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
		if (mvpBuffers[i] != VK_NULL_HANDLE) {
			vkDestroyBuffer(Engine::device, mvpBuffers[i], nullptr);
		}
		if (mvpBuffersMemory[i] != VK_NULL_HANDLE) {
			vkFreeMemory(Engine::device, mvpBuffersMemory[i], nullptr);
		}
	}

	if (vertexBuffer != VK_NULL_HANDLE) {
		vkDestroyBuffer(Engine::device, vertexBuffer, nullptr);
	}
	if (vertexBufferMemory != VK_NULL_HANDLE) {
		vkFreeMemory(Engine::device, vertexBufferMemory, nullptr);
	}
	if (indexBuffer != VK_NULL_HANDLE) {
		vkDestroyBuffer(Engine::device, indexBuffer, nullptr);
	}
	if (indexBufferMemory != VK_NULL_HANDLE) {
		vkFreeMemory(Engine::device, indexBufferMemory, nullptr);
	}
	if (descriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(Engine::device, descriptorPool, nullptr);
	}
	if (descriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(Engine::device, descriptorSetLayout, nullptr);
	}
	if (graphicsPipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(Engine::device, graphicsPipeline, nullptr);
	}
	if (pipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(Engine::device, pipelineLayout, nullptr);
	}
}

void Model::copyUBO() { memcpy(mvpBuffersMapped[Engine::currentFrame], &ubo, sizeof(ubo)); }

void Model::setOnMouseClick(std::function<void(int, int, int)> cb) {
	auto callback = [this, cb](int button, int action, int mods) {
		if (mouseIsOver) {
			cb(button, action, mods);
		}
	};
	Events::mouseCallbacks.push_back(callback);
}

void Model::setOnKeyboardKeyPress(std::function<void(int, int, int, int)> cb) {
	auto callback = [this, cb](int key, int scancode, int action, int mods) {
		if (selected) {
			cb(key, scancode, action, mods);
		}
	};
	Events::keyboardCallbacks.push_back(callback);
}

void Model::setMouseIsOver(bool over) {
	std::function<void()> enterCb;
	bool fireEnter = false;

	{
		std::lock_guard lk(m);
		if (over == mouseIsOver) {
			return; // no state change
		}
		// transition detection
		fireEnter = (!mouseIsOver && over);
		mouseIsOver = over;
		if (!over) {
			// wake any waiting exit-watcher
			cv.notify_all();
		}
		if (fireEnter)
			enterCb = onMouseEnter; // copy under lock
	}

	if (fireEnter) {
		if (enterCb)
			enterCb();
		// Arm the watcher ONCE, at the moment we enter.
		onMouseExitEvent();
	}
}

void Model::onMouseExitEvent() {
	if (!onMouseExit) {
		return;
	}

	if (watcher.joinable()) {
		watcher.request_stop();
		cv.notify_all();
	}

	watcher = Platform::jthread([this](Platform::stop_token st) {
		std::unique_lock lk(m);
		cv.wait(lk, [this, st] { return st.stop_requested() || !mouseIsOver; });
		if (st.stop_requested()) {
			return;
		}
		auto cb = onMouseExit;
		lk.unlock();
		if (cb) {
			cb();
		}
	});
}

void Model::updateMVP(optional<mat4> model, optional<mat4> view, optional<mat4> proj) {
	if (model.has_value()) {
		ubo.model = model.value();
	}
	if (view.has_value()) {
		ubo.view = view.value();
	}
	if (proj.has_value()) {
		ubo.proj = proj.value();
		ubo.proj[1][1] *= -1;
	}
}

void Model::updateUniformBuffer(const MVP &ubo) {
	this->ubo = ubo;
	this->ubo.proj[1][1] *= -1;
}

void Model::updateScreenParams(const ScreenParams &screenParams) { this->screenParams = screenParams; }

void Model::buildBVH() {}

void Model::createDescriptorSetLayout() {
	mvpLayoutBinding.binding = 0;
	mvpLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	mvpLayoutBinding.descriptorCount = 1;
	mvpLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	mvpLayoutBinding.pImmutableSamplers = nullptr;

	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &mvpLayoutBinding;

	if (vkCreateDescriptorSetLayout(Engine::device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}
}

void Model::createVertexBuffer() {}

void Model::createIndexBuffer() {
	if (indices.empty())
		throw std::runtime_error("Create Index Buffer: No indices");
	VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

	VkBuffer stg;
	VkDeviceMemory stgMem;
	Engine::createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stg, stgMem);

	void *data = nullptr;
	vkMapMemory(Engine::device, stgMem, 0, bufferSize, 0, &data);
	std::memcpy(data, indices.data(), (size_t)bufferSize);
	vkUnmapMemory(Engine::device, stgMem);

	Engine::createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

	Engine::copyBuffer(stg, indexBuffer, bufferSize);

	vkDestroyBuffer(Engine::device, stg, nullptr);
	vkFreeMemory(Engine::device, stgMem, nullptr);
}

void Model::createUniformBuffers() { createUniformBuffers<MVP>(mvpBuffers, mvpBuffersMemory, mvpBuffersMapped); }

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
	vector<VkDescriptorSetLayout> layouts(Engine::MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
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
		bufferInfo.buffer = mvpBuffers[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(MVP);

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

void Model::setupGraphicsPipeline() {}

void Model::createGraphicsPipeline() {
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	shaderProgram = Assets::compileShaderProgram(shaderPath);
	shaderStages = {Engine::createShaderStageInfo(shaderProgram.vertexShader, VK_SHADER_STAGE_VERTEX_BIT), Engine::createShaderStageInfo(shaderProgram.fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT)};

	// Viewport and Scissor State (using dynamic states)
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	// Rasterization State
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;

	rasterizer.cullMode = isOrtho() ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;

	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

	// Multisampling State
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	// Depth Stencil
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	// Color Blending Attachment
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	// Color Blending State
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	// Pipeline Layout
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	std::array<VkDescriptorSetLayout, 2> setLayouts = {
		descriptorSetLayout,   // set = 0 (MVP UBO)
		Engine::sceneSetLayout // set = 1 (sampler2D uScene)
	};
	pipelineLayoutInfo.setLayoutCount = (uint32_t)setLayouts.size();
	pipelineLayoutInfo.pSetLayouts = setLayouts.data();

	setupGraphicsPipeline();

	colorBlending.pAttachments = &colorBlendAttachment;

	if (vkCreatePipelineLayout(Engine::device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create pipeline layout!");
	}

	// Graphics Pipeline Creation
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
	pipelineInfo.pDepthStencilState = &depthStencil;

	if (vkCreateGraphicsPipelines(Engine::device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create graphics pipeline!");
	}
}

void Model::render() {
	copyUBO();

	auto cmd = Engine::currentCommandBuffer();

	if (blur) {
		blur->render();
		return;
	}

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

	vkCmdSetViewport(cmd, 0, 1, &screenParams.viewport);

	vkCmdSetScissor(cmd, 0, 1, &screenParams.scissor);

	VkBuffer vertexBuffers[] = {vertexBuffer};
	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[Engine::currentFrame], 0, nullptr);

	vkCmdDrawIndexed(cmd, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
}
