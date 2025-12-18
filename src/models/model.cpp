#include "model.hpp"
#include "debug.hpp"
#include "engine.hpp"
#include "events.hpp"
#include "mouse.hpp"
#include "scene.hpp"
#include "scenes.hpp"
#include <GLFW/glfw3.h>
#include <cstring>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

Model::Model(Scene *scene) : scene(scene) {
	pipeline = std::make_unique<Pipeline>();
	Events::registerMouseClick([this](int button, int action, int mods) {
		if (action != Events::ACTION_RELEASE || button != Events::MOUSE_BUTTON_LEFT)
			return;

		const Scenes &scenes = this->scene->getScenes();
		Model *picked = scenes.getRayPicked();
		if (picked != this) {
			if (onMouseDeselect && selected_) {
				selected_ = false;
				onMouseDeselect(this);
			}
			return;
		}

		if (!selected_) {
			if (onMouseSelect) {
				selected_ = true;
				onMouseSelect(this);
			}
		}

		if (onMouseClick) {
			onMouseClick(this, picking ? picking->hitInfo.primId : 0u);
		}
	});
}

Model::~Model() { destroy(); }

void Model::swapChainUpdate(float vw, float vh, float fbw, float fbh) {
	setFrameBuffer(fbw, fbh);
	setViewport(vw, vh, 0.0f, 0.0f);
	if (onScreenResize) {
		onScreenResize(this, vw, vh, fbw, fbh);
	}
}

void Model::compute(VkCommandBuffer cmd) {
	if (picking) {
		if (pickingInstancesDirty)
			syncPickingInstances();

		int winW = 0, winH = 0;
		glfwGetWindowSize(engine->getWindow(), &winW, &winH);
		VkExtent2D ext = engine->getSwapchain().getExtent();

		float mx, my;
		Mouse::getPixel(mx, my);
		bool inside = false;
		vec2 ndc = Mouse::toNDC(mx, my, winW, winH, (int)ext.width, (int)ext.height, viewport.x, viewport.y, viewport.width, viewport.height, &inside);
		if (!inside) {
			picking->hitInfo.hit = 0u;
			pickingDispatched_ = false;

			const Scenes &scenes = scene->getScenes();
			if (scenes.getRayPicked() == this) {
				scenes.setRayPicked(nullptr);
			}

			return;
		}

		picking->updateUBO(vp.view, vp.proj, ndc);
		picking->record(cmd);
		pickingDispatched_ = true;
	}
}

void Model::tick(double dt, double t) {
	if (onTick) {
		onTick(this, dt, t);
	}
}

void Model::setView(const mat4 &V) {
	vp.view = V;
	uboDirty = true;
}

void Model::setProj(const mat4 &P) {
	vp.proj = P;
	uboDirty = true;
}

void Model::billboard(bool enable) {
	vp.billboard = enable;
	uboDirty = true;
}

void Model::setViewport(float w, float h, float x, float y) {
	viewport.x = x;
	viewport.y = y;
	viewport.width = w;
	viewport.height = h;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	scissor.offset = {(int32_t)x, (int32_t)y};
	scissor.extent = {static_cast<uint32_t>(viewport.width), static_cast<uint32_t>(viewport.height)};
}

void Model::setFrameBuffer(float w, float h) {
	fbw = w;
	fbh = h;
}

bool Model::mouseIsOver() {
	const Scenes &scenes = this->scene->getScenes();
	Model *picked = scenes.getRayPicked();
	return picked == this;
}

uint32_t Model::getPickedInstance() { return mouseIsOver() ? picking->hitInfo.primId : -1u; }

