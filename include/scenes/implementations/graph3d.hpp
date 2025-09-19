#pragma once

#include "circuit.hpp"
#include "scenes.hpp"
#include "shapes.hpp"
#include "text.hpp"

class Graph3D : public Scene {
  public:
	Graph3D(Scenes &scenes);
	Graph3D(Graph3D &&) = default;
	Graph3D(const Graph3D &) = delete;
	Graph3D &operator=(Graph3D &&) = delete;
	Graph3D &operator=(const Graph3D &) = delete;
	~Graph3D() = default;

	std::string getName() override { return "Graph3D"; }

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
