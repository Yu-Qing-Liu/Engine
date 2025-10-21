#include "textraytracing.hpp"
#include "engine.hpp"
#include "model.hpp"
#include <cstring>
#include <stdexcept>

void TextRayTracing::createComputeDescriptorSetLayout() {
	// binding 0: UBO (PickingUBO), 1: spans (GlyphSpanGPU[]), 2: hit (HitOutCPU)
	VkDescriptorSetLayoutBinding b[3]{};

	b[0] = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
	b[1] = {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
	b[2] = {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};

	VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
	ci.bindingCount = 3;
	ci.pBindings = b;
	if (vkCreateDescriptorSetLayout(Engine::device, &ci, nullptr, &computeDescriptorSetLayout) != VK_SUCCESS)
		throw std::runtime_error("TextRT: descriptor set layout failed");

	// pool: 1 UBO + 2 SSBOs
	std::array<VkDescriptorPoolSize, 2> ps{VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}, VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2}};
	VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
	pci.maxSets = 1;
	pci.poolSizeCount = (uint32_t)ps.size();
	pci.pPoolSizes = ps.data();
	if (vkCreateDescriptorPool(Engine::device, &pci, nullptr, &computePool) != VK_SUCCESS)
		throw std::runtime_error("TextRT: descriptor pool failed");
}

void TextRayTracing::createShaderStorageBuffers() {
	// Spans (host visible, mapped)
	const VkDeviceSize spansSz = sizeof(GlyphSpanGPU) * std::max<uint32_t>(maxGlyphs, 1);
	Engine::createBuffer(spansSz, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, spansBuf, spansMem);
	if (vkMapMemory(Engine::device, spansMem, 0, VK_WHOLE_SIZE, 0, &spansMapped) != VK_SUCCESS)
		throw std::runtime_error("TextRT: map spans failed");

	// UBO (host visible, mapped)
	Engine::createBuffer(sizeof(PickingUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, pickUBO, pickUBOMem);
	if (vkMapMemory(Engine::device, pickUBOMem, 0, VK_WHOLE_SIZE, 0, &pickUBOMapped) != VK_SUCCESS)
		throw std::runtime_error("TextRT: map UBO failed");

	// Hit SSBO (host visible, mapped)
	Engine::createBuffer(sizeof(HitOutCPU), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, hitBuf, hitMem);
	if (vkMapMemory(Engine::device, hitMem, 0, VK_WHOLE_SIZE, 0, (void **)&hitMapped) != VK_SUCCESS)
		throw std::runtime_error("TextRT: map hit failed");
	std::memset(hitMapped, 0, sizeof(HitOutCPU));
}

void TextRayTracing::createComputeDescriptorSets() {
	VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
	ai.descriptorPool = computePool;
	ai.descriptorSetCount = 1;
	ai.pSetLayouts = &computeDescriptorSetLayout;
	if (vkAllocateDescriptorSets(Engine::device, &ai, &computeDescriptorSet) != VK_SUCCESS)
		throw std::runtime_error("TextRT: DS alloc failed");

	VkDescriptorBufferInfo ub{pickUBO, 0, sizeof(PickingUBO)};
	VkDescriptorBufferInfo sb{spansBuf, 0, VK_WHOLE_SIZE};
	VkDescriptorBufferInfo hb{hitBuf, 0, sizeof(HitOutCPU)};

	VkWriteDescriptorSet w[3]{};
	auto W = [&](uint32_t i, uint32_t binding, VkDescriptorType t, const VkDescriptorBufferInfo *bi) {
		w[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[i].dstSet = computeDescriptorSet;
		w[i].dstBinding = binding;
		w[i].descriptorType = t;
		w[i].descriptorCount = 1;
		w[i].pBufferInfo = bi;
	};
	W(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &ub);
	W(1, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &sb);
	W(2, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &hb);

	vkUpdateDescriptorSets(Engine::device, 3, w, 0, nullptr);
}

void TextRayTracing::createComputePipeline() {
	rayTracingProgram = Assets::compileShaderProgram(rayTracingShaderPath);
	if (rayTracingProgram.computeShader == VK_NULL_HANDLE)
		throw std::runtime_error("TextRT: compute shader missing!");

	VkPipelineLayoutCreateInfo pli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	pli.setLayoutCount = 1;
	pli.pSetLayouts = &computeDescriptorSetLayout;
	if (vkCreatePipelineLayout(Engine::device, &pli, nullptr, &computePipelineLayout) != VK_SUCCESS)
		throw std::runtime_error("TextRT: pipeline layout failed");

	VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
	ci.stage = Engine::createShaderStageInfo(rayTracingProgram.computeShader, VK_SHADER_STAGE_COMPUTE_BIT);
	ci.layout = computePipelineLayout;
	if (vkCreateComputePipelines(Engine::device, VK_NULL_HANDLE, 1, &ci, nullptr, &computePipeline) != VK_SUCCESS)
		throw std::runtime_error("TextRT: compute pipeline failed");
}

void TextRayTracing::uploadSpans(std::span<const GlyphSpanGPU> spans) {
	if (!spansMapped)
		return;
	const size_t n = std::min(spans.size(), size_t(maxGlyphs));
	if (n == 0) {
		glyphCount = 0;
		return;
	}
	std::memcpy(spansMapped, spans.data(), n * sizeof(GlyphSpanGPU));

	const VkMappedMemoryRange r{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, spansMem, 0, VK_WHOLE_SIZE};
	vkFlushMappedMemoryRanges(Engine::device, 1, &r);
	glyphCount = (uint32_t)n;
}

void TextRayTracing::updateComputeUniformBuffer() {
	float mousePx = 0.f, mousePy = 0.f;
	Platform::GetPointerInFramebufferPixels(mousePx, mousePy);
	setRayTraceFromViewportPx(mousePx, mousePy, model->screenParams.viewport);

	glm::mat4 invVP = inverse(model->mvp.proj * model->mvp.view);
	glm::mat4 invV = inverse(model->mvp.view);
	glm::vec3 cam = glm::vec3(invV[3]);

	PickingUBO p{};
	p.invViewProj = invVP;
	p.invModel = glm::inverse(model->mvp.model);
	p.mouseNdc = rayTraceParams.mouseNdc;
	p.camPos = (rayTraceParams.camPos == glm::vec3(0)) ? cam : rayTraceParams.camPos;
	p._pad = (int)glyphCount; // reuse padding for count

	std::memcpy(pickUBOMapped, &p, sizeof(PickingUBO));

	if (hitMapped && hitMapped->hit) {
		hitPos = hitMapped->hitPos;
		rayLength = hitMapped->rayLen;
		hitMapped->hit = 0;
	} else {
		hitPos.reset();
		rayLength.reset();
		model->setMouseIsOver(false);
	}
}

void TextRayTracing::compute() {
	if (glyphCount == 0)
		return;

	VkCommandBuffer cmd = Engine::currentComputeCommandBuffer();
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &computeDescriptorSet, 0, nullptr);

	// exactly one workgroup; shader loops over all glyphs
	vkCmdDispatch(cmd, 1, 1, 1);

	VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
	mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	mb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr);
}
