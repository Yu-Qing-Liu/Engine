#pragma once

#include "colors.hpp"
#include "model.hpp"

class Rectangle : public Model {
  public:
	Rectangle(Scene *scene);
	~Rectangle() = default;

	struct InstanceData {
		mat4 model{1.0f};
		vec4 color = Colors::Red;
		vec4 outlineColor = Colors::Yellow;
		float outlineWidth = 1.0f;
		float borderRadius = 1.0f;
		float _pad1 = 0.0f, _pad2 = 0.0f;
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
