#pragma once

#include "assets.hpp"
#include "pipeline.hpp"
#include "raypicking.hpp"

#include <functional>
#include <glm/glm.hpp>
#include <span>
#include <vulkan/vulkan_core.h>

using namespace glm;
using std::vector;

class Scene;
class Engine;

struct VPMatrix {
	mat4 view{1.0f};
	mat4 proj{1.0f};
	uint32_t billboard = 0u;
};

class Model {
  public:
	Model() : scene(nullptr) {}
	Model(Scene *scene);
	~Model();

	struct VertexSource {
		const void *data = nullptr;
		size_t bytes = 0;
		uint32_t stride = 0;
	};

	struct VertexAttr {
		uint32_t location = 0, binding = 0;
		VkFormat fmt = VK_FORMAT_R32G32B32_SFLOAT;
		uint32_t offset = 0;
	};

	struct IndexSource {
		const uint32_t *data = nullptr;
		size_t count = 0;
	};

	struct Mesh {
		VertexSource vsrc;
		IndexSource isrc;
		std::vector<VertexAttr> vertexAttrs;
	};

	struct InitInfo {
		VkDescriptorPool dpool{};
		VkSampleCountFlagBits samples{VK_SAMPLE_COUNT_1_BIT};
		Assets::ShaderModules shaders;
		Mesh mesh;
		uint32_t maxInstances{1};
		uint32_t instanceStrideBytes{0};
	};

	bool isVisible() { return visible; }
	void hide() { visible = false; }
	void swapChainUpdate(float vw, float vh, float fbw, float fbh);
	void tick(double timeSinceLastFrameMs, double timeMs);

	template <typename D> bool getInstance(int id, D &out) const {
		auto it = idToSlot.find(id);
		if (it == idToSlot.end())
			return false;
		if (iStride != sizeof(D))
			return false; // stride/type mismatch
		const uint32_t slot = it->second;
		const uint8_t *src = cpu.data() + slot * iStride;
		std::memcpy(&out, src, sizeof(D)); // safe, no aliasing/alignment issues
		return true;
	}

	template <typename D> void upsertInstance(int id, const D &d) {
		if (has(id)) {
			setInstance<D>(id, d);
		} else {
			std::span<const uint8_t> bytes{reinterpret_cast<const uint8_t *>(&d), sizeof(D)};
			upsertBytes(id, bytes);
		}
	}

  public:
	std::function<void(Model *, float, float, float, float)> onScreenResize;
	std::function<void(Model *, double, double)> onTick;
	std::function<void(Model *)> onMouseSelect;
	std::function<void(Model *)> onMouseDeselect;
	std::function<void(Model *, uint32_t)> onMouseClick;

  public:
	virtual void init();
	void destroy();

  public:
	void enableRayPicking();
	std::unique_ptr<RayPicking> picking;

  public:
	void setView(const mat4 &V);
	void setProj(const mat4 &P);
	void billboard(bool enable);
	void setViewport(float w, float h, float x = 0.f, float y = 0.f);
	void setFrameBuffer(float w, float h);

	VPMatrix &getVP() { return vp; }
	const VkViewport &getViewport() const { return viewport; }

  public:
	Scene *getScene() { return scene; }
	Engine *getEngine() { return engine.get(); }
	Pipeline *getPipeline() { return pipeline.get(); }

  public:
	void setMaxInstances(uint32_t maxInstances) { initInfo.maxInstances = maxInstances; }
	bool has(int id) const { return idToSlot.count(id); }
	void upsertBytes(int id, std::span<const uint8_t> bytes);
	void erase(int id);
	uint32_t instanceCount() const { return count; }
	uint32_t instanceStride() const { return iStride; }
	uint8_t *mappedInstancePtr() { return mappedSSBO; }

  public:
	bool mouseIsOver();
	uint32_t getPickedInstance();
	void setIsSelected(bool selected) { selected_ = selected; }
	bool selected() { return selected_; }

  public:
	virtual void compute(VkCommandBuffer cmd);
	virtual void record(VkCommandBuffer cmd);
	virtual void recordUI(VkCommandBuffer cmd, uint32_t blurLayerIdx);

  protected:
	Scene *scene;
	std::shared_ptr<Engine> engine;
	std::unique_ptr<Pipeline> pipeline;
	InitInfo initInfo{};
	float fbw{1.f}, fbh{1.f};

