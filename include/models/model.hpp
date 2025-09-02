#pragma once

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

	virtual void updateComputeUniformBuffer();
	virtual void compute();

	void updateUniformBuffer(optional<mat4> model = std::nullopt, optional<mat4> view = std::nullopt, optional<mat4> proj = std::nullopt);
	void updateUniformBuffer(const UBO &ubo);
	virtual void render(const UBO &ubo, const ScreenParams &screenParams);

  protected:
	optional<UBO> ubo = std::nullopt;

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

	virtual void createDescriptorSetLayout();
	virtual void createUniformBuffers();
	virtual void createDescriptorPool();
	virtual void createDescriptorSets();
	virtual void createVertexBuffer();
	virtual void createIndexBuffer();
	virtual void createBindingDescriptions();
	virtual void createGraphicsPipeline();
};
