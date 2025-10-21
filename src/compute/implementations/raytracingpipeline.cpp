#include "raytracingpipeline.hpp"
#include "model.hpp"
#include <algorithm>

RayTracingPipeline::RayTracingPipeline(Model *model) : model(model) {}

RayTracingPipeline::~RayTracingPipeline() {
	if (rayTracingProgram.computeShader != VK_NULL_HANDLE) {
		vkDestroyShaderModule(Engine::device, rayTracingProgram.computeShader, nullptr);
		rayTracingProgram.computeShader = VK_NULL_HANDLE;
	}

	auto D = [&](VkBuffer &b, VkDeviceMemory &m) {
		if (b) {
			vkDestroyBuffer(Engine::device, b, nullptr);
		}
		if (m) {
			vkFreeMemory(Engine::device, m, nullptr);
		}
		b = VK_NULL_HANDLE;
		m = VK_NULL_HANDLE;
	};

	D(nodesBuf, nodesMem);
	D(trisBuf, trisMem);
	D(posBuf, posMem);
	if (pickUBOMapped) {
		vkUnmapMemory(Engine::device, pickUBOMem);
	}
	D(pickUBO, pickUBOMem);
	if (hitMapped) {
		vkUnmapMemory(Engine::device, hitMem);
	}
	D(hitBuf, hitMem);
}

void RayTracingPipeline::initialize() {
    if (initialized) {
        return;
    }
	createComputeDescriptorSetLayout();
	createShaderStorageBuffers();
	createComputeDescriptorSets();
	createComputePipeline();
    initialized = true;
}

void RayTracingPipeline::setRayTraceFromViewportPx(float px, float py, const VkViewport &vp) {
	// Handle possible negative-height viewports (legal in Vulkan).
	const float w = vp.width;
	const float hAbs = std::abs(vp.height);

	// Convert pixel -> viewport-local pixels (y respecting sign of height)
	const float xLocal = (px - vp.x) + 0.5f;
	const float yLocal = (vp.height >= 0.0f) ? (py - vp.y) + 0.5f  // origin at vp.y (top-left style)
											 : (vp.y - py) + 0.5f; // flipped viewport

	// Normalize -> [0,1]
	const float sx = xLocal / w;
	const float sy = yLocal / hAbs;

	// Map to NDC [-1,1] with +Y up (since you already flip proj[1][1]*=-1)
	const float ndcX = sx * 2.0f - 1.0f;
	const float ndcY = sy * 2.0f - 1.0f;

	rayTraceParams.mouseNdc = {ndcX, ndcY};
}

RayTracingPipeline::AABB RayTracingPipeline::merge(const AABB &a, const AABB &b) { return {glm::min(a.bmin, b.bmin), glm::max(a.bmax, b.bmax)}; }

RayTracingPipeline::AABB RayTracingPipeline::triAabb(const vec3 &A, const vec3 &B, const vec3 &C) const {
	AABB r;
	r.bmin = glm::min(A, glm::min(B, C));
	r.bmax = glm::max(A, glm::max(B, C));
	return r;
}

int RayTracingPipeline::buildNode(std::vector<BuildTri> &tris, int begin, int end, int depth, std::vector<BuildNode> &out) {
	BuildNode node;
	node.b = {vec3(FLT_MAX), vec3(-FLT_MAX)};
	for (int i = begin; i < end; ++i)
		node.b = merge(node.b, tris[i].b);

	const int count = end - begin;
	const int maxLeaf = 8;
	if (count <= maxLeaf || depth > 32) {
		node.firstTri = begin;
		node.triCount = count;
		out.push_back(node);
		return (int)out.size() - 1;
	}

	// Split along largest axis by centroid median
	vec3 ext = node.b.bmax - node.b.bmin;
	int axis = (ext.x > ext.y && ext.x > ext.z) ? 0 : (ext.y > ext.z ? 1 : 2);
	int mid = (begin + end) / 2;
	std::nth_element(tris.begin() + begin, tris.begin() + mid, tris.begin() + end, [axis](const BuildTri &a, const BuildTri &b) { return a.centroid[axis] < b.centroid[axis]; });

	int leftIdx = buildNode(tris, begin, mid, depth + 1, out);
	int rightIdx = buildNode(tris, mid, end, depth + 1, out);

	node.left = leftIdx;
	node.right = rightIdx;
	out.push_back(node);
	return (int)out.size() - 1;
}

