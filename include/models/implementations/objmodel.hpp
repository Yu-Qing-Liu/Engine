#pragma once

#include "model.hpp"
#include <vulkan/vulkan_core.h>

class OBJModel : public Model {
  public:
	OBJModel(const std::string &objPath, const std::vector<TexVertex> &vertices, const std::vector<uint16_t> &indices);
	OBJModel(OBJModel &&) = default;
	OBJModel(const OBJModel &) = delete;
	OBJModel &operator=(OBJModel &&) = delete;
	OBJModel &operator=(const OBJModel &) = delete;
	~OBJModel() override;

	const std::string texturePath;
	const std::string objPath;

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

