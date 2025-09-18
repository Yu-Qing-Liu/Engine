#include "main.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include "camera.hpp"
#include "colors.hpp"
#include "engine.hpp"
#include "events.hpp"
#include "fonts.hpp"

using std::unordered_set;

// ------------ helpers ------------
static inline glm::quat quatFromTo(const glm::vec3 &fromRaw, const glm::vec3 &toRaw) {
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

static inline glm::quat rotatePlusXTo(const glm::vec3 &dirN) { return quatFromTo(glm::vec3(1, 0, 0), dirN); }

// ---------- normalization helpers ----------
static std::string toUpperCopy(std::string s) {
	for (char &c : s)
		c = (char)std::toupper((unsigned char)c);
	return s;
}
static std::string normalizedId(std::string s) {
	auto pos = s.find_last_of('/');
	if (pos != std::string::npos)
		s = s.substr(pos + 1);
	auto slash = s.find('/');
	if (slash != std::string::npos)
		s = s.substr(0, slash);
	auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
	s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
	return toUpperCopy(s);
}
static bool startsWith(const std::string &upId, const char *upPrefix) {
	const size_t n = std::strlen(upPrefix);
	return upId.size() >= n && std::memcmp(upId.data(), upPrefix, n) == 0;
}

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
	Unknown
};

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

static Kind classify8WithEdges(const Circuit *circuit, const std::string &rawId) {
	const std::string id = normalizedId(rawId);

	if (startsWith(id, "CSE-PCGH") || startsWith(id, "CSE-PCHG"))
		return Kind::PCGH;

	// Quick ID-based families
	if (startsWith(id, "TTC-"))
		return Kind::SensorTTC;
	if (startsWith(id, "SPN-"))
		return Kind::BJ_Primary_Installed; // splices (diamonds)

	// Secondary family (291)
	if (startsWith(id, "BJ-291B"))
		return Kind::BJ_Secondary_InstalledConnected;
	if (startsWith(id, "BJ-291"))
		return Kind::BJ_Secondary_NotHeated;

	// Not installed family (297)
	if (startsWith(id, "BJ-297"))
		return Kind::BJ_Primary_NotInstalled;

	// Adduction water (192) — broadened from 192A to any 192*
	if (startsWith(id, "BJ-192"))
		return Kind::BJ_AdductionWater;

	// Known connected primaries
	if (id == "BJ-295" || id == "BJ-287A" || id == "BJ-287B")
		return Kind::BJ_Primary_InstalledConnected;

	// Drainage: exact BJ-186A or hint from text (see below)
	if (startsWith(id, "BJ-"))
		return Kind::Drainage;

	// Text-based heuristics using incident edges
	auto touches = [&](auto pred) {
		for (const auto &e : circuit->edges()) {
			// Compare against raw node id in edges because edges store original IDs
			if (e.u.id == rawId || e.v.id == rawId) {
				std::string cond = toUpperCopy(e.conditionAndCaliber);
				std::string cable = toUpperCopy(e.cableName);
				if (pred(cond) || pred(cable))
					return true;
			}
		}
		return false;
	};

	// NOT INSTALLED / SPARE / NON-INSTALLÉ (FR)
	if (touches([](const std::string &s) {
			return s.find("NOT INSTALLED") != std::string::npos || s.find("SPARE") != std::string::npos || s.find("NON INSTALLE") != std::string::npos || // handles NON INSTALLÉ (accents stripped)
				   s.find("NON INSTALLÉ") != std::string::npos;
		}))
		return Kind::BJ_Primary_NotInstalled;

	// DRAINAGE / DRAIN
	if (touches([](const std::string &s) { return s.find("DRAINAGE") != std::string::npos || s.find("DRAIN") != std::string::npos; }))
		return Kind::Drainage;

	// ADDUCTION / WATER / EAU
	if (touches([](const std::string &s) { return s.find("ADDUCTION") != std::string::npos || s.find("WATER") != std::string::npos || s.find("EAU") != std::string::npos; }))
		return Kind::BJ_AdductionWater;

	std::cout << id << std::endl;
	return Kind::Unknown;
}

