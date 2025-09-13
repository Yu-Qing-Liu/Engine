#pragma once

#include "model.hpp"
#include <vulkan/vulkan_core.h>

class Particles : public Model {
  public:
	Particles(Scene *scene, const UBO &ubo, ScreenParams &screenParams, uint32_t particleCount, uint32_t width, uint32_t height);
	Particles(Particles &&) = delete;
	Particles(const Particles &) = delete;
	Particles &operator=(Particles &&) = delete;
	Particles &operator=(const Particles &) = delete;
	~Particles() override;

	struct Particle {
		vec2 position;
		vec2 velocity;
		vec4 color;

		static VkVertexInputBindingDescription getBindingDescription() {
			VkVertexInputBindingDescription bindingDescription{};
			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(Particle);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return bindingDescription;
		}

		static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
			std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

			attributeDescriptions[0].binding = 0;
			attributeDescriptions[0].location = 0;
			attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
			attributeDescriptions[0].offset = offsetof(Particle, position);

			attributeDescriptions[1].binding = 0;
			attributeDescriptions[1].location = 1;
			attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			attributeDescriptions[1].offset = offsetof(Particle, color);

			return attributeDescriptions;
		}
	};

	struct UniformBufferObject {
		float deltatime = 1.0f;
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
