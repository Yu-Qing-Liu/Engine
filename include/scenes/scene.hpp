#pragma once

#include "model.hpp"

#include <boost/graph/adjacency_list.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>

class Scenes;

// Vertex payload is a raw pointer; ownership is kept in Scene::ownedModels_
using ModelGraph = boost::adjacency_list<boost::vecS,			// out-edge container
										 boost::vecS,			// vertex container (no deletions)
										 boost::bidirectionalS, // allow parent lookup
										 Model *,				// vertex data: non-owning pointer
										 boost::no_property>;

using ModelNode = ModelGraph::vertex_descriptor;

class Scene {
  public:
	Scene();
	Scene(Scenes *scenes, bool show = true);
	virtual ~Scene() = default;

	// --- Graph API (single-root model hierarchy) ---
	// Root is a dummy node (payload=nullptr). Use addChild() to attach real models under root.
	ModelNode addChild(const std::string &name, Model *m, ModelNode parent = ModelNode()); // parent defaults to root
	void link(ModelNode parent, ModelNode child);										   // reparent child
	void detach(ModelNode node);														   // move under root (no standalones)

	// --- Name lookups ---
	bool has(const std::string &name) const;
	ModelNode nodeByName(const std::string &name) const;	// throws if not found
	ModelNode tryNodeByName(const std::string &name) const; // returns ModelNode() if not found
	Model *modelByName(const std::string &name);			// nullptr if not found
	const Model *modelByName(const std::string &name) const;

	// --- Accessors ---
	Model &obj(ModelNode v) { return *modelGraph_[v]; }
	const Model &obj(ModelNode v) const { return *modelGraph_[v]; }
	ModelNode root() const { return rootNode; }
	const ModelGraph &getModelGraph() const { return modelGraph_; }

	bool isVisible() const { return visible; }
	void setVisible(bool v) { visible = v; }

	const Scenes &getScenes() const { return *scenes; }

	void setParent(Scene *parent) { this->parent = parent; }

	float vpx() const { return vpx_; }
	float vpy() const { return vpy_; }
	float vpw() const { return vpw_; }
	float vph() const { return vph_; }

	void setSceneVpx(float vpx) { vpx_ = vpx; }
	void setSceneVpy(float vpy) { vpy_ = vpy; }
	void setSceneVpw(float vpw) { vpw_ = vpw; }
	void setSceneVph(float vph) { vph_ = vph; }

	void setFbw(float fbw) { fbw_ = fbw; }
	void setFbh(float fbh) { fbh_ = fbh; }

	std::shared_ptr<Engine> getEngine();
	VkDevice getDevice();

	VPMatrix &getCamera() { return camera; }
	vec3 &getCamTarget() { return camTarget; }

  protected:
	Scenes *scenes = nullptr;
	Scene *parent = nullptr;
	bool visible = true;

	VPMatrix camera{};
	vec3 camTarget = vec3(0.f);

	float vpx_ = 0.f;
	float vpy_ = 0.f;
	float vpw_ = 0.f;
	float vph_ = 0.f;

	float fbw_ = 0.f;
	float fbh_ = 0.f;

  private:
	ModelGraph modelGraph_;
	ModelNode rootNode = ModelNode();

	std::unordered_map<std::string, ModelNode> nameToNode_;

	void ensureRootExists();
	void registerName(const std::string &name, ModelNode v);
};
