#include "graph.hpp"

#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_map>
#include <unordered_set>
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

static uint64_t hash64(const std::string &s) {
	uint64_t h = 1469598103934665603ull; // FNV-1a
	for (unsigned char c : s) {
		h ^= c;
		h *= 1099511628211ull;
	}
	return h;
}

static glm::quat quatFromTo(const glm::vec3 &fromRaw, const glm::vec3 &toRaw) {
	glm::vec3 from = glm::normalize(fromRaw);
	glm::vec3 to = glm::normalize(toRaw);
	float c = glm::dot(from, to);
	if (c < -1.0f + 1e-6f) {
		glm::vec3 ortho = glm::abs(from.x) < 0.5f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
		glm::vec3 axis = glm::normalize(glm::cross(from, ortho));
		return glm::angleAxis(glm::pi<float>(), axis);
	}
	if (c > 1.0f - 1e-6f)
		return glm::quat(1, 0, 0, 0);
	glm::vec3 axis = glm::cross(from, to);
	float s = glm::sqrt((1.0f + c) * 2.0f);
	float invs = 1.0f / s;
	return glm::normalize(glm::quat(s * 0.5f, axis.x * invs, axis.y * invs, axis.z * invs));
}

static glm::quat rotatePlusXTo(const glm::vec3 &dirN) { return quatFromTo(glm::vec3(1, 0, 0), dirN); }

