#pragma once

#include "model.hpp"
#include "texture.hpp"
#include <memory>

using std::make_unique;
using std::unique_ptr;

using MVP = Model::MVP;
using ScreenParams = Model::ScreenParams;

namespace Textures {

inline unique_ptr<Texture> icon(Scene *scene, const MVP &ubo, ScreenParams &screenParams, const string &texturePath, const VkRenderPass &renderPass = Engine::renderPass) {
	return make_unique<Texture>(scene, ubo, screenParams, texturePath,
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

} // namespace Textures
