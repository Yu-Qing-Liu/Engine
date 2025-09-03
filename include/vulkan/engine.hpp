#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <openssl/sha.h>
#include <shaderc/shaderc.h>
#include <shaderc/shaderc.hpp>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan_core.h>

using namespace shaderc;
using namespace std::filesystem;

namespace Engine {

inline const int MAX_FRAMES_IN_FLIGHT = 2;

inline std::string shaderRootPath = std::string(PROJECT_ROOT_DIR) + "/src/shaders";
inline std::string textureRootPath = std::string(PROJECT_ROOT_DIR) + "/src/textures";
inline std::string shaderCachePath = std::string(PROJECT_ROOT_DIR) + "/src/shaders/cache";
inline std::string modelRootPath = std::string(PROJECT_ROOT_DIR) + "/src/meshes";
inline std::string fontRootPath = std::string(PROJECT_ROOT_DIR) + "/src/fonts";

inline VkDebugUtilsMessengerEXT debugMessenger;
inline GLFWwindow *window;
inline VkSurfaceKHR surface;

inline VkInstance instance;

inline Compiler compiler;
inline CompileOptions options;

inline VkQueue graphicsQueue;
inline VkQueue computeQueue;
inline VkQueue presentQueue;

inline VkDevice device;
inline VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

inline VkRenderPass renderPass;
inline VkExtent2D swapChainExtent;

inline VkCommandPool commandPool;

inline VkFormat swapChainImageFormat;
inline std::vector<VkImageView> swapChainImageViews;
inline std::vector<VkImage> swapChainImages;

inline std::vector<VkCommandBuffer> commandBuffers;
inline std::vector<VkCommandBuffer> computeCommandBuffers;
inline uint32_t currentFrame = 0;

inline VkSwapchainKHR swapChain;
inline std::vector<VkFramebuffer> swapChainFramebuffers;

inline VkImage depthImage;
inline VkDeviceMemory depthImageMemory;
inline VkImageView depthImageView;

inline float startTime = 0.0f;
inline float lastFrameTime = 0.0f;
inline double lastTime = 0.0f;
inline float time = 0.0f;

inline void createDirectory(const std::string &path) {
	if (!exists(path)) {
		try {
			if (create_directory(path)) {
				std::cout << "Directory created successfully: " << path << std::endl;
			} else {
				std::cerr << "Failed to create directory: " << path << std::endl;
			}
		} catch (const filesystem_error &e) {
			std::cerr << "Error while creating directory: " << e.what() << std::endl;
		}
	} else {
		std::cout << "Directory already exists: " << path << std::endl;
	}
}

inline void initialize() {
	createDirectory(shaderRootPath);
	createDirectory(textureRootPath);
	createDirectory(shaderCachePath);
}

struct ShaderModules {
	VkShaderModule vertexShader = VK_NULL_HANDLE;
	VkShaderModule tessellationControlShader = VK_NULL_HANDLE;
	VkShaderModule tessellationEvaluationShader = VK_NULL_HANDLE;
	VkShaderModule geometryShader = VK_NULL_HANDLE;
	VkShaderModule fragmentShader = VK_NULL_HANDLE;
	VkShaderModule computeShader = VK_NULL_HANDLE;
};

struct ShaderBinaries {
	std::vector<uint32_t> vertexShader;
	std::vector<uint32_t> tessellationControlShader;
	std::vector<uint32_t> tessellationEvaluationShader;
	std::vector<uint32_t> geometryShader;
	std::vector<uint32_t> fragmentShader;
	std::vector<uint32_t> computeShader;
};

inline std::unordered_map<std::string, shaderc_shader_kind> shaderExtensions = {{".vert", shaderc_glsl_vertex_shader}, {".tesc", shaderc_glsl_tess_control_shader}, {".tese", shaderc_glsl_tess_evaluation_shader}, {".geom", shaderc_glsl_geometry_shader}, {".frag", shaderc_glsl_fragment_shader}, {".comp", shaderc_glsl_compute_shader}};

inline VkCommandBuffer currentCommandBuffer() { return commandBuffers[currentFrame]; }

inline VkCommandBuffer currentComputeCommandBuffer() { return computeCommandBuffers[currentFrame]; }

inline std::string readFile(const std::string &filePath) {
	std::ifstream shader_file(filePath);
	if (!shader_file.is_open()) {
		std::cerr << "Failed to open shader file: " << filePath << std::endl;
		return "";
	}
	std::stringstream buffer;
	buffer << shader_file.rdbuf();
	std::string content = buffer.str();
	return content;
}

inline std::string computeHash(const std::string &input) {
	unsigned char hash[SHA_DIGEST_LENGTH];
	SHA1(reinterpret_cast<const unsigned char *>(input.data()), input.size(), hash);
	std::stringstream ss;
	for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
		ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
	}
	return ss.str();
}

