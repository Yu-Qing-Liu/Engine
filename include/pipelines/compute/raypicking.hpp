#pragma once

#include <cstdint>
#include <cstring>
#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <span>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "assets.hpp"
#include "pipeline.hpp"

using namespace glm;

class RayPicking {
  public:
	// ---- GPU layout (matches the compute shader) ----
	struct BVHNodeGPU {
		glm::vec4 bmin;
		uint32_t leftFirst;
		uint32_t _pad0[3]{};
		glm::vec4 bmax;
		uint32_t rightOrCount;
		uint32_t _pad1[3]{};
	};
	struct TriIndexGPU {
		uint32_t i0, i1, i2, _pad;
	};
	struct InstanceXformGPU {
		glm::mat4 model;
		glm::mat4 invModel;
	};

	// ---- CPU-side BVH helpers (NEW) ----
	struct AABB {
		glm::vec3 bmin, bmax;
	};
	struct BuildTri {
		AABB b;
		uint32_t i0, i1, i2;
		glm::vec3 centroid;
	};
	struct BuildNode {
		AABB b;
		int left = -1, right = -1;
		uint32_t firstTri = 0, triCount = 0; // triCount > 0 => leaf
	};

	// kept as-is ...
	struct PickingUBO {
		glm::mat4 invViewProj;
		glm::vec2 mouseNdc;
		glm::vec2 _pad0{};
		glm::vec3 camPos;
		int instanceCount = 0;
		int _pad1[3];
	};
	struct HitOutCPU {
		uint32_t hit = 0, primId = 0;
		float t = 0.f, rayLen = 0.f;
		glm::vec4 hitPos{0.f};
	};
	struct InitInfo {
		VkDescriptorPool dpool = VK_NULL_HANDLE;
		Assets::ShaderModules shaders{};
		uint32_t maxInstances = 1;
		size_t nodesBytes = 0, trisBytes = 0, posBytes = 0;
	};

  public:
	RayPicking();
	~RayPicking();

	InitInfo initInfo{};
	HitOutCPU hitInfo{};

	void buildBVH(const std::vector<glm::vec3> &vertices, const std::vector<uint32_t> &indices);

	// lifecycle / uploads / record / etc. unchanged…
	void init(VkDevice device, VkPhysicalDevice physicalDevice);
	void destroy();
	void uploadStatic(std::span<const BVHNodeGPU> nodes, std::span<const TriIndexGPU> tris, std::span<const glm::vec4> positions);
	void uploadInstances(std::span<const InstanceXformGPU> instances, std::span<const int> ids);
	void updateUBO(const glm::mat4 &view, const glm::mat4 &proj, const glm::vec2 &mouseNdc, const std::optional<glm::vec3> &camOverride = std::nullopt);
	void record(VkCommandBuffer cmd, uint32_t gx = 1, uint32_t gy = 1, uint32_t gz = 1);
	bool readback(HitOutCPU &out);
	void resizeInstanceBuffer(uint32_t newMax);
	void setDevice(VkDevice d) { device = d; }
	void setPhysicalDevice(VkPhysicalDevice p) { physicalDevice = p; }

  private:
	// ---- BVH internals (NEW) ----
	static inline AABB merge(const AABB &a, const AABB &b) { return {glm::min(a.bmin, b.bmin), glm::max(a.bmax, b.bmax)}; }
	static inline AABB triAabb(const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c) {
		AABB r;
		r.bmin = glm::min(a, glm::min(b, c));
		r.bmax = glm::max(a, glm::max(b, c));
		return r;
	}
	// builds nodes into 'out', returns root index in 'out' (or -1 if empty)
	int buildNode(std::vector<BuildTri> &tris, int begin, int end, int /*depth*/, std::vector<BuildNode> &out);

	// descriptors
	void createDescriptors();
	static size_t nz(size_t s) { return s ? s : size_t(1); }

  private:
	VkDevice device = VK_NULL_HANDLE;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	std::unique_ptr<Pipeline> pipeline;

	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

	VkBuffer nodesBuf = VK_NULL_HANDLE, trisBuf = VK_NULL_HANDLE, posBuf = VK_NULL_HANDLE;
	VkDeviceMemory nodesMem = VK_NULL_HANDLE, trisMem = VK_NULL_HANDLE, posMem = VK_NULL_HANDLE;

	VkBuffer instBuf = VK_NULL_HANDLE, idsBuf = VK_NULL_HANDLE, outBuf = VK_NULL_HANDLE, uboBuf = VK_NULL_HANDLE;
	VkDeviceMemory instMem = VK_NULL_HANDLE, idsMem = VK_NULL_HANDLE, outMem = VK_NULL_HANDLE, uboMem = VK_NULL_HANDLE;

	void *mappedInst = nullptr;
	void *mappedIds = nullptr;
	void *mappedOut = nullptr;
	void *mappedUBO = nullptr;

	uint32_t maxInstances = 1;
	size_t nodesBytes = 0, trisBytes = 0, posBytes = 0;
	uint32_t liveInstances = 0;
	bool uboDirty = true;

	// NEW: CPU copies used by buildBVH → uploadStatic (optional)
	std::vector<BVHNodeGPU> bvhNodes;
	std::vector<TriIndexGPU> triGPU;
	std::vector<glm::vec4> posGPU;

	HitOutCPU last{};
};
