#include "pipeline.hpp"
#include "debug.hpp"
#include "memory.hpp"

Pipeline::~Pipeline() {
	if (device == VK_NULL_HANDLE)
		return;

	if (pipeline) {
		vkDestroyPipeline(device, pipeline, nullptr);
		pipeline = VK_NULL_HANDLE;
	}
	if (pipelineLayout) {
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		pipelineLayout = VK_NULL_HANDLE;
	}
	if (descriptorSets.descriptorSetsLayout.size() > 0) {
		for (auto &d : descriptorSets.descriptorSetsLayout) {
			if (d != VK_NULL_HANDLE) {
				vkDestroyDescriptorSetLayout(device, d, nullptr);
			}
		}
		descriptorSets.descriptorSetsLayout.clear();
	}
	if (descriptorPool) {
		vkDestroyDescriptorPool(device, descriptorPool, nullptr);
		descriptorPool = VK_NULL_HANDLE;
	}

	Assets::destroyShaderProgram(shaders, device);

	device = VK_NULL_HANDLE;
}

void Pipeline::createDescriptorPool(uint32_t setCount) {
	VkDescriptorPoolCreateInfo dpci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
	dpci.maxSets = setCount;
	;
	dpci.poolSizeCount = (uint32_t)descriptorPoolSizes.size();
	dpci.pPoolSizes = descriptorPoolSizes.data();
	VK_CHECK(vkCreateDescriptorPool(device, &dpci, nullptr, &descriptorPool));
}

void Pipeline::createDescriptors() {
	const uint32_t setCount = static_cast<uint32_t>(descriptorSets.descriptorSetLayoutBindings.size());
	if (setCount == 0)
		return;

	// 1) Create each set layout
	descriptorSets.descriptorSetsLayout.resize(setCount);
	for (uint32_t i = 0; i < setCount; ++i) {
		const auto &bindings = descriptorSets.descriptorSetLayoutBindings[i];
		descriptorSets.descriptorSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorSets.descriptorSetLayoutCI.bindingCount = static_cast<uint32_t>(bindings.size());
		descriptorSets.descriptorSetLayoutCI.pBindings = bindings.empty() ? nullptr : bindings.data();
		VK_CHECK(vkCreateDescriptorSetLayout(device, &descriptorSets.descriptorSetLayoutCI, nullptr, &descriptorSets.descriptorSetsLayout[i]));
	}

	// 3) Allocate sets
	descriptorSets.descriptorSets.resize(setCount);
	descriptorSets.descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSets.descriptorSetAllocateInfo.descriptorPool = descriptorPool;
	descriptorSets.descriptorSetAllocateInfo.descriptorSetCount = setCount;
	descriptorSets.descriptorSetAllocateInfo.pSetLayouts = descriptorSets.descriptorSetsLayout.data();
	VK_CHECK(vkAllocateDescriptorSets(device, &descriptorSets.descriptorSetAllocateInfo, descriptorSets.descriptorSets.data()));

	// 4) Patch writes with actual set handles, and collect dynamic order
	std::vector<VkWriteDescriptorSet> allWrites;
	for (uint32_t set = 0; set < setCount; ++set) {
		const auto &writes = (set < descriptorSets.writeDescriptorSets.size()) ? descriptorSets.writeDescriptorSets[set] : std::vector<VkWriteDescriptorSet>{};
		const auto &indices = (set < descriptorSets.writeDescriptorBufferInfoIndex.size()) ? descriptorSets.writeDescriptorBufferInfoIndex[set] : std::vector<uint32_t>{};

		for (size_t i = 0; i < writes.size(); ++i) {
			auto w = writes[i];
			w.dstSet = descriptorSets.descriptorSets[set];

			// If this write is a buffer write, hook up the (now-stable) pointer.
			if ((w.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) || (w.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) || (w.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) || (w.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)) {
				uint32_t idx = indices[i];
				w.pBufferInfo = &descriptorSets.descriptorBuffersInfo[idx];
			}

			allWrites.push_back(w);
		}
	}
	if (!allWrites.empty()) {
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(allWrites.size()), allWrites.data(), 0, nullptr);
	}

	// --- Rebuild dynamic bindings order (set → binding → arrayElement) ---
	descriptorSets.dynamicOrder.clear();
	descriptorSets.dynamicOffsets.clear();

	for (uint32_t setIdx = 0; setIdx < descriptorSets.descriptorSetLayoutBindings.size(); ++setIdx) {
		const auto &binds = descriptorSets.descriptorSetLayoutBindings[setIdx];
		for (const auto &b : binds) {
			const bool isDyn = (b.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) || (b.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
			if (!isDyn)
				continue;

			for (uint32_t ae = 0; ae < b.descriptorCount; ++ae) {
				descriptorSets.dynamicOrder.push_back({setIdx, b.binding, ae});
				descriptorSets.dynamicOffsets.push_back(0u); // filled per-frame with setDynamicOffset(...)
			}
		}
	}
}