inline std::vector<uint32_t> readBinaryFile(const std::string &path) {
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		return {};
	}

	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);

	if (size % sizeof(uint32_t) != 0) {
		std::cerr << "Cache file is corrupted: " << path << std::endl;
		return {};
	}

	std::vector<uint32_t> data(size / sizeof(uint32_t));
	if (!file.read(reinterpret_cast<char *>(data.data()), size)) {
		std::cerr << "Failed to read cache file: " << path << std::endl;
		return {};
	}

	return data;
}

inline shaderc_shader_kind getShaderKind(const std::string &filePath) {
	path p(filePath);
	std::string extension = p.extension().string();
	if (shaderExtensions.contains(extension)) {
		return shaderExtensions[extension];
	} else {
		std::cerr << "Unsupported shader extension: " << extension << std::endl;
		exit(1);
	}
}

inline void writeBinaryFile(const std::string &path, const std::vector<uint32_t> &data) {
	std::ofstream file(path, std::ios::binary);
	if (file.is_open()) {
		file.write(reinterpret_cast<const char *>(data.data()), data.size() * sizeof(uint32_t));
		file.close();
	} else {
		std::cerr << "Failed to write cache file: " << path << std::endl;
	}
}

inline std::vector<uint32_t> compileShader(const std::string &shaderPath) {
	std::string shaderCode = readFile(shaderPath);
	if (shaderCode.empty()) {
		return {};
	}

	path p(shaderPath);
	std::string extension = p.extension().string();

	std::string hash_input = extension + shaderCode;
	std::string hash_str = computeHash(hash_input);

	path cache_dir(shaderCachePath);
	path cached_path = cache_dir / (hash_str + ".spv");

	if (exists(cached_path)) {
		auto cached_binary = readBinaryFile(cached_path.string());
		if (!cached_binary.empty()) {
			return cached_binary;
		}
	}

	shaderc_shader_kind kind = getShaderKind(shaderPath);

	SpvCompilationResult result = compiler.CompileGlslToSpv(shaderCode, kind, shaderPath.c_str(), options);
	if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
		std::cerr << "Failed to compile shader: " << shaderPath << "\nError: " << result.GetErrorMessage() << std::endl;
		return {};
	}

	std::vector<uint32_t> spirv(result.cbegin(), result.cend());
	writeBinaryFile(cached_path.string(), spirv);

	return spirv;
}

inline ShaderBinaries compileShader(const std::vector<std::string> &shaderPaths) {
	ShaderBinaries binaries;
	for (const auto &p : shaderPaths) {
		shaderc_shader_kind shader_kind = getShaderKind(p);
		switch (shader_kind) {
		case shaderc_glsl_vertex_shader:
			binaries.vertexShader = compileShader(p);
			break;
		case shaderc_glsl_tess_control_shader:
			binaries.tessellationControlShader = compileShader(p);
			break;
		case shaderc_glsl_tess_evaluation_shader:
			binaries.tessellationEvaluationShader = compileShader(p);
			break;
		case shaderc_glsl_geometry_shader:
			binaries.geometryShader = compileShader(p);
			break;
		case shaderc_glsl_fragment_shader:
			binaries.fragmentShader = compileShader(p);
			break;
		case shaderc_glsl_compute_shader:
			binaries.computeShader = compileShader(p);
			break;
		default:
			std::cerr << "Unsupported shader type: " << path(p).extension() << std::endl;
			exit(0);
		}
	}
	return binaries;
}

