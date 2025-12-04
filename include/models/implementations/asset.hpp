#pragma once

#include "colors.hpp"
#include "model.hpp"
#include "scene.hpp"

#include <glm/glm.hpp>
#include <span>
#include <vector>

using std::string;

class Asset : public Model {
  public:
	explicit Asset(Scene *scene);
	~Asset();

	// --------- configuration ---------
	// Change this to your rig cap. Must match shader usage.
	static constexpr uint32_t MAX_BONES = 128;

	// --------- Per-vertex (binding 0) ---------
	struct Vertex {
		glm::vec3 pos;		// location = 0
		glm::vec3 normal;	// location = 1
		glm::vec4 color;	// location = 2
		glm::vec2 uv;		// location = 3
		glm::vec4 tanSgn;	// location = 4 (xyz=tangent, w=sign)
		uint32_t matId;		// location = 5
		glm::uvec4 boneIds; // location = 10
		glm::vec4 weights;	// location = 11
	};

	// --------- Per-instance (binding 1) ---------
	// model rows (6..9), outline color (13), outline width (14),
	// bonesBase (12) -> base index into the big SSBO of bone matrices (in mat4 units)
	struct InstanceData {
		glm::mat4 model = mat4(1.f);						// loc 6..9
		glm::vec4 outlineColor = Colors::Transparent(0.0f); // loc 13
		float outlineWidth = 0.0f;							// loc 14
		uint32_t bonesBase;									// loc 12
		float _pad0{};										// keep 16B alignment
	};

	// ---------- lifecycle ----------
	void init() override;

	// Load geometry from disk (obj/dae/glb/…) into GPU buffers and ensure an instance exists for id.
	void upsertInstance(int id, const string &assetPath);

	// Update per-instance payload (transform/outline). bonesBase is auto-filled by the class.
	void upsertInstance(int id, const InstanceData &data);

	mat4 getBoneTransform(int id, string boneName);
	void applyBoneTransform(int id, string boneName, mat4 model, bool override = false);

  protected:
	// Set the bone palette for one instance (size will be clamped to MAX_BONES)
	void setBones(int id, std::span<const glm::mat4> palette);

	// ---------- Model virtuals we extend ----------
	uint32_t createDescriptorPool() override;
	void createDescriptors() override;
	void createGraphicsPipeline() override;
	void record(VkCommandBuffer cmd) override; // flush bones before draw & guard null buffers

	void syncPickingInstances() override;

  private:
	std::unique_ptr<Pipeline> outline;
	std::string outlineShaderPath = Assets::shaderRootPath + "/outline";

	// CPU copies alive through Model::init()
	std::vector<Vertex> cpuVerts_;
	std::vector<uint32_t> cpuIdx_;

	// Set=1, binding=0: bones SSBO (all instances concatenated)
	VkBuffer bonesSsbo_ = VK_NULL_HANDLE;
	VkDeviceMemory bonesMem_ = VK_NULL_HANDLE;
	uint8_t *bonesMapped_ = nullptr;

	// CPU shadow of all bone palettes (count * MAX_BONES)
	std::vector<glm::mat4> bonesCPU_;
	std::vector<bool> bonesDirty_; // dirty flags per-slot

	// Descriptor write state
	bool set1Dirty_ = true;

	std::unordered_map<string, uint32_t> boneMap;
	std::vector<glm::mat4> boneBase;
	std::vector<glm::mat4> boneOffset;
	std::vector<glm::mat4> boneOffsetInv;

	std::vector<int> boneParent_;					  // -1 = no parent
	std::vector<std::vector<uint32_t>> boneChildren_; // adjacency list

	// Internals
	void writeSet1Descriptors();
	void ensureSet1Ready();
	void createBonesSsbo_();
	void destroyBonesSsbo_();

	// When an instance is (up)inserted, ensure its bonesBase matches its slot.
	void ensureBonesBaseFor(int id);

	// Outline pipeline
	void createOutlinePipeline();
	void recordOutline(VkCommandBuffer cmd);
};
