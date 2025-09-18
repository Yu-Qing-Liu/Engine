#pragma once

#include "circuit.hpp"
#include "scenes.hpp"
#include "shapes.hpp"
#include "text.hpp"

class Main : public Scene {
  public:
	Main(Scenes &scenes);
	Main(Main &&) = default;
	Main(const Main &) = delete;
	Main &operator=(Main &&) = delete;
	Main &operator=(const Main &) = delete;
	~Main() = default;

	std::string getName() override { return "Main"; }

	void updateScreenParams() override;

	void updateComputeUniformBuffers() override;
	void computePass() override;

	void updateUniformBuffers() override;
	void renderPass() override;
	void swapChainUpdate() override;

  private:
	Model::UBO persp{};
	Model::UBO orthographic{};

	unique_ptr<Circuit> circuit;
	unique_ptr<InstancedPolygon> nodes;
	unique_ptr<InstancedPolygon> edges;

	struct NodeData {
		string name;
	};

	struct EdgeData {
		int cableId;
		float length;
	};

	unordered_map<int, NodeData> nodeMap;
	unordered_map<int, EdgeData> edgeMap;

	unique_ptr<Text> nodeName;
	vec3 nodePos;
	string nodeLabel;

	unique_ptr<Text> wireId;
	vec3 wirePos;
	string wireLabel;
};