void Pipeline::createGraphicsPipeline() {
	auto &pl = graphicsPipeline.pipelineLayoutCI;
	pl.setLayoutCount = static_cast<uint32_t>(descriptorSets.descriptorSetsLayout.size());
	pl.pSetLayouts = descriptorSets.descriptorSetsLayout.empty() ? nullptr : descriptorSets.descriptorSetsLayout.data();
	if (graphicsPipeline.pushConstantRangeCount > 0) {
		pl.pushConstantRangeCount = graphicsPipeline.pushConstantRangeCount;
		pl.pPushConstantRanges = &graphicsPipeline.pushContantRanges;
	}
	VK_CHECK(vkCreatePipelineLayout(device, &pl, nullptr, &pipelineLayout));

	auto &vi = graphicsPipeline.vertexInputStateCI;
	auto &b = graphicsPipeline.vertexInputBindingDescriptions;
	auto &attrs = graphicsPipeline.vertexInputAttributeDescription;
	vi.vertexBindingDescriptionCount = b.size();
	vi.pVertexBindingDescriptions = b.data();
	vi.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
	vi.pVertexAttributeDescriptions = attrs.data();

	auto &ia = graphicsPipeline.inputAssemblyStateCI;
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	auto &dyn = graphicsPipeline.dynamicStateCI;
	dyn.dynamicStateCount = 2;
	dyn.pDynamicStates = graphicsPipeline.dynamicStates;
	graphicsPipeline.dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
	graphicsPipeline.dynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;

	auto &vpst = graphicsPipeline.viewportStateCI;
	vpst.viewportCount = 1;
	vpst.scissorCount = 1;

	auto &rs = graphicsPipeline.rasterizationStateCI;
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rs.lineWidth = 1.0f;

	auto &ms = graphicsPipeline.multisamplingStateCI;
	ms.rasterizationSamples = samplesCountFlagBits ? samplesCountFlagBits : VK_SAMPLE_COUNT_1_BIT;

	auto &ds = graphicsPipeline.depthStencilStateCI;
	ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

	auto &att = graphicsPipeline.colorBlendAttachmentState;
	att.blendEnable = VK_TRUE;
	att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	att.colorBlendOp = VK_BLEND_OP_ADD;
	att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	att.alphaBlendOp = VK_BLEND_OP_ADD;
	att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	auto &cb = graphicsPipeline.colorBlendStateCI;
	cb.attachmentCount = 1;
	cb.pAttachments = &att;

	auto push_stage = [&](VkShaderModule mod, VkShaderStageFlagBits st) {
		if (mod != VK_NULL_HANDLE) {
			VkPipelineShaderStageCreateInfo s{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
			s.stage = st;
			s.module = mod;
			s.pName = "main";
			graphicsPipeline.pipelineShaderStageCI.push_back(s);
		}
	};

	// Graphics-only:
	push_stage(shaders.vertexShader, VK_SHADER_STAGE_VERTEX_BIT);
	push_stage(shaders.tessellationControlShader, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
	push_stage(shaders.tessellationEvaluationShader, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
	push_stage(shaders.geometryShader, VK_SHADER_STAGE_GEOMETRY_BIT);
	push_stage(shaders.fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT);

	if (graphicsPipeline.pipelineShaderStageCI.empty())
		throw std::runtime_error("Model::createPipeline: no graphics shader stages supplied");

	auto &renderingInfo = graphicsPipeline.pipelineRenderingCI;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachmentFormats = &graphicsPipeline.colorFormat;
	renderingInfo.depthAttachmentFormat = graphicsPipeline.depthFormat;

	auto &gp = graphicsPipeline.graphicsPipelineCI;
	auto &stages = graphicsPipeline.pipelineShaderStageCI;
	gp.pNext = &renderingInfo;
	gp.renderPass = VK_NULL_HANDLE;
	gp.renderPass = VK_NULL_HANDLE;
	gp.stageCount = static_cast<uint32_t>(stages.size());
	gp.pStages = stages.data();
	gp.pVertexInputState = &vi;
	gp.pInputAssemblyState = &ia;
	gp.pViewportState = &vpst;
	gp.pRasterizationState = &rs;
	gp.pMultisampleState = &ms;
	gp.pDepthStencilState = &ds;
	gp.pColorBlendState = &cb;
	gp.pDynamicState = &dyn;
	gp.layout = pipelineLayout;
	gp.subpass = 0;

	VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gp, nullptr, &pipeline));
}

void Pipeline::createComputePipeline() {
	if (shaders.computeShader == VK_NULL_HANDLE) {
		throw std::runtime_error("Pipeline::createComputePipeline: compute shader is null");
	}

	computePipeline.pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	computePipeline.pipelineLayoutCI.setLayoutCount = (uint32_t)descriptorSets.descriptorSetsLayout.size();
	computePipeline.pipelineLayoutCI.pSetLayouts = descriptorSets.descriptorSetsLayout.empty() ? nullptr : descriptorSets.descriptorSetsLayout.data();
	VK_CHECK(vkCreatePipelineLayout(device, &computePipeline.pipelineLayoutCI, nullptr, &pipelineLayout));

	VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = shaders.computeShader;
	stage.pName = "main";

	computePipeline.computePipelineCI.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipeline.computePipelineCI.stage = stage;
	computePipeline.computePipelineCI.layout = pipelineLayout;

	VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipeline.computePipelineCI, nullptr, &pipeline));
}

