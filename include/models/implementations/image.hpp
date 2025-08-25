#pragma once

#include "model.hpp"
#include <vulkan/vulkan_core.h>

class Image : public Model {
  public:
	Image(const std::string &texturePath);
	Image(Image &&) = default;
	Image(const Image &) = delete;
	Image &operator=(Image &&) = delete;
	Image &operator=(const Image &) = delete;
	~Image() override;

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
