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

	std::string getName() override { return "Graph"; }

	void updateScreenParams() override;

	void updateComputeUniformBuffers() override;
	void computePass() override;

	void updateUniformBuffers() override;
	void renderPass() override;
	void swapChainUpdate() override;

	// ---------- 8-color classifier ----------
	enum class Kind {
		PCGH,							 // Purple
		Drainage,						 // DarkBlue
		BJ_Primary_Installed,			 // Turquoise (SPN-*)
		BJ_Primary_NotInstalled,		 // Orange (BJ-297* or text flags)
		BJ_Primary_InstalledConnected,	 // Green (BJ-295/287* + generic BJ-*)
		BJ_AdductionWater,				 // Pink (BJ-192* or text flags)
		BJ_Secondary_NotHeated,			 // DeepPink (BJ-291* except 291B)
		BJ_Secondary_InstalledConnected, // Blue (BJ-291B)
		SensorTTC,						 // Yellow (TTC-*)
		Unknown,
		End
	};

	static string stringFor(Kind k) {
		switch (k) {
		case Kind::PCGH:
			return "PCGH";
		case Kind::Drainage:
			return "Drainage"; // 0
		case Kind::BJ_Primary_Installed:
			return "BJ Primary Installed"; // 1 (SPN-*)
		case Kind::BJ_Primary_NotInstalled:
			return "BJ Primary Not Installed"; // 2
		case Kind::BJ_Primary_InstalledConnected:
			return "BJ Primary Installed Connected"; // 3
		case Kind::BJ_AdductionWater:
			return "BJ Adduction Water"; // 4
		case Kind::BJ_Secondary_NotHeated:
			return "BJ Secondary Not Heated"; // 5
		case Kind::BJ_Secondary_InstalledConnected:
			return "BJ Secondary Installed Connected"; // 6
		case Kind::SensorTTC:
			return "Sensor TTC"; // 7
		default:
			return "Unknown"; // 8
		}
	}

	static glm::vec4 colorFor(Kind k) {
		switch (k) {
		case Kind::PCGH:
			return Colors::Purple;
		case Kind::Drainage:
			return Colors::DarkBlue; // 0
		case Kind::BJ_Primary_Installed:
			return Colors::Turquoise; // 1 (SPN-*)
		case Kind::BJ_Primary_NotInstalled:
			return Colors::Orange; // 2
		case Kind::BJ_Primary_InstalledConnected:
			return Colors::Green; // 3
		case Kind::BJ_AdductionWater:
			return Colors::Pink; // 4
		case Kind::BJ_Secondary_NotHeated:
			return Colors::DeepPink; // 5
		case Kind::BJ_Secondary_InstalledConnected:
			return Colors::Blue; // 6
		case Kind::SensorTTC:
			return Colors::Yellow; // 7
		default:
			return Colors::Red; // 8
		}
	}

  private:
	Model::UBO persp{};

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
