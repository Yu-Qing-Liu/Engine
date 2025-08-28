#pragma once

#include "model.hpp"
#include <vulkan/vulkan_core.h>

class Texture : public Model {
  public:
	Texture(const std::string &texturePath, const std::vector<TexVertex> &vertices, const std::vector<uint16_t> &indices);
	Texture(Texture &&) = default;
	Texture(const Texture &) = delete;
	Texture &operator=(Texture &&) = delete;
	Texture &operator=(const Texture &) = delete;
	~Texture() override;

	const std::string texturePath;

  private:
	VkImage textureImage;
	VkImageView textureImageView;
	VkSampler textureSampler;
	VkDeviceMemory textureImageMemory;

	VkImageCreateInfo imageInfo{};
	VkDescriptorSetLayoutBinding samplerLayoutBinding{};

	void createTextureImage();
	void createTextureImageView();
	void createTextureSampler();

	void createDescriptorSetLayout() override;
	void createDescriptorPool() override;
	void createDescriptorSets() override;
	void createBindingDescriptions() override;

	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
};
