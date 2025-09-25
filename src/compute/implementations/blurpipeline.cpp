#include "blurpipeline.hpp"
#include "engine.hpp"
#include "model.hpp"
#include <stdexcept>

using namespace Engine;

BlurPipeline::BlurPipeline(Model *model) : model(model) {}
BlurPipeline::~BlurPipeline() { destroyPipeAndSets(); }

void BlurPipeline::initialize() {
	// Borrow vertex input and VS from the model, so this works for *any* Model child
	bindingDesc = model->bindingDescription;
	attribs = model->attributeDescriptions;
	modelDSL = model->descriptorSetLayout;
	modelVS = model->shaderProgram.vertexShader;

	// Compile shaders:
	// - Vertex: we can reuse modelVS (preferred). If you want a dedicated VS, compile it here instead.
	// - Fragment: new blur FS that samples sceneColor and does a small kernel around gl_FragCoord.
	prog = Assets::compileShaderProgram(Assets::shaderRootPath + "/blur"); // expects: ui_blur.frag (VS is reused from model)

	if (!modelVS || !prog.fragmentShader)
		throw std::runtime_error("BlurPipeline: shaders missing (model VS and/or ui_blur.frag).");

	createPipeAndSets();
}

void BlurPipeline::updateCopyViewport(const VkViewport &vp, const VkRect2D &sc) {
	copyViewport = vp;
	copyScissor = sc;
}

void BlurPipeline::copy(VkCommandBuffer cmd) {
	// 1) Copy sceneColor -> swapchain (only once per swapchain image this frame)
	if (copiedForImage != Engine::currentImageIndex) {
		copiedForImage = Engine::currentImageIndex;

		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, copyPipe);
		VkDescriptorSet set0 = Engine::sceneSets[Engine::currentImageIndex];
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, copyPL, 0, 1, &set0, 0, nullptr);

		vkCmdSetViewport(cmd, 0, 1, &copyViewport);
		vkCmdSetScissor(cmd, 0, 1, &copyScissor);

		// full-screen triangle (no VBOs)
		vkCmdDraw(cmd, 3, 1, 0, 0);
	}
}

void BlurPipeline::render() {
	auto cmd = Engine::currentCommandBuffer();
	// 2) Draw the actual blurred model on top
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, blurPipe);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, blurPL, 0, 1, &model->descriptorSets[Engine::currentFrame], 0, nullptr);
	VkDescriptorSet set1 = Engine::sceneSets[Engine::currentImageIndex];
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, blurPL, 1, 1, &set1, 0, nullptr);

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

	// draw model geometry
	VkDeviceSize ofs = 0;
	vkCmdBindVertexBuffers(cmd, 0, 1, &model->vertexBuffer, &ofs);
	vkCmdBindIndexBuffer(cmd, model->indexBuffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(cmd, static_cast<uint32_t>(model->indices.size()), 1, 0, 0, 0);
}

void BlurPipeline::createPipeAndSets() {
	// pipeline layout: set0 = model DSL, set1 = blur DSL, plus push constants
	std::array<VkDescriptorSetLayout, 2> sets{modelDSL, Engine::sceneSetLayout};
	VkPushConstantRange pcr{};
	pcr.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	pcr.offset = 0;
	pcr.size = sizeof(Push); // matches struct Push

	VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	lci.setLayoutCount = (uint32_t)sets.size();
	lci.pSetLayouts = sets.data();
	lci.pushConstantRangeCount = 1;
	lci.pPushConstantRanges = &pcr;
	if (vkCreatePipelineLayout(device, &lci, nullptr, &blurPL) != VK_SUCCESS)
		throw std::runtime_error("BlurPipeline: pipeline layout failed");

	// Graphics pipeline (subpass = 1)
	VkPipelineShaderStageCreateInfo stages[2] = {createShaderStageInfo(modelVS, VK_SHADER_STAGE_VERTEX_BIT), createShaderStageInfo(prog.fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT)};

	VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
	vi.vertexBindingDescriptionCount = 1;
	vi.pVertexBindingDescriptions = &bindingDesc;
	vi.vertexAttributeDescriptionCount = (uint32_t)attribs.size();
	vi.pVertexAttributeDescriptions = attribs.data();

	VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
	vp.viewportCount = 1;
	vp.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = VK_CULL_MODE_NONE; // important for quads/2D
	rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rs.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
	ds.depthTestEnable = VK_FALSE;
	ds.depthWriteEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState cba{};
	cba.colorWriteMask = 0xF;
	cba.blendEnable = VK_FALSE;
	cba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	cba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	cba.colorBlendOp = VK_BLEND_OP_ADD;
	cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	cba.alphaBlendOp = VK_BLEND_OP_ADD;

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
	g.renderPass = Engine::renderPass1;
	g.subpass = 0;

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &g, nullptr, &blurPipe) != VK_SUCCESS)
		throw std::runtime_error("BlurPipeline: graphics pipeline failed");
}

void BlurPipeline::createCopyPipeAndSets() {
	// ---- COPY PIPELINE (subpass 1) ----
	copyProg = Assets::compileShaderProgram(Assets::shaderRootPath + "/screen"); // fullscreen.vert + copy.frag

	VkPipelineShaderStageCreateInfo stages[2] = {createShaderStageInfo(copyProg.vertexShader, VK_SHADER_STAGE_VERTEX_BIT), createShaderStageInfo(copyProg.fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT)};

	// Own pipeline layout: only set 0 = scene DSL, no push constants
	VkDescriptorSetLayout copySets[] = {Engine::sceneSetLayout};
	VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	lci.setLayoutCount = 1;
	lci.pSetLayouts = copySets;
	lci.pushConstantRangeCount = 0;
	lci.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(device, &lci, nullptr, &copyPL) != VK_SUCCESS)
		throw std::runtime_error("BlurPipeline: copy pipeline layout failed");

	// No vertex input (full-screen triangle in VS)
	VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

	VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
	vp.viewportCount = 1;
	vp.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = VK_CULL_MODE_NONE;
	rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rs.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
	ds.depthTestEnable = VK_FALSE;
	ds.depthWriteEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState cba{};
	cba.colorWriteMask = 0xF;
	cba.blendEnable = VK_FALSE;

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
	g.layout = copyPL;
	g.renderPass = Engine::renderPass1;
	g.subpass = 0;

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &g, nullptr, &copyPipe) != VK_SUCCESS)
		throw std::runtime_error("BlurPipeline: copy pipeline failed");
}

void BlurPipeline::destroyPipeAndSets() {
	if (blurPipe)
		vkDestroyPipeline(device, blurPipe, nullptr);
	if (blurPL)
		vkDestroyPipelineLayout(device, blurPL, nullptr);
	if (copyPipe)
		vkDestroyPipeline(device, copyPipe, nullptr);
	if (copyPL)
		vkDestroyPipelineLayout(device, copyPL, nullptr);

	if (prog.fragmentShader) {
		vkDestroyShaderModule(device, prog.fragmentShader, nullptr);
	}
	if (copyProg.vertexShader) {
		vkDestroyShaderModule(device, copyProg.vertexShader, nullptr);
	}
	if (copyProg.fragmentShader) {
		vkDestroyShaderModule(device, copyProg.fragmentShader, nullptr);
	}
}
