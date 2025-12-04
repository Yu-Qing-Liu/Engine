#include "raypicking.hpp"
#include "assets.hpp"
#include "debug.hpp"

#include <algorithm>
#include <functional>
#include <stdexcept>

RayPicking::RayPicking() { pipeline = std::make_unique<Pipeline>(); }

RayPicking::~RayPicking() { destroy(); }

void RayPicking::init(VkDevice device, VkPhysicalDevice physicalDevice) {
	if (!device || !physicalDevice)
		throw std::runtime_error("RayPicking::init: device/physicalDevice not set");

	maxInstances = initInfo.maxInstances ? initInfo.maxInstances : 1;
	nodesBytes = initInfo.nodesBytes;
	trisBytes = initInfo.trisBytes;
	posBytes = initInfo.posBytes;

	// pipeline core wiring (like Model::init)
	pipeline->device = device;
	pipeline->physicalDevice = physicalDevice;
	pipeline->descriptorPool = initInfo.dpool;

	if (pipeline->descriptorPool == VK_NULL_HANDLE) {
		VkDescriptorPoolSize sizes[] = {
			{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6},
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
		};
		VkDescriptorPoolCreateInfo dpci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
		dpci.maxSets = 1;
		dpci.poolSizeCount = (uint32_t)std::size(sizes);
		dpci.pPoolSizes = sizes;
		VK_CHECK(vkCreateDescriptorPool(device, &dpci, nullptr, &pipeline->descriptorPool));
		descriptorPool = pipeline->descriptorPool;
	} else {
		descriptorPool = initInfo.dpool;
	}

	// create buffers (HOST_VISIBLE|COHERENT for clarity)
	pipeline->createBuffer(nz(nodesBytes), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, nodesBuf, nodesMem);
	pipeline->createBuffer(nz(trisBytes), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, trisBuf, trisMem);
	pipeline->createBuffer(nz(posBytes), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, posBuf, posMem);

	pipeline->createBuffer(sizeof(HitOutCPU), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, outBuf, outMem);
	pipeline->createBuffer(sizeof(InstanceXformGPU) * maxInstances, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, instBuf, instMem);
	pipeline->createBuffer(sizeof(int) * maxInstances, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, idsBuf, idsMem);
	pipeline->createBuffer(sizeof(PickingUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uboBuf, uboMem);

	// persistently map what we frequently update/read
	VK_CHECK(vkMapMemory(device, instMem, 0, VK_WHOLE_SIZE, 0, &mappedInst));
	VK_CHECK(vkMapMemory(device, idsMem, 0, VK_WHOLE_SIZE, 0, &mappedIds));
	VK_CHECK(vkMapMemory(device, outMem, 0, VK_WHOLE_SIZE, 0, &mappedOut));
	VK_CHECK(vkMapMemory(device, uboMem, 0, VK_WHOLE_SIZE, 0, &mappedUBO));

	std::memset(mappedOut, 0, sizeof(HitOutCPU));
	uboDirty = true;

	pipeline->shaders = initInfo.shaders;

	createDescriptors();
	pipeline->createComputePipeline();

	uploadStatic(bvhNodes, triGPU, posGPU);
}

void RayPicking::destroy() {
	if (!pipeline)
		return;

	const auto &dev = pipeline->device;

	if (mappedInst) {
		vkUnmapMemory(dev, instMem);
		mappedInst = nullptr;
	}
	if (mappedIds) {
		vkUnmapMemory(dev, idsMem);
		mappedIds = nullptr;
	}
	if (mappedOut) {
		vkUnmapMemory(dev, outMem);
		mappedOut = nullptr;
	}
	if (mappedUBO) {
		vkUnmapMemory(dev, uboMem);
		mappedUBO = nullptr;
	}

	if (nodesBuf) {
		vkDestroyBuffer(dev, nodesBuf, nullptr);
		nodesBuf = VK_NULL_HANDLE;
	}
	if (trisBuf) {
		vkDestroyBuffer(dev, trisBuf, nullptr);
		trisBuf = VK_NULL_HANDLE;
	}
	if (posBuf) {
		vkDestroyBuffer(dev, posBuf, nullptr);
		posBuf = VK_NULL_HANDLE;
	}
	if (instBuf) {
		vkDestroyBuffer(dev, instBuf, nullptr);
		instBuf = VK_NULL_HANDLE;
	}
	if (idsBuf) {
		vkDestroyBuffer(dev, idsBuf, nullptr);
		idsBuf = VK_NULL_HANDLE;
	}
	if (outBuf) {
		vkDestroyBuffer(dev, outBuf, nullptr);
		outBuf = VK_NULL_HANDLE;
	}
	if (uboBuf) {
		vkDestroyBuffer(dev, uboBuf, nullptr);
		uboBuf = VK_NULL_HANDLE;
	}

	if (nodesMem) {
		vkFreeMemory(dev, nodesMem, nullptr);
		nodesMem = VK_NULL_HANDLE;
	}
	if (trisMem) {
		vkFreeMemory(dev, trisMem, nullptr);
		trisMem = VK_NULL_HANDLE;
	}
	if (posMem) {
		vkFreeMemory(dev, posMem, nullptr);
		posMem = VK_NULL_HANDLE;
	}
	if (instMem) {
		vkFreeMemory(dev, instMem, nullptr);
		instMem = VK_NULL_HANDLE;
	}
	if (idsMem) {
		vkFreeMemory(dev, idsMem, nullptr);
		idsMem = VK_NULL_HANDLE;
	}
	if (outMem) {
		vkFreeMemory(dev, outMem, nullptr);
		outMem = VK_NULL_HANDLE;
	}
	if (uboMem) {
		vkFreeMemory(dev, uboMem, nullptr);
		uboMem = VK_NULL_HANDLE;
	}
}

// ---------- descriptors / pipeline ----------

void RayPicking::createDescriptors() {
	// set=0 bindings 0..6 (compute stage)
	const auto CS = VK_SHADER_STAGE_COMPUTE_BIT;
	pipeline->createDescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, CS); // nodes
	pipeline->createDescriptorSetLayoutBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, CS); // tris
	pipeline->createDescriptorSetLayoutBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, CS); // pos
	pipeline->createDescriptorSetLayoutBinding(3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, CS); // ubo
	pipeline->createDescriptorSetLayoutBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, CS); // out
	pipeline->createDescriptorSetLayoutBinding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, CS); // inst
	pipeline->createDescriptorSetLayoutBinding(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, CS); // ids

	// write descriptors
	VkDescriptorBufferInfo nfo{nodesBuf, 0, nz(nodesBytes)};
	VkDescriptorBufferInfo tfo{trisBuf, 0, nz(trisBytes)};
	VkDescriptorBufferInfo pfo{posBuf, 0, nz(posBytes)};
	VkDescriptorBufferInfo ufo{uboBuf, 0, sizeof(PickingUBO)};
	VkDescriptorBufferInfo ofo{outBuf, 0, sizeof(HitOutCPU)};
	VkDescriptorBufferInfo ifo{instBuf, 0, sizeof(InstanceXformGPU) * maxInstances};
	VkDescriptorBufferInfo idfo{idsBuf, 0, sizeof(int) * maxInstances};

	pipeline->createWriteDescriptorSet(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nfo);
	pipeline->createWriteDescriptorSet(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, tfo);
	pipeline->createWriteDescriptorSet(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, pfo);
	pipeline->createWriteDescriptorSet(3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, ufo);
	pipeline->createWriteDescriptorSet(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, ofo);
	pipeline->createWriteDescriptorSet(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, ifo);
	pipeline->createWriteDescriptorSet(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, idfo);

	pipeline->createDescriptors();
}

