#include "raytraycespipeline.hpp"
#include "model.hpp"
#include <algorithm>

RayTraycesPipeline::RayTraycesPipeline(Model *model, vector<InstanceXformGPU> &instCPU, vector<int> &idsCPU, uint32_t &instanceCount, uint32_t maxInstances) : instCPU(instCPU), idsCPU(idsCPU), instanceCount(instanceCount), maxInstances(maxInstances), RayTracingPipeline(model) { rayTracingShaderPath = Assets::shaderRootPath + "/instancedraytracing"; }

RayTraycesPipeline::~RayTraycesPipeline() {
	if (instBuf)
		vkDestroyBuffer(Engine::device, instBuf, nullptr);
	if (idBuf)
		vkDestroyBuffer(Engine::device, idBuf, nullptr);
	if (instMem) {
		vkUnmapMemory(Engine::device, instMem);
		vkFreeMemory(Engine::device, instMem, nullptr);
	}
	if (idMem) {
		vkUnmapMemory(Engine::device, idMem);
		vkFreeMemory(Engine::device, idMem, nullptr);
	}
}

void RayTraycesPipeline::upload(std::span<const InstanceXformGPU> x, std::span<const int> ids) {
	// Preconditions: pipeline initialized and buffers mapped
	if (!initialized || !instMapped || !idMapped)
		return;

	// Clamp to capacity just in case
	const size_t n = std::min({x.size(), ids.size(), static_cast<size_t>(maxInstances)});
	if (n == 0)
		return;

	// Copy to mapped GPU memory
	std::memcpy(instMapped, x.data(), n * sizeof(InstanceXformGPU));
	std::memcpy(idMapped, ids.data(), n * sizeof(int));

	// Flush if memory is non-coherent. Using WHOLE_SIZE is valid and avoids atom-size math.
	const VkMappedMemoryRange ranges[2] = {{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, instMem, 0, VK_WHOLE_SIZE}, {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, idMem, 0, VK_WHOLE_SIZE}};
	vkFlushMappedMemoryRanges(Engine::device, 2, ranges);
}

void RayTraycesPipeline::updateComputeUniformBuffer() {
	float mousePx = 0, mousePy = 0;
	Platform::GetPointerInFramebufferPixels(mousePx, mousePy);
	setRayTraceFromViewportPx(mousePx, mousePy, model->screenParams.viewport);

	glm::mat4 invVP = inverse(model->ubo.proj * model->ubo.view);
	glm::mat4 invV = inverse(model->ubo.view);
	glm::vec3 cam = glm::vec3(invV[3]);

	PickingUBO p{};
	p.invViewProj = invVP;
	p.invModel = glm::mat4(1.0f); // not used by instanced path
	p.mouseNdc = rayTraceParams.mouseNdc;
	p.camPos = (rayTraceParams.camPos == glm::vec3(0)) ? cam : rayTraceParams.camPos;
	p._pad = (int)instanceCount; // <--- instance count

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

void RayTraycesPipeline::createComputeDescriptorSetLayout() {
	// Base had 5 bindings (0..4). Add 5: instances, 6: ids
	std::array<VkDescriptorSetLayoutBinding, 7> b{};

	b[0] = {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}; // nodes
	b[1] = {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}; // tris
	b[2] = {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}; // pos
	b[3] = {3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}; // params
	b[4] = {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}; // out
	b[5] = {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}; // inst xforms
	b[6] = {6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}; // slot->key

	VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
	ci.bindingCount = (uint32_t)b.size();
	ci.pBindings = b.data();
	if (vkCreateDescriptorSetLayout(Engine::device, &ci, nullptr, &computeDescriptorSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("compute DSL failed");
	}

	// small pool for 1 set (6 storage + 1 uniform)
	std::array<VkDescriptorPoolSize, 2> ps{VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6}, VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}};
	VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
	pci.maxSets = 1;
	pci.poolSizeCount = (uint32_t)ps.size();
	pci.pPoolSizes = ps.data();
	if (vkCreateDescriptorPool(Engine::device, &pci, nullptr, &computePool) != VK_SUCCESS) {
		throw std::runtime_error("compute pool failed");
	}
}

