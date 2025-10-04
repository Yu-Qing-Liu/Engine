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
#include "overlay.hpp"

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

static inline void rtrim(std::string &x) {
	while (!x.empty() && (x.back() == ' ' || x.back() == '_'))
		x.pop_back();
}

static inline void ltrim(std::string &x) {
	size_t k = 0;
	while (k < x.size() && (x[k] == ' ' || x[k] == '_'))
		++k;
	if (k)
		x.erase(0, k);
}

static std::string baseKeyFromId(const std::string &s) {
	if (s.empty())
		return {};

	int first = (int)s.find('-');
	if (first == std::string::npos) {
		// 0 hyphens → whole string
		return s;
	}

	int last = (int)s.find_last_of('-');
	if (first == last) {
		// 1 hyphen → everything before it
		return s.substr(0, first);
	}

	// 2+ hyphens → from start up to (not including) last hyphen
	return s.substr(0, last);
}

glm::vec4 Graph::colorFromKey(const std::string &key) {
	// Stronger 64-bit mix so early/late chars influence all bits
	uint64_t h = hash64(key);
	h ^= (h >> 33);
	h *= 0xff51afd7ed558ccdULL;
	h ^= (h >> 33);
	h *= 0xc4ceb9fe1a85ec53ULL;
	h ^= (h >> 33);

	// Use low and high bits to vary hue, saturation, and value
	float hue = ((h & 0xFFFFFFFFULL) / float(0x100000000ULL));				// full 32 bits → 0..1
	float sat = 0.55f + 0.35f * ((h >> 32 & 0xFFFFULL) / float(0xFFFFULL)); // 0.55–0.90
	float val = 0.70f + 0.25f * ((h >> 48) / float(0xFFFFULL));				// 0.70–0.95

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

void Graph::buildGraph() {
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
	ids.assign(idset.begin(), idset.end());
	std::sort(ids.begin(), ids.end());
	N = (int)ids.size();
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
	cableIdByUV.clear();

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

				avgLen += e.length;
				++m;

				long long kdir = packUV(u, v);
				if (!cableIdByUV.count(kdir))
					cableIdByUV[kdir] = e.id;
				long long krev = packUV(v, u);
				if (!cableIdByUV.count(krev))
					cableIdByUV[krev] = e.id;
			}
		}
	}
	if (m)
		avgLen /= float(m);
	if (!m)
		avgLen = 10.f;

	// ---------- choose roots / components ----------
	std::vector<int> roots;
	for (int i = 0; i < N; ++i)
		if (indeg[i] == 0)
			roots.push_back(i);

	std::vector<int> comp(N, -1);
	int compCount = 0;
	{
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

	// ---------- BFS for depth/parent ----------
	depth.assign(N, INT_MAX);
	parent.assign(N, -1);
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
		for (int v = 0; v < N; ++v)
			if (depth[v] == INT_MAX) {
				if (!radj[v].empty()) {
					parent[v] = radj[v][0];
					depth[v] = (depth[parent[v]] == INT_MAX ? 1 : depth[parent[v]] + 1);
				} else if (!roots.empty()) {
					parent[v] = roots[0];
					depth[v] = 1;
				} else {
					parent[v] = -1;
					depth[v] = 0;
				}
			}
	}

	// ---------- groups & colors ----------
	std::vector<std::string> groupKey(N);
	for (int v = 0; v < N; ++v)
		groupKey[v] = baseKeyFromId(ids[v]);
	for (int v = 0; v < N; ++v) {
		if (!familyColor.count(groupKey[v]))
			familyColor[groupKey[v]] = colorFromKey(groupKey[v]);
	}

	// ---------- layout (once) ----------
	auto sideSign = [&](int v) -> int {
		// use indeg/outdeg captured by build
		int s = (int)adj[v].size() > indeg[v] ? +1 : ((int)adj[v].size() < indeg[v] ? -1 : +1);
		return (depth[v] == 0) ? 0 : s;
	};

	trunkY = 0.0f;
	dx = std::max(9.0f, avgLen * 0.26f);
	tierBase = std::max(8.0f, avgLen * 0.10f);
	tierStep = std::max(5.0f, avgLen * 0.10f);

	std::vector<int> order(N);
	std::iota(order.begin(), order.end(), 0);
	std::sort(order.begin(), order.end(), [&](int a, int b) { return ids[a] < ids[b]; });

	std::vector<int> ups1, downs1, mids0;
	for (int v : order) {
		if (depth[v] == 0) {
			mids0.push_back(v);
			continue;
		}
		if (depth[v] == 1)
			(sideSign(v) > 0 ? ups1 : downs1).push_back(v);
	}

	xcol.assign(N, 0.f);
	auto placeLane = [&](const std::vector<int> &lane, float x0) {
		for (int i = 0; i < (int)lane.size(); ++i)
			xcol[lane[i]] = x0 + float(i) * dx;
	};
	placeLane(ups1, 0.0f);
	placeLane(downs1, 0.5f * dx);
	for (int i = 0; i < (int)mids0.size(); ++i)
		xcol[mids0[i]] = float(i) * dx;

	std::vector<std::vector<int>> kids(N);
	for (int v = 0; v < N; ++v)
		if (parent[v] >= 0)
			kids[parent[v]].push_back(v);
	for (int p = 0; p < N; ++p)
		std::sort(kids[p].begin(), kids[p].end(), [&](int a, int b) { return ids[a] < ids[b]; });

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

	pos.assign(N, glm::vec3(0));
	for (int v = 0; v < N; ++v)
		pos[v] = glm::vec3(xcol[v], yFor(v), 0.f);

	// ---------- instances: nodes ----------
	const float nodeScale = 2.0f;
	nodeMap.clear();
	for (int i = 0; i < N; ++i) {
		const glm::vec4 color = familyColor[groupKey[i]];
		nodes->updateInstance(i, InstancedPolygonData(pos[i], glm::vec3(nodeScale), color, Colors::Black));
		nodeMap[i] = {ids[i]};
	}

	// ---------- instances: edges (including trunk + splits) ----------
	edgeMap.clear();
	int eIdx = 0;

	auto packUndirected = [](int a, int b) -> long long {
		if (a > b)
			std::swap(a, b);
		return ((long long)a << 32) ^ (unsigned long long)(b & 0xffffffff);
	};
	std::unordered_set<long long> drawnPairs;

	auto addSegRaw = [&](const glm::vec3 &P0, const glm::vec3 &P1, int cableId) {
		glm::vec3 d = P1 - P0;
		float L = glm::length(d);
		if (L < 1e-6f)
			return;
		glm::vec3 mid = 0.5f * (P0 + P1);
		glm::quat rot = rotatePlusXTo(d / L);
		edgeMap[eIdx] = {cableId, L};
		edges->updateInstance(eIdx++, InstancedPolygonData(mid, glm::vec3(L, 0.1f, 0.1f), rot, Colors::Gray, Colors::Black));
	};
	auto addSegByNodes = [&](int a, int b, int cableId) {
		if (a == b)
			return;
		long long key = packUndirected(a, b);
		if (!drawnPairs.insert(key).second)
			return;
		const glm::vec3 &P0 = pos[a];
		const glm::vec3 &P1 = pos[b];
		glm::vec3 d = P1 - P0;
		float L = glm::length(d);
		if (L < 1e-6f)
			return;
		glm::vec3 mid = 0.5f * (P0 + P1);
		glm::quat rot = rotatePlusXTo(d / L);
		edgeMap[eIdx] = {cableId, L};
		edges->updateInstance(eIdx++, InstancedPolygonData(mid, glm::vec3(L, 0.1f, 0.1f), rot, Colors::Gray, Colors::Black));
	};

	const float pad = 0.75f * dx;
	float minX = 1e9f, maxX = -1e9f;
	for (int i = 0; i < N; ++i) {
		minX = std::min(minX, pos[i].x);
		maxX = std::max(maxX, pos[i].x);
	}

	// main trunk
	addSegRaw(glm::vec3(minX - pad, trunkY, 0.f), glm::vec3(maxX + pad, trunkY, 0.f), 0);

	// helper to decide if (a,b) crosses trunk
	auto crossesTrunk = [&](int a, int b) -> bool {
		float ya = pos[a].y - trunkY, yb = pos[b].y - trunkY;
		return (ya == 0.f || yb == 0.f) ? true : (ya * yb < 0.f);
	};

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
			addSegRaw(glm::vec3(xcol[v], trunkY, 0.f), glm::vec3(xcol[v], pos[v].y, 0.f), cableId);
		}

	// depth>=2: connect (split when crossing trunk)
	for (int v = 0; v < N; ++v)
		if (depth[v] >= 2 && parent[v] >= 0) {
			int p = parent[v];
			int cableId = 0;
			auto it = cableIdByUV.find(packUV(p, v));
			if (it != cableIdByUV.end())
				cableId = it->second;

			if (crossesTrunk(p, v)) {
				addSegRaw(glm::vec3(xcol[v], trunkY, 0.f), glm::vec3(xcol[v], pos[v].y, 0.f), cableId);
				addSegRaw(glm::vec3(xcol[p], trunkY, 0.f), glm::vec3(xcol[p], pos[p].y, 0.f), cableId);
			} else {
				addSegByNodes(p, v, cableId);
			}
		}

	// ---------- static labels (for 2D mode) ----------
	nodeLabels.clear();
	edgeLabels.clear();

	nodeLabels.resize(N);
	for (int i = 0; i < N; ++i) {
		if (!nodeLabels[i]) {
			Text::FontParams nodeFP{Fonts::ArialBold, 16};
			auto t = std::make_unique<Text>(this, mvp, screenParams, nodeFP);
			t->textParams.text = nodeMap[i].name;
			glm::vec3 p = pos[i];
			t->textParams.billboardParams = Text::BillboardParams{p, {30.0f, -30.0f}, true};
			t->textParams.color = Colors::Orange;
			nodeLabels[i] = std::move(t);
		}
	}

	edgeLabels.resize((int)edgeMap.size());
	for (int idx = 0; idx < (int)edgeLabels.size(); ++idx) {
		if (!edgeLabels[idx]) {
			Text::FontParams edgeFP{Fonts::ArialBold, 16};
			auto t = std::make_unique<Text>(this, mvp, screenParams, edgeFP);
			int cableId = edgeMap[idx].cableId;
			t->textParams.text = "#" + std::to_string(cableId);

			const InstancedPolygonData inst = edges->getInstance(idx);
			glm::vec3 mid(inst.model[3].x, inst.model[3].y, inst.model[3].z);
			t->textParams.billboardParams = Text::BillboardParams{mid, {10.0f, 0.0f}, true};
			t->textParams.color = Colors::Green;
			edgeLabels[idx] = std::move(t);
		}
	}

	// ---------- legend ----------
	std::unordered_map<std::string, int> groupCounts;
	for (auto &gk : groupKey)
		groupCounts[gk]++;
	std::vector<LegendEntry> newLegend;
	newLegend.reserve(groupCounts.size());
	for (auto &kv : groupCounts) {
		const std::string &label = kv.first;
		const glm::vec4 &col = familyColor[label];
		newLegend.push_back({label, col});
	}
	std::sort(newLegend.begin(), newLegend.end(), [](auto &a, auto &b) { return a.label < b.label; });

	bool diff = (newLegend.size() != legendEntries.size());
	if (!diff) {
		for (size_t i = 0; i < newLegend.size(); ++i) {
			if (legendEntries[i].label != newLegend[i].label || legendEntries[i].color != newLegend[i].color) {
				diff = true;
				break;
			}
		}
	}
	if (diff) {
		legendEntries = std::move(newLegend);
		if (auto overlay = std::dynamic_pointer_cast<Overlay>(scenes.getScene("Overlay"))) {
			overlay->updateLegend();
		}
	}

	graphBuilt = true;
}

