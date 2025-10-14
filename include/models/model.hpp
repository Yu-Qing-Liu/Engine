#pragma once

#include <condition_variable>
#include <functional>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_TYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "assets.hpp"
#include "blurpipeline.hpp"
#include "engine.hpp"
#include "platform.hpp"
#include "raytracingpipeline.hpp"
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

	struct MVP {
		mat4 model;
		mat4 view;
		mat4 proj;
	};

	struct ScreenParams {
		VkViewport viewport{1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
		VkRect2D scissor{{1, 1}, {1, 1}};
	};

	Model(Scene *scene, const MVP &ubo, ScreenParams &screenParams, const string &shaderPath, const VkRenderPass &renderPass = Engine::renderPass);
	virtual ~Model();

	const VkRenderPass &renderPass;

	std::unique_ptr<RayTracingPipeline> rayTracing;
	std::unique_ptr<BlurPipeline> blur;

	bool rayTracingEnabled = false;
	bool mouseIsOver{false};
	bool selected{false};

	MVP mvp{};
	ScreenParams &screenParams;

	std::function<void()> onMouseHover;
	std::function<void()> onMouseEnter;
	std::function<void()> onMouseExit;

	bool isOrtho() const {
		constexpr float epsilon = 1e-5f;
		return std::abs(mvp.proj[2][3]) < epsilon && std::abs(mvp.proj[3][3] - 1.0f) < epsilon;
	}

	int onMouseClickCbIdx = -1;
	void setOnMouseClick(std::function<void(int, int, int)> cb);
	int onKbCbIdx = -1;
	void setOnKeyboardKeyPress(std::function<void(int, int, int, int)> cb);

	void enableRayTracing(bool v) {
		rayTracingEnabled = v;
		if (v) {
			rayTracing->initialize();
		}
	}

	virtual void enableBlur(const std::string &blurShaderPath = Assets::shaderRootPath + "/blur") {
		if (!blur) {
			blur = std::make_unique<BlurPipeline>(this);
			blur->shaderPath = blurShaderPath;
			blur->initialize();
		}
	}

	void setMouseIsOver(bool over);
	void onMouseExitEvent();

	void updateMVP(optional<mat4> model = std::nullopt, optional<mat4> view = std::nullopt, optional<mat4> proj = std::nullopt);
	void updateMVP(const MVP &ubo);
	void updateScreenParams(const ScreenParams &screenParams);
	void translate(const vec3 &pos, const mat4 &model = mat4(1.0f));
	void scale(const vec3 &scale, const mat4 &model = mat4(1.0f));
	void rotate(float angle, const vec3 &rotation, const mat4 &model = mat4(1.0f));
	void copyUBO();

	virtual void buildBVH();
	virtual void render();

	VkBuffer vertexBuffer = VK_NULL_HANDLE;
	VkBuffer indexBuffer = VK_NULL_HANDLE;

	VkVertexInputBindingDescription bindingDescription;
	vector<VkVertexInputAttributeDescription> attributeDescriptions;
	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
	vector<VkDescriptorSet> descriptorSets;

	Assets::ShaderModules shaderProgram;

	vector<uint32_t> indices;

  protected:
	Scene *scene;

	std::function<void(int, int, int, int)> onKeyboardKeyPress;

	string shaderPath;

	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkPipeline graphicsPipeline = VK_NULL_HANDLE;
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

	vector<VkPipelineShaderStageCreateInfo> shaderStages;

	VkDescriptorSetLayoutBinding mvpLayoutBinding{};
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

	vector<VkBuffer> mvpBuffers;
	vector<VkDeviceMemory> mvpBuffersMemory;
	vector<void *> mvpBuffersMapped;

	VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
	VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;

	virtual void createDescriptorSetLayout();
	virtual void createUniformBuffers();
	template <typename U> void createUniformBuffers(vector<VkBuffer> &uniformBuffers, vector<VkDeviceMemory> &uniformBuffersMemory, vector<void *> &uniformBuffersMapped) {
		VkDeviceSize bufferSize = sizeof(U);

		uniformBuffers.resize(Engine::MAX_FRAMES_IN_FLIGHT);
		uniformBuffersMemory.resize(Engine::MAX_FRAMES_IN_FLIGHT);
		uniformBuffersMapped.resize(Engine::MAX_FRAMES_IN_FLIGHT);

		for (size_t i = 0; i < Engine::MAX_FRAMES_IN_FLIGHT; i++) {
			Engine::createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);
			vkMapMemory(Engine::device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
		}
	}

	virtual void createDescriptorPool();
	virtual void createDescriptorSets();

	virtual void createVertexBuffer();
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

	virtual void createIndexBuffer();
	virtual void createBindingDescriptions() = 0;
	virtual void setupGraphicsPipeline();

	void createGraphicsPipeline();

  private:
	std::mutex m;
	Platform::jthread watcher;
	std::condition_variable cv;
};
