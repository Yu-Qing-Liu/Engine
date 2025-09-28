#include "graph.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include "camera.hpp"
#include "colors.hpp"
#include "engine.hpp"
#include "fonts.hpp"

Graph::Graph(Scenes &scenes) : Scene(scenes) {
	// Enable controls
	disableMouseMode();

	// Make sure screenParams are valid before constructing drawables
	updateScreenParams();

	// Set an initial camera (will be resized in swapChainUpdate)
	if (!Scene::mouseMode) {
		mvp = Camera::blenderPerspectiveMVP(screenParams.viewport.width, screenParams.viewport.height, lookAt(vec3(12.0f, 12.0f, 12.0f), vec3(0.0f), vec3(0.0f, 0.0f, 1.0f)));
	}

	// Circuit: default ctor loads the correct path (as you said)
	circuit = std::make_unique<Circuit>();

	// Setup: construct instanced meshes
	Text::FontParams tp{Fonts::ArialBold, 32};
	nodeName = make_unique<Text>(this, mvp, screenParams, tp);
	wireId = make_unique<Text>(this, mvp, screenParams, tp);

	nodes = Shapes::dodecahedra(this, mvp, screenParams);
	nodes->onMouseEnter = [&]() {
		if (!nodes->rayTracing->hitMapped) {
			return;
		}
		int id = nodes->rayTracing->hitMapped->primId;
		InstancedPolygonData prev = nodes->getInstance(id);
		prev.outlineColor = Colors::inverse(prev.color);
		prev.outlineWidth = 4.0f;
		nodes->updateInstance(id, prev);

		nodePos = vec3(prev.model[3].x, prev.model[3].y, prev.model[3].z + 2);
		nodeLabel = nodeMap[id].name;
	};
	nodes->onMouseExit = [&]() {
		if (!nodes->rayTracing->hitMapped) {
			return;
		}
		int id = nodes->rayTracing->hitMapped->primId;
		InstancedPolygonData prev = nodes->getInstance(id);
		prev.outlineColor = Colors::Black;
		prev.outlineWidth = 1.0f;
		nodes->updateInstance(id, prev);

		nodeLabel = "";
	};
	nodes->enableRayTracing(true);

	edges = Shapes::cubes(this, mvp, screenParams);
	edges->onMouseEnter = [&]() {
		if (!edges->rayTracing->hitMapped) {
			return;
		}
		int id = edges->rayTracing->hitMapped->primId;
		InstancedPolygonData prev = edges->getInstance(id);
		prev.outlineColor = Colors::Yellow;
		prev.outlineWidth = 4.0f;
		edges->updateInstance(id, prev);

		wirePos = vec3(prev.model[3].x, prev.model[3].y, prev.model[3].z + 1.0);
		wireLabel = "#" + std::to_string(edgeMap[id].cableId);
	};
	edges->onMouseExit = [&]() {
		if (!edges->rayTracing->hitMapped) {
			return;
		}
		int id = edges->rayTracing->hitMapped->primId;
		InstancedPolygonData prev = edges->getInstance(id);
		prev.outlineColor = Colors::Black;
		prev.outlineWidth = 1.0f;
		edges->updateInstance(id, prev);

		wireLabel = "";
	};
	edges->enableRayTracing(true);

	auto kbState = [this](int key, int, int action, int) {
		if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE) {
			enableMouseMode();
		}
	};
	Events::keyboardCallbacks.push_back(kbState);
}

void Graph::updateScreenParams() {
	screenParams.viewport.x = 0.0f;
	screenParams.viewport.y = 0.0f;
	screenParams.viewport.width = (float)Engine::swapChainExtent.width;
	screenParams.viewport.height = (float)Engine::swapChainExtent.height;
	screenParams.viewport.minDepth = 0.0f;
	screenParams.viewport.maxDepth = 1.0f;
	screenParams.scissor.offset = {(int32_t)screenParams.viewport.x, (int32_t)screenParams.viewport.y};
	screenParams.scissor.extent = {(uint32_t)screenParams.viewport.width, (uint32_t)screenParams.viewport.height};
}

void Graph::swapChainUpdate() {
	if (!circuit)
		return;

	const auto &G = circuit->unifilar();
	if (G.adj.empty() && G.level.empty())
		return;

	// ---- camera ----
	float sceneRadius = 1.f;
	// for (auto &p : pos)
	// 	sceneRadius = std::max(sceneRadius, glm::length(p));
	const float w = screenParams.viewport.width;
	const float h = screenParams.viewport.height;
	const float aspect = w / h;
	const float fovY = radians(45.0f);
	const float desiredDist = std::max(18.0f, sceneRadius * 0.1f);
	glm::vec3 dir = glm::normalize((camPos == glm::vec3(0)) ? glm::vec3(1, 1, 1) : camPos);
	camPos = dir * desiredDist;
	const float nearP = 0.05f, farP = std::max(desiredDist * 6.f, sceneRadius * 8.f);
	if (!Scene::mouseMode) {
		mvp.view = lookAt(camPos, camTarget, camUp);
		mvp.proj = perspective(fovY, aspect, nearP, farP);
	} else {
		glm::mat4 view = glm::lookAt(camPosOrtho, camPosOrtho + glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));
		const float yOffsetLocal = -10.0f;
		mvp.view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -yOffsetLocal, 0.0f)) * view;

		fovH = 2.0f * std::atan((Camera::sensorWidth * 0.5f) / Camera::focalLength);
		fovV = 2.0f * std::atan(std::tan(fovH * 0.5f) / aspect);
		baseH = 2.0f * 125.0f * std::tan(fovV * 0.5f);
		baseW = baseH * aspect;

		const float visH = baseH / zoom;
		const float visW = baseW / zoom;
		const float orthoScale = (aspect >= 1.0f) ? visW : visH;
		mvp.proj = Camera::blenderOrthographicMVP(w, h, orthoScale, mvp.view).proj;
	}
}

void Graph::updateComputeUniformBuffers() {}

void Graph::computePass() {}

void Graph::updateUniformBuffers() {
	if (!Scene::mouseMode) {
		firstPersonMouseControls();
		firstPersonKeyboardControls();
		mvp.view = lookAt(camPos, lookAtCoords, camUp);
	} else if (!is3D) {
		mapMouseControls();
		mapKeyboardControls();
		glm::mat4 view = glm::lookAt(camPosOrtho, camPosOrtho + glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));
		const float yOffset = -10.0f;
		mvp.view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -yOffset, 0.0f)) * view;
	}

	nodes->updateMVP(std::nullopt, mvp.view);
	edges->updateMVP(std::nullopt, mvp.view);
	nodeName->updateMVP(std::nullopt, mvp.view);
	wireId->updateMVP(std::nullopt, mvp.view);
}

void Graph::renderPass() {
	nodes->render();
	edges->render();
	float nodeTextLength = nodeName->getPixelWidth(nodeLabel);
	float wireTextLength = wireId->getPixelWidth(wireLabel);

    nodeName->textParams.text = nodeLabel;
    nodeName->textParams.billboardParams = Text::BillboardParams{nodePos, {-nodeTextLength / 2, 0}, true};
    nodeName->textParams.color = Colors::Orange;
	nodeName->render();

    wireId->textParams.text = wireLabel;
    wireId->textParams.billboardParams = Text::BillboardParams{wirePos, {-wireTextLength / 2, 0}, true}; 
    wireId->textParams.color = Colors::Green;
	wireId->render();
}
