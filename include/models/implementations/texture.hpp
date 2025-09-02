#pragma once

#include "model.hpp"
#include <assimp/texture.h>
#include <vulkan/vulkan_core.h>

class Texture : public Model {
  public:
	Texture(const string &texturePath, const vector<TexVertex> &vertices, const vector<uint16_t> &indices);
	Texture(const aiTexture &embeddedTex, const vector<TexVertex> &vertices, const vector<uint16_t> &indices);
	Texture(Texture &&) = default;
	Texture(const Texture &) = delete;
	Texture &operator=(Texture &&) = delete;
	Texture &operator=(const Texture &) = delete;
	~Texture() override;

  private:
	string texturePath;
	aiTexture embeddedTex;

	VkImage textureImage = VK_NULL_HANDLE;
	VkImageView textureImageView = VK_NULL_HANDLE;
	VkSampler textureSampler = VK_NULL_HANDLE;
	VkDeviceMemory textureImageMemory = VK_NULL_HANDLE;

	VkImageCreateInfo imageInfo{};
	VkDescriptorSetLayoutBinding samplerLayoutBinding{};

	void createTextureImageFromFile();
	void createTextureImageFromEmbedded();
	void createTextureImageView();
	void createTextureSampler();

	void createDescriptorSetLayout() override;
	void createDescriptorPool() override;
	void createDescriptorSets() override;
	void createBindingDescriptions() override;
};
