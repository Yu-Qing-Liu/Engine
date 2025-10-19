#pragma once

#include "image.hpp"
#include "model.hpp"
#include "texture.hpp"
#include <memory>

using std::make_unique;
using std::unique_ptr;

using MVP = Model::MVP;
using ScreenParams = Model::ScreenParams;

namespace Textures {

inline unique_ptr<Texture> icon(Scene *scene, const MVP &mvp, ScreenParams &screenParams, const string &texturePath, const VkRenderPass &renderPass = Engine::renderPass) {
	return make_unique<Texture>(scene, mvp, screenParams, texturePath,
								std::vector<Texture::Vertex>{
									{{-0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
									{{0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
									{{0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
									{{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
								},
								std::vector<uint32_t>{
									0,
									1,
									2,
									2,
									3,
									0,
								},
								renderPass);
}

inline unique_ptr<Image> icon(Scene *scene, const MVP &mvp, ScreenParams &screenParams, const VkRenderPass &renderPass = Engine::renderPass) {
	return make_unique<Image>(scene, mvp, screenParams,
							  std::vector<Image::Vertex>{
								  {{-0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
								  {{0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
								  {{0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
								  {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
							  },
							  std::vector<uint32_t>{
								  0,
								  1,
								  2,
								  2,
								  3,
								  0,
							  },
							  renderPass);
}

} // namespace Textures
