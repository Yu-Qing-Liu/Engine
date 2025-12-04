#pragma once

#include "colors.hpp"
#include "model.hpp"

class Line : public Model {
  public:
	Line(Scene *scene);
	~Line() = default;

	static constexpr uint32_t MAX_POINTS = 128;

	struct InstanceData {
		mat4 model{1.0f};
		vec3 p1;
		vec3 p2;
		vec4 color = Colors::Red;
		float lineWidth = 1.0f;
	};

	struct Vertex {
		vec3 pos;
	};

	struct LinePC {
		vec2 viewportSize;
	};

	void init() override;

  protected:
	void createGraphicsPipeline() override;
	void syncPickingInstances() override;
	void pushConstants(VkCommandBuffer cmd, VkPipelineLayout pipeLayout) override;

  private:
	LinePC pc{};

	void buildUnitQuadMesh();
};
