#include "blurspipeline.hpp"
#include "model.hpp"

BlursPipeline::BlursPipeline(Model *model, std::array<VkVertexInputBindingDescription, 2> &bindings, std::vector<VkBuffer> &instanceBuffers, uint32_t &instanceCount) : bindings(bindings), instanceCount(instanceCount), instanceBuffers(instanceBuffers), BlurPipeline(model) {}

void BlursPipeline::initialize() {
	attribs = model->attributeDescriptions;
	modelDSL = model->descriptorSetLayout;
	modelVS = model->shaderProgram.vertexShader;

	prog = Assets::compileShaderProgram(Assets::shaderRootPath + "/instanced/blur");

	if (!modelVS || !prog.fragmentShader)
		throw std::runtime_error("BlurPipeline: shaders missing (model VS and/or ui_blur.frag).");

	createPipeAndSets();
}

void BlursPipeline::render() {
	auto cmd = Engine::currentCommandBuffer();

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, blurPipe);

	// set=0: model UBO (MVP), set=1: scene sampler(s)
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, blurPL, 0, 1, &model->descriptorSets[Engine::currentFrame], 0, nullptr);
	VkDescriptorSet sceneSet = Engine::sceneSets[Engine::currentImageIndex];
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, blurPL, 1, 1, &sceneSet, 0, nullptr);

	// viewport/scissor
	vkCmdSetViewport(cmd, 0, 1, &model->screenParams.viewport);
	vkCmdSetScissor(cmd, 0, 1, &model->screenParams.scissor);

	// push constants
	Push pc{};
	pc.invExtent = {1.0f / Engine::swapChainExtent.width, 1.0f / Engine::swapChainExtent.height};
	pc.radius = radius;
	pc.lodScale = lodScale;
    pc.tint = tint;
	pc.microTent = microTent;
	pc.cornerRadiusPxOverride = cornerRadiusPxOverride;
	vkCmdPushConstants(cmd, blurPL, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Push), &pc);

	// bind buffers: binding 0 = mesh, binding 1 = instance (from parent vector)
	VkBuffer vbs[2] = {model->vertexBuffer, instanceBuffers[Engine::currentFrame]};
	VkDeviceSize ofs[2] = {0, 0};
	vkCmdBindVertexBuffers(cmd, 0, 2, vbs, ofs);
	vkCmdBindIndexBuffer(cmd, model->indexBuffer, 0, VK_INDEX_TYPE_UINT32);

	// instanced draw
	vkCmdDrawIndexed(cmd, static_cast<uint32_t>(model->indices.size()), instanceCount, 0, 0, 0);
}

void BlursPipeline::createPipeAndSets() {
	// ---- Pipeline layout (set0 = model UBO, set1 = scene sampler) ----
	std::array<VkDescriptorSetLayout, 2> sets{modelDSL, Engine::sceneSetLayout};
	VkPushConstantRange pcr{};
	pcr.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	pcr.offset = 0;
	pcr.size = sizeof(Push);

	VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	lci.setLayoutCount = (uint32_t)sets.size();
	lci.pSetLayouts = sets.data();
	lci.pushConstantRangeCount = 1;
	lci.pPushConstantRanges = &pcr;

	if (vkCreatePipelineLayout(Engine::device, &lci, nullptr, &blurPL) != VK_SUCCESS) {
		throw std::runtime_error("BlursPipeline: pipeline layout failed");
	}

	// ---- Shader stages (dedicated instanced blur VS/FS) ----
	VkPipelineShaderStageCreateInfo stages[2] = {Engine::createShaderStageInfo(modelVS, VK_SHADER_STAGE_VERTEX_BIT), Engine::createShaderStageInfo(prog.fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT)};

	// ---- Vertex Input (use parent-provided arrays) ----
	VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
	vi.vertexBindingDescriptionCount = (uint32_t)bindings.size(); // 2 (mesh+instance)
	vi.pVertexBindingDescriptions = bindings.data();
	vi.vertexAttributeDescriptionCount = (uint32_t)attribs.size(); // mesh + per-instance cols
	vi.pVertexAttributeDescriptions = attribs.data();

	// ---- Fixed function ----
	VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
	vp.viewportCount = 1;
	vp.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = VK_CULL_MODE_NONE; // UI quads may be double-sided
	rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rs.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
	ds.depthTestEnable = VK_FALSE;
	ds.depthWriteEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState cba{};
	cba.colorWriteMask = 0xF;
	cba.blendEnable = VK_FALSE; // your FS writes premult or straight depending on tint.a usage

	VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
	cb.attachmentCount = 1;
	cb.pAttachments = &cba;

	std::array<VkDynamicState, 2> dyn{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo dync{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
	dync.dynamicStateCount = (uint32_t)dyn.size();
	dync.pDynamicStates = dyn.data();

	VkGraphicsPipelineCreateInfo g{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
	g.stageCount = 2;
	g.pStages = stages;
	g.pVertexInputState = &vi;
	g.pInputAssemblyState = &ia;
	g.pViewportState = &vp;
	g.pRasterizationState = &rs;
	g.pMultisampleState = &ms;
	g.pDepthStencilState = &ds;
	g.pColorBlendState = &cb;
	g.pDynamicState = &dync;
	g.layout = blurPL;
	g.renderPass = Engine::renderPass1; // same subpass used by your base BlurPipeline
	g.subpass = 0;

	if(vkCreateGraphicsPipelines(Engine::device, VK_NULL_HANDLE, 1, &g, nullptr, &blurPipe) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create blur pipeline");
    }
}

void BlursPipeline::destroyPipeAndSets() {
	if (blurPipe)
		vkDestroyPipeline(Engine::device, blurPipe, nullptr);
	if (blurPL)
		vkDestroyPipelineLayout(Engine::device, blurPL, nullptr);

	if (prog.vertexShader)
		vkDestroyShaderModule(Engine::device, prog.vertexShader, nullptr);
	if (prog.fragmentShader)
		vkDestroyShaderModule(Engine::device, prog.fragmentShader, nullptr);

	blurPipe = VK_NULL_HANDLE;
	blurPL = VK_NULL_HANDLE;
	prog.vertexShader = VK_NULL_HANDLE;
	prog.fragmentShader = VK_NULL_HANDLE;
}
