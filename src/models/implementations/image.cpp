#include "image.hpp"
#include "debug.hpp"
#include "engine.hpp"
#include "memory.hpp"
#include "scenes.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.hpp>

#include <algorithm>
#include <cstring>

namespace {
constexpr uint32_t kTexArraySize = 1024; // fixed-sized descriptor array
} // namespace

// --------------------------------------------------
// Helpers
// --------------------------------------------------

static inline uint32_t atLeast1(uint32_t v) { return v ? v : 1u; }

// Remove (or stop using) coverScale/coverOffset and add:
static inline glm::vec2 pixelCropScale(uint32_t texW, uint32_t texH, float viewW, float viewH) {
	// fraction of the texture we want to sample in each axis
	// clamp to [0,1] (assumption: tex >= view)
	auto s = glm::vec2((texW > 0 ? viewW / float(texW) : 1.0f), (texH > 0 ? viewH / float(texH) : 1.0f));
	return glm::clamp(s, glm::vec2(0.0f), glm::vec2(1.0f));
}
static inline glm::vec2 pixelCropOffset(const glm::vec2 &scale) {
	// center the crop window
	return (glm::vec2(1.0f) - scale) * 0.5f;
}

Image::Image(Scene *scene) : Model(scene) {}
Image::~Image() { destroyAllTextures(); }

// --------------------------------------------------
// Private: descriptor maintenance
// --------------------------------------------------

void Image::writeSet1Descriptors() {
	if (!pipeline)
		return;
	if (pipeline->descriptorSets.descriptorSets.size() <= 1)
		return;
	VkDescriptorSet set1 = pipeline->descriptorSets.descriptorSets[1];
	if (set1 == VK_NULL_HANDLE)
		return;
	if (imageInfos.empty())
		return;

	// ***** IMPORTANT: avoid VUID-03047 by ensuring the set isn't in use *****
	// This is the simplest safe option without changing your frame graph:
	// stall before rewriting descriptors bound by in-flight command buffers.
	vkDeviceWaitIdle(pipeline->device);

	// pad to full array so any index is valid
	std::vector<VkDescriptorImageInfo> padded = imageInfos;
	while (padded.size() < kTexArraySize)
		padded.push_back(padded[0]);

	VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	w.dstSet = set1;
	w.dstBinding = 0;
	w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	w.descriptorCount = static_cast<uint32_t>(padded.size());
	w.pImageInfo = padded.data();

	vkUpdateDescriptorSets(pipeline->device, 1, &w, 0, nullptr);
	set1Dirty = false;
}

void Image::ensureSet1Ready() {
	// If we can write now, do it; otherwise mark as dirty and a later call will succeed.
	if (!pipeline || pipeline->descriptorSets.descriptorSets.size() <= 1) {
		set1Dirty = true;
		return;
	}
	if (pipeline->descriptorSets.descriptorSets[1] == VK_NULL_HANDLE) {
		set1Dirty = true;
		return;
	}
	if (imageInfos.empty()) {
		set1Dirty = true;
		return;
	}
	writeSet1Descriptors();
}

// --------------------------------------------------
// Init
// --------------------------------------------------

void Image::init() {
	engine = scene->getScenes().getEngine();

	buildUnitQuadMesh();
	initInfo.instanceStrideBytes = sizeof(InstanceData);

	initInfo.shaders = Assets::compileShaderProgram(Assets::shaderRootPath + "/image", engine->getDevice());

	// Ensure we have at least one white tex so set=1 can be valid
	loadAllFramesCPU(framesPerInstance);

	pipeline->graphicsPipeline.pushConstantRangeCount = 1;
	pipeline->graphicsPipeline.pushContantRanges.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	pipeline->graphicsPipeline.pushContantRanges.offset = 0;
	pipeline->graphicsPipeline.pushContantRanges.size = sizeof(uint32_t) * 8;

	// Base pipeline/buffers/descriptors (set=0 created here; Image::create* will add set=1)
	Model::init();

	// Upload textures and (try to) write set=1
	uploadAllFramesGPU();
	ensureSet1Ready();
}