inline VkShaderModule createShaderModule(const std::vector<uint32_t> &binary) {
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = binary.size() * sizeof(uint32_t);
	createInfo.pCode = binary.data();

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shader module!");
	}

	return shaderModule;
}

inline ShaderModules compileShaderProgram(const std::string &shaderRootDir) {
	namespace fs = std::filesystem;

	ShaderModules modules;
	std::vector<std::string> shader_paths;

	try {
		fs::directory_iterator dir_iter(shaderRootDir);

		for (const auto &entry : dir_iter) {
			if (entry.is_regular_file()) {
				const std::string ext = entry.path().extension().string();
				if (shaderExtensions.contains(ext)) {
					shader_paths.push_back(entry.path().string());
				}
			}
		}
	} catch (const fs::filesystem_error &e) {
		std::cerr << "Failed to read shader directory: " << e.what() << "\n";
		exit(1);
	}

	if (shader_paths.empty()) {
		std::cerr << "No shader files found in directory: " << shaderRootDir << "\n";
		exit(1);
	}

	// Compile all shaders to SPIR-V binaries
	ShaderBinaries binaries = compileShader(shader_paths);

	// Create Vulkan shader modules from binaries
	auto createModule = [&](const auto &binary, VkShaderModule &module) {
		if (!binary.empty()) {
			module = createShaderModule(binary);
		}
	};

	createModule(binaries.vertexShader, modules.vertexShader);
	createModule(binaries.tessellationControlShader, modules.tessellationControlShader);
	createModule(binaries.tessellationEvaluationShader, modules.tessellationEvaluationShader);
	createModule(binaries.geometryShader, modules.geometryShader);
	createModule(binaries.fragmentShader, modules.fragmentShader);
	createModule(binaries.computeShader, modules.computeShader);

	return modules;
}

inline VkPipelineShaderStageCreateInfo createShaderStageInfo(VkShaderModule &shaderModule, VkShaderStageFlagBits stage) {
	VkPipelineShaderStageCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	info.stage = stage;
	info.module = shaderModule;
	info.pName = "main";
	return info;
}

inline uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable memory type!");
}

inline void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer &buffer, VkDeviceMemory &bufferMemory) {
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to create buffer!");
	}

	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memoryRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate buffer memory!");
	}

	vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

inline VkCommandBuffer beginSingleTimeCommands() {
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

inline void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(graphicsQueue);

	vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

inline void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
	VkCommandBuffer commandBuffer = beginSingleTimeCommands();

	VkBufferCopy copyRegion{};
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	endSingleTimeCommands(commandBuffer);
}

inline void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage &image, VkDeviceMemory &imageMemory) {
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
		throw std::runtime_error("failed to create image!");
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device, image, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate image memory!");
	}

	vkBindImageMemory(device, image, imageMemory, 0);
}

inline VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = aspectFlags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	VkImageView imageView;
	if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
		throw std::runtime_error("failed to create image view!");
	}

	return imageView;
}

inline void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
	VkCommandBuffer commandBuffer = Engine::beginSingleTimeCommands();

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	} else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	} else {
		throw std::invalid_argument("unsupported layout transition!");
	}

	vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	Engine::endSingleTimeCommands(commandBuffer);
}

inline void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
	VkCommandBuffer commandBuffer = Engine::beginSingleTimeCommands();

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = {0, 0, 0};
	region.imageExtent = {width, height, 1};

	vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	Engine::endSingleTimeCommands(commandBuffer);
}

}; // namespace Engine
