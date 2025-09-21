#pragma once

#include "object.hpp"
#include "scene.hpp"
#include "texture.hpp"

class Background : public Scene {
  public:
	Background(Scenes &scenes);
	Background(Background &&) = delete;
	Background(const Background &) = delete;
	Background &operator=(Background &&) = delete;
	Background &operator=(const Background &) = delete;
	~Background() = default;

	std::string getName() override { return "Background"; }

	void updateScreenParams() override;

	void updateComputeUniformBuffers() override;
	void computePass() override;

	void updateUniformBuffers() override;
	void renderPass() override;
	void swapChainUpdate() override;

  private:
	unique_ptr<Model> particles;
	unique_ptr<Texture> backgroundImage;
};