// ------------ Main ------------
Main::Main(Scenes &scenes) : Scene(scenes) {
	// Make sure screenParams are valid before constructing drawables
	updateScreenParams();

	// Set an initial camera (will be resized in swapChainUpdate)
	persp = Camera::blenderPerspectiveMVP(screenParams.viewport.width, screenParams.viewport.height, lookAt(vec3(12.0f, 12.0f, 12.0f), vec3(0.0f), vec3(0.0f, 0.0f, 1.0f)));

	// Circuit: default ctor loads the correct path (as you said)
	circuit = std::make_unique<Circuit>();

	// Setup: construct instanced meshes
	Text::TextParams tp{Fonts::ArialBold, 32};
	nodeName = make_unique<Text>(this, persp, screenParams, tp);
	wireName = make_unique<Text>(this, persp, screenParams, tp);

	nodes = Shapes::dodecahedra(this, persp, screenParams, 4000);
	nodes->onMouseEnter = [&]() {
		if (!nodes->hitMapped) {
			return;
		}
		int id = nodes->hitMapped->primId;
		InstancedPolygonData prev = nodes->getInstance(id);
		prev.outlineColor = Colors::inverse(prev.color);
		prev.outlineWidth = 4.0f;
		nodes->updateInstance(id, prev);

		nodePos = vec3(prev.model[3].x, prev.model[3].y, prev.model[3].z + 2);
		nodeLabel = nodeMap[id].name;
	};
	nodes->onMouseExit = [&]() {
		if (!nodes->hitMapped) {
			return;
		}
		int id = nodes->hitMapped->primId;
		InstancedPolygonData prev = nodes->getInstance(id);
		prev.outlineColor = Colors::Black;
		prev.outlineWidth = 1.0f;
		nodes->updateInstance(id, prev);

		nodeLabel = "";
	};
	nodes->setRayTraceEnabled(true);

	edges = Shapes::cubes(this, persp, screenParams, 4000);
	edges->onMouseEnter = [&]() {
		if (!edges->hitMapped) {
			return;
		}
		int id = edges->hitMapped->primId;
		InstancedPolygonData prev = edges->getInstance(id);
		prev.outlineColor = Colors::Yellow;
		prev.outlineWidth = 4.0f;
		edges->updateInstance(id, prev);

		wirePos = vec3(prev.model[3].x, prev.model[3].y, prev.model[3].z + 1.0);
		wireLabel = "#" + std::to_string(edgeMap[id].cableId);
	};
	edges->onMouseExit = [&]() {
		if (!edges->hitMapped) {
			return;
		}
		int id = edges->hitMapped->primId;
		InstancedPolygonData prev = edges->getInstance(id);
		prev.outlineColor = Colors::Black;
		prev.outlineWidth = 1.0f;
		edges->updateInstance(id, prev);

		wireLabel = "";
	};
	edges->setRayTraceEnabled(true);

	auto kbState = [this](int key, int, int action, int) {
		if (key >= 0 && key <= GLFW_KEY_LAST) {
			if (action == GLFW_PRESS)
				keyDown[key] = true;
			if (action == GLFW_RELEASE)
				keyDown[key] = false;
		}
	};
	Events::keyboardCallbacks.push_back(kbState);

	GLFWwindow *win = Engine::window;
	if (win) {
		glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // capture/hide
		glfwSetInputMode(win, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);  // smoother deltas (if supported)
		// initialize the cursor center reference
		int ww, hh;
		glfwGetWindowSize(win, &ww, &hh);
		lastPointerX = ww * 0.5;
		lastPointerY = hh * 0.5;
		glfwSetCursorPos(win, lastPointerX, lastPointerY);
	}

	// Initialize yaw/pitch from current view direction so we face the scene
	{
		glm::vec3 f0 = glm::normalize(lookAtCoords - camPos); // if lookAtCoords==origin, this points to origin
		f0 = glm::normalize(glm::vec3(-1, -1, -1));
		yaw = atan2f(f0.y, f0.x);
		pitch = asinf(glm::clamp(f0.z, -1.0f, 1.0f));
	}

	// Capture the cursor
	if (Engine::window) {
		glfwSetInputMode(Engine::window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		if (glfwRawMouseMotionSupported())
			glfwSetInputMode(Engine::window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
		lastPointerX = -1.0;
		lastPointerY = -1.0;
	}
}

void Main::mouseLookFPS() {
	GLFWwindow *win = Engine::window;
	if (!win)
		return;
	if (!glfwGetWindowAttrib(win, GLFW_FOCUSED))
		return;

	// center of the window
	int ww = 0, hh = 0;
	glfwGetWindowSize(win, &ww, &hh);
	const double cx = ww * 0.5;
	const double cy = hh * 0.5;

	// read current cursor, compute delta from center
	double mx = 0.0, my = 0.0;
	glfwGetCursorPos(win, &mx, &my);

	// first frame after focus or startup: just recenter without a jump
	if (lastPointerX < 0.0 || lastPointerY < 0.0) {
		glfwSetCursorPos(win, cx, cy);
		lastPointerX = cx;
		lastPointerY = cy;
		return;
	}

	const double dx = mx - cx;
	const double dy = my - cy;

	// immediately re-center so next frame's delta is relative
	glfwSetCursorPos(win, cx, cy);
	lastPointerX = cx;
	lastPointerY = cy;

	// update yaw/pitch from mouse delta
	yaw -= float(dx) * mouseSens;	// yaw wraps naturally → 360°+
	pitch -= float(dy) * mouseSens; // invert if you prefer
	pitch = glm::clamp(pitch, glm::radians(-89.0f), glm::radians(89.0f));

	// build forward from yaw/pitch (Z-up)
	const float cyaw = cosf(yaw), syaw = sinf(yaw);
	const float cp = cosf(pitch), sp = sinf(pitch);
	glm::vec3 f = glm::normalize(glm::vec3(cyaw * cp, syaw * cp, sp));

	// keep your current look distance (fallback to 1)
	float lookDist = glm::length(lookAtCoords - camPos);
	if (!(lookDist > 1e-4f))
		lookDist = 1.0f;

	// drive the existing lookAt target
	lookAtCoords = camPos + f * lookDist;
}

void Main::handleCameraInput(float dt) {
	GLFWwindow *win = Engine::window;
	if (!win)
		return;
	if (!glfwGetWindowAttrib(win, GLFW_FOCUSED))
		return;

	// read keys directly from GLFW
	auto down = [&](int k) { return glfwGetKey(win, k) == GLFW_PRESS; };

	float moveX = 0.f, moveY = 0.f, moveZ = 0.f; // strafe right, forward, up
	if (down(GLFW_KEY_D) || down(GLFW_KEY_RIGHT))
		moveX += 1.f;
	if (down(GLFW_KEY_A) || down(GLFW_KEY_LEFT))
		moveX -= 1.f;
	if (down(GLFW_KEY_W) || down(GLFW_KEY_UP))
		moveY += 1.f;
	if (down(GLFW_KEY_S) || down(GLFW_KEY_DOWN))
		moveY -= 1.f;
	if (down(GLFW_KEY_E))
		moveZ += 1.f;
	if (down(GLFW_KEY_Q))
		moveZ -= 1.f;

	// speed modifiers (same as before)
	float kbSens = 0.24f;
	if (down(GLFW_KEY_LEFT_CONTROL) || down(GLFW_KEY_RIGHT_CONTROL))
		kbSens *= 5.0f;
	if (down(GLFW_KEY_LEFT_ALT) || down(GLFW_KEY_RIGHT_ALT))
		kbSens *= 0.2f;

	// ------- build camera-aligned basis from current aim (cursor) -------
	const glm::vec3 worldUp(0, 0, 1);

	// forward = where the cursor points (what you use in lookAt())
	glm::vec3 f = lookAtCoords - camPos;
	if (glm::dot(f, f) < 1e-12f)
		f = glm::vec3(0, 1, 0);
	f = glm::normalize(f);

	// right = perpendicular to forward in the horizontal sense
	glm::vec3 right = glm::cross(f, worldUp);
	if (glm::dot(right, right) < 1e-12f)
		right = glm::vec3(1, 0, 0);
	right = glm::normalize(right);

	// up for flying (keeps Q/E vertical lift)
	const glm::vec3 up = worldUp;

	// movement aligned to cursor (W/S uses full forward, including pitch)
	glm::vec3 delta = moveX * right + moveY * f + moveZ * up;

	if (glm::dot(delta, delta) > 0.0f) {
		delta = glm::normalize(delta) * (camSpeed * kbSens * (dt > 0 ? dt : 1.f));
		camPos += delta;
		lookAtCoords += delta; // pan the *actual* aim point you render with
		camTarget += delta;	   // keep in sync if you still use camTarget elsewhere
	}
}

void Main::updateScreenParams() {
	screenParams.viewport.x = 0.0f;
	screenParams.viewport.y = 0.0f;
	screenParams.viewport.width = (float)Engine::swapChainExtent.width;
	screenParams.viewport.height = (float)Engine::swapChainExtent.height;
	screenParams.viewport.minDepth = 0.0f;
	screenParams.viewport.maxDepth = 1.0f;
	screenParams.scissor.offset = {(int32_t)screenParams.viewport.x, (int32_t)screenParams.viewport.y};
	screenParams.scissor.extent = {(uint32_t)screenParams.viewport.width, (uint32_t)screenParams.viewport.height};
}

void Main::swapChainUpdate() {
	if (!circuit)
		return;

	const auto &G = circuit->unifilar();
	if (G.adj.empty() && G.level.empty())
		return;

	// ---- collect ids (stable) ----
	std::unordered_set<std::string> idset;
	for (const auto &kv : G.adj) {
		idset.insert(kv.first);
		for (const auto &e : kv.second)
			idset.insert(e.child);
	}
	std::vector<std::string> ids(idset.begin(), idset.end());
	std::sort(ids.begin(), ids.end());
	const int N = (int)ids.size();

	std::unordered_map<std::string, int> idToIdx;
	idToIdx.reserve(N);
	for (int i = 0; i < N; ++i)
		idToIdx[ids[i]] = i;

	// Build a cable-name index keyed by directed (uIdx,vIdx)
	std::unordered_map<long long, int> cableIdByUV;
	auto pack = [](int u, int v) -> long long { return ((long long)u << 32) ^ (unsigned long long)(v & 0xffffffff); };

	for (const auto &ce : circuit->edges()) {
		std::string su = normalizedId(ce.u.id);
		std::string sv = normalizedId(ce.v.id);
		auto itu = idToIdx.find(su);
		auto itv = idToIdx.find(sv);
		if (itu == idToIdx.end() || itv == idToIdx.end())
			continue;
		long long k = pack(itu->second, itv->second);
		// keep first seen; or overwrite if you prefer latest
		if (!cableIdByUV.count(k))
			cableIdByUV[k] = ce.id;

		// optional: also allow reverse lookup (in case directions differ)
		long long krev = pack(itv->second, itu->second);
		if (!cableIdByUV.count(krev))
			cableIdByUV[krev] = ce.id;
	}

	// ---- sizing stats ----
	float avgLen = 0.f;
	int m = 0;
	for (const auto &e : circuit->edges()) {
		avgLen += e.length;
		++m;
	}
	if (m)
		avgLen /= float(m);

	// ---- taxonomy: sign only (above/below/center) ----
	auto sideSign = [](const std::string &s) -> int {
		auto starts = [&](const char *p) { return s.rfind(p, 0) == 0; };
		if (starts("TTC-"))
			return -1; // below
		if (starts("CSE-"))
			return 0; // on bus
		return +1;	  // above (BJ-, SPN-, ...)
	};

	// ---- horizontal order key (same as you had) ----
	auto familyKey = [](const std::string &s) -> int {
		auto starts = [&](const char *p) { return s.rfind(p, 0) == 0; };
		if (starts("CSE-"))
			return 0;
		if (starts("BJ-29"))
			return 10;
		if (starts("BJ-28"))
			return 11;
		if (starts("BJ-"))
			return 12;
		if (starts("SPN-") || starts("SPS-"))
			return 20;
		if (starts("TTC-"))
			return 30;
		return 40;
	};
	auto numTail = [](const std::string &s) -> int {
		int n = 0, i = (int)s.size() - 1;
		while (i >= 0 && std::isdigit((unsigned char)s[i]))
			--i;
		if (i + 1 < (int)s.size()) {
			try {
				n = std::stoi(s.substr(i + 1));
			} catch (...) {
			}
		}
		return n;
	};

	std::vector<int> order(N);
	std::iota(order.begin(), order.end(), 0);
	std::sort(order.begin(), order.end(), [&](int a, int b) {
		int fa = familyKey(ids[a]), fb = familyKey(ids[b]);
		if (fa != fb)
			return fa < fb;
		int na = numTail(ids[a]), nb = numTail(ids[b]);
		if (na != nb)
			return na < nb;
		return ids[a] < ids[b];
	});

	// ---------------- BFS forest: depth[] and parent[] ----------------
	// Build adj (u->v) with dedupe and indegrees
	std::vector<std::vector<int>> adj(N), radj(N);
	std::vector<int> indeg(N, 0);

	{
		std::unordered_set<long long> seen;
		auto pack = [](int u, int v) -> long long { return ((long long)u << 32) ^ (unsigned long long)(v & 0xffffffff); };

		for (const auto &kv : G.adj) {
			int u = idToIdx[kv.first];
			for (const auto &ee : kv.second) {
				auto it = idToIdx.find(ee.child);
				if (it == idToIdx.end())
					continue;
				int v = it->second;
				long long k = pack(u, v);
				if (seen.insert(k).second) {
					adj[u].push_back(v);
					radj[v].push_back(u);
					indeg[v]++;
				}
			}
		}
	}

	// roots: prefer CSE-; else indeg==0; final fallback=0
	std::vector<int> roots;
	for (int i = 0; i < N; ++i)
		if (ids[i].rfind("CSE-", 0) == 0)
			roots.push_back(i);
	if (roots.empty()) {
		for (int i = 0; i < N; ++i)
			if (indeg[i] == 0)
				roots.push_back(i);
	}
	if (roots.empty())
		roots.push_back(0);

	std::vector<int> depth(N, INT_MAX), parent(N, -1);
	std::queue<int> q;
	for (int r : roots) {
		depth[r] = 0;
		parent[r] = -1;
		q.push(r);
	}

	while (!q.empty()) {
		int u = q.front();
		q.pop();
		for (int v : adj[u]) {
			if (depth[v] > depth[u] + 1) {
				depth[v] = depth[u] + 1;
				parent[v] = u; // choose u as the tree parent
				q.push(v);
			}
		}
	}
	// Unreached -> attach to any predecessor if exists; else near roots
	for (int v = 0; v < N; ++v) {
		if (depth[v] == INT_MAX) {
			if (!radj[v].empty()) {
				parent[v] = radj[v][0];
				depth[v] = depth[parent[v]] + 1;
			} else {
				depth[v] = 1;
				parent[v] = roots[0];
			}
		}
	}

	// --------------- X placement (columns) ----------------
	// Columns are created by depth==1 anchors (the nodes that touch the bus).
	// Ups on integer slots, downs on half slots. Deeper nodes inherit their
	// parent’s column with tiny sibling offsets to keep wires distinct.
	const float trunkY = 0.0f;
	const float dx = std::max(9.0f, avgLen * 0.26f); // column spacing
	const float epsCol = dx * 0.12f;				 // sibling x offset

	std::vector<int> ups1, downs1, mids0;
	for (int v : order) {
		if (depth[v] == 0) {
			mids0.push_back(v);
			continue;
		} // on bus (CSE)
		if (depth[v] == 1) {
			(sideSign(ids[v]) > 0 ? ups1 : downs1).push_back(v);
		}
	}

	// assign column x for anchors
	std::vector<float> xcol(N, 0.f);
	auto placeLane = [&](const std::vector<int> &lane, float x0) {
		for (int i = 0; i < (int)lane.size(); ++i)
			xcol[lane[i]] = x0 + float(i) * dx;
	};
	placeLane(ups1, 0.0f);		  // integer slots
	placeLane(downs1, 0.5f * dx); // half slots
	for (int i = 0; i < (int)mids0.size(); ++i)
		xcol[mids0[i]] = float(i) * dx;

	// children lists for sibling offsets
	std::vector<std::vector<int>> kids(N);
	for (int v = 0; v < N; ++v)
		if (parent[v] >= 0)
			kids[parent[v]].push_back(v);

	// DFS/BFS by depth to assign deeper columns = parent column +/- small offsets
	// (keeps stacks vertical but wires distinct)
	for (int d = 2; d < N + 2; ++d) {
		bool any = false;
		for (int p = 0; p < N; ++p) {
			if (depth[p] != d - 1)
				continue;
			auto &ch = kids[p];
			// deterministic order
			std::sort(ch.begin(), ch.end(), [&](int a, int b) {
				int sa = sideSign(ids[a]), sb = sideSign(ids[b]);
				if (sa != sb)
					return sa > sb; // above before below
				int fa = familyKey(ids[a]), fb = familyKey(ids[b]);
				if (fa != fb)
					return fa < fb;
				int na = numTail(ids[a]), nb = numTail(ids[b]);
				if (na != nb)
					return na < nb;
				return ids[a] < ids[b];
			});
			for (int k = 0; k < (int)ch.size(); ++k) {
				int v = ch[k];
				if (depth[v] != d)
					continue;
				xcol[v] = xcol[p];
				any = true;
			}
		}
		if (!any)
			break;
	}

	// --------------- Y placement (tiers by depth) ----------------
	const float tierBase = std::max(8.0f, avgLen * 0.1f); // depth==1 distance
	const float tierStep = std::max(5.0f, avgLen * 0.1f); // per extra depth

	auto yFor = [&](int v) -> float {
		if (depth[v] == 0)
			return trunkY;
		float mag = tierBase + float(depth[v] - 1) * tierStep;
		int s = sideSign(ids[v]);
		if (s == 0)
			s = +1; // non-TTC default above if depth>0 but on trunk taxonomy
		return trunkY + (s > 0 ? +mag : -mag);
	};

	std::vector<glm::vec3> pos(N, glm::vec3(0));
	for (int v = 0; v < N; ++v)
		pos[v] = glm::vec3(xcol[v], yFor(v), 0.f);

	// extents
	float minX = 1e9f, maxX = -1e9f;
	for (int i = 0; i < N; ++i) {
		minX = std::min(minX, pos[i].x);
		maxX = std::max(maxX, pos[i].x);
	}

	// ---- camera ----
	float sceneRadius = 1.f;
	for (auto &p : pos)
		sceneRadius = std::max(sceneRadius, glm::length(p));
	const float aspect = screenParams.viewport.width / screenParams.viewport.height;
	const float fovY = radians(45.0f);
	const float desiredDist = std::max(18.0f, sceneRadius * 0.1f);
	glm::vec3 dir = glm::normalize((camPos == glm::vec3(0)) ? glm::vec3(1, 1, 1) : camPos);
	camPos = dir * desiredDist;
	const float nearP = 0.05f, farP = std::max(desiredDist * 6.f, sceneRadius * 8.f);
	persp.view = lookAt(camPos, camTarget, camUp);
	persp.proj = perspective(fovY, aspect, nearP, farP);
	nodeName->updateUniformBuffer(std::nullopt, persp.view, persp.proj);

	// ---- draw nodes ----
	const float nodeScale = 2.0f;
	for (int i = 0; i < N; ++i) {
		auto kind = classify8WithEdges(circuit.get(), ids[i]);
		nodes->updateInstance(i, InstancedPolygonData(pos[i], glm::vec3(nodeScale), colorFor(kind), Colors::Black));
		nodeMap[i] = {ids[i]};
	}
	nodes->updateUniformBuffer(std::nullopt, std::nullopt, persp.proj);

	// ---- wires ----
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
		edgeMap[eIdx] = {cableId, L}; // <-- use real label
		edges->updateInstance(eIdx++, InstancedPolygonData(mid, glm::vec3(L, edgeThickness, edgeThickness), rot, wireColor, Colors::Black));
	};

	// Draw trunk
	const float pad = 0.75f * dx;
	addSeg(glm::vec3(minX - pad, trunkY, 0.f), glm::vec3(maxX + pad, trunkY, 0.f), 0);

	// depth==1: single vertical to bus
	for (int v = 0; v < N; ++v) {
		if (depth[v] == 1) {
			int p = parent[v]; // BFS parent that sits on/near the bus
			int cableId;
			if (p >= 0) {
				auto it = cableIdByUV.find(pack(p, v));
				if (it != cableIdByUV.end())
					cableId = it->second;
			}
			addSeg(pos[v], glm::vec3(xcol[v], trunkY, 0.f), cableId);
		}
	}

	// depth>=2: connect to parent
	for (int v = 0; v < N; ++v) {
		if (depth[v] >= 2 && parent[v] >= 0) {
			int p = parent[v];
			int cableId;
			auto it = cableIdByUV.find(pack(p, v));
			if (it != cableIdByUV.end())
				cableId = it->second;
			addSeg(pos[v], pos[p], cableId);
		}
	}

	edges->updateUniformBuffer(std::nullopt, std::nullopt, persp.proj);
}

void Main::updateComputeUniformBuffers() {}

void Main::computePass() {}

void Main::updateUniformBuffers() {
	mouseLookFPS();
	handleCameraInput(1.0f);
	persp.view = lookAt(camPos, lookAtCoords, camUp);
	nodes->updateUniformBuffer(std::nullopt, persp.view);
	edges->updateUniformBuffer(std::nullopt, persp.view);
	nodeName->updateUniformBuffer(std::nullopt, persp.view);
	wireName->updateUniformBuffer(std::nullopt, persp.view);
}

void Main::renderPass() {
	nodes->render();
	edges->render();
	float nodeTextLength = nodeName->getPixelWidth(nodeLabel);
	float wireTextLength = wireName->getPixelWidth(wireLabel);
	nodeName->renderBillboard(nodeLabel, Text::BillboardParams{nodePos, {-nodeTextLength / 2, 0}}, Colors::Orange);
	wireName->renderBillboard(wireLabel, Text::BillboardParams{wirePos, {-wireTextLength / 2, 0}}, Colors::Green);
}
