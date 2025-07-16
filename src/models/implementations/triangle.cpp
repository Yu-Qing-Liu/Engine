#include "models/implementations/triangle.hpp"

Triangle::Triangle(VkDevice &device, const std::string &modelRoot, VkRenderPass &renderPass, VkExtent2D &swapChainExtent) : Model(device, modelRoot, renderPass, swapChainExtent) {
	// Create pipeline stages
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages = {shaderUtils->createShaderStageInfo(shader_program.vertexShader, VK_SHADER_STAGE_VERTEX_BIT), shaderUtils->createShaderStageInfo(shader_program.fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT)};
	// Pipeline configuration
    setup();
	createGraphicsPipeline(shaderStages, vertexInputInfo, inputAssembly);
}

void Triangle::setup() {
    vertices = {
        {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
    };

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

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

}

void Triangle::draw(VkCommandBuffer &commandBuffer, const vec3 &position, const quat &rotation, const vec3 &scale, const vec3 &color) {
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)swapChainExtent.width;
	viewport.height = (float)swapChainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = {0, 0};
	scissor.extent = swapChainExtent;
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	vkCmdDraw(commandBuffer, 3, 1, 0, 0);
}
