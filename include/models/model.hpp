#pragma once

#include <functional>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_TYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "engine.hpp"
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

class Model {
  public:
	Model(Model &&) = default;
	Model(const Model &) = delete;
	Model &operator=(Model &&) = delete;
	Model &operator=(const Model &) = delete;

	struct UBO {
		mat4 model;
		mat4 view;
		mat4 proj;
	};

	struct ScreenParams {
		VkViewport viewport{};
		VkRect2D scissor{};
	};

	struct Vertex {
		vec3 pos;
		vec4 color;

		static VkVertexInputBindingDescription getBindingDescription() {
			VkVertexInputBindingDescription bindingDescription{};
			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(Vertex);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return bindingDescription;
		}

		static array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
			array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

			attributeDescriptions[0].binding = 0;
			attributeDescriptions[0].location = 0;
			attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[0].offset = offsetof(Vertex, pos);

			attributeDescriptions[1].binding = 0;
			attributeDescriptions[1].location = 1;
			attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			attributeDescriptions[1].offset = offsetof(Vertex, color);

			return attributeDescriptions;
		}
	};

	struct TexVertex {
		vec3 pos;
		vec4 color;
		vec2 texCoord;

		static VkVertexInputBindingDescription getBindingDescription() {
			VkVertexInputBindingDescription bindingDescription{};
			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(TexVertex);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return bindingDescription;
		}

		static array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
			array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

			attributeDescriptions[0].binding = 0;
			attributeDescriptions[0].location = 0;
			attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[0].offset = offsetof(TexVertex, pos);

			attributeDescriptions[1].binding = 0;
			attributeDescriptions[1].location = 1;
			attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			attributeDescriptions[1].offset = offsetof(TexVertex, color);

			attributeDescriptions[2].binding = 0;
			attributeDescriptions[2].location = 2;
			attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
			attributeDescriptions[2].offset = offsetof(TexVertex, texCoord);

			return attributeDescriptions;
		}
	};

	Model(const string &shaderPath);
	Model(const string &shaderPath, const vector<Vertex> &vertices, const vector<uint16_t> &indices);
	Model(const string &shaderPath, const vector<TexVertex> &vertices, const vector<uint16_t> &indices);
	virtual ~Model();

	/*
	 *  Compute setup
	 * */

	struct PickingParams {
		vec2 mouseNdc = vec2(0.0f); // in [-1, 1], y already flipped for Vulkan
		vec3 camPos = vec3(0.0f);	// world-space camera position
		int isOrtho = 0;			// 0=perspective, 1=orthographic
	};

	void setPickingFromViewportPx(float px, float py, const VkViewport &vp, bool isOrtho = false);
	void setPickingEnabled(bool v) { pickingEnabled = v; }

	virtual void updateComputeUniformBuffer();
	virtual void compute();

	void setOnHover(const std::function<void()> &callback) { onHover = callback; }

	/*
	 *  Graphics setup
	 * */

	void updateUniformBuffer(optional<mat4> model = std::nullopt, optional<mat4> view = std::nullopt, optional<mat4> proj = std::nullopt);
	void updateUniformBuffer(const UBO &ubo);
	void updateScreenParams(const ScreenParams &screenParams);
	virtual void render(const UBO &ubo, const ScreenParams &screenParams);

  protected:
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
		uint32_t _pad;
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
	VkDescriptorSetLayout computeDSL = VK_NULL_HANDLE;
	VkPipelineLayout computePL = VK_NULL_HANDLE;
	VkPipeline computePipe = VK_NULL_HANDLE;
	VkDescriptorPool computePool = VK_NULL_HANDLE;
	VkDescriptorSet computeDS = VK_NULL_HANDLE;

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
	bool pickingEnabled = true;
	PickingParams pickParams{};

	// CPU copies (once, unless geometry changes)
	std::vector<BVHNodeGPU> bvhNodes;
	std::vector<TriIndexGPU> triGPU;
	std::vector<vec3> posGPU;

	// Build & upload
	void buildBVH(); // CPU BVH builder
	AABB triAabb(const vec3 &a, const vec3 &b, const vec3 &c) const;
	int buildNode(std::vector<BuildTri> &tris, int begin, int end, int depth, std::vector<BuildNode> &out);

	std::function<void()> onHover;

	/*
	 *  Graphics setup
	 * */

	optional<UBO> ubo = std::nullopt;
	optional<ScreenParams> screenParams = std::nullopt;

	string shaderPath;
	Engine::ShaderModules shaderProgram;

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

	vector<Vertex> vertices;
	vector<TexVertex> texVertices;
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
	virtual void createBindingDescriptions();
	virtual void createGraphicsPipeline();

  private:
	AABB merge(const AABB &a, const AABB &b);
};
