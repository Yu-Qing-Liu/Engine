#pragma once
#include "scene.hpp"

using std::make_unique;
using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::vector;

class Widget {
  public:
	Widget(Scene *scene);
	~Widget() = default;

	Model *getModel(const string &name) { return models.at(name); }
	const unordered_map<string, Model *> &getModels() const { return models; }

  protected:
	Scene *scene;
	unordered_map<string, Model *> models;
};
