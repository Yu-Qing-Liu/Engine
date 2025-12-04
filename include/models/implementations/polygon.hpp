#pragma once

#include "colors.hpp"
#include "model.hpp"

class Polygon : public Model {
  public:
	Polygon(Scene *scene);

	struct InstanceData {
		mat4 model{1.0f};					// locations 4..7
		vec4 color = Colors::Red;			// location 8
		vec4 outlineColor = Colors::Yellow; // location 9
		float outlineWidth = 1.0f;			// location 10
		float _pad0{0}, _pad1{0}, _pad2{0}; // keep 16B alignment (std430)
	};

	struct Vertex {
		vec3 pos;	  // loc 0
		vec3 normal;  // loc 1 (ignored by outline pass)
		glm::vec2 uv; // loc 2 (ignored by outline pass)
		vec4 color;	  // loc 3
	};

	// CPU-only expanded attribute set used by the outline shader
	struct Attributes {
		vec3 pos;
		vec4 color;
		vec3 bary;
		vec3 edgeMask;
	};

	// Initialize GPU buffers and pipeline inputs. Expands verts to barycentric form.
	void init(const std::vector<Vertex> &verts, const std::vector<uint32_t> &idx);
	void initNGon(size_t n = 3);
	void initCube();

	void enableDepth() { cull_ = true; }

  protected:
	void syncPickingInstances() override;
	void createGraphicsPipeline() override;

  private:
	struct EdgeKey {
		uint32_t a, b;
		bool operator==(const EdgeKey &o) const noexcept { return a == o.a && b == o.b; }
	};
	struct EdgeKeyHash {
		size_t operator()(const EdgeKey &k) const noexcept { return (size_t(k.a) << 32) ^ size_t(k.b); }
	};

	template <typename T> static void expandForOutlines(const std::vector<T> &inVerts, const std::vector<uint32_t> &inIdx, std::vector<Attributes> &outVerts, std::vector<uint32_t> &outIdx);

	bool cull_ = false;

	std::vector<Attributes> s_cpuVerts;
	std::vector<uint32_t> s_cpuIdx;
};
