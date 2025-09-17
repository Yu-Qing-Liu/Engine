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

	static const std::string getName() { return "Main"; }

	void updateScreenParams() override;

	void updateComputeUniformBuffers() override;
	void computePass() override;

	void updateUniformBuffers() override;
	void renderPass() override;
	void swapChainUpdate() override;

  private:
	Model::UBO persp{};
	Model::UBO orthographic{};

	// Camera state (meters)
	glm::vec3 camPos{12.0f, 12.0f, 12.0f};
	glm::vec3 camTarget{0.0f, 0.0f, 0.0f};
	glm::vec3 camUp{0.0f, 0.0f, 1.0f};
	float camSpeed = 1.0f;

	// FPS mouselook state
	float yaw = 0.0f;		   // radians, wraps freely
	float pitch = 0.0f;		   // radians, clamp to ~(-89°, +89°)
	float mouseSens = 0.0025f; // tweak to taste
	vec3 lookAtCoords;

	// mouse-aim state
	double lastPointerX = -1.0;
	double lastPointerY = -1.0;

	std::array<bool, GLFW_KEY_LAST + 1> keyDown{};

	unique_ptr<Circuit> circuit;
	unique_ptr<InstancedPolygon> nodes;
	unique_ptr<InstancedPolygon> edges;

	struct NodeData {
		string name;
	};

	struct EdgeData {
		float length;
	};

	unordered_map<int, NodeData> nodeMap;
	unordered_map<int, EdgeData> edgeMap;

	unique_ptr<Text> nodeName;
	vec3 textPos;
	string label;

	void handleCameraInput(float dt);
	void mouseLookFPS();
};
