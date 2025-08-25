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
	~Image();

	const std::string texturePath;

  private:
	VkImage textureImage;
	VkImageView textureImageView;
	VkDeviceMemory textureImageMemory;
	VkImageCreateInfo imageInfo{};

	void createTextureImage();
	void createTextureImageView();

	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
};