void Image::syncPickingInstances() { Model::syncPickingInstances<InstanceData>(); }

// --------------------------------------------------
// Public API: dynamic updates
// --------------------------------------------------

void Image::upsert(int id, const string &path) {
	framesPerInstance[id] = std::vector<std::string>{path};

	if (!instanceModel.count(id))
		instanceModel[id] = glm::mat4(1.0f);
	instanceLocalFrame[id] = 0u;

	rebuildTexturePool(); // content change
	ensureSet1Ready();	  // descriptor may need rewrite after swapchain events
}

void Image::upsert(int id, const vector<string> &paths) {
	framesPerInstance[id] = paths;

	if (!instanceModel.count(id))
		instanceModel[id] = glm::mat4(1.0f);
	instanceLocalFrame[id] = 0u;

	rebuildTexturePool();
	ensureSet1Ready();
}

void Image::setFrame(int id, uint32_t frameIndex) {
	if (!framesPerInstance.count(id))
		return;

	uint32_t count = instanceFrameCount.count(id) ? instanceFrameCount[id] : 0u;
	if (count == 0)
		return;

	instanceLocalFrame[id] = std::min(frameIndex, count - 1);

	uint32_t base = instanceFirstIndex.count(id) ? instanceFirstIndex[id] : 0u;
	const uint32_t cap = std::max(1u, std::min<uint32_t>((uint32_t)imageInfos.size(), kTexArraySize));
	const uint32_t global = std::min(base + instanceLocalFrame[id], (cap ? cap : 1u) - 1u);

	glm::vec2 uvScale(1.0f), uvOffset(0.0f);
	if (global < gpuTextures.size()) {
		const uint32_t tw = std::max(1u, gpuTextures[global].w);
		const uint32_t th = std::max(1u, gpuTextures[global].h);

		// viewport.width, viewport.height are your current viewport dimensions in pixels (already on the class)
		const float viewW = (viewport.width > 0.0f) ? viewport.width : 1.0f;
		const float viewH = (viewport.height > 0.0f) ? viewport.height : 1.0f;

		uvScale = pixelCropScale(tw, th, viewW, viewH); // size of the crop window in UVs
		uvOffset = pixelCropOffset(uvScale);			// centered
	}

	InstanceData data{};
	data.model = instanceModel.count(id) ? instanceModel[id] : glm::mat4(1.0f);
	data.frameIndex = global;
	data.uvScale = uvScale;
	data.uvOffset = uvOffset;

	upsertInternal(id, data);
	ensureSet1Ready();
}

void Image::erase(int id) {
	framesPerInstance.erase(id);
	instanceModel.erase(id);
	instanceLocalFrame.erase(id);
	instanceFirstIndex.erase(id);
	instanceFrameCount.erase(id);

	Model::erase(id);
	rebuildTexturePool();
	ensureSet1Ready();
}

// --------------------------------------------------
// Internal: keep Model state up to date
// --------------------------------------------------

void Image::upsertInternal(int id, const InstanceData &data) {
	std::span<const uint8_t> bytes(reinterpret_cast<const uint8_t *>(&data), sizeof(InstanceData));
	upsertBytes(id, bytes);
	if (set1Dirty)
		ensureSet1Ready();
}

