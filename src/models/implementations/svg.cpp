#include "svg.hpp"
#include "debug.hpp"
#include "engine.hpp"
#include "memory.hpp"
#include "scenes.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>

#include <lunasvg.h>

using namespace lunasvg;

namespace {
constexpr uint32_t kTexArraySize = 1024;

// helper: lowercase file extension
std::string getExtLower(const std::string &p) {
	auto dot = p.find_last_of('.');
	if (dot == std::string::npos)
		return {};
	std::string ext = p.substr(dot + 1);
	for (auto &c : ext)
		c = (char)std::tolower((unsigned char)c);
	return ext;
}

// static inline glm::vec2 pixelCropScale(uint32_t texW, uint32_t texH, float viewW, float viewH) {
// 	auto s = glm::vec2((texW > 0 ? viewW / float(texW) : 1.0f), (texH > 0 ? viewH / float(texH) : 1.0f));
// 	return glm::clamp(s, glm::vec2(0.0f), glm::vec2(1.0f));
// }
// static inline glm::vec2 pixelCropOffset(const glm::vec2 &scale) { return (glm::vec2(1.0f) - scale) * 0.5f; }

static SVG::CpuPixels cropAlphaTight(const SVG::CpuPixels &in, uint8_t alphaThreshold, int pad) {
	const int w = in.w, h = in.h;
	if (w <= 0 || h <= 0)
		return in;

	int minx = w, miny = h, maxx = -1, maxy = -1;

	// Find bounds where alpha > threshold
	for (int y = 0; y < h; ++y) {
		const uint8_t *row = in.rgba.data() + size_t(y) * size_t(w) * 4;
		for (int x = 0; x < w; ++x) {
			uint8_t a = row[x * 4 + 3];
			if (a > alphaThreshold) {
				minx = std::min(minx, x);
				miny = std::min(miny, y);
				maxx = std::max(maxx, x);
				maxy = std::max(maxy, y);
			}
		}
	}

	// If fully transparent, return as-is
	if (maxx < minx || maxy < miny)
		return in;

	// Add padding
	minx = std::max(0, minx - pad);
	miny = std::max(0, miny - pad);
	maxx = std::min(w - 1, maxx + pad);
	maxy = std::min(h - 1, maxy + pad);

	const int cw = (maxx - minx + 1);
	const int ch = (maxy - miny + 1);

	SVG::CpuPixels out;
	out.w = cw;
	out.h = ch;
	out.comp = 4;
	out.rgba.resize(size_t(cw) * size_t(ch) * 4);

	for (int y = 0; y < ch; ++y) {
		const uint8_t *src = in.rgba.data() + size_t(miny + y) * size_t(w) * 4 + size_t(minx) * 4;
		uint8_t *dst = out.rgba.data() + size_t(y) * size_t(cw) * 4;
		std::memcpy(dst, src, size_t(cw) * 4);
	}

	return out;
}

} // namespace

SVG::SVG(Scene *scene) : Model(scene) {}
SVG::~SVG() { destroyAllTextures(); }

// ---------------- Descriptor maintenance ----------------