void Graph::swapChainUpdate() {
	// Now: only per-frame / per-resize work.
	if (!circuit)
		return;
	if (!graphBuilt)
		buildGraph(); // safety (e.g., if circuit changed)

	// Camera & projection depend on viewport/aspect each frame:
	const float w = screenParams.viewport.width, h = screenParams.viewport.height, aspect = w / h;
	const float fovY = radians(45.0f);

	// Fit camera distance to scene extents computed in buildGraph()
	float sceneRadius = 1.f;
	for (auto &p : pos)
		sceneRadius = std::max(sceneRadius, glm::length(p));
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

	// Update MVPs affected by camera changes (instances already built)
	nodeName->updateMVP(std::nullopt, std::nullopt, mvp.proj);
	wireId->updateMVP(std::nullopt, std::nullopt, mvp.proj);
	nodes->updateMVP(std::nullopt, std::nullopt, mvp.proj);
	edges->updateMVP(std::nullopt, std::nullopt, mvp.proj);

	for (auto &t : nodeLabels)
		if (t)
			t->updateMVP(std::nullopt, std::nullopt, mvp.proj);
	for (auto &t : edgeLabels)
		if (t)
			t->updateMVP(std::nullopt, std::nullopt, mvp.proj);
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

	if (!is3D) {
		for (const auto &l : nodeLabels) {
			l->updateMVP(std::nullopt, mvp.view);
		}
		for (const auto &l : edgeLabels) {
			l->updateMVP(std::nullopt, mvp.view);
		}
	}
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

	if (!is3D) {
		for (const auto &l : nodeLabels) {
			l->render();
		}
		for (const auto &l : edgeLabels) {
			l->render();
		}
	}
}
