#include "scene.hpp"
#include "engine.hpp"
#include "scenes.hpp"
#include <boost/graph/graph_traits.hpp>
#include <stdexcept>
#include <vector>

Scene::Scene() : scenes(nullptr), visible(true), rootNode(ModelNode()) { ensureRootExists(); }

Scene::Scene(Scenes *scenes, bool show) : scenes(scenes), visible(show) { ensureRootExists(); }

std::shared_ptr<Engine> Scene::getEngine() { return scenes->getEngine(); }

VkDevice Scene::getDevice() { return getEngine()->getDevice(); }

void Scene::ensureRootExists() {
	if (rootNode == ModelNode() || boost::num_vertices(modelGraph_) == 0) {
		// Dummy root vertex with no payload
		rootNode = boost::add_vertex(nullptr, modelGraph_);
		// Reserve the name for root so callers can't accidentally reuse it
		nameToNode_.emplace("__root__", rootNode);
	}
}

void Scene::registerName(const std::string &name, ModelNode v) {
	auto [it, inserted] = nameToNode_.emplace(name, v);
	if (!inserted) {
		throw std::runtime_error("Scene: duplicate model name: " + name);
	}
}

bool Scene::has(const std::string &name) const { return nameToNode_.find(name) != nameToNode_.end(); }

ModelNode Scene::nodeByName(const std::string &name) const {
	if (auto it = nameToNode_.find(name); it != nameToNode_.end())
		return it->second;
	throw std::runtime_error("Scene: model not found: " + name);
}

ModelNode Scene::tryNodeByName(const std::string &name) const {
	if (auto it = nameToNode_.find(name); it != nameToNode_.end())
		return it->second;
	return ModelNode(); // null descriptor
}

Model *Scene::modelByName(const std::string &name) {
	auto it = nameToNode_.find(name);
	if (it == nameToNode_.end())
		return nullptr;
	return modelGraph_[it->second];
}

const Model *Scene::modelByName(const std::string &name) const {
	auto it = nameToNode_.find(name);
	if (it == nameToNode_.end())
		return nullptr;
	return modelGraph_[it->second];
}

ModelNode Scene::addChild(const std::string &name, Model *m, ModelNode parent) {
	ensureRootExists();

	if (!m)
		throw std::runtime_error("Scene::addChild: null Model*");
	if (name.empty())
		throw std::runtime_error("Scene::addChild: empty name is not allowed");
	if (has(name))
		throw std::runtime_error("Scene::addChild: duplicate model name: " + name);

	if (parent == ModelNode())
		parent = rootNode;

	// Store the raw pointer in the graph
	ModelNode v = boost::add_vertex(m, modelGraph_);
	boost::add_edge(parent, v, modelGraph_);

	registerName(name, v);
	return v;
}

void Scene::link(ModelNode parent, ModelNode child) {
	if (child == rootNode)
		return; // root cannot be reparented

	auto [ib, ie] = boost::in_edges(child, modelGraph_);
	std::vector<ModelGraph::edge_descriptor> rm(ib, ie);
	for (auto e : rm)
		boost::remove_edge(e, modelGraph_);

	boost::add_edge(parent, child, modelGraph_);
}

void Scene::detach(ModelNode node) {
	if (node == rootNode)
		return;

	auto [ib, ie] = boost::in_edges(node, modelGraph_);
	std::vector<ModelGraph::edge_descriptor> rm(ib, ie);
	for (auto e : rm)
		boost::remove_edge(e, modelGraph_);

	boost::add_edge(rootNode, node, modelGraph_);
}
