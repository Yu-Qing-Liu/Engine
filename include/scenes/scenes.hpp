#pragma once

#include "scene.hpp"

#include <boost/graph/adjacency_list.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan_core.h>

using std::string;
using std::unordered_map;
using std::vector;

class Engine;

// Single-root scene graph: vertices ARE Scene objects
using SceneGraph = boost::adjacency_list<boost::vecS,			// out-edge container
										 boost::vecS,			// vertex container (no deletions; stable indices)
										 boost::bidirectionalS, // parent lookup allowed
										 Scene *,				// vertex payload
										 boost::no_property>;

using SceneNode = SceneGraph::vertex_descriptor;

class Scenes {
  public:
	Scenes(std::shared_ptr<Engine> engine);
	~Scenes() = default;

	// Engine hooks
	void swapChainUpdate(float vw, float vh, float fbw, float fbh);
	void tick(float timeSinceLastFrameMs, float timeMs);
	void compute(VkCommandBuffer cmd);
	void record(VkCommandBuffer cmd);
	void recordUI(VkCommandBuffer cmd, uint32_t blurLayerIdx);

	// Single-root graph API (no world compute here)
	SceneNode addChild(const string &name, Scene *child, SceneNode parent = SceneNode()); // parent defaults to root
	void link(SceneNode parent, SceneNode child);										  // reparent (root cannot be child)
	void detach(SceneNode node);														  // move under root (no standalones)

	// Name-based lookup
	SceneNode nodeByName(const string &name) const;		// throws if not found
	SceneNode tryNodeByName(const string &name) const;	// returns SceneNode() if not found
	Scene *sceneByName(const string &name);				// nullptr if not found
	const Scene *sceneByName(const string &name) const; // nullptr if not found

	// Direct access
	Scene &obj(SceneNode v) { return *graph[v]; }
	const Scene &obj(SceneNode v) const { return *graph[v]; }
	SceneNode root() const { return rootNode; }

	std::shared_ptr<Engine> getEngine() const { return engine; }

	Model *getRayPicked() const { return rayPicked; }
	void setRayPicked(Model *picked) const { rayPicked = picked; }

  private:
	void ensureRootExists(); // create a default root if missing
	void registerName(const string &name, SceneNode v);
	void renameInternal(SceneNode v, const string &newName); // optional if you ever rename nodes
	void initializeRenderingOrder();

  private:
	std::shared_ptr<Engine> engine;
	SceneGraph graph;
	SceneNode rootNode = SceneNode();			 // valid after setRoot/ensureRootExists
	unordered_map<string, SceneNode> nameToNode; // unique names -> nodes
	vector<vector<Model *>> renderingOrder;

	static Model *rayPicked;

  private:
};
