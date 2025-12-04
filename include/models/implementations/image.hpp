#pragma once

#include "model.hpp"

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

using std::string;
using std::vector;

class Image : public Model {
  public:
	Image(Scene *scene);
	~Image();

	// Initialize renderer (no images required up-front).
	void init() override;

	// Instance management
	struct InstanceData {
		glm::mat4 model{1.0f};
		uint32_t frameIndex{0}; // GLOBAL index in the merged texture pool (computed internally)
		uint32_t cover{0};
		uint32_t _pad1{0}, _pad2{0}; // std140 alignment
		vec2 uvScale;
		vec2 uvOffset;
	};

	// Create/update an instance’s frames:
	// - upsert(id, path): single-frame instance (replace if already present)
	// - upsert(id, paths): multi-frame instance (replace if already present)
	void upsert(int id, const string &path);
	void upsert(int id, const vector<string> &paths);

	// Change the active *local* frame for an instance (0..frameCount-1). Clamped.
	void setFrame(int id, uint32_t frameIndex);

	void erase(int id);

	// Query how many GPU textures we have
	uint32_t textureCount() const { return static_cast<uint32_t>(gpuTextures.size()); }

	vec2 getPixelDimensions(int idx, int texIdx);

	void recalcUV();

	void record(VkCommandBuffer cmd) override;

  protected:
	// Model hooks
	uint32_t createDescriptorPool() override;
	void createDescriptors() override;
	void createGraphicsPipeline() override;

	void syncPickingInstances() override;

  private:
	struct Vertex {
		glm::vec3 pos;
		glm::vec2 uv;
	};

	struct GpuTex {
		VkImage image{VK_NULL_HANDLE};
		VkDeviceMemory memory{VK_NULL_HANDLE};
		VkImageView view{VK_NULL_HANDLE};
		VkSampler sampler{VK_NULL_HANDLE};
		uint32_t w{0}, h{0};
	};

	// CPU pixels for staging
	struct CpuPixels {
		int w{0}, h{0}, comp{4};
		std::vector<uint8_t> rgba;
	};

	bool set1Dirty = false;

	uint32_t cpuTextureCount() const { return static_cast<uint32_t>(cpuFrames.size()); }

	void writeSet1Descriptors();
	void ensureSet1Ready();

	void upsertInternal(int id, const InstanceData &data);

	// ----- Mesh & buffers -----
	void buildUnitQuadMesh(); // 2 triangles, Z=0, UVs in [0,1]

	// ----- Dynamic frames management -----
	// Public-facing “database” of frames by instance (authoritative).
	std::map<int, std::vector<std::string>> framesPerInstance;

	// Instance shadow to remember transforms and selected *local* frame across rebuilds.
	std::unordered_map<int, glm::mat4> instanceModel;	  // id -> model
	std::unordered_map<int, uint32_t> instanceLocalFrame; // id -> local frame index (0..N_i-1)

	// Rebuild the flattened texture pool and descriptor set from framesPerInstance.
	void rebuildTexturePool();

	// Load all frames (CPU), then upload to GPU and create descriptors.
	void loadAllFramesCPU(const std::map<int, std::vector<std::string>> &framesPerInstance);
	void uploadAllFramesGPU(); // uses a small staging path
	void destroyAllTextures();

	// Helpers for Vulkan image upload
	void transition(VkImage img, VkImageLayout oldL, VkImageLayout newL, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, VkAccessFlags srcAccess, VkAccessFlags dstAccess);
	void copyBufferToImage(VkBuffer staging, VkImage img, uint32_t w, uint32_t h);

	// ----- Flattened pool & per-instance mapping -----
	std::vector<CpuPixels> cpuFrames;
	std::vector<GpuTex> gpuTextures;

	// For each instance: first texture global index and count
	std::unordered_map<int, uint32_t> instanceFirstIndex;
	std::unordered_map<int, uint32_t> instanceFrameCount;

	// Descriptor set layout for images (set=1, binding=0)
	// We'll write N VkDescriptorImageInfo (one per texture)
	std::vector<VkDescriptorImageInfo> imageInfos;
};
