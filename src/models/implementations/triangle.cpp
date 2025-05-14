#include "models/implementations/triangle.hpp"

Triangle::Triangle(VkDevice &device, const std::string &modelRoot, VkRenderPass &renderPass, VkExtent2D &swapChainExtent) : Model(device, modelRoot, renderPass, swapChainExtent) {
	// Create pipeline stages
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages = {shader_utils->createShaderStageInfo(shader_program.vertexShader, VK_SHADER_STAGE_VERTEX_BIT), shader_utils->createShaderStageInfo(shader_program.fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT)};

	// Pipeline configuration
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

	createGraphicsPipeline(shaderStages, vertexInputInfo, inputAssembly);
}

void Triangle::draw(VkCommandBuffer &commandBuffer, const vec3 &position, const quat &rotation, const vec3 &scale, const vec3 &color) {
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
	vkCmdDraw(commandBuffer, 3, 1, 0, 0);
}