glm::vec4 Graph::colorFromKey(const std::string &key) {
	uint64_t h = hash64(key);
	float hue = ((h >> 8) & 0xFFFFFF) / float(0x1000000u); // 24-bit hue
	float sat = 0.65f + 0.1f * ((h & 0x7) / 7.0f);		   // 0.65..0.75
	float val = 0.88f;									   // bright
	return Colors::hsv2rgba(hue, sat, val, 1.0f);
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

	// ---------- collect node ids ----------
	std::unordered_set<std::string> idset;
	for (const auto &kv : G.adj) {
		idset.insert(kv.first);
		for (const auto &e : kv.second)
			idset.insert(e.child);
	}
	std::vector<std::string> ids(idset.begin(), idset.end());
	std::sort(ids.begin(), ids.end());
	const int N = (int)ids.size();
	if (N == 0)
		return;

	std::unordered_map<std::string, int> idToIdx;
	idToIdx.reserve(N);
	for (int i = 0; i < N; ++i)
		idToIdx[ids[i]] = i;

	// ---------- build directed adj / radj (dedup) ----------
	std::vector<std::vector<int>> adj(N), radj(N);
	std::vector<int> indeg(N, 0), outdeg(N, 0);

	// Also: avg edge length and a map (u,v) -> cableId from G.adj
	float avgLen = 0.f;
	int m = 0;
	std::unordered_map<long long, int> cableIdByUV;
	auto packUV = [](int u, int v) -> long long { return ((long long)u << 32) ^ (unsigned long long)(v & 0xffffffff); };

	{
		std::unordered_set<long long> seen;
		auto pack = [](int u, int v) -> long long { return ((long long)u << 32) ^ (unsigned long long)(v & 0xffffffff); };

		for (const auto &kv : G.adj) {
			auto itu = idToIdx.find(kv.first);
			if (itu == idToIdx.end())
				continue;
			int u = itu->second;

			for (const auto &e : kv.second) {
				auto itv = idToIdx.find(e.child);
				if (itv == idToIdx.end())
					continue;
				int v = itv->second;

				long long k = pack(u, v);
				if (seen.insert(k).second) {
					adj[u].push_back(v);
					radj[v].push_back(u);
					outdeg[u]++;
					indeg[v]++;
				}

				// Edge meta
				avgLen += e.length;
				++m;
				long long kdir = packUV(u, v);
				if (!cableIdByUV.count(kdir))
					cableIdByUV[kdir] = e.id;
				long long krev = packUV(v, u);
				if (!cableIdByUV.count(krev))
					cableIdByUV[krev] = e.id; // allow lookup either way
			}
		}
	}
	if (m)
		avgLen /= float(m);
	if (!m)
		avgLen = 10.f;

	// ---------- choose roots (pure topology) ----------
	std::vector<int> roots;
	for (int i = 0; i < N; ++i)
		if (indeg[i] == 0)
			roots.push_back(i);

	// If no roots (cycles), choose one representative per weakly connected component.
	std::vector<int> comp(N, -1);
	int compCount = 0;
	{
		// build undirected adjacency
		std::vector<std::vector<int>> uadj(N);
		for (int u = 0; u < N; ++u) {
			for (int v : adj[u]) {
				uadj[u].push_back(v);
				uadj[v].push_back(u);
			}
		}
		for (int i = 0; i < N; ++i) {
			if (comp[i] != -1)
				continue;
			std::queue<int> q;
			q.push(i);
			comp[i] = compCount;
			while (!q.empty()) {
				int u = q.front();
				q.pop();
				for (int v : uadj[u])
					if (comp[v] == -1) {
						comp[v] = compCount;
						q.push(v);
					}
			}
			if (roots.empty()) {
				int rep = i;
				for (int u = 0; u < N; ++u)
					if (comp[u] == compCount && ids[u] < ids[rep])
						rep = u;
				roots.push_back(rep);
			}
			compCount++;
		}
	}

	// ---------- BFS from roots to get depth/parent ----------
	std::vector<int> depth(N, INT_MAX), parent(N, -1);
	{
		std::queue<int> q;
		for (int r : roots) {
			depth[r] = 0;
			parent[r] = -1;
			q.push(r);
		}
		while (!q.empty()) {
			int u = q.front();
			q.pop();
			for (int v : adj[u])
				if (depth[v] > depth[u] + 1) {
					depth[v] = depth[u] + 1;
					parent[v] = u;
					q.push(v);
				}
		}
		// Attach any unreached nodes to a nearby predecessor or to a root
		for (int v = 0; v < N; ++v)
			if (depth[v] == INT_MAX) {
				if (!radj[v].empty()) {
					parent[v] = radj[v][0];
					depth[v] = (depth[parent[v]] == INT_MAX ? 1 : depth[parent[v]] + 1);
				} else {
					parent[v] = roots[0];
					depth[v] = 1;
				}
			}
	}

	// ---------- families from topology ----------
	auto topAncestor = [&](int v) -> int {
		int u = v;
		while (parent[u] != -1)
			u = parent[u];
		return u;
	};

	// Family key: ROOT:<root-id> for nodes under a root; otherwise COMP:<c>:<rep>
	std::vector<std::string> familyKey(N);
	for (int v = 0; v < N; ++v) {
		bool isRoot = (depth[v] == 0);
		if (!roots.empty()) {
			int r = isRoot ? v : topAncestor(v);
			familyKey[v] = std::string("ROOT:") + ids[r];
		} else {
			int c = comp[v];
			int rep = v;
			for (int u = 0; u < N; ++u)
				if (comp[u] == c && ids[u] < ids[rep])
					rep = u;
			familyKey[v] = "COMP:" + std::to_string(c) + ":" + ids[rep];
		}
	}

	// Deterministic color per family
	for (int v = 0; v < N; ++v)
		if (!familyColor.count(familyKey[v])) {
			familyColor[familyKey[v]] = colorFromKey(familyKey[v]); // your deterministic HSV-from-hash helper
		}

	// ---------- layout ----------
	// Column order: stable sort by id
	std::vector<int> order(N);
	std::iota(order.begin(), order.end(), 0);
	std::sort(order.begin(), order.end(), [&](int a, int b) { return ids[a] < ids[b]; });

	// Side (above/below) by simple topology: sign(outdeg - indeg); roots on trunk
	auto sideSign = [&](int v) -> int {
		int s = (outdeg[v] > indeg[v]) ? +1 : (outdeg[v] < indeg[v] ? -1 : +1);
		return (depth[v] == 0) ? 0 : s;
	};

	const float trunkY = 0.0f;
	const float dx = std::max(9.0f, avgLen * 0.26f);
	const float tierBase = std::max(8.0f, avgLen * 0.10f);
	const float tierStep = std::max(5.0f, avgLen * 0.10f);

	// Place anchors at depth==1 into lanes
	std::vector<int> ups1, downs1, mids0;
	for (int v : order) {
		if (depth[v] == 0) {
			mids0.push_back(v);
			continue;
		}
		if (depth[v] == 1)
			(sideSign(v) > 0 ? ups1 : downs1).push_back(v);
	}

	std::vector<float> xcol(N, 0.f);
	auto placeLane = [&](const std::vector<int> &lane, float x0) {
		for (int i = 0; i < (int)lane.size(); ++i)
			xcol[lane[i]] = x0 + float(i) * dx;
	};
	placeLane(ups1, 0.0f);		  // integer slots
	placeLane(downs1, 0.5f * dx); // half slots
	for (int i = 0; i < (int)mids0.size(); ++i)
		xcol[mids0[i]] = float(i) * dx;

	// Children vectors for consistent ordering deeper than 1
	std::vector<std::vector<int>> kids(N);
	for (int v = 0; v < N; ++v)
		if (parent[v] >= 0)
			kids[parent[v]].push_back(v);
	for (int p = 0; p < N; ++p)
		std::sort(kids[p].begin(), kids[p].end(), [&](int a, int b) { return ids[a] < ids[b]; });

	// Inherit parent column for deeper levels
	for (int d = 2; d < N + 2; ++d) {
		bool any = false;
		for (int p = 0; p < N; ++p) {
			if (depth[p] != d - 1)
				continue;
			for (int v : kids[p])
				if (depth[v] == d) {
					xcol[v] = xcol[p];
					any = true;
				}
		}
		if (!any)
			break;
	}

	auto yFor = [&](int v) -> float {
		if (depth[v] == 0)
			return trunkY;
		float mag = tierBase + float(depth[v] - 1) * tierStep;
		int s = sideSign(v);
		if (s == 0)
			s = +1;
		return trunkY + (s > 0 ? +mag : -mag);
	};

	std::vector<glm::vec3> pos(N, glm::vec3(0));
	for (int v = 0; v < N; ++v)
		pos[v] = glm::vec3(xcol[v], yFor(v), 0.f);

	// Extents & camera
	float minX = 1e9f, maxX = -1e9f;
	for (int i = 0; i < N; ++i) {
		minX = std::min(minX, pos[i].x);
		maxX = std::max(maxX, pos[i].x);
	}
	float sceneRadius = 1.f;
	for (auto &p : pos)
		sceneRadius = std::max(sceneRadius, glm::length(p));
	const float w = screenParams.viewport.width, h = screenParams.viewport.height, aspect = w / h;
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
		const float visH = baseH / zoom, visW = baseW / zoom;
		const float orthoScale = (aspect >= 1.0f) ? visW : visH;
		mvp.proj = Camera::blenderOrthographicMVP(w, h, orthoScale, mvp.view).proj;
	}

	nodeName->updateMVP(std::nullopt, mvp.view, mvp.proj);
	wireId->updateMVP(std::nullopt, mvp.view, mvp.proj);

	// ---------- draw nodes with family colors ----------
	const float nodeScale = 2.0f;
	for (int i = 0; i < N; ++i) {
		const glm::vec4 color = familyColor[familyKey[i]];
		nodes->updateInstance(i, InstancedPolygonData(pos[i], glm::vec3(nodeScale), color, Colors::Black));
		nodeMap[i] = {ids[i]};
	}
	nodes->updateMVP(std::nullopt, std::nullopt, mvp.proj);

	// ---------- draw wires ----------
	const float edgeThickness = glm::clamp(avgLen * 0.016f, 0.016f, 0.20f);
	const glm::vec4 wireColor = Colors::Gray;
	int eIdx = 0;
	auto addSeg = [&](const glm::vec3 &P0, const glm::vec3 &P1, int cableId) {
		glm::vec3 d = P1 - P0;
		float L = glm::length(d);
		if (L < 1e-6f)
			return;
		glm::vec3 mid = 0.5f * (P0 + P1);
		glm::quat rot = rotatePlusXTo(d / L);
		edgeMap[eIdx] = {cableId, L};
		edges->updateInstance(eIdx++, InstancedPolygonData(mid, glm::vec3(L, edgeThickness, edgeThickness), rot, wireColor, Colors::Black));
	};

	// Trunk line
	const float pad = 0.75f * dx;
	addSeg(glm::vec3(minX - pad, trunkY, 0.f), glm::vec3(maxX + pad, trunkY, 0.f), 0);

	// depth==1: verticals to trunk
	for (int v = 0; v < N; ++v)
		if (depth[v] == 1) {
			int p = parent[v];
			int cableId = 0;
			if (p >= 0) {
				auto it = cableIdByUV.find(packUV(p, v));
				if (it != cableIdByUV.end())
					cableId = it->second;
			}
			addSeg(pos[v], glm::vec3(xcol[v], trunkY, 0.f), cableId);
		}

	// depth>=2: connect to parent
	for (int v = 0; v < N; ++v)
		if (depth[v] >= 2 && parent[v] >= 0) {
			int p = parent[v];
			int cableId = 0;
			auto it = cableIdByUV.find(packUV(p, v));
			if (it != cableIdByUV.end())
				cableId = it->second;
			addSeg(pos[v], pos[p], cableId);
		}

	edges->updateMVP(std::nullopt, std::nullopt, mvp.proj);

	// ---------- legend (one entry per family in view) ----------
	std::unordered_map<std::string, int> famCounts;
	for (auto &fk : familyKey)
		famCounts[fk]++;
	std::vector<LegendEntry> newLegend;
	newLegend.reserve(famCounts.size());
	for (auto &kv : famCounts) {
		const std::string &key = kv.first;
		int sz = kv.second;
		std::string label;
		if (key.rfind("ROOT:", 0) == 0) {
			label = key.substr(5) + " (family, " + std::to_string(sz) + ")";
		} else {
			auto colon = key.find(':', 5);
			std::string idx = key.substr(5, colon - 5);
			std::string rep = key.substr(colon + 1);
			label = "Component " + idx + " (rep: " + rep + ", " + std::to_string(sz) + ")";
		}
		newLegend.push_back({label, familyColor[key]});
	}
	std::sort(newLegend.begin(), newLegend.end(), [](auto &a, auto &b) { return a.label < b.label; });

	if (newLegend.size() != legendEntries.size()) {
		legendEntries = std::move(newLegend);
	} else {
		bool diff = false;
		for (size_t i = 0; i < newLegend.size(); ++i) {
			if (legendEntries[i].label != newLegend[i].label || legendEntries[i].color != newLegend[i].color) {
				diff = true;
				break;
			}
		}
		if (diff)
			legendEntries = std::move(newLegend);
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
