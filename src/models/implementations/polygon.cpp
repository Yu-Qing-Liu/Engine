#include "polygon.hpp"
#include "engine.hpp"
#include <cstring>
#include <vulkan/vulkan_core.h>

Polygon::Polygon(Scene &scene, const vector<Vertex> &vertices, const vector<uint16_t> &indices) : vertices(vertices), Model(scene, Engine::shaderRootPath + "/polygon", indices) {
	createDescriptorSetLayout();
	createUniformBuffers();
	createDescriptorPool();
	createDescriptorSets();
	createVertexBuffer<Vertex>(vertices);
	createIndexBuffer();
	createBindingDescriptions();
	createGraphicsPipeline();

	createComputeDescriptorSetLayout();
	createShaderStorageBuffers();
	createComputeDescriptorSets();
	createComputePipeline();
}

void Polygon::buildBVH() {
	// Gather positions and triangles from current mesh
	posGPU.clear();
	triGPU.clear();
	if (!vertices.empty()) {
		posGPU.reserve(vertices.size());
		for (auto &v : vertices) {
			posGPU.push_back(v.pos);
        }
	} else {
		throw std::runtime_error("BVH build: no vertices");
	}

	std::vector<BuildTri> tris;
	tris.reserve(indices.size() / 3);
	for (size_t t = 0; t < indices.size(); t += 3) {
		uint32_t i0 = indices[t + 0], i1 = indices[t + 1], i2 = indices[t + 2];
		const vec3 &A = posGPU[i0];
		const vec3 &B = posGPU[i1];
		const vec3 &C = posGPU[i2];
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

	// Rebuild GPU triangles in the final order used by leaves
	triGPU.clear();
	triGPU.reserve(tris.size());
	for (const auto &t : tris) {
		triGPU.push_back({t.i0, t.i1, t.i2, 0u});
	}

	// Flatten to GPU nodes (depth-first, implicit right=left+1 for internal nodes)
	bvhNodes.clear();
	bvhNodes.resize(tmp.size());
	// Map temp indices to linear DFS order
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

	// Fill nodes in DFS order
	std::function<void(int)> emit = [&](int ni) {
		int me = map[ni];
		const BuildNode &n = tmp[ni];
		BVHNodeGPU gn;
		gn.bmin = vec4(n.b.bmin, 0.0f);
		gn.bmax = vec4(n.b.bmax, 0.0f);

		if (n.triCount == 0) {
			gn.leftFirst = map[n.left];
			gn.rightOrCount = (uint32_t(map[n.right]) | 0x80000000u); // INTERNAL
			bvhNodes[me] = gn;
			emit(n.left);
			emit(n.right);
		} else {
			gn.leftFirst = n.firstTri;
			gn.rightOrCount = n.triCount; // leaf => count, no high bit
			bvhNodes[me] = gn;
		}
	};
	emit(root);
}

void Polygon::createBindingDescriptions() {
	bindingDescription = Vertex::getBindingDescription();
	auto attrs = Vertex::getAttributeDescriptions();
	attributeDescriptions = vector<VkVertexInputAttributeDescription>(attrs.begin(), attrs.end());
}