	VkViewport viewport{};
	VkRect2D scissor{};
	VPMatrix vp{};

	bool selected_ = false;
	bool pickingDispatched_ = false;

	virtual void pushConstants(VkCommandBuffer cmd, VkPipelineLayout pipeLayout) {}
	virtual uint32_t createDescriptorPool();
	virtual void createDescriptors();
	virtual void createGraphicsPipeline();

	virtual void syncPickingInstances() {};
	template <typename D> void syncPickingInstances() {
		if (!picking)
			return;

		if (iStride == 0 || count == 0) {
			// no instances -> tell picker there are none
			picking->uploadInstances(std::span<const RayPicking::InstanceXformGPU>{}, std::span<const int>{});
			pickingInstancesDirty = false;
			return;
		}

		std::vector<RayPicking::InstanceXformGPU> inst;
		std::vector<int> ids;
		inst.reserve(count);
		ids.reserve(count);

		// Inverse map: slot -> id (because idToSlot is id -> slot)
		std::vector<int> slotToId(count, -1);
		for (auto &kv : idToSlot) {
			int id = kv.first;
			uint32_t slot = kv.second;
			if (slot < slotToId.size())
				slotToId[slot] = id;
		}

		// Precompute camera rotation (world-space) for billboarded models
		// vp.view is the current view matrix used for this Model.
		glm::mat3 camRot(1.0f);
		if (vp.billboard) {
			// View = R^T * T in typical camera math
			// inverse(view) has R and -R*t; its upper 3x3 is the camera world rotation.
			glm::mat3 Rinv = glm::mat3(glm::inverse(vp.view));
			camRot = Rinv; // columns: camera right, up, forward in world space
		}

		for (uint32_t slot = 0; slot < count; ++slot) {
			const uint8_t *p = cpu.data() + slot * iStride;

			// Read full instance struct so we can grab the model mat at the right offset.
			D src{};
			std::memcpy(&src, p, sizeof(D));

			glm::mat4 modelMtx = src.model;

			if (vp.billboard) {
				// Decompose original model to get translation + scale
				glm::vec3 pos = glm::vec3(modelMtx[3]);

				glm::vec3 col0 = glm::vec3(modelMtx[0]);
				glm::vec3 col1 = glm::vec3(modelMtx[1]);
				glm::vec3 col2 = glm::vec3(modelMtx[2]);

				float sx = glm::length(col0);
				float sy = glm::length(col1);
				float sz = glm::length(col2);

				// Build a model matrix whose axes follow the camera, but keep the
				// original per-axis scale and translation.
				glm::mat4 billM(1.0f);
				billM[0] = glm::vec4(camRot[0] * sx, 0.0f); // right
				billM[1] = glm::vec4(camRot[1] * sy, 0.0f); // up
				billM[2] = glm::vec4(camRot[2] * sz, 0.0f); // forward
				billM[3] = glm::vec4(pos, 1.0f);			// translation

				modelMtx = billM;
			}

			RayPicking::InstanceXformGPU x{};
			x.model = modelMtx;
			x.invModel = glm::inverse(modelMtx);

			inst.push_back(x);
			ids.push_back(slotToId[slot]); // keep your logical ids
		}

		picking->uploadInstances(inst, ids);
		pickingInstancesDirty = false;
	}

  protected:
	bool visible = true;
	bool pickingInstancesDirty = true;
	bool ssboDirty = true;
	bool uboDirty = true;

	// buffers (host-visible for brevity)
	VkBuffer vbuf{}, ibuf{}, ubo{}, ssbo{};
	VkDeviceMemory vmem{}, imem{}, umem{}, smem{};
	uint8_t *mappedSSBO{nullptr}; // persistently mapped for speed

	// mesh
	Mesh mesh{};
	uint32_t indexCount{};

	// instances
	uint32_t maxInstances{}, iStride{}, count{};
	std::vector<uint8_t> cpu;
	std::unordered_map<int, uint32_t> idToSlot;

  private:
	template <typename D> bool setInstance(int id, const D &value) {
		auto it = idToSlot.find(id);
		if (it == idToSlot.end())
			return false;
		if (iStride != sizeof(D))
			return false;
		const uint32_t slot = it->second;
		uint8_t *dst = cpu.data() + slot * iStride;
		std::memcpy(dst, &value, sizeof(D));
		ssboDirty = true;
		pickingInstancesDirty = true;
		return true;
	}
};