void Image::recalcUV() {
	for (const auto &kv : framesPerInstance) {
		const int id = kv.first;

		uint32_t local = instanceLocalFrame.count(id) ? instanceLocalFrame[id] : 0u;
		uint32_t base = instanceFirstIndex.count(id) ? instanceFirstIndex[id] : 0u;
		const uint32_t cap = std::max(1u, std::min<uint32_t>((uint32_t)imageInfos.size(), kTexArraySize));
		const uint32_t global = std::min(base + local, (cap ? cap : 1u) - 1u);

		glm::vec2 uvScale(1.0f), uvOffset(0.0f);
		if (global < gpuTextures.size()) {
			const uint32_t tw = std::max(1u, gpuTextures[global].w);
			const uint32_t th = std::max(1u, gpuTextures[global].h);

			// viewport.width, viewport.height are your current viewport dimensions in pixels (already on the class)
			const float viewW = (viewport.width > 0.0f) ? viewport.width : 1.0f;
			const float viewH = (viewport.height > 0.0f) ? viewport.height : 1.0f;

			uvScale = pixelCropScale(tw, th, viewW, viewH); // size of the crop window in UVs
			uvOffset = pixelCropOffset(uvScale);			// centered
		}

		InstanceData data{};
		data.model = instanceModel.count(id) ? instanceModel[id] : glm::mat4(1.0f);
		data.frameIndex = global;
		data.uvScale = uvScale;
		data.uvOffset = uvOffset;

		upsertInternal(id, data);
	}
	ensureSet1Ready();
}

// --------------------------------------------------
// Mesh & pipeline
// --------------------------------------------------

void Image::buildUnitQuadMesh() {
	static const Vertex kVerts[4] = {
		{{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f}},
		{{0.5f, -0.5f, 0.0f}, {1.0f, 0.0f}},
		{{0.5f, 0.5f, 0.0f}, {1.0f, 1.0f}},
		{{-0.5f, 0.5f, 0.0f}, {0.0f, 1.0f}},
	};
	static const uint32_t kIdx[6] = {0, 1, 2, 0, 2, 3};

	Model::Mesh m{};
	m.vsrc.data = kVerts;
	m.vsrc.bytes = sizeof(kVerts);
	m.vsrc.stride = sizeof(Vertex);

	m.isrc.data = kIdx;
	m.isrc.count = 6;

	using F = VkFormat;
	m.vertexAttrs = {
		{0, 0, F::VK_FORMAT_R32G32B32_SFLOAT, uint32_t(offsetof(Vertex, pos))},
		{1, 0, F::VK_FORMAT_R32G32_SFLOAT, uint32_t(offsetof(Vertex, uv))},

		{2, 1, F::VK_FORMAT_R32G32B32A32_SFLOAT, uint32_t(offsetof(InstanceData, model) + sizeof(glm::vec4) * 0)},
		{3, 1, F::VK_FORMAT_R32G32B32A32_SFLOAT, uint32_t(offsetof(InstanceData, model) + sizeof(glm::vec4) * 1)},
		{4, 1, F::VK_FORMAT_R32G32B32A32_SFLOAT, uint32_t(offsetof(InstanceData, model) + sizeof(glm::vec4) * 2)},
		{5, 1, F::VK_FORMAT_R32G32B32A32_SFLOAT, uint32_t(offsetof(InstanceData, model) + sizeof(glm::vec4) * 3)},
		{6, 1, F::VK_FORMAT_R32_UINT, uint32_t(offsetof(InstanceData, frameIndex))},
		{7, 1, F::VK_FORMAT_R32_UINT, uint32_t(offsetof(InstanceData, cover))},
		{8, 1, F::VK_FORMAT_R32G32_SFLOAT, uint32_t(offsetof(InstanceData, uvScale))},
		{9, 1, F::VK_FORMAT_R32G32_SFLOAT, uint32_t(offsetof(InstanceData, uvOffset))},
	};

	initInfo.mesh = m;
}

glm::vec2 Image::getPixelDimensions(int /*idx*/, int texIdx) {
	if (texIdx < 0 || static_cast<size_t>(texIdx) >= gpuTextures.size())
		return glm::vec2(1.0f, 1.0f);

	const auto &t = gpuTextures[static_cast<size_t>(texIdx)];
	return glm::vec2(static_cast<float>(t.w), static_cast<float>(t.h));
}

uint32_t Image::createDescriptorPool() {
	uint32_t baseSets = Model::createDescriptorPool();

	// capacity for set=1
	pipeline->descriptorPoolSizes.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kTexArraySize});

	return std::max(baseSets, 2u);
}

