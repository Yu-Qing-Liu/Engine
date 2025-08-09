#pragma once

#include "model.hpp"
#include <unordered_map>

class Models {
  public:
	Models();
	Models(Models &&) = default;
	Models(const Models &) = default;
	Models &operator=(Models &&) = delete;
	Models &operator=(const Models &) = delete;
	~Models() = default;

	enum ModelName {
		TRIANGLE,
        RECTANGLE,
	};

	std::unordered_map<ModelName, std::unique_ptr<Model>> models;
};
