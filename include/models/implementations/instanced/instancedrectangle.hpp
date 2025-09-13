#pragma once

#include "colors.hpp"
#include "model.hpp"
#include <memory>
#include <unordered_map>

using std::shared_ptr;
using std::unordered_map;

class InstancedRectangle : public Model {
  public:
	InstancedRectangle(InstancedRectangle &&) = delete;
	InstancedRectangle(const InstancedRectangle &) = delete;
	InstancedRectangle &operator=(InstancedRectangle &&) = delete;
	InstancedRectangle &operator=(const InstancedRectangle &) = delete;

	struct InstanceData {
		mat4 model{1.0f};
		vec4 color{Colors::Green};
		vec4 outlineColor{Colors::Transparent(0.0f)};
		float outlineWidth{0.0f};
		float borderRadius{0.0f};
		float _pad0[2]{0.0f, 0.0f};

		InstanceData() = default;

		InstanceData(float x, float y, vec2 size, vec4 color = Colors::Green, vec4 outlineColor = Colors::Transparent(0.0f), float outlineWidth = 0.0f, float borderRadius = 0.0f) : model(glm::translate(mat4(1.0f), vec3(x, y, 0.0f)) * glm::scale(mat4(1.0), vec3(size, 1.0))), color(color), outlineColor(outlineColor), outlineWidth(outlineWidth), borderRadius(borderRadius) {}
	};

	struct Vertex {
		vec3 pos;

		static VkVertexInputBindingDescription getBindingDescription() {
			VkVertexInputBindingDescription bd{};
			bd.binding = 0;
			bd.stride = sizeof(Vertex);
			bd.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			return bd;
		}

		static std::array<VkVertexInputAttributeDescription, 1> getAttributeDescriptions() {
			std::array<VkVertexInputAttributeDescription, 1> a{};
			a[0].binding = 0;
			a[0].location = 0;
			a[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			a[0].offset = offsetof(Vertex, pos);
			return a;
		}
	};

	InstancedRectangle(const UBO &ubo, ScreenParams &screenParams, shared_ptr<unordered_map<int, InstanceData>> instances, uint32_t maxInstances = 65536);
	~InstancedRectangle() override;

	void render() override; // bind 2 VBs and draw N instances
	void updateInstance(int id, const InstanceData &newData);
	void deleteInstance(int id);

  protected:
	// ---- Model overrides we need for instancing ----
	void createBindingDescriptions() override; // adds instance binding + attrs
	void setupGraphicsPipeline() override;	   // culling/depth tweaks (optional)

  private:
	shared_ptr<unordered_map<int, InstanceData>> instances;
	bool modified = true;

	std::vector<Vertex> vertices = {
		{{-0.5f, -0.5f, 0.0f}},
		{{0.5f, -0.5f, 0.0f}},
		{{0.5f, 0.5f, 0.0f}},
		{{-0.5f, 0.5f, 0.0f}},
	};

	// Per-frame instance buffers
	std::vector<VkBuffer> instanceBuffers;
	std::vector<VkDeviceMemory> instanceMemories;
	std::vector<void *> instanceMapped;

	// We’ll store both bindings here to pass them in createGraphicsPipeline()
	VkVertexInputBindingDescription vertexBD{};	  // binding 0 (per-vertex)
	VkVertexInputBindingDescription instanceBD{}; // binding 1 (per-instance)

	std::array<VkVertexInputBindingDescription, 2> bindings{};

	// Dense packing (slot i -> instance i)
	std::vector<InstanceData *> cpuPtrs; // pointer into instances->element.second
	std::vector<int> slotToKey;
	std::unordered_map<int, uint32_t> keyToSlot;

	uint32_t instanceCount = 0; // number of *live* slots (0..instanceCount-1 are valid)

	// Dirty tracking (upload only when modified)
	std::array<bool, Engine::MAX_FRAMES_IN_FLIGHT> frameDirty{}; // mark which frames need refresh

	void createInstanceBuffers(size_t maxInstances = 65536);
    void uploadIfDirty();
};