void SVG::writeSet1Descriptors() {
	if (!pipeline)
		return;
	if (pipeline->descriptorSets.descriptorSets.size() <= 1)
		return;
	VkDescriptorSet set1 = pipeline->descriptorSets.descriptorSets[1];
	if (set1 == VK_NULL_HANDLE)
		return;
	if (imageInfos.empty())
		return;

	// Avoid VUID about updating in-use sets: stall (simple, safe)
	vkDeviceWaitIdle(pipeline->device);

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

void SVG::ensureSet1Ready() {
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

// ---------------- Init ----------------

void SVG::init() {
	engine = scene->getScenes().getEngine();

	buildUnitQuadMesh();
	initInfo.instanceStrideBytes = sizeof(InstanceData);

	// reuse image shaders, or point to "svg" if you duplicate them
	initInfo.shaders = Assets::compileShaderProgram(Assets::shaderRootPath + "/svg", engine->getDevice());

	// ensure we have at least one white tex so set=1 is valid
	loadAllFramesCPU(framesPerInstance);

	pipeline->graphicsPipeline.pushConstantRangeCount = 1;
	pipeline->graphicsPipeline.pushContantRanges.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	pipeline->graphicsPipeline.pushContantRanges.offset = 0;
	pipeline->graphicsPipeline.pushContantRanges.size = sizeof(uint32_t) * 8;

	Model::init();

	uploadAllFramesGPU();
	ensureSet1Ready();
}

void SVG::syncPickingInstances() { Model::syncPickingInstances<InstanceData>(); }

// ---------------- Public API ----------------

void SVG::upsert(int id, const string &path) {
	framesPerInstance[id] = std::vector<std::string>{path};

	if (!instanceModel.count(id))
		instanceModel[id] = glm::mat4(1.0f);
	instanceLocalFrame[id] = 0u;

	rebuildTexturePool();
	ensureSet1Ready();
}

void SVG::upsert(int id, const vector<string> &paths) {
	framesPerInstance[id] = paths;

	if (!instanceModel.count(id))
		instanceModel[id] = glm::mat4(1.0f);
	instanceLocalFrame[id] = 0u;

	rebuildTexturePool();
	ensureSet1Ready();
}

void SVG::setFrame(int id, uint32_t frameIndex) {
	if (!framesPerInstance.count(id))
		return;

	uint32_t count = instanceFrameCount.count(id) ? instanceFrameCount[id] : 0u;
	if (count == 0)
		return;

	instanceLocalFrame[id] = std::min(frameIndex, count - 1);

	uint32_t base = instanceFirstIndex.count(id) ? instanceFirstIndex[id] : 0u;
	const uint32_t cap = std::max(1u, std::min<uint32_t>((uint32_t)imageInfos.size(), kTexArraySize));
	const uint32_t global = std::min(base + instanceLocalFrame[id], (cap ? cap : 1u) - 1u);

	InstanceData data{};
	data.model = instanceModel.count(id) ? instanceModel[id] : glm::mat4(1.0f);
	data.frameIndex = global;
	data.uvScale = glm::vec2(1.0f);
	data.uvOffset = glm::vec2(0.0f);

	upsertInternal(id, data);
	ensureSet1Ready();
}

void SVG::erase(int id) {
	framesPerInstance.erase(id);
	instanceModel.erase(id);
	instanceLocalFrame.erase(id);
	instanceFirstIndex.erase(id);
	instanceFrameCount.erase(id);

	Model::erase(id);
	rebuildTexturePool();
	ensureSet1Ready();
}

// ---------------- Internal ----------------

void SVG::upsertInternal(int id, const InstanceData &data) {
	std::span<const uint8_t> bytes(reinterpret_cast<const uint8_t *>(&data), sizeof(InstanceData));
	upsertBytes(id, bytes);
	if (set1Dirty)
		ensureSet1Ready();
}

void SVG::recalcUV() {
	for (const auto &kv : framesPerInstance) {
		const int id = kv.first;

		uint32_t local = instanceLocalFrame.count(id) ? instanceLocalFrame[id] : 0u;
		uint32_t base = instanceFirstIndex.count(id) ? instanceFirstIndex[id] : 0u;
		const uint32_t cap = std::max(1u, std::min<uint32_t>((uint32_t)imageInfos.size(), kTexArraySize));
		const uint32_t global = std::min(base + local, (cap ? cap : 1u) - 1u);

		glm::vec2 uvScale(1.0f), uvOffset(0.0f);
		// if (global < gpuTextures.size()) {
		// 	const uint32_t tw = std::max(1u, gpuTextures[global].w);
		// 	const uint32_t th = std::max(1u, gpuTextures[global].h);

		// 	const float viewW = (viewport.width > 0.0f) ? viewport.width : 1.0f;
		// 	const float viewH = (viewport.height > 0.0f) ? viewport.height : 1.0f;

		// 	uvScale = pixelCropScale(tw, th, viewW, viewH);
		// 	uvOffset = pixelCropOffset(uvScale);
		// }

		InstanceData data{};
		data.model = instanceModel.count(id) ? instanceModel[id] : glm::mat4(1.0f);
		data.frameIndex = global;
		data.uvScale = uvScale;
		data.uvOffset = uvOffset;

		upsertInternal(id, data);
	}
	ensureSet1Ready();
}

// ---------------- Mesh & pipeline ----------------

void SVG::buildUnitQuadMesh() {
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
		{7, 1, F::VK_FORMAT_R32G32B32A32_SFLOAT, uint32_t(offsetof(InstanceData, color))},
		{8, 1, F::VK_FORMAT_R32G32_SFLOAT, uint32_t(offsetof(InstanceData, uvScale))},
		{9, 1, F::VK_FORMAT_R32G32_SFLOAT, uint32_t(offsetof(InstanceData, uvOffset))},
	};

	initInfo.mesh = m;
}

glm::vec2 SVG::getPixelDimensions(int /*idx*/, int texIdx) {
	if (texIdx < 0 || static_cast<size_t>(texIdx) >= gpuTextures.size())
		return glm::vec2(1.0f, 1.0f);

	const auto &t = gpuTextures[static_cast<size_t>(texIdx)];
	return glm::vec2(static_cast<float>(t.w), static_cast<float>(t.h));
}

uint32_t SVG::createDescriptorPool() {
	uint32_t baseSets = Model::createDescriptorPool();
	pipeline->descriptorPoolSizes.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kTexArraySize});
	return std::max(baseSets, 2u);
}

