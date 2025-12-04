#pragma once

#include "colors.hpp"
#include "model.hpp"

class Grid : public Model {
  public:
	Grid(Scene *scene);
	~Grid() = default;

	struct InstanceData {
		mat4 model{1.0f};
		vec4 color = Colors::Gray;
		float cellSize = 1.0f; // 1 X 1 m
		float lineWidth = 1.0f;
	};

	struct Vertex {
		vec3 pos;
	};

	void init() override;

  protected:
	void syncPickingInstances() override;
	void createGraphicsPipeline() override;

  private:
	void buildUnitQuadMesh();
};