void Pipeline::createDescriptorSetLayoutBinding(uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags flags, uint32_t descriptorCount, uint32_t setIndex) {
	if (descriptorSets.descriptorSetLayoutBindings.size() <= setIndex) {
		descriptorSets.descriptorSetLayoutBindings.resize(setIndex + 1);
	}

	VkDescriptorSetLayoutBinding b{};
	b.binding = binding;
	b.descriptorType = descriptorType;
	b.descriptorCount = descriptorCount;
	b.stageFlags = flags;

	descriptorSets.descriptorSetLayoutBindings[setIndex].push_back(b);
}

void Pipeline::createWriteDescriptorSet(uint32_t dstBinding, VkDescriptorType descriptorType, const VkDescriptorBufferInfo &bufInfo, uint32_t descriptorCount /*=1*/, uint32_t setIndex /*=0*/) {
	if (descriptorSets.writeDescriptorSets.size() <= setIndex)
		descriptorSets.writeDescriptorSets.resize(setIndex + 1);
	if (descriptorSets.writeDescriptorBufferInfoIndex.size() <= setIndex)
		descriptorSets.writeDescriptorBufferInfoIndex.resize(setIndex + 1);

	descriptorSets.descriptorBuffersInfo.push_back(bufInfo);
	uint32_t idx = static_cast<uint32_t>(descriptorSets.descriptorBuffersInfo.size() - 1);
	descriptorSets.writeDescriptorBufferInfoIndex[setIndex].push_back(idx);

	VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	w.dstBinding = dstBinding; // dstSet patched later
	w.descriptorType = descriptorType;
	w.descriptorCount = descriptorCount;
	w.pBufferInfo = nullptr;
	descriptorSets.writeDescriptorSets[setIndex].push_back(w);
}

void Pipeline::setDynamicOffset(uint32_t setIndex, uint32_t binding, uint32_t offsetBytes, uint32_t arrayElement) {
	for (size_t i = 0; i < descriptorSets.dynamicOrder.size(); ++i) {
		const auto &d = descriptorSets.dynamicOrder[i];
		if (d.setIndex == setIndex && d.binding == binding && d.arrayElement == arrayElement) {
			descriptorSets.dynamicOffsets[i] = offsetBytes;
			return;
		}
	}
}

void Pipeline::createVertexInputBindingDescription(uint32_t binding, uint32_t stride, VkVertexInputRate inputRate) {
	VkVertexInputBindingDescription b{};
	b.binding = binding;
	b.stride = stride;
	b.inputRate = inputRate;
	graphicsPipeline.vertexInputBindingDescriptions.push_back(b);
}

void Pipeline::createBuffer(VkDeviceSize sz, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer &buf, VkDeviceMemory &mem) {
	VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	bi.size = sz;
	bi.usage = usage;
	bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK(vkCreateBuffer(device, &bi, nullptr, &buf));

	VkMemoryRequirements req{};
	vkGetBufferMemoryRequirements(device, buf, &req);

	VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	ai.allocationSize = req.size;
	ai.memoryTypeIndex = Memory::findMemoryType(physicalDevice, req.memoryTypeBits, props);

	VK_CHECK(vkAllocateMemory(device, &ai, nullptr, &mem));
	VK_CHECK(vkBindBufferMemory(device, buf, mem, 0));
}
