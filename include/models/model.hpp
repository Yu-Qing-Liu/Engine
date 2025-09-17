#pragma once

#include <condition_variable>
#include <functional>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_TYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "assets.hpp"
#include "engine.hpp"
#include "platform.hpp"
#include <array>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <optional>
#include <string>
#include <vulkan/vulkan_core.h>

using namespace glm;
using std::array;
using std::optional;
using std::string;
using std::vector;

class Scene;

class Model {
  public:
	Model(Model &&) = delete;
	Model(const Model &) = delete;
	Model &operator=(Model &&) = delete;
	Model &operator=(const Model &) = delete;

	struct UBO {
		mat4 model;
		mat4 view;
		mat4 proj;
	};

	struct ScreenParams {
		VkViewport viewport{1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
		VkRect2D scissor{{1, 1}, {1, 1}};
	};

	Model(Scene *scene, const UBO &ubo, ScreenParams &screenParams, const string &shaderPath);
	Model(Scene *scene, const UBO &ubo, ScreenParams &screenParams, const string &shaderPath, const vector<uint32_t> &indices);
	virtual ~Model();

	/*
	 *  Compute setup
	 * */

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

	bool rayTracingEnabled = false;
	bool mouseIsOver{false};
	bool selected{false};

	UBO ubo{};
	ScreenParams &screenParams;

	optional<vec3> hitPos;
	optional<float> rayLength;
	HitOutCPU *hitMapped = nullptr;

	std::function<void()> onMouseHover;
	std::function<void()> onMouseEnter;
	std::function<void()> onMouseExit;

	void setOnMouseClick(std::function<void(int, int, int)> cb);
	void setOnKeyboardKeyPress(std::function<void(int, int, int, int)> cb);

	void setRayTraceFromViewportPx(float px, float py, const VkViewport &vp);
	void setRayTraceEnabled(bool v) { rayTracingEnabled = v; }

	virtual void updateRayTraceUniformBuffer();
	void rayTrace();

	void setMouseIsOver(bool over);
	void onMouseExitEvent();

	virtual void updateComputeUniformBuffer();
	virtual void compute();

	/*
	 *  Graphics setup
	 * */

	void updateUniformBuffer(optional<mat4> model = std::nullopt, optional<mat4> view = std::nullopt, optional<mat4> proj = std::nullopt);
	void updateUniformBuffer(const UBO &ubo);
	void updateScreenParams(const ScreenParams &screenParams);
	void copyUBO();
	virtual void render();

  protected:
	Scene *scene;

	std::function<void(int, int, int, int)> onKeyboardKeyPress;

	/*
	 *  Compute setup
	 * */

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

	// GPU resources (compute)
	VkDescriptorSetLayout computeDescriptorSetLayout = VK_NULL_HANDLE;
	VkPipelineLayout computePipelineLayout = VK_NULL_HANDLE;
	VkPipeline computePipeline = VK_NULL_HANDLE;
	VkDescriptorPool computePool = VK_NULL_HANDLE;
	VkDescriptorSet computeDescriptorSet = VK_NULL_HANDLE;

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

	// CPU copies (once, unless geometry changes)
	std::vector<BVHNodeGPU> bvhNodes;
	std::vector<TriIndexGPU> triGPU;
	std::vector<vec3> posGPU;

	// Build & upload
	virtual void buildBVH();

	template <typename V> void buildBVH(const std::vector<V> &vertices) {
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

	AABB triAabb(const vec3 &a, const vec3 &b, const vec3 &c) const;
	int buildNode(std::vector<BuildTri> &tris, int begin, int end, int depth, std::vector<BuildNode> &out);

	/*
	 *  Graphics setup
	 * */

	string shaderPath;
	string rayTracingShaderPath = Assets::shaderRootPath + "/raytracing";
	Assets::ShaderModules shaderProgram;
	Assets::ShaderModules rayTracingProgram;

	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkPipeline graphicsPipeline = VK_NULL_HANDLE;
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

	vector<VkPipelineShaderStageCreateInfo> shaderStages;

	VkVertexInputBindingDescription bindingDescription;
	vector<VkVertexInputAttributeDescription> attributeDescriptions;
	vector<VkDescriptorSet> descriptorSets;

	VkDescriptorSetLayoutBinding uboLayoutBinding{};
	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	VkDescriptorPoolSize poolSize{};
	VkDescriptorPoolCreateInfo poolInfo{};
	VkDescriptorSetAllocateInfo allocInfo{};

	// Graphics pipeline
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	VkPipelineViewportStateCreateInfo viewportState{};
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	VkPipelineMultisampleStateCreateInfo multisampling{};
	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	VkPipelineColorBlendStateCreateInfo colorBlending{};
	vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo dynamicState{};
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	VkGraphicsPipelineCreateInfo pipelineInfo{};

	vector<uint32_t> indices;
	vector<VkBuffer> uniformBuffers;
	vector<VkDeviceMemory> uniformBuffersMemory;
	vector<void *> uniformBuffersMapped;

	VkBuffer vertexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
	VkBuffer indexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;

	virtual void createComputeDescriptorSetLayout();
	virtual void createComputePipeline();
	virtual void createShaderStorageBuffers();
	virtual void createComputeDescriptorSets();

	virtual void createDescriptorSetLayout();
	virtual void createUniformBuffers();
	virtual void createDescriptorPool();
	virtual void createDescriptorSets();
	virtual void createVertexBuffer();
	virtual void createIndexBuffer();
	virtual void createBindingDescriptions() = 0;
	virtual void setupGraphicsPipeline();

	void createGraphicsPipeline();

	template <typename V> void createVertexBuffer(const std::vector<V> &vertices) {
		if (vertices.empty())
			throw std::runtime_error("Create Vertex Buffer: No vertices");
		VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

		VkBuffer stg;
		VkDeviceMemory stgMem;
		Engine::createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stg, stgMem);

		void *data = nullptr;
		vkMapMemory(Engine::device, stgMem, 0, bufferSize, 0, &data);
		std::memcpy(data, vertices.data(), (size_t)bufferSize);
		vkUnmapMemory(Engine::device, stgMem);

		Engine::createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

		Engine::copyBuffer(stg, vertexBuffer, bufferSize);

		vkDestroyBuffer(Engine::device, stg, nullptr);
		vkFreeMemory(Engine::device, stgMem, nullptr);
	}

  private:
	std::mutex m;
	Platform::jthread watcher;
	std::condition_variable cv;

	AABB merge(const AABB &a, const AABB &b);
};