void Image::createDescriptors() {
	// Declare set=1 layout FIRST so base allocates it.
	pipeline->createDescriptorSetLayoutBinding(
		/*binding*/ 0,
		/*type*/ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		/*stages*/ VK_SHADER_STAGE_FRAGMENT_BIT,
		/*count*/ kTexArraySize,
		/*set*/ 1);

	// Allocate pool & sets (set=0 and set=1)
	Model::createDescriptors();

	// If a resize triggered reallocation, satisfy any pending write now.
	ensureSet1Ready();
}

void Image::createGraphicsPipeline() {
	// Pipeline can be recreated on resize; after that, set=1 may be reallocated.
	Model::createGraphicsPipeline();

	pipeline->graphicsPipeline.rasterizationStateCI.cullMode = VK_CULL_MODE_NONE;

    pipeline->graphicsPipeline.depthStencilStateCI.depthTestEnable = VK_FALSE;
    pipeline->graphicsPipeline.depthStencilStateCI.depthWriteEnable = VK_FALSE;

	if (set1Dirty)
		ensureSet1Ready();
}

// --------------------------------------------------
// Rebuild pipeline resources when frames change
// --------------------------------------------------

void Image::rebuildTexturePool() {
	loadAllFramesCPU(framesPerInstance);
	uploadAllFramesGPU();

	for (const auto &kv : framesPerInstance) {
		const int id = kv.first;

		if (!instanceModel.count(id))
			instanceModel[id] = glm::mat4(1.0f);
		uint32_t local = instanceLocalFrame.count(id) ? instanceLocalFrame[id] : 0u;

		uint32_t base = instanceFirstIndex.count(id) ? instanceFirstIndex[id] : 0u;
		uint32_t count = instanceFrameCount.count(id) ? instanceFrameCount[id] : 0u;
		if (count == 0) {
			local = 0;
		} else {
			local = std::min(local, count - 1);
		}

		const uint32_t cap = std::max(1u, std::min<uint32_t>((uint32_t)imageInfos.size(), kTexArraySize));
		const uint32_t global = std::min(base + local, (cap ? cap : 1u) - 1u);

		glm::vec2 uvScale(1.0f), uvOffset(0.0f);
		if (global < gpuTextures.size()) {
			const uint32_t tw = std::max(1u, gpuTextures[global].w);
			const uint32_t th = std::max(1u, gpuTextures[global].h);

			// viewport.width, viewport.height are your current viewport dimensions in pixels (already on the class)
			const float viewW = (viewport.width > 0.0f) ? viewport.width : 1.0f;
			const float viewH = (viewport.height > 0.0f) ? viewport.height : 1.0f;

			uvScale = pixelCropScale(tw, th, viewW, viewH); // size of the crop window in UVs
			uvOffset = pixelCropOffset(uvScale);			// centered
		}

		InstanceData data{};
		data.model = instanceModel[id];
		data.frameIndex = global;
		data.uvScale = uvScale;
		data.uvOffset = uvOffset;

		upsertInternal(id, data);
	}

	ensureSet1Ready();
}

// --------------------------------------------------
// Frame loading (CPU) & upload (GPU)
// --------------------------------------------------