// ---------- uploads ----------

void RayPicking::uploadStatic(std::span<const BVHNodeGPU> nodes, std::span<const TriIndexGPU> tris, std::span<const glm::vec4> positions) {
	if (!pipeline)
		return;
	const auto &dev = pipeline->device;

	if (!nodes.empty()) {
		if (nodesBytes < nodes.size_bytes())
			throw std::runtime_error("RayPicking: nodes buffer too small");
		void *p = nullptr;
		VK_CHECK(vkMapMemory(dev, nodesMem, 0, VK_WHOLE_SIZE, 0, &p));
		std::memcpy(p, nodes.data(), nodes.size_bytes());
		vkUnmapMemory(dev, nodesMem);
	}
	if (!tris.empty()) {
		if (trisBytes < tris.size_bytes())
			throw std::runtime_error("RayPicking: tris buffer too small");
		void *p = nullptr;
		VK_CHECK(vkMapMemory(dev, trisMem, 0, VK_WHOLE_SIZE, 0, &p));
		std::memcpy(p, tris.data(), tris.size_bytes());
		vkUnmapMemory(dev, trisMem);
	}
	if (!positions.empty()) {
		if (posBytes < positions.size_bytes())
			throw std::runtime_error("RayPicking: pos buffer too small");
		void *p = nullptr;
		VK_CHECK(vkMapMemory(dev, posMem, 0, VK_WHOLE_SIZE, 0, &p));
		std::memcpy(p, positions.data(), positions.size_bytes());
		vkUnmapMemory(dev, posMem);
	}
}

