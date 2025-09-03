#include "default.hpp"
#include "engine.hpp"
#include "objmodel.hpp"
#include "particles.hpp"
#include "texture.hpp"
#include "polygon.hpp"
#include "scenes.hpp"
#include <memory>
#include <optional>

Default::Default(Scenes &scenes) : Scene(scenes) {
    this->triangle = make_unique<Polygon>(
        std::vector<Model::Vertex> {
            {{0.0f, 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{-0.433f, -0.25f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
            {{0.433f, -0.25f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
        },
        std::vector<uint16_t> {
            0, 1, 2
        }
    );

    this->example = make_unique<Texture>(
        Engine::textureRootPath + "/example/example.png", 
        std::vector<Model::TexVertex> {
            {{0.0f, -0.5f, 0.25f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
            {{1.0f, -0.5f, 0.25f}, {0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
            {{1.0f, 0.5f, 0.25f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
            {{0.0f, 0.5f, 0.25f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

            {{-0.5f, -0.5f, -0.25f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
            {{0.5f, -0.5f, -0.25f}, {0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
            {{0.5f, 0.5f, -0.25f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
            {{-0.5f, 0.5f, -0.25f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
        },
        std::vector<uint16_t> {
            0, 1, 2, 2, 3, 0,
            4, 5, 6, 6, 7, 4
        }
    );

    this->particles = make_unique<Particles>(8192, screenParams.viewport.width, screenParams.viewport.height);

    this->room = make_unique<OBJModel>(Engine::modelRootPath + "/example/example.obj");

    Text::TextParams tp{ Engine::fontRootPath + "/arial.ttf", 48 };
    this->text = make_unique<Text>(tp);
}

void Default::updateScreenParams() {
    screenParams.viewport.x        = 0.0f;
    screenParams.viewport.y        = 0.0f;
    screenParams.viewport.width    = (float) 800;
    screenParams.viewport.height   = (float) 800;
    screenParams.viewport.minDepth = 0.0f;
    screenParams.viewport.maxDepth = 1.0f;
    screenParams.scissor.offset = {0, 0};
    screenParams.scissor.extent = {800, 800};
}

void Default::updateComputeUniformBuffers() {
    particles->updateComputeUniformBuffer();
}

void Default::computePass() {
    particles->compute();
}

void Default::updateUniformBuffers() {
    example->updateUniformBuffer(rotate(mat4(1.0f), Engine::time * radians(90.0f), vec3(0.0f, 0.0f, 1.0f)));
    triangle->updateUniformBuffer(rotate(mat4(1.0f), Engine::time * radians(90.0f), vec3(0.0f, 0.0f, 1.0f)));
    room->updateUniformBuffer(rotate(mat4(1.0f), Engine::time * radians(90.0f), vec3(0.0f, 0.0f, 1.0f)));
    text->updateUniformBuffer();
}

void Default::swapChainUpdate() {
    updateScreenParams();

    example->updateUniformBuffer(std::nullopt, std::nullopt, perspective(radians(45.0f), screenParams.viewport.width / screenParams.viewport.height, 0.1f, 10.0f));
    triangle->updateUniformBuffer(std::nullopt, std::nullopt, perspective(radians(45.0f), screenParams.viewport.width / screenParams.viewport.height, 0.1f, 10.0f));
    room->updateUniformBuffer(std::nullopt, std::nullopt, perspective(radians(45.0f), screenParams.viewport.width / screenParams.viewport.height, 0.1f, 10.0f));
    text->updateUniformBuffer(
        translate(mat4(1.0f), glm::vec3(screenParams.viewport.width * 0.5f, screenParams.viewport.height * 0.15f, 0.0f)),
        std::nullopt, 
        ortho(0.0f, screenParams.viewport.width, 0.0f, -screenParams.viewport.height, -1.0f, 1.0f)
    );
}

void Default::renderPass() {
    example->render(persp, screenParams);
    triangle->render(persp, screenParams);
    room->render(persp, screenParams);
    text->renderText(orthographic, screenParams, "Hello World", 1.0f);

    particles->render(orthographic, screenParams);
}