void Model::enableRayPicking() {
	if (!picking) {
		picking = std::make_unique<RayPicking>();
	}

	// Find position attribute (location 0, binding 0 is a common convention)
	const Model::VertexAttr *posAttr = nullptr; // <-- FIXED TYPE
	for (const auto &a : mesh.vertexAttrs) {
		if (a.location == 0 && a.binding == 0) {
			posAttr = &a;
			break;
		}
	}

	if (!posAttr) {
		std::fprintf(stderr, "[RayPicking] No position attribute (location=0) found; BVH not built.\n");
		return;
	}

	const uint32_t vStride = mesh.vsrc.stride;
	const uint8_t *vData = static_cast<const uint8_t *>(mesh.vsrc.data);
	const size_t vCount = (vStride > 0) ? (mesh.vsrc.bytes / vStride) : 0;

	const bool fmtVec3 = (posAttr->fmt == VK_FORMAT_R32G32B32_SFLOAT);
	const bool fmtVec4 = (posAttr->fmt == VK_FORMAT_R32G32B32A32_SFLOAT);

	if (!fmtVec3 && !fmtVec4) {
		std::fprintf(stderr, "[RayPicking] Unsupported position format (need R32G32B32 or R32G32B32A32 SFLOAT); BVH not built.\n");
		return;
	}
	if (vCount == 0) {
		std::fprintf(stderr, "[RayPicking] Empty vertex buffer; BVH not built.\n");
		return;
	}

	// Build a vertex array with a .pos member (what buildBVH expects)
	std::vector<vec3> verts(vCount);

	const size_t posOff = posAttr->offset; // offset within each vertex
	for (size_t i = 0; i < vCount; ++i) {
		const uint8_t *p = vData + i * vStride + posOff;
		const float *f = reinterpret_cast<const float *>(p);
		// both vec3 and vec4 share the first 3 floats
		verts[i] = glm::vec3(f[0], f[1], f[2]);
	}

	// Indices (assumed uint32_t)
	std::vector<uint32_t> indices;
	indices.reserve(mesh.isrc.count);
	if (mesh.isrc.count) {
		const uint32_t *id = static_cast<const uint32_t *>(mesh.isrc.data);
		indices.insert(indices.end(), id, id + mesh.isrc.count);
	}

	// Build BVH into RayPicking's CPU buffers
	picking->buildBVH(verts, indices);

	// Init the ray-picking compute pipeline, then upload the built data
	picking->initInfo.shaders = Assets::compileShaderProgram(Assets::shaderRootPath + "/raypicking", pipeline->device);
	picking->initInfo.maxInstances = std::max(1u, maxInstances);
	// Sizes were set by buildBVH() (in our version); if not, keep your own sizes.
	picking->init(pipeline->device, pipeline->physicalDevice);
}

void Model::init() {
	engine = scene->getScenes().getEngine();
	pipeline->device = engine->getDevice();
	pipeline->physicalDevice = engine->getPhysicalDevice();

	pipeline->graphicsPipeline.colorFormat = engine->getGraphicsBuffer().getSceneColorFormat();
	pipeline->graphicsPipeline.depthFormat = engine->getGraphicsBuffer().getDepthFormat();
	pipeline->samplesCountFlagBits = initInfo.samples;
	pipeline->shaders = initInfo.shaders;

	mesh = initInfo.mesh;
	maxInstances = initInfo.maxInstances;
	iStride = initInfo.instanceStrideBytes;

	auto extent = engine->getSwapchain().getExtent();
	auto vw = (float)extent.width;
	auto vh = (float)extent.height;
	viewport = {0.0f, 0.0f, vw, vh, 0.0f, 1.0f};

	// --- helper: stage copy (same pattern you use elsewhere) ---
	auto stageCopy = [&](const void *src, VkDeviceSize bytes, VkBuffer dst) {
		if (bytes == 0 || dst == VK_NULL_HANDLE)
			return;

		VkBuffer staging = VK_NULL_HANDLE;
		VkDeviceMemory smem = VK_NULL_HANDLE;
		pipeline->createBuffer(bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging, smem);

		void *p = nullptr;
		vkMapMemory(pipeline->device, smem, 0, VK_WHOLE_SIZE, 0, &p);
		std::memcpy(p, src, (size_t)bytes);
		vkUnmapMemory(pipeline->device, smem);

		auto begin = engine->getLogicalDevice().beginSingleUseCmd();
		VkBufferCopy reg{0, 0, bytes};
		vkCmdCopyBuffer(begin, staging, dst, 1, &reg);
		engine->getLogicalDevice().endSingleUseCmdGraphics(begin);

		vkDestroyBuffer(pipeline->device, staging, nullptr);
		vkFreeMemory(pipeline->device, smem, nullptr);
	};

	// --- Vertex buffer: DEVICE_LOCAL + TRANSFER_DST ---
	if (mesh.vsrc.bytes) {
		pipeline->createBuffer(mesh.vsrc.bytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vbuf, vmem);
		stageCopy(mesh.vsrc.data, mesh.vsrc.bytes, vbuf);
	}

	// --- Index buffer: DEVICE_LOCAL + TRANSFER_DST ---
	if (mesh.isrc.count) {
		VkDeviceSize ibytes = sizeof(uint32_t) * mesh.isrc.count;
		pipeline->createBuffer(ibytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, ibuf, imem);
		stageCopy(mesh.isrc.data, ibytes, ibuf);
		indexCount = (uint32_t)mesh.isrc.count;
	}

	// --- UBO (view/proj) ---
	pipeline->createBuffer(sizeof(VPMatrix), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, ubo, umem);

	void *up = nullptr;
	VK_CHECK(vkMapMemory(pipeline->device, umem, 0, VK_WHOLE_SIZE, 0, &up));
	std::memcpy(up, &vp, sizeof(VPMatrix));
	vkUnmapMemory(pipeline->device, umem);

	// --- Instance data buffer (SSBO + also used as instance vertex binding #1) ---
	if (iStride > 0 && maxInstances > 0) {
		pipeline->createBuffer(maxInstances * iStride, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, ssbo, smem);

		// persistently map once
		VK_CHECK(vkMapMemory(pipeline->device, smem, 0, VK_WHOLE_SIZE, 0, reinterpret_cast<void **>(&mappedSSBO)));
		cpu.resize(maxInstances * iStride, 0);
	}

	uint32_t setCount = createDescriptorPool();
	pipeline->createDescriptorPool(setCount);

	createDescriptors();
	pipeline->createDescriptors();

	createGraphicsPipeline();
	pipeline->createGraphicsPipeline();
}

