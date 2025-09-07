#pragma once

#include <condition_variable>
#include <functional>
#include <thread>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_TYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "engine.hpp"
#include "platform.hpp"
#include "assets.hpp"
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

	struct Params {
		vec4 color;
		vec4 outlineColor;
		float outlineWidth = 2.0f; // pixels
		float _pad0 = 0, _pad1 = 0, _pad2 = 0;
	};

	struct ScreenParams {
		VkViewport viewport{};
		VkRect2D scissor{};
	};

	Model(Scene &scene, const UBO &ubo, ScreenParams &screenParams, const string &shaderPath);
	Model(Scene &scene, const UBO &ubo, ScreenParams &screenParams, const string &shaderPath, const vector<uint16_t> &indices);
	virtual ~Model();

	/*
	 *  Compute setup
	 * */

	struct RayTraceParams {
		vec2 mouseNdc = vec2(0.0f); // in [-1, 1], y already flipped for Vulkan
		vec3 camPos = vec3(0.0f);	// world-space camera position
		int isOrtho = 0;			// 0=perspective, 1=orthographic
	};

	bool rayTracingEnabled = false;
	bool mouseIsOver{false};
	bool selected{false};

	UBO ubo{};
	Params params{};
	ScreenParams &screenParams;

	optional<vec3> hitPos;
	optional<float> rayLength;

	std::function<void()> onMouseHover;
	std::function<void()> onMouseEnter;
	std::function<void()> onMouseExit;

	void setOnMouseClick(std::function<void(int, int, int)> cb);
	void setOnKeyboardKeyPress(std::function<void(int, int, int, int)> cb);

	void setRayTraceFromViewportPx(float px, float py, const VkViewport &vp);
	void setRayTraceEnabled(bool v) { rayTracingEnabled = v; }
	void setRayTraceOrtho(bool isOrtho) { rayTraceParams.isOrtho = isOrtho; }

	void updateRayTraceUniformBuffer();
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
	Scene &scene;

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
		int isOrtho;
	};

	struct HitOutCPU {
		uint32_t hit;
		uint32_t primId;
		float t;
		float rayLen;
		vec4 hitPos;
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
	HitOutCPU *hitMapped = nullptr;
	RayTraceParams rayTraceParams{};

	// CPU copies (once, unless geometry changes)
	std::vector<BVHNodeGPU> bvhNodes;
	std::vector<TriIndexGPU> triGPU;
	std::vector<vec3> posGPU;

	// Build & upload
	virtual void buildBVH(); // CPU BVH builder
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

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	VkDescriptorSetLayoutBinding uboLayoutBinding{};
	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	VkDescriptorPoolSize poolSize{};
	VkDescriptorPoolCreateInfo poolInfo{};
	VkDescriptorSetAllocateInfo allocInfo{};

	vector<uint16_t> indices;
	vector<VkBuffer> uniformBuffers;
	vector<VkDeviceMemory> uniformBuffersMemory;
	vector<void *> uniformBuffersMapped;

	VkBuffer stagingBuffer = VK_NULL_HANDLE;
	VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;
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
	virtual void createGraphicsPipeline();

	template <typename V> void createVertexBuffer(const std::vector<V> &vertices) {
		VkDeviceSize bufferSize;
		if (!vertices.empty()) {
			bufferSize = sizeof(vertices[0]) * vertices.size();
		} else {
			throw std::runtime_error("No vertices specified for Vertex Buffer");
		}

		Engine::createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void *data;
		vkMapMemory(Engine::device, stagingBufferMemory, 0, bufferSize, 0, &data);
		if (!vertices.empty()) {
			memcpy(data, vertices.data(), (size_t)bufferSize);
		} else {
			throw std::runtime_error("No vertices specified for Vertex Buffer");
		}
		vkUnmapMemory(Engine::device, stagingBufferMemory);

		Engine::createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

		Engine::copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

		vkDestroyBuffer(Engine::device, stagingBuffer, nullptr);
		vkFreeMemory(Engine::device, stagingBufferMemory, nullptr);
	}

  private:
	std::mutex m;
	Platform::jthread watcher;
	std::condition_variable cv;

	AABB merge(const AABB &a, const AABB &b);
};
