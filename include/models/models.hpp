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
	};

	std::unordered_map<ModelName, std::unique_ptr<Model>> models;

  private:
	std::string modelsPath = std::string(PROJECT_ROOT_DIR) + "/src/shaders/triangle";
};
