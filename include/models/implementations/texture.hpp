#pragma once

#include "model.hpp"
#include <assimp/texture.h>
#include <vulkan/vulkan_core.h>

class Texture : public Model {
  public:
	Texture(Texture &&) = default;
	Texture(const Texture &) = delete;
	Texture &operator=(Texture &&) = delete;
	Texture &operator=(const Texture &) = delete;
	~Texture() override;

	struct Vertex {
		vec3 pos;
		vec4 color;
		vec2 texCoord;

		static VkVertexInputBindingDescription getBindingDescription() {
			VkVertexInputBindingDescription bindingDescription{};
			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(Vertex);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return bindingDescription;
		}

		static array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
			array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

			attributeDescriptions[0].binding = 0;
			attributeDescriptions[0].location = 0;
			attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[0].offset = offsetof(Vertex, pos);

			attributeDescriptions[1].binding = 0;
			attributeDescriptions[1].location = 1;
			attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			attributeDescriptions[1].offset = offsetof(Vertex, color);

			attributeDescriptions[2].binding = 0;
			attributeDescriptions[2].location = 2;
			attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
			attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

			return attributeDescriptions;
		}
	};

	Texture(Scene &scene, const string &texturePath, const vector<Vertex> &vertices, const vector<uint16_t> &indices);
	Texture(Scene &scene, const aiTexture &embeddedTex, const vector<Vertex> &vertices, const vector<uint16_t> &indices);

  protected:
	void buildBVH() override;

	void createTextureImageFromFile();
	void createTextureImageFromEmbedded();
	void createTextureImageView();
	void createTextureSampler();

	void createDescriptorSetLayout() override;
	void createDescriptorPool() override;
	void createDescriptorSets() override;
	void createBindingDescriptions() override;

  private:
	string texturePath;
	aiTexture embeddedTex;

	VkImage textureImage = VK_NULL_HANDLE;
	VkImageView textureImageView = VK_NULL_HANDLE;
	VkSampler textureSampler = VK_NULL_HANDLE;
	VkDeviceMemory textureImageMemory = VK_NULL_HANDLE;

	VkImageCreateInfo imageInfo{};
	VkDescriptorSetLayoutBinding samplerLayoutBinding{};

	vector<Vertex> vertices;
};
