#pragma once

#include "circuit.hpp"
#include "scenes.hpp"
#include "shapes.hpp"
#include "text.hpp"

class Graph : public Scene {
  public:
	Graph(Scenes &scenes);
	Graph(Graph &&) = delete;
	Graph(const Graph &) = delete;
	Graph &operator=(Graph &&) = delete;
	Graph &operator=(const Graph &) = delete;
	~Graph() = default;

	struct LegendEntry {
		std::string label;
		vec4 color;
	};

	const std::vector<LegendEntry> &legend() const { return legendEntries; }

	std::string getName() override { return "Graph"; }

	void updateScreenParams() override;

	void updateComputeUniformBuffers() override;
	void computePass() override;

	void updateUniformBuffers() override;
	void renderPass() override;
	void swapChainUpdate() override;

  private:
	unique_ptr<Circuit> circuit;
	unique_ptr<InstancedPolygon> nodes;
	unique_ptr<InstancedPolygon> edges;

	std::unordered_map<std::string, glm::vec4> familyColor;
	std::vector<LegendEntry> legendEntries;
	static glm::vec4 colorFromKey(const std::string &key);

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