void RayTracingPipeline::updateComputeUniformBuffer() {
	float mousePx = 0.0f, mousePy = 0.0f;
	Platform::GetPointerInFramebufferPixels(mousePx, mousePy);

	setRayTraceFromViewportPx(mousePx, mousePy, model->screenParams.viewport);

	mat4 invVP = inverse(model->mvp.proj * model->mvp.view);
	mat4 invV = inverse(model->mvp.view);

	vec3 camPos = vec3(invV[3]);

	PickingUBO p{};
	p.invViewProj = invVP;
	p.invModel = inverse(model->mvp.model);
	p.mouseNdc = rayTraceParams.mouseNdc;
	p.camPos = rayTraceParams.camPos == vec3(0.0f) ? camPos : rayTraceParams.camPos;
	p._pad = rayTraceParams._pad0;

    std::memcpy(pickUBOMapped, &p, sizeof(PickingUBO));

	// if (hitMapped) {
	// 	const uint32_t code = hitMapped->hit;
	// 	if (code != 0) {
	// 		std::cout << "[PickDBG] code=" << code << " primId=" << hitMapped->primId << " t=" << hitMapped->t << std::endl;

	// 		// Important: clear so you only print once per event
	// 		hitMapped->hit = 0;
	// 	}
	// }

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

void RayTracingPipeline::compute() {
	VkCommandBuffer cmd = Engine::currentComputeCommandBuffer();
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &computeDescriptorSet, 0, nullptr);
	vkCmdDispatch(cmd, 1, 1, 1);

	// Make the write to hitBuf visible to the host when the queue completes
	VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
	mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	mb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr);
}