void Image::loadAllFramesCPU(const std::map<int, std::vector<std::string>> &list) {
	cpuFrames.clear();
	instanceFirstIndex.clear();
	instanceFrameCount.clear();

	uint32_t cursor = 0;
	for (const auto &kv : list) {
		const int instanceId = kv.first;
		const auto &paths = kv.second;

		instanceFirstIndex[instanceId] = cursor;
		instanceFrameCount[instanceId] = static_cast<uint32_t>(paths.size());

		if (paths.empty()) {
			// 1×1 white
			CpuPixels cp;
			cp.w = cp.h = 1;
			cp.comp = 4;
			cp.rgba = {255, 255, 255, 255};
			cpuFrames.push_back(std::move(cp));
			cursor++;
			instanceFrameCount[instanceId] = 1u;
			continue;
		}

		for (const auto &p : paths) {
			int w = 0, h = 0, comp = 0;
			stbi_uc *px = stbi_load(p.c_str(), &w, &h, &comp, STBI_rgb_alpha);
			if (!px || w <= 0 || h <= 0) {
				// 1×1 magenta on failure
				CpuPixels cp;
				cp.w = cp.h = 1;
				cp.comp = 4;
				cp.rgba = {255, 0, 255, 255};
				cpuFrames.push_back(std::move(cp));
			} else {
				CpuPixels cp;
				cp.w = w;
				cp.h = h;
				cp.comp = 4;
				cp.rgba.resize(static_cast<size_t>(w * h * 4));
				std::memcpy(cp.rgba.data(), px, cp.rgba.size());
				stbi_image_free(px);
				cpuFrames.push_back(std::move(cp));
			}
			cursor++;
		}
	}

	if (cpuFrames.empty()) {
		CpuPixels cp;
		cp.w = cp.h = 1;
		cp.comp = 4;
		cp.rgba = {255, 255, 255, 255};
		cpuFrames.push_back(std::move(cp));
	}
}

void Image::transition(VkImage img, VkImageLayout oldL, VkImageLayout newL, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, VkAccessFlags srcAccess, VkAccessFlags dstAccess) {
	VkCommandBuffer cmd = engine->getLogicalDevice().beginSingleUseCmd();

	VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
	b.oldLayout = oldL;
	b.newLayout = newL;
	b.srcAccessMask = srcAccess;
	b.dstAccessMask = dstAccess;
	b.image = img;
	b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	b.subresourceRange.baseMipLevel = 0;
	b.subresourceRange.levelCount = 1;
	b.subresourceRange.baseArrayLayer = 0;
	b.subresourceRange.layerCount = 1;

	vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &b);
	engine->getLogicalDevice().endSingleUseCmdGraphics(cmd);
}

void Image::copyBufferToImage(VkBuffer staging, VkImage img, uint32_t w, uint32_t h) {
	VkCommandBuffer cmd = engine->getLogicalDevice().beginSingleUseCmd();

	VkBufferImageCopy reg{};
	reg.bufferOffset = 0;
	reg.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	reg.imageSubresource.mipLevel = 0;
	reg.imageSubresource.baseArrayLayer = 0;
	reg.imageSubresource.layerCount = 1;
	reg.imageExtent = {w, h, 1};

	vkCmdCopyBufferToImage(cmd, staging, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &reg);
	engine->getLogicalDevice().endSingleUseCmdGraphics(cmd);
}

