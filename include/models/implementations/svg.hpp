#pragma once

#include "colors.hpp"
#include "model.hpp"

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

using std::string;
using std::vector;

class SVG : public Model {
  public:
	SVG(Scene *scene);
	~SVG();

	// Initialize renderer (no images required up-front).
	void init() override;

	// Instance management
	struct InstanceData {
		glm::mat4 model{1.0f};
		uint32_t frameIndex{0}; // GLOBAL index in the merged texture pool
		vec4 color = Colors::White;
		glm::vec2 uvScale{1.0f};
		glm::vec2 uvOffset{0.0f};
	};

	// Same API as Image: single- or multi-frame per instance
	void upsert(int id, const string &path);
	void upsert(int id, const vector<string> &paths);

	// Change the active *local* frame for an instance (0..frameCount-1). Clamped.
	void setFrame(int id, uint32_t frameIndex);

	void erase(int id);

	uint32_t textureCount() const { return static_cast<uint32_t>(gpuTextures.size()); }

	glm::vec2 getPixelDimensions(int idx, int texIdx);

	void recalcUV();

	void record(VkCommandBuffer cmd) override;

  protected:
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

	void writeSet1Descriptors();
	void ensureSet1Ready();

	void upsertInternal(int id, const InstanceData &data);

	void buildUnitQuadMesh(); // 2 triangles, Z=0, UVs in [0,1]

	// Public-facing “database” of frames by instance (same as Image)
	std::map<int, std::vector<std::string>> framesPerInstance;
	std::unordered_map<int, glm::mat4> instanceModel;
	std::unordered_map<int, uint32_t> instanceLocalFrame; // 0..N_i-1

	void rebuildTexturePool();

	// *** SVG loading path ***
	void loadAllFramesCPU(const std::map<int, std::vector<std::string>> &framesPerInstance);
	void uploadAllFramesGPU();
	void destroyAllTextures();

	void transition(VkImage img, VkImageLayout oldL, VkImageLayout newL, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, VkAccessFlags srcAccess, VkAccessFlags dstAccess);
	void copyBufferToImage(VkBuffer staging, VkImage img, uint32_t w, uint32_t h);

	std::vector<CpuPixels> cpuFrames;
	std::vector<GpuTex> gpuTextures;

	std::unordered_map<int, uint32_t> instanceFirstIndex;
	std::unordered_map<int, uint32_t> instanceFrameCount;

	std::vector<VkDescriptorImageInfo> imageInfos;
};