void RayTracingPipeline::createComputeDescriptorSetLayout() {
	// set=0 bindings: 0=nodes SSBO, 1=tris SSBO, 2=positions SSBO, 3=UBO, 4=result SSBO
	VkDescriptorSetLayoutBinding b[5]{};

	b[0].binding = 0;
	b[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	b[0].descriptorCount = 1;
	b[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	b[1].binding = 1;
	b[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	b[1].descriptorCount = 1;
	b[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	b[2].binding = 2;
	b[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	b[2].descriptorCount = 1;
	b[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	b[3].binding = 3;
	b[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	b[3].descriptorCount = 1;
	b[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	b[4].binding = 4;
	b[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	b[4].descriptorCount = 1;
	b[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
	ci.bindingCount = 5;
	ci.pBindings = b;

	if (vkCreateDescriptorSetLayout(Engine::device, &ci, nullptr, &computeDescriptorSetLayout) != VK_SUCCESS)
		throw std::runtime_error("compute DSL failed");

	// Small pool for one set
	std::array<VkDescriptorPoolSize, 2> ps{};
	ps[0] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4};
	ps[1] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1};

	VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
	pci.maxSets = 1;
	pci.poolSizeCount = (uint32_t)ps.size();
	pci.pPoolSizes = ps.data();

	if (vkCreateDescriptorPool(Engine::device, &pci, nullptr, &computePool) != VK_SUCCESS) {
		throw std::runtime_error("compute pool failed");
	}
}

void RayTracingPipeline::createShaderStorageBuffers() {
	model->buildBVH();

	auto createHostVisible = [&](VkDeviceSize sz, VkBufferUsageFlags usage, VkBuffer &buf, VkDeviceMemory &mem, void **mapped) {
		if (sz == 0)
			throw std::runtime_error("createHostVisible: size is 0");
		Engine::createBuffer(sz, usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, buf, mem);
		if (mapped) {
			vkMapMemory(Engine::device, mem, 0, sz, 0, mapped);
		}
	};

	auto createDeviceLocalUpload = [&](const void *src, VkDeviceSize sz, VkBufferUsageFlags usage, VkBuffer &buf, VkDeviceMemory &mem) {
		if (sz == 0)
			throw std::runtime_error("createDeviceLocalUpload: size is 0");
		// staging
		VkBuffer stg;
		VkDeviceMemory stgMem;
		Engine::createBuffer(sz, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stg, stgMem);
		void *p;
		vkMapMemory(Engine::device, stgMem, 0, sz, 0, &p);
		std::memcpy(p, src, sz);
		vkUnmapMemory(Engine::device, stgMem);
		// device local
		Engine::createBuffer(sz, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buf, mem);
		Engine::copyBuffer(stg, buf, sz);
		vkDestroyBuffer(Engine::device, stg, nullptr);
		vkFreeMemory(Engine::device, stgMem, nullptr);
	};

	// Sanity: BVH/triangles must exist
	if (bvhNodes.empty() || triGPU.empty() || posGPU.empty()) {
        throw std::runtime_error("BVH is empty");
	}

	// Nodes SSBO
	createDeviceLocalUpload(bvhNodes.data(), sizeof(BVHNodeGPU) * bvhNodes.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, nodesBuf, nodesMem);

	// Tris SSBO
	createDeviceLocalUpload(triGPU.data(), sizeof(TriIndexGPU) * triGPU.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, trisBuf, trisMem);

	// Positions SSBO (std430-friendly vec4, shader reads .xyz)
	std::vector<glm::vec4> posPacked;
	posPacked.reserve(posGPU.size());
	for (const auto &p : posGPU) {
		posPacked.emplace_back(p, 1.0f);
	}

	createDeviceLocalUpload(posPacked.data(), sizeof(glm::vec4) * posPacked.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, posBuf, posMem);

	// UBO (host visible)
	createHostVisible(sizeof(PickingUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, pickUBO, pickUBOMem, &pickUBOMapped);

	// Result SSBO (host visible)
	createHostVisible(sizeof(HitOutCPU), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, hitBuf, hitMem, (void **)&hitMapped);

	std::memset(hitMapped, 0, sizeof(HitOutCPU));
}

void RayTracingPipeline::createComputeDescriptorSets() {
	VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
	ai.descriptorPool = computePool;
	ai.descriptorSetCount = 1;
	ai.pSetLayouts = &computeDescriptorSetLayout;

	if (vkAllocateDescriptorSets(Engine::device, &ai, &computeDescriptorSet) != VK_SUCCESS)
		throw std::runtime_error("compute DS alloc failed");

	VkDescriptorBufferInfo nb{nodesBuf, 0, VK_WHOLE_SIZE};
	VkDescriptorBufferInfo tb{trisBuf, 0, VK_WHOLE_SIZE};
	VkDescriptorBufferInfo pb{posBuf, 0, VK_WHOLE_SIZE};
	VkDescriptorBufferInfo ub{pickUBO, 0, sizeof(PickingUBO)};
	VkDescriptorBufferInfo rb{hitBuf, 0, sizeof(HitOutCPU)};

	std::array<VkWriteDescriptorSet, 5> w{};
	auto W = [&](int i, uint32_t binding, VkDescriptorType t, const VkDescriptorBufferInfo *bi) {
		w[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[i].dstSet = computeDescriptorSet;
		w[i].dstBinding = binding;
		w[i].descriptorType = t;
		w[i].descriptorCount = 1;
		w[i].pBufferInfo = bi;
	};
	W(0, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &nb);
	W(1, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &tb);
	W(2, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &pb);
	W(3, 3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &ub);
	W(4, 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &rb);

	vkUpdateDescriptorSets(Engine::device, (uint32_t)w.size(), w.data(), 0, nullptr);
}

void RayTracingPipeline::createComputePipeline() {
	rayTracingProgram = Assets::compileShaderProgram(rayTracingShaderPath);
	if (rayTracingProgram.computeShader == VK_NULL_HANDLE) {
		throw std::runtime_error("compute shader missing (expect raytracing.comp)!");
	}

	VkPipelineLayoutCreateInfo pli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	pli.setLayoutCount = 1;
	pli.pSetLayouts = &computeDescriptorSetLayout;
	if (vkCreatePipelineLayout(Engine::device, &pli, nullptr, &computePipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("compute pipeline layout failed");
	}

	VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
	ci.stage = Engine::createShaderStageInfo(rayTracingProgram.computeShader, VK_SHADER_STAGE_COMPUTE_BIT);
	ci.layout = computePipelineLayout;

	if (vkCreateComputePipelines(Engine::device, VK_NULL_HANDLE, 1, &ci, nullptr, &computePipeline) != VK_SUCCESS) {
		throw std::runtime_error("compute pipeline failed");
	}
}
