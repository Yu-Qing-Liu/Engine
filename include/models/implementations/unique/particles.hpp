#pragma once

#include "model.hpp"
#include <vulkan/vulkan_core.h>

class Particles : public Model {
  public:
	Particles(Scene *scene, const MVP &ubo, ScreenParams &screenParams, uint32_t particleCount, uint32_t width, uint32_t height);
	Particles(Particles &&) = delete;
	Particles(const Particles &) = delete;
	Particles &operator=(Particles &&) = delete;
	Particles &operator=(const Particles &) = delete;
	~Particles() override;

	struct Particle {
		vec2 position;
		vec2 velocity;
		vec4 color;
		float size;
		float speedScale;
		float sizeFreq;
		float sizePhase;
		float baseSize;
		float _pad0; // 52..55
		float _pad1; // 56..59
		float _pad2; // 60..63

		static VkVertexInputBindingDescription getBindingDescription() {
			VkVertexInputBindingDescription bindingDescription{};
			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(Particle);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return bindingDescription;
		}

		static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
			std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

			attributeDescriptions[0].binding = 0;
			attributeDescriptions[0].location = 0;
			attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
			attributeDescriptions[0].offset = offsetof(Particle, position);

			attributeDescriptions[1].binding = 0;
			attributeDescriptions[1].location = 1;
			attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			attributeDescriptions[1].offset = offsetof(Particle, color);

			attributeDescriptions[2].binding = 0;
			attributeDescriptions[2].location = 2;
			attributeDescriptions[2].format = VK_FORMAT_R32_SFLOAT;
			attributeDescriptions[2].offset = offsetof(Particle, size);

			attributeDescriptions[3].binding = 0;
			attributeDescriptions[3].location = 3;
			attributeDescriptions[3].format = VK_FORMAT_R32_SFLOAT;
			attributeDescriptions[3].offset = offsetof(Particle, baseSize);

			return attributeDescriptions;
		}
	};

	struct UniformBufferObject {
		float deltatime = 1.0f;
		uint32_t particleCount;
		uint32_t _pad0 = 0, _pat1 = 0;
	};

	void updateComputeUniformBuffer() override;
	void compute() override;
	void render() override;

  protected:
	uint32_t particleCount;
	uint32_t height;
	uint32_t width;

	vector<VkBuffer> shaderStorageBuffers;
	vector<VkDeviceMemory> shaderStorageBuffersMemory;
	vector<VkDescriptorSet> computeDescriptorSets;

	void createComputeDescriptorSetLayout() override;
	void createBindingDescriptions() override;
	void createComputePipeline() override;
	void createShaderStorageBuffers() override;
	void createUniformBuffers() override;
	void createDescriptorPool() override;
	void createComputeDescriptorSets() override;
	void setupGraphicsPipeline() override;
};