void Image::uploadAllFramesGPU() {
	const auto &dev = engine->getDevice();
	const auto &pdev = engine->getPhysicalDevice();

	auto destroyTex = [&](GpuTex &t) {
		if (t.sampler) {
			vkDestroySampler(dev, t.sampler, nullptr);
			t.sampler = VK_NULL_HANDLE;
		}
		if (t.view) {
			vkDestroyImageView(dev, t.view, nullptr);
			t.view = VK_NULL_HANDLE;
		}
		if (t.image) {
			vkDestroyImage(dev, t.image, nullptr);
			t.image = VK_NULL_HANDLE;
		}
		if (t.memory) {
			vkFreeMemory(dev, t.memory, nullptr);
			t.memory = VK_NULL_HANDLE;
		}
	};

	// Build new textures into temporaries
	std::vector<GpuTex> newTextures(cpuFrames.size());
	std::vector<VkDescriptorImageInfo> newInfos;
	newInfos.reserve(cpuFrames.size());

	for (size_t i = 0; i < cpuFrames.size(); ++i) {
		const auto &cp = cpuFrames[i];

		VkImageCreateInfo ici{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
		ici.imageType = VK_IMAGE_TYPE_2D;
		ici.format = VK_FORMAT_R8G8B8A8_SRGB;
		ici.extent = {(uint32_t)cp.w, (uint32_t)cp.h, 1};
		ici.mipLevels = 1;
		ici.arrayLayers = 1;
		ici.samples = VK_SAMPLE_COUNT_1_BIT;
		ici.tiling = VK_IMAGE_TILING_OPTIMAL;
		ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VK_CHECK(vkCreateImage(dev, &ici, nullptr, &newTextures[i].image));

		VkMemoryRequirements req{};
		vkGetImageMemoryRequirements(dev, newTextures[i].image, &req);

		VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
		ai.allocationSize = req.size;
		ai.memoryTypeIndex = Memory::findMemoryType(pdev, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK(vkAllocateMemory(dev, &ai, nullptr, &newTextures[i].memory));
		VK_CHECK(vkBindImageMemory(dev, newTextures[i].image, newTextures[i].memory, 0));

		// staging upload
		VkBuffer staging{};
		VkDeviceMemory stagingMem{};
		VkDeviceSize bytes = static_cast<VkDeviceSize>(cp.rgba.size());
		pipeline->createBuffer(bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging, stagingMem);
		void *p = nullptr;
		VK_CHECK(vkMapMemory(dev, stagingMem, 0, VK_WHOLE_SIZE, 0, &p));
		std::memcpy(p, cp.rgba.data(), cp.rgba.size());
		vkUnmapMemory(dev, stagingMem);

		transition(newTextures[i].image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
		copyBufferToImage(staging, newTextures[i].image, (uint32_t)cp.w, (uint32_t)cp.h);
		transition(newTextures[i].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);

		vkDestroyBuffer(dev, staging, nullptr);
		vkFreeMemory(dev, stagingMem, nullptr);

		VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
		vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
		vci.image = newTextures[i].image;
		vci.format = VK_FORMAT_R8G8B8A8_SRGB;
		vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		vci.subresourceRange.levelCount = 1;
		vci.subresourceRange.layerCount = 1;
		VK_CHECK(vkCreateImageView(dev, &vci, nullptr, &newTextures[i].view));

		VkSamplerCreateInfo sci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
		sci.magFilter = VK_FILTER_NEAREST;
		sci.minFilter = VK_FILTER_NEAREST;
		sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sci.maxLod = 0.0f;
		VK_CHECK(vkCreateSampler(dev, &sci, nullptr, &newTextures[i].sampler));

		newTextures[i].w = (uint32_t)cp.w;
		newTextures[i].h = (uint32_t)cp.h;

		VkDescriptorImageInfo ii{};
		ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		ii.imageView = newTextures[i].view;
		ii.sampler = newTextures[i].sampler;
		newInfos.push_back(ii);
	}

	// swap them in (we'll write descriptors lazily below)
	vkDeviceWaitIdle(dev);
	for (auto &t : gpuTextures)
		destroyTex(t);
	gpuTextures = std::move(newTextures);
	imageInfos = std::move(newInfos);

	ensureSet1Ready();
}

void Image::destroyAllTextures() {
	const auto &dev = engine->getDevice();
	for (auto &t : gpuTextures) {
		if (t.sampler) {
			vkDestroySampler(dev, t.sampler, nullptr);
			t.sampler = VK_NULL_HANDLE;
		}
		if (t.view) {
			vkDestroyImageView(dev, t.view, nullptr);
			t.view = VK_NULL_HANDLE;
		}
		if (t.image) {
			vkDestroyImage(dev, t.image, nullptr);
			t.image = VK_NULL_HANDLE;
		}
		if (t.memory) {
			vkFreeMemory(dev, t.memory, nullptr);
			t.memory = VK_NULL_HANDLE;
		}
	}
	gpuTextures.clear();
	imageInfos.clear();
}

void Image::record(VkCommandBuffer cmd) {
	uvec4 fbPx{(uint32_t)fbw, (uint32_t)fbh, 0, 0};
	uvec4 vpPx{(uint32_t)viewport.x, (uint32_t)viewport.y, (uint32_t)viewport.width, (uint32_t)viewport.height};
	struct {
		uvec4 fb;
		uvec4 vp;
	} pc = {fbPx, vpPx};

	vkCmdPushConstants(cmd, pipeline->pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
	Model::record(cmd);
}
