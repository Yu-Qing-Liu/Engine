#pragma once

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "model.hpp"
#include <vulkan/vulkan_core.h>

using std::make_unique;
using std::unique_ptr;

#ifndef OBJMODEL_MAX_TEXTURES
#define OBJMODEL_MAX_TEXTURES 64
#endif

class OBJModel : public Model {
  public:
	OBJModel(Scene *scene, const UBO &ubo, ScreenParams &screenParams, const string &objPath);
	OBJModel(OBJModel &&) = delete;
	OBJModel(const OBJModel &) = delete;
	OBJModel &operator=(OBJModel &&) = delete;
	OBJModel &operator=(const OBJModel &) = delete;
	~OBJModel();

	struct OBJVertex {
		vec3 pos;
		vec3 nrm;
		vec4 col;
		vec2 uv;
		vec4 tan_sgn; // tangent.xyz, bitangent sign in .w
		uint32_t materialId;

		static VkVertexInputBindingDescription getBindingDescription() {
			VkVertexInputBindingDescription b{};
			b.binding = 0;
			b.stride = sizeof(OBJVertex);
			b.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			return b;
		}

		static vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
			vector<VkVertexInputAttributeDescription> a(6);
			a[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(OBJVertex, pos)};
			a[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(OBJVertex, nrm)};
			a[2] = {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(OBJVertex, col)};
			a[3] = {3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(OBJVertex, uv)};
			a[4] = {4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(OBJVertex, tan_sgn)};
			a[5] = {5, 0, VK_FORMAT_R32_UINT, offsetof(OBJVertex, materialId)};
			return a;
		}
	};

	struct MaterialGPU {
		// 9 texture indices
		int32_t baseColor;
		int32_t normal;
		int32_t roughness;
		int32_t metallic;
		int32_t specular;
		int32_t ao;
		int32_t emissive;
		int32_t opacity;
		int32_t displacement;

		// pad to next 16-byte boundary (9 * 4 = 36 -> next multiple of 16 is 48; needs 12 bytes)
		int32_t _padA0;
		int32_t _padA1;
		int32_t _padA2;

		// factors
		glm::vec4 baseColorFactor;			// rgba
		glm::vec4 emissiveFactor_roughness; // xyz = emissive, w = roughness
		glm::vec4 metallic_flags_pad;		// x = metallic, y = flags (uintBitsToFloat), z/w pad
	};

	struct LoadedTexture {
		VkImage image = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		VkImageView view = VK_NULL_HANDLE;
		VkSampler sampler = VK_NULL_HANDLE;
		uint32_t width = 0, height = 0;
	};

	void render() override;

  protected:
	void buildBVH() override;
	void createBindingDescriptions() override;
	void createVertexBuffer() override;
	void createIndexBuffer() override;
	void setupGraphicsPipeline() override;

  private:
	void loadModel();
	void processNode(aiNode *node, const aiScene *scene);
	void bakeTexturesAndMaterials(const aiScene *scene);

	int getOrLoadTexture(const aiScene *scene, const std::string &directory, const aiMaterial *mat, aiTextureType type, unsigned slot = 0);
	int loadTextureFromAssimpString(const aiScene *scene, const std::string &directory, const aiString &str, aiTextureType type);

	void createMaterialDescriptorSetLayout();
	void createMaterialResources();
	void createMaterialDescriptorSets();

	VkSampler createDefaultSampler() const;
	void destroyLoadedTextures();
	int createSolidTexture(uint8_t r, uint8_t g, uint8_t b, uint8_t a, VkFormat fmt);

	static VkFormat formatFor(aiTextureType type);
	static std::string cacheKeyWithFormat(const std::string &raw, VkFormat fmt);

	const string objPath;
	string directory;

	// Geometry
	std::vector<OBJVertex> vertices;
	std::vector<uint16_t> indices;

	// Materials
	std::vector<MaterialGPU> materialsGPU;
	std::vector<LoadedTexture> textures;			   // loaded images (not descriptor slots!)
	std::unordered_map<std::string, int> textureCache; // "path|FMT" or "*N|FMT" -> textures[] index

	std::vector<uint8_t> matHasBaseColorTex;
	std::vector<int> materialRemap;

	// Stable texture->slot map (slot 0 is reserved white)
	std::vector<int> texSlots; // size == textures.size(), values in [0..255], 0 means white/dummy

	// GPU buffers for materials
	VkBuffer materialsBuf = VK_NULL_HANDLE;
	VkDeviceMemory materialsMem = VK_NULL_HANDLE;

	// Descriptor set/layout for materials/textures (set = 1)
	VkDescriptorSetLayout materialDSL = VK_NULL_HANDLE;
	VkDescriptorPool materialPool = VK_NULL_HANDLE;
	VkDescriptorSet materialDS = VK_NULL_HANDLE;

	std::array<VkDescriptorSetLayout, 2> setLayouts{};

	// Dummies
	int dummyWhiteIndex = -1;	   // VK_FORMAT_R8G8B8A8_SRGB
	int dummyFlatNormalIndex = -1; // VK_FORMAT_R8G8B8A8_UNORM
};