uint32_t Model::createDescriptorPool() {
	pipeline->descriptorPoolSizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1});
	pipeline->descriptorPoolSizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1});
	return 2;
}

void Model::createDescriptors() {
	// set=0, binding 0: UBO (VS|FS)
	pipeline->createDescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	// set=0, binding 1: SSBO (VS)
	if (iStride > 0) {
		pipeline->createDescriptorSetLayoutBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
	}

	// write descriptors
	VkDescriptorBufferInfo uboInfo{ubo, 0, sizeof(VPMatrix)};
	pipeline->createWriteDescriptorSet(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uboInfo);

	if (iStride > 0) {
		VkDescriptorBufferInfo sboInfo{ssbo, 0, maxInstances * iStride};
		pipeline->createWriteDescriptorSet(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sboInfo);
	}
}

void Model::createGraphicsPipeline() {
	auto &vattrs = pipeline->graphicsPipeline.vertexInputAttributeDescription;
	vattrs.reserve(mesh.vertexAttrs.size());
	for (const auto &a : mesh.vertexAttrs) {
		vattrs.push_back(VkVertexInputAttributeDescription{a.location, a.binding, a.fmt, a.offset});
	}

	pipeline->createVertexInputBindingDescription(0, mesh.vsrc.stride, VK_VERTEX_INPUT_RATE_VERTEX);
	if (iStride > 0) {
		pipeline->createVertexInputBindingDescription(1, iStride, VK_VERTEX_INPUT_RATE_INSTANCE);
	}

	pipeline->graphicsPipeline.rasterizationStateCI.cullMode = VK_CULL_MODE_NONE;

	pipeline->graphicsPipeline.depthStencilStateCI.depthTestEnable = VK_TRUE;
	pipeline->graphicsPipeline.depthStencilStateCI.depthWriteEnable = VK_TRUE;
}

void Model::destroy() {
	const auto &dev = pipeline->device;
	if (mappedSSBO && smem) {
		vkUnmapMemory(dev, smem);
		mappedSSBO = nullptr;
	}
	if (vbuf) {
		vkDestroyBuffer(dev, vbuf, nullptr);
		vbuf = VK_NULL_HANDLE;
	}
	if (vmem) {
		vkFreeMemory(dev, vmem, nullptr);
		vmem = VK_NULL_HANDLE;
	}
	if (ibuf) {
		vkDestroyBuffer(dev, ibuf, nullptr);
		ibuf = VK_NULL_HANDLE;
	}
	if (imem) {
		vkFreeMemory(dev, imem, nullptr);
		imem = VK_NULL_HANDLE;
	}
	if (ubo) {
		vkDestroyBuffer(dev, ubo, nullptr);
		ubo = VK_NULL_HANDLE;
	}
	if (umem) {
		vkFreeMemory(dev, umem, nullptr);
		umem = VK_NULL_HANDLE;
	}
	if (ssbo) {
		vkDestroyBuffer(dev, ssbo, nullptr);
		ssbo = VK_NULL_HANDLE;
	}
	if (smem) {
		vkFreeMemory(dev, smem, nullptr);
		smem = VK_NULL_HANDLE;
	}
}