void RayPicking::uploadInstances(std::span<const InstanceXformGPU> instances, std::span<const int> ids) {
	if (!pipeline)
		return;
	size_t n = std::min(instances.size(), ids.size());
	n = std::min<size_t>(n, maxInstances);
	liveInstances = static_cast<uint32_t>(n);

	if (n == 0) {
		liveInstances = 0;
		return;
	}

	std::memcpy(mappedInst, instances.data(), n * sizeof(InstanceXformGPU));
	std::memcpy(mappedIds, ids.data(), n * sizeof(int));
	uboDirty = true; // count changed
}

void RayPicking::updateUBO(const glm::mat4 &view, const glm::mat4 &proj, const glm::vec2 &mouseNdc, const std::optional<glm::vec3> &camOverride) {
	PickingUBO u{};
	u.invViewProj = glm::inverse(proj * view);
	u.mouseNdc = mouseNdc;
	const glm::vec3 cam = camOverride ? *camOverride : glm::vec3(glm::inverse(view)[3]);
	u.camPos = glm::vec4(cam, 1.0f);
	u.instanceCount = int(liveInstances);
	std::memcpy(mappedUBO, &u, sizeof(PickingUBO));
}

// ---------- record / readback ----------

void RayPicking::record(VkCommandBuffer cmd, uint32_t gx, uint32_t gy, uint32_t gz) {
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->pipeline);
	if (!pipeline->descriptorSets.descriptorSets.empty()) {
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->pipelineLayout, 0, (uint32_t)pipeline->descriptorSets.descriptorSets.size(), pipeline->descriptorSets.descriptorSets.data(), 0, nullptr);
	}
	vkCmdDispatch(cmd, gx, gy, gz);

	VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
	mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	mb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr);
}

bool RayPicking::readback(HitOutCPU &out) {
	if (!mappedOut)
		return false;
	std::memcpy(&out, mappedOut, sizeof(HitOutCPU));
	last = out;
	return true;
}

// ---------- dynamic capacity ----------

