#include "main.hpp"

#include <algorithm>
#include <cmath>
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

		textPos = vec3(prev.model[3].x, prev.model[3].y, prev.model[3].z + 2);
		label = nodeMap[id].name;
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
		label = "";
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

	// Use the unifilar graph, but place by semantic "tiers", not depth
	const auto &G = circuit->unifilar();
	if (G.adj.empty() && G.level.empty())
		return;

	// --- collect unique node ids present in unifilar (parents & children) ---
	std::unordered_set<std::string> idset;
	for (const auto &kv : G.adj) {
		idset.insert(kv.first);
		for (const auto &e : kv.second)
			idset.insert(e.child);
	}
	std::vector<std::string> nodeIds(idset.begin(), idset.end());
	std::sort(nodeIds.begin(), nodeIds.end());

	std::unordered_map<std::string, int> idToIdx;
	idToIdx.reserve(nodeIds.size());
	for (int i = 0; i < (int)nodeIds.size(); ++i)
		idToIdx[nodeIds[i]] = i;

	const int N = (int)nodeIds.size();
	const float EPS = 1e-6f;

	// ---- stats (keep your sizing heuristics) ----
	float avgLen = 0.0f;
	int m = 0;
	for (const auto &e : circuit->edges()) {
		avgLen += e.length * 2;
		++m;
	}
	if (m > 0)
		avgLen /= float(m);

	// ---- define tier from ID (adjust to your taxonomy) ----

	auto tierOf = [](const std::string &s) -> int {
		auto starts = [&](const char *p) { return s.rfind(p, 0) == 0; };

		// TOP: feeders/panels
		if (starts("CSE-PCHG") || starts("CSE-PCGH"))
			return 0;

		// Sensors (next row down)
		if (starts("TTC-"))
			return 1;

		// splices
		if (starts("SPN-") || starts("SPS-"))
			return 2;

		// device rows (adjust buckets as you like)
		if (starts("BJ-29"))
			return 3;
		if (starts("BJ-28"))
			return 4;
		if (starts("BJ-"))
			return 4;

		// EPI/CEV labels – keep near sensors/splices or give them their own row
		if (starts("CSE-EPI-") || starts("CEV-EPI-"))
			return 2;

		// unknowns
		return 3;
	};

	// ---- group nodes by tier & assign positions ----
	const float dy = std::max(1.2f, avgLen * 0.06f); // vertical separation between rows
	const float dx = std::max(2.5f, avgLen * 0.10f); // horizontal spacing between siblings

	std::unordered_map<int, std::vector<std::string>> byTier;
	int minTier = INT_MAX, maxTier = INT_MIN;
	std::unordered_map<std::string, int> tier;
	tier.reserve(nodeIds.size());
	for (const auto &id : nodeIds) {
		int t = tierOf(id);
		tier[id] = t;
		byTier[t].push_back(id);
		minTier = std::min(minTier, t);
		maxTier = std::max(maxTier, t);
	}

	// sort each tier for stable layout and center lanes around y=0
	std::vector<glm::vec3> pos(N, glm::vec3(0));
	for (auto &kv : byTier) {
		int t = kv.first;
		auto &vec = kv.second;
		std::sort(vec.begin(), vec.end());

		// place this tier at Y = (tier index), mapped to world coordinates
		float y = float(maxTier - t) * dy; // top tiers get larger y (like your image)

		// simple left->right packing; keep similar families near each other by sort
		// center around x=0 for symmetry
		float x0 = -0.5f * float(vec.size() - 1) * dx;
		for (size_t i = 0; i < vec.size(); ++i) {
			auto it = idToIdx.find(vec[i]);
			if (it == idToIdx.end())
				continue;
			pos[it->second] = glm::vec3(x0 + float(i) * dx, y, 0.0f);
		}
	}

	// ---- fit camera ----
	float sceneRadius = 0.0f;
	for (auto &p : pos)
		sceneRadius = std::max(sceneRadius, glm::length(p));
	sceneRadius = std::max(sceneRadius, 1.0f);

	const float aspect = screenParams.viewport.width / screenParams.viewport.height;
	const float fovY = radians(45.0f);
	const float desiredDist = std::max(12.0f, sceneRadius * 2.2f);

	glm::vec3 dir = glm::normalize((camPos == glm::vec3(0)) ? glm::vec3(1, 1, 1) : camPos);
	camPos = dir * desiredDist;

	const float nearP = 0.05f;
	const float farP = std::max(desiredDist * 4.0f, sceneRadius * 6.0f);
	persp.view = lookAt(camPos, camTarget, camUp);
	persp.proj = perspective(fovY, aspect, nearP, farP);

	nodeName->updateUniformBuffer(std::nullopt, persp.view, persp.proj);

	// ---- stamp nodes (8 legend colors) ----
	float nodeScale = 2.0f;
	for (int i = 0; i < N; ++i) {
		const std::string &raw = nodeIds[i];
		auto k = classify8WithEdges(circuit.get(), raw);
		auto color = colorFor(k);
		nodes->updateInstance(i, InstancedPolygonData(pos[i], glm::vec3(nodeScale), color, Colors::Black));
        nodeMap[i] = {raw};
	}
	nodes->updateUniformBuffer(std::nullopt, std::nullopt, persp.proj);

	// Build reverse adjacency (child -> parents) from G.adj
	std::unordered_map<std::string, std::vector<std::string>> parentsOf;
	for (auto &kv : G.adj) {
		const auto &p = kv.first;
		for (auto &e : kv.second)
			parentsOf[e.child].push_back(p);
	}

	// After you compute a first pass of pos[] (or at least parent tier X’s),
	// do one reorder pass for each tier t>minTier:
	for (auto &kv : byTier) {
		int t = kv.first;
		auto &vec = kv.second;

		// Compute sort key = average parent X (fallback to own name)
		struct Item {
			std::string id;
			float key;
		};
		std::vector<Item> items;
		items.reserve(vec.size());
		for (auto &id : vec) {
			float key = 0.f;
			int cnt = 0;
			auto pit = parentsOf.find(id);
			if (pit != parentsOf.end()) {
				for (auto &p : pit->second) {
					auto itP = idToIdx.find(p);
					if (itP != idToIdx.end()) {
						key += pos[itP->second].x;
						++cnt;
					}
				}
			}
			if (cnt > 0)
				key /= float(cnt);
			else {
				// leaf or no known parents: keep deterministic fallback
				key = 0.f;
			}
			items.push_back({id, key});
		}
		std::sort(items.begin(), items.end(), [](const Item &a, const Item &b) { return a.key < b.key || (a.key == b.key && a.id < b.id); });
		// rewrite vec in this order
		for (size_t i = 0; i < vec.size(); ++i)
			vec[i] = items[i].id;
	}

	// Then re-assign x positions for each tier using your usual spacing

	// ---- stamp edges (parent -> child) ----
	// same offsetting as before for parallel edges
	struct PairKey {
		int a, b;
		bool operator==(const PairKey &o) const { return a == o.a && b == o.b; }
	};
	struct PairKeyHash {
		size_t operator()(const PairKey &k) const { return (size_t)k.a * 1000003u ^ (size_t)k.b; }
	};
	std::unordered_map<PairKey, int, PairKeyHash> pairCounts;

	const float edgeThickness = glm::clamp(avgLen * 0.01f, 0.01f, 0.10f);
	const float edgeOffset = glm::clamp(avgLen * 0.02f, 0.03f, 0.15f);

	int eIdx = 0;
	// ---- edge stamping with orthogonal routing (3 segments) ----
	auto addSegment = [&](const glm::vec3 &P0, const glm::vec3 &P1, float thickness, const glm::vec4 &color) {
		glm::vec3 d = P1 - P0;
		float L = glm::length(d);
		if (L < 1e-6f)
			return;
		glm::vec3 dirN = d / L;
		glm::vec3 mid = 0.5f * (P0 + P1);
		glm::quat rot = rotatePlusXTo(dirN);
		edges->updateInstance(eIdx++, InstancedPolygonData(mid, glm::vec3(L, thickness, thickness), rot, color, Colors::Black));
	};

	// choose a bus height between tiers (works even if tiers are equal)
	auto busY = [&](float yA, float yB) {
		// midpoint bus; if you prefer a fixed track per tier, replace with a table of per-tier y's
		return 0.5f * (yA + yB);
	};

	const glm::vec4 wireColor = Colors::Gray;

	eIdx = 0; // reset edge instance cursor
	for (const auto &adjKV : G.adj) {
		auto itU = idToIdx.find(adjKV.first);
		if (itU == idToIdx.end())
			continue;
		const int iu = itU->second;
		const glm::vec3 A = pos[iu];

		for (const auto &ue : adjKV.second) {
			auto itV = idToIdx.find(ue.child);
			if (itV == idToIdx.end())
				continue;
			const int iv = itV->second;
			const glm::vec3 B = pos[iv];

			// 3-piece orthogonal: drop1, span, drop2
			const float yBus = busY(A.y, B.y);

			// drop1: A -> (A.x, yBus)
			addSegment(A, glm::vec3(A.x, yBus, A.z), edgeThickness, wireColor);

			// span : (A.x, yBus) -> (B.x, yBus)
			addSegment(glm::vec3(A.x, yBus, A.z), glm::vec3(B.x, yBus, B.z), edgeThickness, wireColor);

			// drop2: (B.x, yBus) -> B
			addSegment(glm::vec3(B.x, yBus, B.z), B, edgeThickness, wireColor);
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
}

void Main::renderPass() {
	nodes->render();
	edges->render();
	float textLen = nodeName->getPixelWidth(label);
	nodeName->renderBillboard(label, Text::BillboardParams{textPos, {-textLen / 2, 0}});
}