void SVG::createDescriptors() {
	pipeline->createDescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, kTexArraySize, 1);

	Model::createDescriptors();
	ensureSet1Ready();
}

void SVG::createGraphicsPipeline() {
	Model::createGraphicsPipeline();

	pipeline->graphicsPipeline.rasterizationStateCI.cullMode = VK_CULL_MODE_NONE;
	pipeline->graphicsPipeline.depthStencilStateCI.depthTestEnable = VK_FALSE;
	pipeline->graphicsPipeline.depthStencilStateCI.depthWriteEnable = VK_FALSE;

	if (set1Dirty)
		ensureSet1Ready();
}

// ---------------- Rebuild on frames change ----------------

void SVG::rebuildTexturePool() {
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
		// if (global < gpuTextures.size()) {
		// 	const uint32_t tw = std::max(1u, gpuTextures[global].w);
		// 	const uint32_t th = std::max(1u, gpuTextures[global].h);

		// 	const float viewW = (viewport.width > 0.0f) ? viewport.width : 1.0f;
		// 	const float viewH = (viewport.height > 0.0f) ? viewport.height : 1.0f;

		// 	uvScale = pixelCropScale(tw, th, viewW, viewH);
		// 	uvOffset = pixelCropOffset(uvScale);
		// }

		InstanceData data{};
		data.model = instanceModel[id];
		data.frameIndex = global;
		data.uvScale = uvScale;
		data.uvOffset = uvOffset;

		upsertInternal(id, data);
	}

	ensureSet1Ready();
}

// ---------------- Frame loading (SVG → CPU) ----------------
void SVG::loadAllFramesCPU(const std::map<int, std::vector<std::string>> &list) {
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
			CpuPixels cp{};
			bool ok = false;

			std::string ext = getExtLower(p);
			if (ext == "svg") {
				// ---- LunaSVG path ----
				auto document = lunasvg::Document::loadFromFile(p);
				if (document) {
					auto bitmap = document->renderToBitmap(256, 256);
					if (!bitmap.isNull()) {
						const int w = bitmap.width();
						const int h = bitmap.height();
						const unsigned char *rgba = bitmap.data(); // RGBA

						cp.w = w;
						cp.h = h;
						cp.comp = 4;
						cp.rgba.resize(static_cast<size_t>(w) * static_cast<size_t>(h) * 4);

						std::memcpy(cp.rgba.data(), rgba, cp.rgba.size());

                        cp = cropAlphaTight(cp, /*alphaThreshold=*/1, /*pad=*/1);
						ok = true;
					}
				}
			}

			if (!ok) {
				// fallback: 1x1 magenta (same as before)
				cp.w = cp.h = 1;
				cp.comp = 4;
				cp.rgba = {255, 0, 255, 255};
			}

			cpuFrames.push_back(std::move(cp));
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

// ---------------- Vulkan upload ----------------

void SVG::transition(VkImage img, VkImageLayout oldL, VkImageLayout newL, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, VkAccessFlags srcAccess, VkAccessFlags dstAccess) {
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

void SVG::copyBufferToImage(VkBuffer staging, VkImage img, uint32_t w, uint32_t h) {
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

void SVG::uploadAllFramesGPU() {
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

	std::vector<GpuTex> newTextures(cpuFrames.size());
	std::vector<VkDescriptorImageInfo> newInfos;
	newInfos.reserve(cpuFrames.size());

	for (size_t i = 0; i < cpuFrames.size(); ++i) {
		const auto &cp = cpuFrames[i];

		VkImageCreateInfo ici{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
		ici.imageType = VK_IMAGE_TYPE_2D;
		ici.format = VK_FORMAT_B8G8R8A8_SRGB;
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
		vci.format = VK_FORMAT_B8G8R8A8_SRGB;
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

	vkDeviceWaitIdle(dev);
	for (auto &t : gpuTextures)
		destroyTex(t);
	gpuTextures = std::move(newTextures);
	imageInfos = std::move(newInfos);

	ensureSet1Ready();
}

void SVG::destroyAllTextures() {
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

void SVG::record(VkCommandBuffer cmd) {
	uvec4 fbPx{(uint32_t)fbw, (uint32_t)fbh, 0, 0};
	uvec4 vpPx{(uint32_t)viewport.x, (uint32_t)viewport.y, (uint32_t)viewport.width, (uint32_t)viewport.height};
	struct {
		uvec4 fb;
		uvec4 vp;
	} pc = {fbPx, vpPx};

	vkCmdPushConstants(cmd, pipeline->pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
	Model::record(cmd);
}
