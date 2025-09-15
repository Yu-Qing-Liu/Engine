#include "default.hpp"
#include "engine.hpp"
#include "particles.hpp"
#include "texture.hpp"
#include "polygon.hpp"
#include "scenes.hpp"

Default::Default(Scenes &scenes) : Scene(scenes) {
    triangle = make_unique<Polygon>(
        this,
        persp,
        screenParams,
        std::vector<Polygon::Vertex> {
            {{0.0f, 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{-0.433f, -0.25f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
            {{0.433f, -0.25f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
        },
        std::vector<uint32_t> {
            0, 1, 2
        }
    );

    example = make_unique<Texture>(
        this,
        persp,
        screenParams,
        Assets::textureRootPath + "/example/example.png", 
        std::vector<Texture::Vertex> {
            {{0.0f, -0.5f, 0.25f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
            {{1.0f, -0.5f, 0.25f}, {0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
            {{1.0f, 0.5f, 0.25f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
            {{0.0f, 0.5f, 0.25f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

            {{-0.5f, -0.5f, -0.25f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
            {{0.5f, -0.5f, -0.25f}, {0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
            {{0.5f, 0.5f, -0.25f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
            {{-0.5f, 0.5f, -0.25f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
        },
        std::vector<uint32_t> {
            0, 1, 2, 2, 3, 0,
            4, 5, 6, 6, 7, 4
        }
    );

    particles = make_unique<Particles>(this, persp, screenParams, 1024, screenParams.viewport.width, screenParams.viewport.height);

    room = make_unique<Object>(this, persp, screenParams, Assets::modelRootPath + "/example/example.obj");
    room->setRayTraceEnabled(true);
    room->onMouseHover = []() {
        std::cout << "Room Hit " << Engine::time << std::endl;
    };

    Text::TextParams tp{ Assets::fontRootPath + "/arial.ttf", 48 };
    text = make_unique<Text>(this, orthographic, screenParams, tp);
    text->updateUniformBuffer(translate(mat4(1.0f), glm::vec3(screenParams.viewport.width * 0.5f, screenParams.viewport.height * 0.15f, 0.0f)));
}

void Default::updateScreenParams() {
    screenParams.viewport.x        = 0.0f;
    screenParams.viewport.y        = 0.0f;
    screenParams.viewport.width    = (float) Engine::swapChainExtent.width / 2;
    screenParams.viewport.height   = (float) Engine::swapChainExtent.height / 2;
    screenParams.viewport.minDepth = 0.0f;
    screenParams.viewport.maxDepth = 1.0f;
    screenParams.scissor.offset = {(int32_t)screenParams.viewport.x, (int32_t)screenParams.viewport.y};
    screenParams.scissor.extent = {(uint32_t)screenParams.viewport.width, (uint32_t)screenParams.viewport.height};
}

void Default::swapChainUpdate() {
    persp.proj = perspective(radians(45.0f), screenParams.viewport.width / screenParams.viewport.height, 0.1f, 10.0f);
    orthographic.proj = ortho(0.0f, screenParams.viewport.width, 0.0f, -screenParams.viewport.height, -1.0f, 1.0f);
    triangle->updateUniformBuffer(std::nullopt, std::nullopt, persp.proj);
    particles->updateUniformBuffer(std::nullopt, std::nullopt ,persp.proj);
    example->updateUniformBuffer(std::nullopt, std::nullopt, persp.proj);
    room->updateUniformBuffer(std::nullopt, std::nullopt, persp.proj);
    text->updateUniformBuffer(translate(mat4(1.0f), glm::vec3(screenParams.viewport.width * 0.5f, screenParams.viewport.height * 0.15f, 0.0f)), std::nullopt, orthographic.proj);
}

void Default::updateComputeUniformBuffers() {
    particles->updateComputeUniformBuffer();
}

void Default::computePass() {
    particles->compute();
}

void Default::updateUniformBuffers() {
    triangle->updateUniformBuffer(rotate(mat4(1.0f), Engine::time * radians(90.0f), vec3(0.0f, 0.0f, 1.0f)));
    example->updateUniformBuffer(rotate(mat4(1.0f), Engine::time * radians(90.0f), vec3(0.0f, 0.0f, 1.0f)));
    room->updateUniformBuffer(rotate(mat4(1.0f), Engine::time * radians(90.0f), vec3(0.0f, 0.0f, 1.0f)));
}

void Default::renderPass() {
    triangle->render();
    example->render();
    room->render();
    text->renderText("Hello World");

    particles->render();
}