void RayTraycesPipeline::createComputeDescriptorSets() {
	// allocate one set with our 7-binding layout
	VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
	ai.descriptorPool = computePool;
	ai.descriptorSetCount = 1;
	ai.pSetLayouts = &computeDescriptorSetLayout;

	if (vkAllocateDescriptorSets(Engine::device, &ai, &computeDescriptorSet) != VK_SUCCESS)
		throw std::runtime_error("instanced compute DS alloc failed");

	// buffer infos
	VkDescriptorBufferInfo nb{nodesBuf, 0, VK_WHOLE_SIZE};
	VkDescriptorBufferInfo tb{trisBuf, 0, VK_WHOLE_SIZE};
	VkDescriptorBufferInfo pb{posBuf, 0, VK_WHOLE_SIZE};
	VkDescriptorBufferInfo ub{pickUBO, 0, sizeof(PickingUBO)};
	VkDescriptorBufferInfo rb{hitBuf, 0, sizeof(HitOutCPU)};
	VkDescriptorBufferInfo xb{instBuf, 0, VK_WHOLE_SIZE}; // binding 5
	VkDescriptorBufferInfo ib{idBuf, 0, VK_WHOLE_SIZE};	  // binding 6

	std::array<VkWriteDescriptorSet, 7> w{};
	auto W = [&](uint32_t i, uint32_t binding, VkDescriptorType type, const VkDescriptorBufferInfo *bi) {
		w[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[i].dstSet = computeDescriptorSet;
		w[i].dstBinding = binding;
		w[i].descriptorType = type;
		w[i].descriptorCount = 1;
		w[i].pBufferInfo = bi;
	};

	W(0, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &nb); // nodes
	W(1, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &tb); // tris
	W(2, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &pb); // pos
	W(3, 3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &ub); // params
	W(4, 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &rb); // out
	W(5, 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &xb); // inst xforms
	W(6, 6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &ib); // ids

	vkUpdateDescriptorSets(Engine::device, (uint32_t)w.size(), w.data(), 0, nullptr);
}

void RayTraycesPipeline::createShaderStorageBuffers() {
	RayTracingPipeline::createShaderStorageBuffers();

	const VkDeviceSize xSzFull = sizeof(InstanceXformGPU) * maxInstances;
	const VkDeviceSize iSzFull = sizeof(int) * maxInstances;

	auto makeHostVisible = [&](VkDeviceSize sz, VkBufferUsageFlags usage, VkBuffer &buf, VkDeviceMemory &mem, void **mapped) {
		Engine::createBuffer(sz, usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, buf, mem);
		if (vkMapMemory(Engine::device, mem, 0, VK_WHOLE_SIZE, 0, mapped) != VK_SUCCESS) {
			throw std::runtime_error("Failed to map instanced raytracing buffers");
		}
	};

	makeHostVisible(xSzFull, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, instBuf, instMem, &instMapped);
	makeHostVisible(iSzFull, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, idBuf, idMem, &idMapped);
}

void RayTraycesPipeline::createComputePipeline() {
	rayTracingProgram = Assets::compileShaderProgram(rayTracingShaderPath);
	if (rayTracingProgram.computeShader == VK_NULL_HANDLE)
		throw std::runtime_error("instanced compute shader missing!");

	VkPipelineLayoutCreateInfo pli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	pli.setLayoutCount = 1;
	pli.pSetLayouts = &computeDescriptorSetLayout;
	if (vkCreatePipelineLayout(Engine::device, &pli, nullptr, &computePipelineLayout) != VK_SUCCESS)
		throw std::runtime_error("compute pipeline layout failed");

	VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
	ci.stage = Engine::createShaderStageInfo(rayTracingProgram.computeShader, VK_SHADER_STAGE_COMPUTE_BIT);
	ci.layout = computePipelineLayout;
	if (vkCreateComputePipelines(Engine::device, VK_NULL_HANDLE, 1, &ci, nullptr, &computePipeline) != VK_SUCCESS) {
		throw std::runtime_error("instanced compute pipeline failed");
	}
}
