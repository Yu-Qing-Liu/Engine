#pragma once

#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>

// GraphicsBuffers:
// - Allocates two color accumulation images (A/B ping-pong) per swapchain image.
// - Allocates a shared depth attachment.
// - Creates sampler + descriptor sets so we can sample either A or B without rewriting descriptors.
// You sample "src" using getSceneSetA(i) or getSceneSetB(i).
//
// Usage flags allow COLOR_ATTACHMENT, SAMPLED, TRANSFER_SRC, TRANSFER_DST for runtime blur/mip gen.

class GraphicsBuffers {
  public:
	GraphicsBuffers() = default;
	~GraphicsBuffers(); // calls destroy()

	// Create offscreen accumulation color (A/B) + depth for given extent and swapImageCount.
	// cmdPool/graphicsQueue are only used if you wanted to do warm-up layout transitions.
	// We don't pre-build mips anymore here; runtime will do it.
	void create(VkPhysicalDevice physicalDevice, VkDevice device, VkExtent2D extent, VkFormat colorFormat, uint32_t swapImageCount);

	void destroy();

	// formats / size
	VkFormat getSceneColorFormat() const { return sceneColorFmt; }
	VkFormat getDepthFormat() const { return depthFmt; }
	VkExtent2D getExtent() const { return size; }
	uint32_t getMipLevels() const { return mipLevels; }

	// accumulation targets A (often used as src or dst depending on frame stage)
	VkImage getColorAImage(uint32_t i) const { return sceneColorAImages[i]; }
	VkImageView getColorAAttView(uint32_t i) const { return sceneColorAAttViews[i]; }		// mip0 only
	VkImageView getColorASampleView(uint32_t i) const { return sceneColorASampleViews[i]; } // full mip chain

	// accumulation targets B
	VkImage getColorBImage(uint32_t i) const { return sceneColorBImages[i]; }
	VkImageView getColorBAttView(uint32_t i) const { return sceneColorBAttViews[i]; }
	VkImageView getColorBSampleView(uint32_t i) const { return sceneColorBSampleViews[i]; }

	// depth target (shared per-swapimage usage in passes)
	VkImage getDepthImage() const { return depthImage; }
	VkImageView getDepthView() const { return depthImageView; }

	// descriptors/sampler: 2 per swapImageIndex (A and B).
	// - sceneSetA[i] samples sceneColorA[i] with sceneSampler
	// - sceneSetB[i] samples sceneColorB[i] with sceneSampler
	VkSampler getSceneSampler() const { return sceneSampler; }
	VkDescriptorSetLayout getSceneSetLayout() const { return sceneSetLayout; }
	VkDescriptorSet getSceneSetA(uint32_t i) const { return sceneSetsA[i]; }
	VkDescriptorSet getSceneSetB(uint32_t i) const { return sceneSetsB[i]; }

	uint32_t getImageCount() const { return swapCount; }

  private:
	// borrowed handles
	VkPhysicalDevice phys = VK_NULL_HANDLE;
	VkDevice dev = VK_NULL_HANDLE;

	VkExtent2D size{};
	uint32_t mipLevels = 1;

	VkFormat sceneColorFmt = VK_FORMAT_R16G16B16A16_SFLOAT;
	VkFormat depthFmt = VK_FORMAT_D32_SFLOAT;
	uint32_t swapCount = 0;

	// A ping target (per swapchain image)
	std::vector<VkImage> sceneColorAImages;
	std::vector<VkDeviceMemory> sceneColorAMemory;
	std::vector<VkImageView> sceneColorAAttViews;	 // COLOR_ATTACHMENT view (mip0 only)
	std::vector<VkImageView> sceneColorASampleViews; // full mip chain, for sampling

	// B ping target (per swapchain image)
	std::vector<VkImage> sceneColorBImages;
	std::vector<VkDeviceMemory> sceneColorBMemory;
	std::vector<VkImageView> sceneColorBAttViews;
	std::vector<VkImageView> sceneColorBSampleViews;

	// depth (single image, single view)
	VkImage depthImage = VK_NULL_HANDLE;
	VkDeviceMemory depthMemory = VK_NULL_HANDLE;
	VkImageView depthImageView = VK_NULL_HANDLE;

	// descriptors/sampler
	VkSampler sceneSampler = VK_NULL_HANDLE;
	VkDescriptorSetLayout sceneSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool sceneDescPool = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> sceneSetsA; // sample A
	std::vector<VkDescriptorSet> sceneSetsB; // sample B

  private:
	uint32_t calcMipLevels(uint32_t w, uint32_t h) const;
	VkFormat chooseDepthFormat() const;
	VkFormat findSupportedDepthFormat(const std::vector<VkFormat> &cands) const;

	void createSceneColorTargets();
	void createDepthTarget();
	void createSamplerAndDescriptors();
	void destroySceneColorTargets();
	void destroyDepthTarget();
	void destroySamplerAndDescriptors();

	// low-level helpers
	void createImage(uint32_t w, uint32_t h, VkFormat format, VkImageUsageFlags usage, VkImage &outImage, VkDeviceMemory &outMem, uint32_t mipLevels, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);

	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect, uint32_t baseMip, uint32_t levelCount);
};
