#pragma once

#include "object.hpp"
#include "scene.hpp"

class Header : public Scene {
  public:
	Header(Scenes &scenes);
	Header(Header &&) = default;
	Header(const Header &) = delete;
	Header &operator=(Header &&) = delete;
	Header &operator=(const Header &) = delete;
	~Header() = default;

	static const std::string getName() { return "Header"; }

	void updateScreenParams() override;

	void updateComputeUniformBuffers() override;
	void computePass() override;

	void updateUniformBuffers() override;
	void renderPass() override;
	void swapChainUpdate() override;

  private:
	Model::UBO fridgeUBO{mat4(1.0f), lookAt(vec3(5.0f, 5.0f, 5.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 1.0f))};
	Model::ScreenParams fridgeVP{};
	std::unique_ptr<Object> fridge;
};
