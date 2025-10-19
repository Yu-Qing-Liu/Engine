#pragma once

#include "colors.hpp"
#include "model.hpp"
#include <assimp/texture.h>
#include <vulkan/vulkan_core.h>

class Image : public Model {
  public:
	Image(Image &&) = delete;
	Image(const Image &) = delete;
	Image &operator=(Image &&) = delete;
	Image &operator=(const Image &) = delete;
	~Image() override;

	struct Params {
		vec2 uvScale = vec2(1.0f);
		vec2 uvOffset = vec2(0.0f);
		vec4 color = Colors::Transparent;
		int texIndex = 0;
		float _pad[3];
	};

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

	static constexpr uint32_t MAX_TEXTURES = 16;

	Image(Scene *scene, const MVP &ubo, ScreenParams &screenParams, const vector<Vertex> &vertices, const vector<uint32_t> &indices, const VkRenderPass &renderPass = Engine::renderPass);
	Params params{};
	int texW = 0, texH = 0;

	void computeAspectUV();

	void updateImage(const void *rgba8Pixels, int width, int height, int channels = 4, int slot = -1);
	void updateImage(const std::string &path, int slot = -1);
	uint32_t textureCount() const { return static_cast<uint32_t>(texImages.size()); }

    void render() override;

  protected:
	void buildBVH() override;

	void createTextureImageFromFile(const std::string &path);
	void createTextureImageView(uint32_t slot);
	void createTextureSampler(uint32_t slot);

	void createUniformBuffers() override;
	void createDescriptorSetLayout() override;
	void createDescriptorPool() override;
	void createDescriptorSets() override;
	void createBindingDescriptions() override;
	void setupGraphicsPipeline() override;

  private:
	aiTexture embeddedTex;

	std::vector<VkBuffer> paramsBuffers;
	std::vector<VkDeviceMemory> paramsBuffersMemory;
	std::vector<void *> paramsBuffersMapped;

	VkDescriptorSetLayoutBinding paramsBinding{};

	vector<VkImage> texImages;
	vector<VkDeviceMemory> texImageMemory;
	vector<VkImageView> texImageViews;
	vector<VkSampler> texSamplers;

	VkImageCreateInfo imageInfo{};
	VkDescriptorSetLayoutBinding samplerLayoutBinding{};

	vector<Vertex> vertices;

	void copyParams();
	void writeTextureArrayDescriptors();
	void createOrResizeImage(uint32_t slot, int width, int height);
	void uploadPixelsToImage(uint32_t slot, const void *rgba8Pixels, int width, int height);
	void setActiveTextureIndex(int idx);
};
