#include "models/implementations/triangle.hpp"
#include "engineutils.hpp"
#include <cstring>
#include <vulkan/vulkan_core.h>

Triangle::Triangle(const std::string &shaderPath) : Model(shaderPath) {
	// Create pipeline stages
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages = {Engine::createShaderStageInfo(shaderProgram.vertexShader, VK_SHADER_STAGE_VERTEX_BIT), Engine::createShaderStageInfo(shaderProgram.fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT)};
	// Pipeline configuration
	setup();
	createGraphicsPipeline(shaderStages, vertexInputInfo, inputAssembly);
	// VBO
	createVertexBuffer();
}

Triangle::~Triangle() {
	vkDestroyBuffer(Engine::device, vertexBuffer, nullptr);
	vkFreeMemory(Engine::device, vertexBufferMemory, nullptr);
}

void Triangle::setup() {
	bindingDescription = Vertex::getBindingDescription();
	auto attrs = Vertex::getAttributeDescriptions();
	attributeDescriptions = std::vector<VkVertexInputAttributeDescription>(attrs.begin(), attrs.end());

	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
}

void Triangle::createVertexBuffer() {
	vertices = {{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}}, {{0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}}, {{-0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}}};

    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
    Engine::createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        vertexBuffer,
        vertexBufferMemory
    );

	vkMapMemory(Engine::device, vertexBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, vertices.data(), (size_t)bufferSize);
	vkUnmapMemory(Engine::device, vertexBufferMemory);
}

void Triangle::draw(const vec3 &position, const quat &rotation, const vec3 &scale, const vec3 &color) {
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

	vkCmdDraw(Engine::currentCommandBuffer(), static_cast<uint32_t>(vertices.size()), 1, 0, 0);
}
