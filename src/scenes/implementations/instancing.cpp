#include "instancing.hpp"
#include "assets.hpp"
#include "colors.hpp"
#include "engine.hpp"
#include "object.hpp"
#include "scenes.hpp"
#include <memory>
#include <optional>


Instancing::Instancing(Scenes &scenes) : Scene(scenes) {
    cells = std::make_shared<std::unordered_map<int, InstancedRectangleData>>(
        std::initializer_list<std::pair<const int, InstancedRectangleData>>{{0, {}}, {1, {}}, {2, {}}, {3, {}}}
    );
    grid = make_unique<InstancedRectangle>(orthographic, screenParams, cells, 4);

    polygonInstances = std::make_shared<std::unordered_map<int, InstancedPolygonData>>(
        std::initializer_list<std::pair<const int, InstancedPolygonData>>{{0, {}}, {1, {}}, {2, {}}, {3, {}}}
    );
    polygons = make_unique<InstancedPolygon>(
        persp,
        screenParams,
        std::vector<InstancedPolygon::Vertex>{
            // idx, position                 // color (RGBA)
            /*0*/ {{-0.5f, -0.5f, -0.5f},     {1.0f, 1.0f, 1.0f, 1.0f}}, // LBB
            /*1*/ {{ 0.5f, -0.5f, -0.5f},     {1.0f, 1.0f, 1.0f, 1.0f}}, // RBB
            /*2*/ {{ 0.5f,  0.5f, -0.5f},     {1.0f, 1.0f, 1.0f, 1.0f}}, // RTB
            /*3*/ {{-0.5f,  0.5f, -0.5f},     {1.0f, 1.0f, 1.0f, 1.0f}}, // LTB
            /*4*/ {{-0.5f, -0.5f,  0.5f},     {1.0f, 1.0f, 1.0f, 1.0f}}, // LBF
            /*5*/ {{ 0.5f, -0.5f,  0.5f},     {1.0f, 1.0f, 1.0f, 1.0f}}, // RBF
            /*6*/ {{ 0.5f,  0.5f,  0.5f},     {1.0f, 1.0f, 1.0f, 1.0f}}, // RTF
            /*7*/ {{-0.5f,  0.5f,  0.5f},     {1.0f, 1.0f, 1.0f, 1.0f}}, // LTF
        },
        std::vector<uint16_t>{
            // Front  (+Z)
            4, 5, 6,   6, 7, 4,
            // Back   (-Z)
            1, 0, 3,   3, 2, 1,
            // Left   (-X)
            0, 4, 7,   7, 3, 0,
            // Right  (+X)
            5, 1, 2,   2, 6, 5,
            // Top    (+Y)
            3, 7, 6,   6, 2, 3,
            // Bottom (-Y)
            0, 1, 5,   5, 4, 0,
        },
        polygonInstances,
        4
    );

    roomInstances = std::make_shared<std::unordered_map<int, InstancedObjectData>>(
        std::initializer_list<std::pair<const int, InstancedObjectData>>{{0, {}}, {1, {}}, {2, {}}, {3, {}}}
    );
    rooms = make_unique<InstancedObject>(persp, screenParams, Assets::modelRootPath + "/example/example.obj" , roomInstances, 4);
}

void Instancing::updateScreenParams() {
    screenParams.viewport.x        = (float) Engine::swapChainExtent.width / 2;
    screenParams.viewport.y        = (float) Engine::swapChainExtent.height / 2;
    screenParams.viewport.width    = (float) Engine::swapChainExtent.width / 2;
    screenParams.viewport.height   = (float) Engine::swapChainExtent.height / 2;
    screenParams.viewport.minDepth = 0.0f;
    screenParams.viewport.maxDepth = 1.0f;
    screenParams.scissor.offset = {(int32_t)screenParams.viewport.x, (int32_t)screenParams.viewport.y};
    screenParams.scissor.extent = {(uint32_t)screenParams.viewport.width, (uint32_t)screenParams.viewport.height};
}

void Instancing::swapChainUpdate() {
	orthographic.proj = ortho(0.0f, screenParams.viewport.width, 0.0f, -screenParams.viewport.height, -1.0f, 1.0f);
    persp.proj = perspective(radians(45.0f), screenParams.viewport.width / screenParams.viewport.height, 0.1f, 10.0f);

    grid->updateInstance(0, InstancedRectangleData(screenParams.viewport.width * 0.25f, screenParams.viewport.height * 0.25f, {100, 100}));
    grid->updateInstance(1, InstancedRectangleData(screenParams.viewport.width * 0.25f, screenParams.viewport.height * 0.75f, {100, 100}));
    grid->updateInstance(2, InstancedRectangleData(screenParams.viewport.width * 0.75f, screenParams.viewport.height * 0.25f, {100, 100}));
    grid->updateInstance(3, InstancedRectangleData(screenParams.viewport.width * 0.75f, screenParams.viewport.height * 0.75f, {100, 100}));
    grid->updateUniformBuffer(std::nullopt, std::nullopt, orthographic.proj);

    const vec3 size = vec3(0.9f); // scale cubes a bit
	polygons->updateInstance(0, InstancedPolygonData(vec3(-1.0f, -1.0f, 0.0f), size, Colors::Green, Colors::Black));
	polygons->updateInstance(1, InstancedPolygonData(vec3(1.0f, -1.0f, 0.0f), size, Colors::Red, Colors::Black));
	polygons->updateInstance(2, InstancedPolygonData(vec3(-1.0f, 1.0f, 0.0f), size, Colors::Purple, Colors::Black));
	polygons->updateInstance(3, InstancedPolygonData(vec3(1.0f, 1.0f, 0.0f), size, Colors::Yellow, Colors::Black));
    polygons->updateUniformBuffer(std::nullopt, std::nullopt, persp.proj);

	rooms->updateInstance(0, InstancedObjectData(vec3(-1.0f, -1.0f, 0.0f), size));
	rooms->updateInstance(1, InstancedObjectData(vec3(1.0f, -1.0f, 0.0f), size));
	rooms->updateInstance(2, InstancedObjectData(vec3(-1.0f, 1.0f, 0.0f), size));
	rooms->updateInstance(3, InstancedObjectData(vec3(1.0f, 1.0f, 0.0f), size));
    rooms->updateUniformBuffer(std::nullopt, std::nullopt, persp.proj);
}

void Instancing::updateComputeUniformBuffers() {}

void Instancing::computePass() {}

void Instancing::updateUniformBuffers() {}

void Instancing::renderPass() {
    polygons->render();
    rooms->render();
}
