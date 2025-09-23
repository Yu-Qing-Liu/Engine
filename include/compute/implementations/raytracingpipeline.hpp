#pragma once

#include "assets.hpp"
#include "computepipeline.hpp"
#include <glm/glm.hpp>
#include <optional>

using namespace glm;
using std::optional;
using std::string;

class Model;

class RayTracingPipeline : public ComputePipeline {
  public:
	RayTracingPipeline(Model *model);
	RayTracingPipeline(RayTracingPipeline &&) = default;
	RayTracingPipeline(const RayTracingPipeline &) = default;
	RayTracingPipeline &operator=(RayTracingPipeline &&) = default;
	RayTracingPipeline &operator=(const RayTracingPipeline &) = default;
	virtual ~RayTracingPipeline();

	struct RayTraceParams {
		vec2 mouseNdc = vec2(0.0f); // in [-1, 1], y already flipped for Vulkan
		vec3 camPos = vec3(0.0f);	// world-space camera position
		int _pad0 = 0;
	};

	struct HitOutCPU {
		uint32_t hit;
		uint32_t primId;
		float t;
		float rayLen;
		vec4 hitPos;
	};

	// ====== GPU BVH picking (compute) ======
	struct BVHNodeGPU {
		vec4 bmin;
		uint32_t leftFirst;
		uint32_t _pad0[3];
		vec4 bmax;
		uint32_t rightOrCount;
		uint32_t _pad1[3];
	};

	struct TriIndexGPU {
		uint i0, i1, i2, _pad;
	};

	struct PickingUBO {
		mat4 invViewProj;
		mat4 invModel;
		vec2 mouseNdc;
		vec2 _pad0;
		vec3 camPos;
		int _pad;
	};

	// CPU-side BVH build helpers
	struct AABB {
		vec3 bmin, bmax;
	};

	struct BuildTri {
		AABB b;
		uint i0, i1, i2;
		vec3 centroid;
	};

	struct BuildNode {
		AABB b;
		int left = -1;
		int right = -1;
		uint firstTri = 0;
		uint triCount = 0;
	};

	optional<vec3> hitPos;
	optional<float> rayLength;
	HitOutCPU *hitMapped = nullptr;

	// CPU copies (once, unless geometry changes)
	std::vector<BVHNodeGPU> bvhNodes;
	std::vector<TriIndexGPU> triGPU;
	std::vector<vec3> posGPU;

	template <typename V> void buildBVH(const std::vector<V> &vertices, const std::vector<uint32_t> &indices) {
		// Gather positions and triangles from current mesh
		posGPU.clear();
		triGPU.clear();
		if (!vertices.empty()) {
			posGPU.reserve(vertices.size());
			for (auto &v : vertices) {
				posGPU.push_back(v.pos);
			}
		} else {
            std::cout << "[Warning] BVH build: no vertices" << std::endl;
            return;
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

    void initialize() override;
	void setRayTraceFromViewportPx(float px, float py, const VkViewport &vp);

	void updateComputeUniformBuffer() override;
	void compute() override;

  protected:
	void createComputeDescriptorSetLayout() override;
	void createComputePipeline() override;
	void createShaderStorageBuffers() override;
	void createComputeDescriptorSets() override;

	string rayTracingShaderPath = Assets::shaderRootPath + "/raytracing";
	Assets::ShaderModules rayTracingProgram;

	Model *model;

	VkBuffer nodesBuf = VK_NULL_HANDLE;
	VkDeviceMemory nodesMem = VK_NULL_HANDLE;
	VkBuffer trisBuf = VK_NULL_HANDLE;
	VkDeviceMemory trisMem = VK_NULL_HANDLE;
	VkBuffer posBuf = VK_NULL_HANDLE;
	VkDeviceMemory posMem = VK_NULL_HANDLE;
	VkBuffer pickUBO = VK_NULL_HANDLE;
	VkDeviceMemory pickUBOMem = VK_NULL_HANDLE;
	VkBuffer hitBuf = VK_NULL_HANDLE;
	VkDeviceMemory hitMem = VK_NULL_HANDLE;

	void *pickUBOMapped = nullptr;
	RayTraceParams rayTraceParams{};

	AABB triAabb(const vec3 &a, const vec3 &b, const vec3 &c) const;
	int buildNode(std::vector<BuildTri> &tris, int begin, int end, int depth, std::vector<BuildNode> &out);
	AABB merge(const AABB &a, const AABB &b);
};
