#include "scenes.hpp"
#include <boost/graph/graph_traits.hpp>
#include <iostream>
#include <queue>
#include <stdexcept>

Model *Scenes::rayPicked = nullptr;

Scenes::Scenes(std::shared_ptr<Engine> engine) : engine(engine) {
	ensureRootExists();
	initializeRenderingOrder();
}

static void buildModelLayers(const Scene *sc, std::vector<std::vector<ModelNode>> &layers) {
	layers.clear();

	ModelNode root = sc->root();
	if (root == ModelNode())
		return;

	const ModelGraph &g = sc->getModelGraph(); // add a const accessor on Scene (shown below)
	const auto nverts = boost::num_vertices(g);
	if (nverts == 0)
		return;

	// depth per vertex (UINT32_MAX = unvisited)
	std::vector<uint32_t> depth(nverts, UINT32_MAX);

	std::queue<ModelNode> q;

	// seed with children of the dummy root
	for (auto [eb, ee] = boost::out_edges(root, g); eb != ee; ++eb) {
		ModelNode c = boost::target(*eb, g);
		depth[c] = 0;
		q.push(c);
	}

	while (!q.empty()) {
		ModelNode v = q.front();
		q.pop();
		uint32_t d = depth[v];
		if (layers.size() <= d)
			layers.resize(d + 1);
		layers[d].push_back(v);

		for (auto [eb, ee] = boost::out_edges(v, g); eb != ee; ++eb) {
			ModelNode w = boost::target(*eb, g);
			if (depth[w] == UINT32_MAX) {
				depth[w] = d + 1;
				q.push(w);
			}
		}
	}
}

void Scenes::initializeRenderingOrder() {
	renderingOrder.clear();
	renderingOrder.resize(1); // 0 = opaque

	if (boost::num_vertices(graph) == 0)
		return;

	// Seed the stack with *children of the root*, not the root itself.
	std::vector<SceneNode> stack;
	for (auto [eb, ee] = boost::out_edges(rootNode, graph); eb != ee; ++eb)
		stack.push_back(boost::target(*eb, graph));

	while (!stack.empty()) {
		SceneNode cur = stack.back();
		stack.pop_back();

		const Scene *sc = graph[cur];
		if (!sc) {
			// Shouldn't happen if we only pushed real children,
			// but guard anyway.
			continue;
		}

		// ----- build model layers for this scene -----
		std::vector<std::vector<ModelNode>> layers;
		buildModelLayers(sc, layers);

		if (layers.size() + 1 > renderingOrder.size())
			renderingOrder.resize(layers.size() + 1);

		const ModelGraph &g = sc->getModelGraph();
		for (size_t li = 0; li < layers.size(); ++li) {
			auto &dst = renderingOrder[li + 1];
			for (ModelNode mn : layers[li]) {
				// vertex payload is Model*; no const_cast needed
				Model *mp = g[mn];
				if (mp)
					dst.push_back(mp);
			}
		}

		// push children scenes (reverse for stable order)
		std::vector<SceneNode> kids;
		for (auto [eb, ee] = boost::out_edges(cur, graph); eb != ee; ++eb)
			kids.push_back(boost::target(*eb, graph));
		for (auto it = kids.rbegin(); it != kids.rend(); ++it)
			stack.push_back(*it);
	}
}

void Scenes::registerName(const string &name, SceneNode v) {
	auto [it, inserted] = nameToNode.emplace(name, v);
	if (!inserted) {
		// If you prefer overwrite instead of error, just assign:
		// nameToNode[name] = v; return;
		throw std::runtime_error("Scenes: duplicate node name: " + name);
	}
}

void Scenes::ensureRootExists() {
	if (rootNode == SceneNode() || boost::num_vertices(graph) == 0) {
		rootNode = boost::add_vertex(nullptr, graph);
		registerName("__root__", rootNode);
	}
}

SceneNode Scenes::addChild(const string &name, Scene *child, SceneNode parent) {
	if (parent == SceneNode())
		parent = rootNode;
	SceneNode v = boost::add_vertex(child, graph);
	boost::add_edge(parent, v, graph);
	registerName(name, v);
	child->setParent(&obj(parent));
	return v;
}

void Scenes::link(SceneNode parent, SceneNode child) {
	if (child == rootNode)
		return; // root cannot be reparented

	// remove existing parent edges (enforce single parent)
	auto [ib, ie] = boost::in_edges(child, graph);
	std::vector<SceneGraph::edge_descriptor> rm(ib, ie);
	for (auto e : rm)
		boost::remove_edge(e, graph);

	boost::add_edge(parent, child, graph);
}

void Scenes::detach(SceneNode node) {
	if (node == rootNode)
		return;
	auto [ib, ie] = boost::in_edges(node, graph);
	std::vector<SceneGraph::edge_descriptor> rm(ib, ie);
	for (auto e : rm)
		boost::remove_edge(e, graph);
	boost::add_edge(rootNode, node, graph);
}

// --- Name lookups ---
SceneNode Scenes::nodeByName(const string &name) const {
	if (auto it = nameToNode.find(name); it != nameToNode.end())
		return it->second;
	throw std::runtime_error("Scenes: node not found: " + name);
}

SceneNode Scenes::tryNodeByName(const string &name) const {
	if (auto it = nameToNode.find(name); it != nameToNode.end())
		return it->second;
	return SceneNode(); // null descriptor
}

Scene *Scenes::sceneByName(const string &name) {
	auto it = nameToNode.find(name);
	if (it == nameToNode.end())
		return nullptr;
	return graph[it->second];
}

const Scene *Scenes::sceneByName(const string &name) const {
	auto it = nameToNode.find(name);
	if (it == nameToNode.end())
		return nullptr;
	return graph[it->second];
}

// --- Engine hooks (no world compute needed) ---
void Scenes::swapChainUpdate(float vw, float vh, float fbw, float fbh) {
	for (auto v : renderingOrder) {
		for (auto &m : v) {
			if (m->isVisible()) {
				m->swapChainUpdate(vw, vh, fbw, fbh);
			}
		}
	}
}

void Scenes::tick(float dt, float t) {
	for (const auto v : renderingOrder) {
		for (const auto &m : v) {
			if (m->isVisible()) {
				m->tick(dt, t);
			}
		}
	}
}

void Scenes::compute(VkCommandBuffer ccmd) {
	for (const auto v : renderingOrder) {
		for (const auto &m : v) {
			if (m->isVisible()) {
				m->compute(ccmd);
			}
		}
	}
}

void Scenes::record(VkCommandBuffer cmd) {
	if (renderingOrder.empty()) {
		std::cerr << "[Warning!] Empty render graph!" << std::endl;
		return;
	}

	for (const auto &m : renderingOrder[0]) {
		if (m->isVisible()) {
			m->record(cmd);
		}
	}
}

void Scenes::recordUI(VkCommandBuffer cmd, uint32_t blurLayer) {
	if (renderingOrder.empty()) {
		std::cerr << "[Warning!] Empty render graph!" << std::endl;
		return;
	}

	for (uint32_t i = 1; i < renderingOrder.size(); i++) {
		for (size_t j = 0; j < renderingOrder[i].size(); j++) {
			if (i - 1 == blurLayer && renderingOrder[i][j]->isVisible()) {
				renderingOrder[i][j]->recordUI(cmd, blurLayer);
			}
		}
	}
}