void Model::upsertBytes(int id, std::span<const uint8_t> bytes) {
	if (bytes.size() != iStride)
		throw std::runtime_error("bad instance stride");
	auto it = idToSlot.find(id);
	if (it == idToSlot.end()) {
		if (count >= maxInstances)
			throw std::runtime_error("capacity");
		uint32_t slot = count++;
		idToSlot[id] = slot;
		std::memcpy(cpu.data() + slot * iStride, bytes.data(), iStride);
	} else {
		uint32_t slot = it->second;
		std::memcpy(cpu.data() + slot * iStride, bytes.data(), iStride);
	}
	ssboDirty = true;
	pickingInstancesDirty = true;
}

void Model::erase(int id) {
	auto it = idToSlot.find(id);
	if (it == idToSlot.end())
		return;
	uint32_t slot = it->second, last = count - 1;
	if (slot != last) {
		std::memcpy(cpu.data() + slot * iStride, cpu.data() + last * iStride, iStride);
		for (auto &kv : idToSlot)
			if (kv.second == last) {
				kv.second = slot;
				break;
			}
	}
	--count;
	idToSlot.erase(it);
	ssboDirty = true;
	pickingInstancesDirty = true;
}

void Model::record(VkCommandBuffer cmd) {
	if (picking && pickingDispatched_) {
		picking->readback(picking->hitInfo);

		auto bestPick = getScene()->getScenes().getRayPicked();

		if (picking->hitInfo.hit) {
			if (bestPick == nullptr) {
				getScene()->getScenes().setRayPicked(this);
			} else if (picking->hitInfo.rayLen <= bestPick->picking->hitInfo.rayLen) {
				getScene()->getScenes().setRayPicked(this);
			}
		} else {
			if (bestPick == this) {
				getScene()->getScenes().setRayPicked(nullptr);
			}
		}
	}

	const auto &dev = pipeline->device;
	const auto &pipe = pipeline->pipeline;
	const auto &pipeLayout = pipeline->pipelineLayout;
	const auto &dsets = pipeline->descriptorSets.descriptorSets;
	const auto &dynOffsets = pipeline->descriptorSets.dynamicOffsets;

	// sync uploads
	if (uboDirty) {
		void *up;
		vkMapMemory(dev, umem, 0, VK_WHOLE_SIZE, 0, &up);
		std::memcpy(up, &vp, sizeof(VPMatrix));
		vkUnmapMemory(dev, umem);
		uboDirty = false;
	}
	if (ssboDirty) {
		std::memcpy(mappedSSBO, cpu.data(), count * iStride);
		ssboDirty = false;
	}

	// If we don't have geometry yet, bail out safely (prevents VUID 04001)
	// You can relax the indexCount requirement if you support non-indexed draws.
	if (vbuf == VK_NULL_HANDLE || indexCount == 0) {
		return;
	}

	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
	if (!dsets.empty()) {
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeLayout, 0, static_cast<uint32_t>(dsets.size()), dsets.data(), static_cast<uint32_t>(dynOffsets.size()), dynOffsets.empty() ? nullptr : dynOffsets.data());
	}

	pushConstants(cmd, pipeLayout);

	// Build the VB list dynamically to avoid passing VK_NULL_HANDLE
	VkBuffer vbs[2];
	VkDeviceSize offs[2] = {0, 0};
	uint32_t vbCount = 0;
	if (vbuf != VK_NULL_HANDLE) {
		vbs[vbCount++] = vbuf;
	}
	if (iStride > 0 && ssbo != VK_NULL_HANDLE) {
		vbs[vbCount++] = ssbo;
	}

	// If binding 0 (vertex stream) is missing, skip (pipeline expects it)
	if (vbCount == 0)
		return;

	vkCmdBindVertexBuffers(cmd, 0, vbCount, vbs, offs);

	if (indexCount)
		vkCmdBindIndexBuffer(cmd, ibuf, 0, VK_INDEX_TYPE_UINT32);

	if (count && indexCount) {
		vkCmdDrawIndexed(cmd, indexCount, count, 0, 0, 0);
	}
}

void Model::recordUI(VkCommandBuffer cmd, uint32_t) { record(cmd); }
