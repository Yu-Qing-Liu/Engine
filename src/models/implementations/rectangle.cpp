#include "models/implementations/rectangle.hpp"
#include "engineutils.hpp"
#include <chrono>
#include <cstring>
#include <vulkan/vulkan_core.h>

using namespace glm;

Rectangle::Rectangle(const std::string &shaderPath) : Model(shaderPath) {
	// Create pipeline stages
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages = {Engine::createShaderStageInfo(shaderProgram.vertexShader, VK_SHADER_STAGE_VERTEX_BIT), Engine::createShaderStageInfo(shaderProgram.fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT)};
	// Pipeline configuration
	createGraphicsPipeline(shaderStages, vertexInputInfo, inputAssembly);
	createVertexBuffer();
    createIndexBuffer();
}

Rectangle::~Rectangle() {
	vkDestroyBuffer(Engine::device, vertexBuffer, nullptr);
	vkFreeMemory(Engine::device, vertexBufferMemory, nullptr);

	vkDestroyBuffer(Engine::device, indexBuffer, nullptr);
	vkFreeMemory(Engine::device, indexBufferMemory, nullptr);
}

void Rectangle::createVertexBuffer() {
	vertices = {
        {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
        {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
    };

    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    Engine::createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingBufferMemory
    );

	void *data;
	vkMapMemory(Engine::device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, vertices.data(), (size_t)bufferSize);
	vkUnmapMemory(Engine::device, stagingBufferMemory);

    Engine::createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        vertexBuffer,
        vertexBufferMemory
    );

    Engine::copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

    vkDestroyBuffer(Engine::device, stagingBuffer, nullptr);
    vkFreeMemory(Engine::device, stagingBufferMemory, nullptr);
}

void Rectangle::createIndexBuffer() {
    indices = {
        0, 1, 2, 2, 3, 0
    };

    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    Engine::createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingBufferMemory
    );

	void *data;
	vkMapMemory(Engine::device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, indices.data(), (size_t)bufferSize);
	vkUnmapMemory(Engine::device, stagingBufferMemory);

    Engine::createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        indexBuffer,
        indexBufferMemory
    );

    Engine::copyBuffer(stagingBuffer, indexBuffer, bufferSize);

    vkDestroyBuffer(Engine::device, stagingBuffer, nullptr);
    vkFreeMemory(Engine::device, stagingBufferMemory, nullptr);
}

void Rectangle::updateUniformBuffer() {
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    ubo.model = rotate(mat4(1.0f), time * radians(90.0f), vec3(0.0f, 0.0f, 1.0f));
    ubo.view = lookAt(vec3(2.0f, 2.0f, 2.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = perspective(radians(45.0f), Engine::swapChainExtent.width / (float) Engine::swapChainExtent.height, 0.1f, 10.0f);
    ubo.proj[1][1] *= -1;

    memcpy(uniformBuffersMapped[Engine::currentFrame], &ubo, sizeof(ubo));
}

void Rectangle::draw(const vec3 &position, const quat &rotation, const vec3 &scale, const vec3 &color) {
	vkCmdBindPipeline(Engine::currentCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)Engine::swapChainExtent.width;
	viewport.height = (float)Engine::swapChainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(Engine::currentCommandBuffer(), 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = {0, 0};
	scissor.extent = Engine::swapChainExtent;
	vkCmdSetScissor(Engine::currentCommandBuffer(), 0, 1, &scissor);

	VkBuffer vertexBuffers[] = {vertexBuffer};
	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(Engine::currentCommandBuffer(), 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(Engine::currentCommandBuffer(), indexBuffer, 0, VK_INDEX_TYPE_UINT16);

	vkCmdDrawIndexed(Engine::currentCommandBuffer(), static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
}