void RayPicking::resizeInstanceBuffer(uint32_t newMax) {
	if (!pipeline)
		return;
	if (newMax == 0)
		newMax = 1;
	if (newMax == maxInstances)
		return;

	// ---- Destroy old pipeline layout and set layouts before re-creating ----
	if (pipeline->pipelineLayout) {
		vkDestroyPipelineLayout(device, pipeline->pipelineLayout, nullptr);
		pipeline->pipelineLayout = VK_NULL_HANDLE;
	}
	if (!pipeline->descriptorSets.descriptorSetsLayout.empty()) {
		for (auto &l : pipeline->descriptorSets.descriptorSetsLayout) {
			if (l)
				vkDestroyDescriptorSetLayout(device, l, nullptr);
		}
		pipeline->descriptorSets.descriptorSetsLayout.clear();
	}

	// Rebuild descriptor write lists for new buffers (you already do this)
	pipeline->descriptorSets.writeDescriptorSets.clear();
	pipeline->descriptorSets.writeDescriptorBufferInfoIndex.clear();
	pipeline->descriptorSets.descriptorBuffersInfo.clear();

	auto push = [&](uint32_t b, VkBuffer buf, VkDeviceSize range, VkDescriptorType t) {
		VkDescriptorBufferInfo bi{buf, 0, range};
		pipeline->createWriteDescriptorSet(b, t, bi, 1, 0);
	};
	push(0, nodesBuf, nz(nodesBytes), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	push(1, trisBuf, nz(trisBytes), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	push(2, posBuf, nz(posBytes), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	push(3, uboBuf, sizeof(PickingUBO), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	push(4, outBuf, sizeof(HitOutCPU), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	push(5, instBuf, sizeof(InstanceXformGPU) * maxInstances, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	push(6, idsBuf, sizeof(int) * maxInstances, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	pipeline->createDescriptors();
}

int RayPicking::buildNode(std::vector<BuildTri> &tris, int begin, int end, int depth, std::vector<BuildNode> &out) {
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

void RayPicking::buildBVH(const vector<vec3> &vertices, const vector<uint32_t> &indices) {
	// Gather positions and triangles from current mesh
	posGPU.clear();
	triGPU.clear();
	if (!vertices.empty()) {
		posGPU.reserve(vertices.size());
		for (auto &v : vertices)
			posGPU.push_back(glm::vec4(v, 1.0f));
	} else {
		std::cout << "[Warning] BVH build: no vertices\n";
		return;
	}

	std::vector<BuildTri> tris;
	tris.reserve(indices.size() / 3);
	for (size_t t = 0; t < indices.size(); t += 3) {
		uint32_t i0 = indices[t + 0], i1 = indices[t + 1], i2 = indices[t + 2];
		const vec3 &A = vec3(posGPU[i0]);
		const vec3 &B = vec3(posGPU[i1]);
		const vec3 &C = vec3(posGPU[i2]);

		BuildTri bt;
		bt.i0 = i0;
		bt.i1 = i1;
		bt.i2 = i2;
		bt.b = triAabb(A, B, C);
		bt.centroid = (A + B + C) * (1.0f / 3.0f);
		tris.push_back(bt);

		triGPU.push_back({i0, i1, i2, 0});
	}

	// Build tree into BuildNode list (temporary)
	std::vector<BuildNode> tmp;
	tmp.reserve(tris.size() * 2);
	int root = buildNode(tris, 0, (int)tris.size(), 0, tmp);
	if (root < 0) {
		bvhNodes.clear();
		return;
	}

	// Rebuild GPU triangles (final order is 'tris' current order)
	triGPU.clear();
	triGPU.reserve(tris.size());
	for (const auto &t : tris)
		triGPU.push_back({t.i0, t.i1, t.i2, 0u});

	// Flatten to GPU nodes (depth-first)
	bvhNodes.clear();
	bvhNodes.resize(tmp.size());

	std::vector<int> map(tmp.size(), -1);
	std::function<void(int, int &)> dfs = [&](int ni, int &outIdx) {
		int my = outIdx++;
		map[ni] = my;
		if (tmp[ni].triCount == 0) {
			dfs(tmp[ni].left, outIdx);
			dfs(tmp[ni].right, outIdx);
		}
	};
	int counter = 0;
	dfs(root, counter);

	std::function<void(int)> emitV = [&](int ni) {
		int me = map[ni];
		const BuildNode &n = tmp[ni];
		BVHNodeGPU gn{};
		gn.bmin = glm::vec4(n.b.bmin, 0.0f);
		gn.bmax = glm::vec4(n.b.bmax, 0.0f);

		if (n.triCount == 0) {
			gn.leftFirst = map[n.left];
			gn.rightOrCount = (uint32_t(map[n.right]) | 0x80000000u); // internal: high bit set
			bvhNodes[me] = gn;
			emitV(n.left);
			emitV(n.right);
		} else {
			gn.leftFirst = n.firstTri;
			gn.rightOrCount = n.triCount; // leaf: count, high bit clear
			bvhNodes[me] = gn;
		}
	};
	emitV(root);

	// Update default buffer size hints (optional)
	initInfo.nodesBytes = bvhNodes.size() * sizeof(BVHNodeGPU);
	initInfo.trisBytes = triGPU.size() * sizeof(TriIndexGPU);
	initInfo.posBytes = posGPU.size() * sizeof(glm::vec4);
}
