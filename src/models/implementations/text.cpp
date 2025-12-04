#include "text.hpp"
#include "assets.hpp"
#include "colors.hpp"
#include "debug.hpp"
#include "engine.hpp"
#include "events.hpp"
#include "memory.hpp"
#include "mouse.hpp"
#include "scenes.hpp"

#include <ft2build.h>
#include <utility>
#include <vulkan/vulkan_core.h>
#include FT_FREETYPE_H
#include <algorithm>
#include <cmath>
#include <codecvt>
#include <freetype/ftmodapi.h>
#include <locale>
#include <optional>
#include <unordered_set>

// ========================= Shared Text Arena =========================
// One global vertex/index buffer for all Text objects, with simple free-lists.
namespace {

struct FreeRange {
	VkDeviceSize off = 0, sz = 0;
};

struct SharedTextArena {
	// Created lazily on first use
	VkDevice device = VK_NULL_HANDLE;
	VkPhysicalDevice phys = VK_NULL_HANDLE;
	VkBuffer vbuf = VK_NULL_HANDLE, ibuf = VK_NULL_HANDLE;
	VkDeviceMemory vmem = VK_NULL_HANDLE, imem = VK_NULL_HANDLE;
	VkDeviceSize vcap = 0, icap = 0;

	std::vector<FreeRange> vfree, ifree;
	VkDeviceSize vtail = 0, itail = 0; // bump pointers
	std::mutex mtx;
	bool inited = false;

	static SharedTextArena &inst() {
		static SharedTextArena A;
		return A;
	}

	static VkDeviceSize alignUp(VkDeviceSize x, VkDeviceSize a) { return (x + (a - 1)) & ~(a - 1); }

	void shutdown() {
		if (!inited)
			return;
		// Make sure no command buffer is still using these
		if (device)
			vkDeviceWaitIdle(device);
		if (vbuf) {
			vkDestroyBuffer(device, vbuf, nullptr);
			vbuf = VK_NULL_HANDLE;
		}
		if (ibuf) {
			vkDestroyBuffer(device, ibuf, nullptr);
			ibuf = VK_NULL_HANDLE;
		}
		if (vmem) {
			vkFreeMemory(device, vmem, nullptr);
			vmem = VK_NULL_HANDLE;
		}
		if (imem) {
			vkFreeMemory(device, imem, nullptr);
			imem = VK_NULL_HANDLE;
		}
		vcap = icap = vtail = itail = 0;
		vfree.clear();
		ifree.clear();
		inited = false;
		device = VK_NULL_HANDLE;
		phys = VK_NULL_HANDLE;
	}

	void ensureInit(Engine *E, VkDeviceSize initV = 32ull << 20, VkDeviceSize initI = 8ull << 20) {
		if (inited)
			return;
		std::lock_guard<std::mutex> lock(mtx);
		if (inited)
			return;
		device = E->getDevice();
		phys = E->getPhysicalDevice();

		auto makeBuffer = [&](VkDeviceSize cap, VkBufferUsageFlags usage, VkBuffer &buf, VkDeviceMemory &mem) {
			VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
			bci.size = cap;
			bci.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			VK_CHECK(vkCreateBuffer(device, &bci, nullptr, &buf));
			VkMemoryRequirements req{};
			vkGetBufferMemoryRequirements(device, buf, &req);
			VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
			ai.allocationSize = req.size;
			ai.memoryTypeIndex = Memory::findMemoryType(phys, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK(vkAllocateMemory(device, &ai, nullptr, &mem));
			VK_CHECK(vkBindBufferMemory(device, buf, mem, 0));
		};

		vcap = initV;
		icap = initI;
		makeBuffer(vcap, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vbuf, vmem);
		makeBuffer(icap, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, ibuf, imem);
		vtail = itail = 0;
		vfree.clear();
		ifree.clear();
		inited = true;
	}

	// very small best-fit from free list; fall back to tail bump
	bool alloc(VkDeviceSize needV, VkDeviceSize needI, VkDeviceSize &vOff, VkDeviceSize &iOff, VkDeviceSize &vCap, VkDeviceSize &iCap) {
		std::lock_guard<std::mutex> lock(mtx);
		auto take = [](std::vector<FreeRange> &fl, VkDeviceSize need, VkDeviceSize &outOff) -> bool {
			size_t best = SIZE_MAX;
			VkDeviceSize bestSz = ~VkDeviceSize(0);
			for (size_t i = 0; i < fl.size(); ++i)
				if (fl[i].sz >= need && fl[i].sz < bestSz) {
					best = i;
					bestSz = fl[i].sz;
				}
			if (best == SIZE_MAX)
				return false;
			outOff = fl[best].off;
			if (fl[best].sz == need)
				fl.erase(fl.begin() + best);
			else {
				fl[best].off += need;
				fl[best].sz -= need;
			}
			return true;
		};
		// try free-lists first
		if (needV && take(vfree, needV, vOff))
			vCap = needV;
		else {
			if (vtail + needV > vcap)
				return false;
			vOff = vtail;
			vtail = alignUp(vtail + needV, 256);
			vCap = needV;
		}
		if (needI && take(ifree, needI, iOff))
			iCap = needI;
		else {
			if (itail + needI > icap)
				return false;
			iOff = itail;
			itail = alignUp(itail + needI, 256);
			iCap = needI;
		}
		return true;
	}

	void free(VkDeviceSize vOff, VkDeviceSize vSz, VkDeviceSize iOff, VkDeviceSize iSz) {
		if (!vSz && !iSz)
			return;
		std::lock_guard<std::mutex> lock(mtx);
		if (vSz)
			vfree.push_back({vOff, vSz});
		if (iSz)
			ifree.push_back({iOff, iSz});
		// (Optional) coalesce; omitted for simplicity
	}

	void upload(Engine *E, const void *vData, VkDeviceSize vBytes, VkDeviceSize vOff, const void *iData, VkDeviceSize iBytes, VkDeviceSize iOff) {
		if ((!vBytes && !iBytes) || (!inited))
			return;
		auto stageCopy = [&](const void *src, VkDeviceSize bytes, VkBuffer dst, VkDeviceSize dstOff) {
			if (!bytes)
				return;
			VkBuffer staging = VK_NULL_HANDLE;
			VkDeviceMemory smem = VK_NULL_HANDLE;
			// host visible staging
			VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
			bci.size = bytes;
			bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			VK_CHECK(vkCreateBuffer(device, &bci, nullptr, &staging));
			VkMemoryRequirements req{};
			vkGetBufferMemoryRequirements(device, staging, &req);
			VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
			ai.allocationSize = req.size;
			ai.memoryTypeIndex = Memory::findMemoryType(phys, req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			VK_CHECK(vkAllocateMemory(device, &ai, nullptr, &smem));
			VK_CHECK(vkBindBufferMemory(device, staging, smem, 0));
			void *p = nullptr;
			vkMapMemory(device, smem, 0, VK_WHOLE_SIZE, 0, &p);
			std::memcpy(p, src, size_t(bytes));
			vkUnmapMemory(device, smem);
			// copy
			auto cmd = E->getLogicalDevice().beginSingleUseCmd();
			VkBufferCopy copy{0, dstOff, bytes};
			vkCmdCopyBuffer(cmd, staging, dst, 1, &copy);
			E->getLogicalDevice().endSingleUseCmdGraphics(cmd);
			vkDestroyBuffer(device, staging, nullptr);
			vkFreeMemory(device, smem, nullptr);
		};
		stageCopy(vData, vBytes, vbuf, vOff);
		stageCopy(iData, iBytes, ibuf, iOff);
	}
};

struct SharedFT {
	FT_Library lib = nullptr;
	std::unordered_map<std::string, FT_Face> faces;
};

SharedFT &getSharedFT() {
	static SharedFT s;
	return s;
}

FT_CharMap findUnicodeCharmap(FT_Face face) {
	FT_CharMap unicode = nullptr;
	for (int i = 0; i < face->num_charmaps; ++i) {
		if (face->charmaps[i]->encoding == FT_ENCODING_UNICODE) {
			unicode = face->charmaps[i];
			break;
		}
	}
	return unicode;
}

// ---------------- Shared Atlas (per-font) ----------------
//
// We share one atlas (glyphs + R8 image) per fontPath across all Text
// instances. Each Text holds a pointer to its font's Atlas, but the
// storage (glyph map, host pixels, packing state, GPU objects) lives here.
struct SharedAtlas {
	struct FontAtlas {
		Text::Atlas atlas;
		std::vector<uint8_t> host; // R8 pixels
		bool atlasReady = false;
		int packX = Text::kPad;
		int packY = Text::kPad;
		int packRowH = 0;
		std::vector<uint32_t> prewarmGlyphs;
		uint32_t refCount = 0;
		std::unordered_set<Text *> users;
	};

	std::unordered_map<std::string, FontAtlas> fonts;
	std::mutex mtx;
};

SharedAtlas &getSharedAtlas() {
	static SharedAtlas s;
	return s;
}

} // namespace

static inline float clampf(float v, float a, float b) { return std::max(a, std::min(b, v)); }

static bool loadGlyphNoHint(FT_Face face, uint32_t cp) {
	FT_UInt gi = FT_Get_Char_Index(face, cp);
	if (!gi)
		return false;

	// Outline load, no bitmap strikes, no hinting → stable grayscale.
	FT_Int32 flags = FT_LOAD_DEFAULT | FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP | FT_LOAD_TARGET_NORMAL;

	if (FT_Load_Glyph(face, gi, flags))
		return false;
	if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_SDF))
		return false;
	return true;
}

static std::u32string utf8_to_u32(const std::string &s) {
	std::u32string out;
	std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv;
	try {
		out = conv.from_bytes(s);
	} catch (...) {
	}
	return out;
}

static inline float modelScaleX(const glm::mat4 &M) {
	// length of the X basis vector (|column 0|). Works for scale+rot+shear.
	glm::vec3 ex(M[0][0], M[1][0], M[2][0]);
	return glm::length(ex);
}

static inline float modelScaleY(const glm::mat4 &M) {
	glm::vec3 ey(M[0][1], M[1][1], M[2][1]);
	return glm::length(ey);
}

static inline glm::vec4 makeRectScreenSpace(const glm::vec2 &a, const glm::vec2 &b) {
	float x0 = std::min(a.x, b.x);
	float y0 = std::min(a.y, b.y);
	float x1 = std::max(a.x, b.x);
	float y1 = std::max(a.y, b.y);
	return glm::vec4(x0, y0, x1, y1);
}

static inline bool rectsOverlap(const glm::vec4 &A, const glm::vec4 &B) {
	float ax0 = std::min(A.x, A.z);
	float ay0 = std::min(A.y, A.w);
	float ax1 = std::max(A.x, A.z);
	float ay1 = std::max(A.y, A.w);

	float bx0 = std::min(B.x, B.z);
	float by0 = std::min(B.y, B.w);
	float bx1 = std::max(B.x, B.z);
	float by1 = std::max(B.y, B.w);

	return (ax0 <= bx1 && ax1 >= bx0 && ay0 <= by1 && ay1 >= by0);
}

// =======================================================
// Shared atlas helpers (CPU + GPU) – per fontPath
// =======================================================
namespace {

// Upload new full atlas image for this font.
void createAtlasGPUForFont(SharedAtlas::FontAtlas &fa, Text *self) {
	if (!self || !self->getEngine())
		return;

	VkDevice dev = self->getEngine()->getDevice();
	VkPhysicalDevice phys = self->getEngine()->getPhysicalDevice();

	// Tear down any previous image/view/sampler for this font.
	if (fa.atlas.image || fa.atlas.view || fa.atlas.sampler || fa.atlas.memory) {
		vkDeviceWaitIdle(dev);
		if (fa.atlas.view) {
			vkDestroyImageView(dev, fa.atlas.view, nullptr);
			fa.atlas.view = VK_NULL_HANDLE;
		}
		if (fa.atlas.image) {
			vkDestroyImage(dev, fa.atlas.image, nullptr);
			fa.atlas.image = VK_NULL_HANDLE;
		}
		if (fa.atlas.memory) {
			vkFreeMemory(dev, fa.atlas.memory, nullptr);
			fa.atlas.memory = VK_NULL_HANDLE;
		}
		if (fa.atlas.sampler) {
			vkDestroySampler(dev, fa.atlas.sampler, nullptr);
			fa.atlas.sampler = VK_NULL_HANDLE;
		}
	}

	if (fa.atlas.texW <= 0 || fa.atlas.texH <= 0 || fa.host.empty())
		return;

	const VkDeviceSize bytes = VkDeviceSize(fa.atlas.texW * fa.atlas.texH);
	VkBuffer staging = VK_NULL_HANDLE;
	VkDeviceMemory stagingMem = VK_NULL_HANDLE;

	self->getPipeline()->createBuffer(bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging, stagingMem);

	void *p = nullptr;
	vkMapMemory(dev, stagingMem, 0, VK_WHOLE_SIZE, 0, &p);
	std::memcpy(p, fa.host.data(), size_t(bytes));
	vkUnmapMemory(dev, stagingMem);

	VkImageCreateInfo ici{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
	ici.imageType = VK_IMAGE_TYPE_2D;
	ici.extent = {(uint32_t)fa.atlas.texW, (uint32_t)fa.atlas.texH, 1};
	ici.mipLevels = 1;
	ici.arrayLayers = 1;
	ici.format = VK_FORMAT_R8_UNORM;
	ici.tiling = VK_IMAGE_TILING_OPTIMAL;
	ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	ici.samples = VK_SAMPLE_COUNT_1_BIT;
	ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK(vkCreateImage(dev, &ici, nullptr, &fa.atlas.image));

	VkMemoryRequirements req{};
	vkGetImageMemoryRequirements(dev, fa.atlas.image, &req);
	VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	ai.allocationSize = req.size;
	ai.memoryTypeIndex = Memory::findMemoryType(phys, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK(vkAllocateMemory(dev, &ai, nullptr, &fa.atlas.memory));
	VK_CHECK(vkBindImageMemory(dev, fa.atlas.image, fa.atlas.memory, 0));

	auto begin = self->getEngine()->getLogicalDevice().beginSingleUseCmd();
	VkImageMemoryBarrier toDst{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
	toDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	toDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	toDst.srcAccessMask = 0;
	toDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	toDst.image = fa.atlas.image;
	toDst.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
	vkCmdPipelineBarrier(begin, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toDst);

	VkBufferImageCopy reg{};
	reg.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	reg.imageSubresource.layerCount = 1;
	reg.imageExtent = {(uint32_t)fa.atlas.texW, (uint32_t)fa.atlas.texH, 1};
	vkCmdCopyBufferToImage(begin, staging, fa.atlas.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &reg);

	VkImageMemoryBarrier toRead{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
	toRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	toRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	toRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	toRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	toRead.image = fa.atlas.image;
	toRead.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
	vkCmdPipelineBarrier(begin, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toRead);
	self->getEngine()->getLogicalDevice().endSingleUseCmdGraphics(begin);

	vkDestroyBuffer(dev, staging, nullptr);
	vkFreeMemory(dev, stagingMem, nullptr);

	VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
	vci.image = fa.atlas.image;
	vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
	vci.format = VK_FORMAT_R8_UNORM;
	vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vci.subresourceRange.levelCount = 1;
	vci.subresourceRange.layerCount = 1;
	VK_CHECK(vkCreateImageView(dev, &vci, nullptr, &fa.atlas.view));

	VkSamplerCreateInfo sci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
	sci.magFilter = VK_FILTER_LINEAR;
	sci.minFilter = VK_FILTER_LINEAR;
	sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	sci.addressModeU = sci.addressModeV = sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	VK_CHECK(vkCreateSampler(dev, &sci, nullptr, &fa.atlas.sampler));

	// Atlas GPU handles changed → all Text instances using this font
	// must refresh their descriptors for set=0,binding=2 (uAtlas).
	for (Text *t : fa.users) {
		if (!t)
			continue;
		t->writeAtlasDescriptor();
	}
}

// Upload sub-rect for newly appended glyphs.
void uploadSubImageShared(SharedAtlas::FontAtlas &fa, int x, int y, int w, int h, const uint8_t *data, Text *self) {
	if (!self || !self->getEngine())
		return;
	if (fa.atlas.image == VK_NULL_HANDLE)
		return;

	VkDevice dev = self->getEngine()->getDevice();

	const VkDeviceSize bytes = VkDeviceSize(size_t(w) * size_t(h));
	VkBuffer staging = VK_NULL_HANDLE;
	VkDeviceMemory smem = VK_NULL_HANDLE;
	self->getPipeline()->createBuffer(bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging, smem);
	void *p = nullptr;
	vkMapMemory(dev, smem, 0, VK_WHOLE_SIZE, 0, &p);
	std::memcpy(p, data, size_t(bytes));
	vkUnmapMemory(dev, smem);

	auto cmd = self->getEngine()->getLogicalDevice().beginSingleUseCmd();

	VkImageMemoryBarrier toDst{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
	toDst.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	toDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	toDst.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	toDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	toDst.image = fa.atlas.image;
	toDst.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toDst);

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = {x, y, 0};
	region.imageExtent = {uint32_t(w), uint32_t(h), 1};
	vkCmdCopyBufferToImage(cmd, staging, fa.atlas.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	VkImageMemoryBarrier toRead{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
	toRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	toRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	toRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	toRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	toRead.image = fa.atlas.image;
	toRead.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toRead);
	self->getEngine()->getLogicalDevice().endSingleUseCmdGraphics(cmd);

	vkDestroyBuffer(dev, staging, nullptr);
	vkFreeMemory(dev, smem, nullptr);
}

bool appendGlyphShared(SharedAtlas::FontAtlas &fa, uint32_t cp, Text::FTData *ft, Text *self) {
	if (!ft || !ft->face)
		return true;

	if (!loadGlyphNoHint(ft->face, cp))
		return true; // nothing to draw, but record metrics below if needed

	auto &slot = *ft->face->glyph;
	auto &bm = slot.bitmap;
	const int gw = (int)bm.width;
	const int gh = (int)bm.rows;

	// Glyph with no bitmap: record metrics only
	if (gw == 0 || gh == 0) {
		Text::Glyph g{};
		g.advanceX = (int)std::round(slot.advance.x / 64.0);
		g.bearingX = slot.bitmap_left;
		g.bearingY = slot.bitmap_top;
		g.width = 0;
		g.height = 0;
		g.sdfSpreadPx = float(ft->sdfSpread);
		fa.atlas.glyphs[cp] = g;
		return true;
	}

	const int gutter = Text::kGutter;
	const int tw = gw + 2 * gutter;
	const int th = gh + 2 * gutter;

	// Shelf wrap
	if (fa.packX + tw + Text::kPad >= fa.atlas.texW) {
		fa.packX = Text::kPad;
		fa.packY += fa.packRowH + Text::kPad;
		fa.packRowH = 0;
	}
	if (fa.packY + std::max(fa.packRowH, th) + Text::kPad >= fa.atlas.texH) {
		return false; // no space → signal full repack
	}
	fa.packRowH = std::max(fa.packRowH, th);

	const int x = fa.packX;
	const int y = fa.packY;

	// Build tight temporary block for the subrect upload
	std::vector<uint8_t> block(size_t(tw * th), 0);
	for (int j = 0; j < th; ++j) {
		if (j >= gutter && j < gutter + gh) {
			std::memcpy(&block[j * tw + gutter], bm.buffer + (j - gutter) * gw, size_t(gw));
		}
	}
	// Mirror into host as well
	for (int j = 0; j < th; ++j) {
		const int sy = y + j;
		uint8_t *dst = &fa.host[sy * fa.atlas.texW + x];
		std::memcpy(dst, &block[j * tw], size_t(tw));
	}

	Text::Glyph g{};
	g.advanceX = (int)std::round(slot.advance.x / 64.0);
	g.bearingX = slot.bitmap_left;
	g.bearingY = slot.bitmap_top;
	g.width = gw;
	g.height = gh;
	g.u0 = float(x) / fa.atlas.texW;
	g.v0 = float(y) / fa.atlas.texH;
	g.u1 = float(x + tw) / fa.atlas.texW;
	g.v1 = float(y + th) / fa.atlas.texH;
	g.sdfSpreadPx = float(ft->sdfSpread);
	fa.atlas.glyphs[cp] = g;

	// Upload the subrect to GPU
	uploadSubImageShared(fa, x, y, tw, th, block.data(), self);

	fa.packX += tw + Text::kPad;
	return true;
}

void rebuildFontAtlas(SharedAtlas::FontAtlas &fa, Text::FTData *ft, const std::unordered_set<uint32_t> &needSet, Text *self) {
	if (!ft || !ft->face)
		return;

	fa.atlas.glyphs.clear();
	fa.host.clear();

	std::vector<uint32_t> glyphList(needSet.begin(), needSet.end());
	std::sort(glyphList.begin(), glyphList.end());

	const int pad = Text::kPad;
	int x = pad, y = pad, rowH = 0;
	const int maxW = Text::kMaxW, maxH = Text::kMaxH;
	fa.atlas.texW = fa.atlas.texH = 0;

	// First pass: measure atlas size
	for (auto cp : glyphList) {
		if (!loadGlyphNoHint(ft->face, cp))
			continue;
		rowH = std::max(rowH, (int)ft->face->glyph->bitmap.rows + 2 * ft->sdfSpread);
		int needW = (int)ft->face->glyph->bitmap.width + 2 * ft->sdfSpread + pad;
		if (x + needW >= maxW) {
			x = pad;
			y += rowH + pad;
			rowH = 0;
		}
		x += needW;
		fa.atlas.texW = std::max(fa.atlas.texW, x);
		fa.atlas.texH = std::max(fa.atlas.texH, y + rowH + pad);
	}

	fa.atlas.texW = std::min(std::max(fa.atlas.texW, 64), maxW);
	fa.atlas.texH = std::min(std::max(fa.atlas.texH, 64), maxH);
	fa.host.assign(fa.atlas.texW * fa.atlas.texH, 0);

	x = pad;
	y = pad;
	rowH = 0;

	for (auto cp : glyphList) {
		if (!loadGlyphNoHint(ft->face, cp))
			continue;

		auto &slot = *ft->face->glyph;
		auto &bm = slot.bitmap;

		int gw = (int)bm.width;
		int gh = (int)bm.rows;

		// If the renderer produced nothing (space, etc.), record advance only.
		if (gw == 0 || gh == 0) {
			Text::Glyph g{};
			g.advanceX = (int)std::round(slot.advance.x / 64.0);
			g.bearingX = slot.bitmap_left;
			g.bearingY = slot.bitmap_top;
			g.width = 0;
			g.height = 0;
			g.sdfSpreadPx = float(ft->sdfSpread);
			fa.atlas.glyphs[cp] = g;
			continue;
		}

		const int gutter = 2;
		int tw = gw + 2 * gutter;
		int th = gh + 2 * gutter;

		if (x + tw + pad >= fa.atlas.texW) {
			x = pad;
			y += rowH + pad;
			rowH = 0;
		}
		rowH = std::max(rowH, th);

		// Blit with neutral 0 outside and the SDF in the center.
		for (int j = 0; j < th; ++j) {
			int sy = y + j;
			if (sy >= fa.atlas.texH)
				break;
			uint8_t *dst = &fa.host[sy * fa.atlas.texW + x];

			std::memset(dst, 0, tw);

			if (j >= gutter && j < gutter + gh) {
				const uint8_t *src = bm.buffer + (j - gutter) * gw;
				std::memcpy(dst + gutter, src, gw);
			}
		}

		Text::Glyph g{};
		g.advanceX = (int)std::round(slot.advance.x / 64.0);
		g.bearingX = slot.bitmap_left;
		g.bearingY = slot.bitmap_top;
		g.width = gw;
		g.height = gh;
		g.u0 = float(x) / fa.atlas.texW;
		g.v0 = float(y) / fa.atlas.texH;
		g.u1 = float(x + tw) / fa.atlas.texW;
		g.v1 = float(y + th) / fa.atlas.texH;
		g.sdfSpreadPx = float(ft->sdfSpread);
		fa.atlas.glyphs[cp] = g;

		x += tw + pad;
	}
	fa.atlasReady = true;
	fa.packX = x;
	fa.packY = y;
	fa.packRowH = rowH;

	// (Re)create GPU image once with full contents
	createAtlasGPUForFont(fa, self);
}

} // namespace

// ---------------- FreeType/SDF plumbing ----------------
Text::Text(Scene *scene) : Model(scene) {}
Text::~Text() {
	if (pipeline && pipeline->device) {
		VkDevice d = pipeline->device;
		// Shared arena owns the big VB/IB; we only return our slices.
		if (arenaAllocated_) {
			SharedTextArena::inst().free(vbOffset_, vbCapacity_, ibOffset_, ibCapacity_);
			arenaAllocated_ = false;
			vbOffset_ = ibOffset_ = vbCapacity_ = ibCapacity_ = 0;
			SharedTextArena::inst().shutdown();
		}

		// Shared atlas lifetime: when the last Text using a font dies,
		// destroy that font's GPU objects.
		if (registeredInSharedAtlas_) {
			auto &SA = getSharedAtlas();
			std::lock_guard<std::mutex> lock(SA.mtx);
			auto it = SA.fonts.find(fontPath);
			if (it != SA.fonts.end()) {
				auto &fa = it->second;
				if (fa.refCount > 0) {
					fa.users.erase(this);
					--fa.refCount;
					if (fa.refCount == 0) {
						vkDeviceWaitIdle(d);
						// All Texts for this font are gone -> free GPU objects.
						if (fa.atlas.view) {
							vkDestroyImageView(d, fa.atlas.view, nullptr);
							fa.atlas.view = VK_NULL_HANDLE;
						}
						if (fa.atlas.image) {
							vkDestroyImage(d, fa.atlas.image, nullptr);
							fa.atlas.image = VK_NULL_HANDLE;
						}
						if (fa.atlas.memory) {
							vkFreeMemory(d, fa.atlas.memory, nullptr);
							fa.atlas.memory = VK_NULL_HANDLE;
						}
						if (fa.atlas.sampler) {
							vkDestroySampler(d, fa.atlas.sampler, nullptr);
							fa.atlas.sampler = VK_NULL_HANDLE;
						}

						fa.host.clear();
						fa.atlas.glyphs.clear();
						fa.atlas.texW = fa.atlas.texH = 0;

						SA.fonts.erase(it);
					}
				}
			}
		}
	}
}

glm::vec2 Text::localToScreen(const glm::vec2 &p) const {
	// local -> world
	glm::vec4 w = pc.model * glm::vec4(p, 0.0f, 1.0f);
	// world -> clip
	glm::vec4 c = vp.proj * vp.view * w;
	if (std::abs(c.w) < 1e-6f)
		return glm::vec2(0.0f); // degenerate, avoid NaNs
	c /= c.w;					// NDC

	// NDC (-1..1) -> screen (pixels)
	float sx = (c.x * 0.5f + 0.5f) * viewport.width + viewport.x;
	// assuming origin at top-left, Y down:
	float sy = (c.y * 0.5f + 0.5f) * viewport.height + viewport.y;

	return glm::vec2(sx, sy);
}

glm::vec2 Text::windowToViewportPx(float mx, float my) const {
	if (!engine) {
		return glm::vec2(0.0f);
	}

	int winW = 0, winH = 0;
	GLFWwindow *wnd = engine->getWindow();
	if (wnd) {
		glfwGetWindowSize(wnd, &winW, &winH);
	}

	VkExtent2D ext = engine->getSwapchain().getExtent();
	const int sw = (int)ext.width;
	const int sh = (int)ext.height;

	bool inside = false;
	// Map window coords -> NDC for THIS viewport (same as Model::compute)
	glm::vec2 ndc = Mouse::toNDC(mx, my, winW, winH, sw, sh, viewport.x, viewport.y, viewport.width, viewport.height, &inside);

	// Invert NDC -> viewport pixel coords.
	// NDC in [-1,1]  → (u,v) in [0,1] within this viewport.
	const float u = (ndc.x + 1.0f) * 0.5f;
	const float v = (1.0f - ndc.y) * 0.5f;

	const float px = viewport.x + u * viewport.width;
	const float py = viewport.y + v * viewport.height;

	return glm::vec2(px, py);
}

void Text::setSelectionBoxPx(const glm::vec2 &startPx, const glm::vec2 &endPx) {
	selectionBoxStartPx_ = startPx;
	selectionBoxEndPx_ = endPx;
	selectionBoxActive_ = true;

	if (charRects.empty()) {
		selectionRanges.clear();
		return;
	}

	glm::vec4 selBox = makeRectScreenSpace(startPx, endPx);

	// Walk all characters, project their local rects to screen, test overlap.
	int firstHit = -1;
	int lastHit = -1;

	for (size_t i = 0; i < charRects.size(); ++i) {
		const glm::vec4 &rLocal = charRects[i];

		// Local corners -> screen
		glm::vec2 p0 = localToScreen(glm::vec2(rLocal.x, rLocal.y));
		glm::vec2 p1 = localToScreen(glm::vec2(rLocal.z, rLocal.w));

		glm::vec4 rScreen = makeRectScreenSpace(p0, p1);

		if (rectsOverlap(rScreen, selBox)) {
			if (firstHit < 0)
				firstHit = (int)i;
			lastHit = (int)i;
		}
	}

	selectionRanges.clear();
	if (firstHit >= 0 && lastHit >= 0) {
		// Inclusive character indices, works for both forward & backward drags
		selectionRanges.emplace_back(static_cast<size_t>(firstHit), static_cast<size_t>(lastHit));
	}
}

void Text::clearSelectionBox() {
	selectionBoxActive_ = false;
	if (!selectionRanges.empty()) {
		auto [first, last] = selectionRanges[0];
		for (uint32_t i = static_cast<uint32_t>(first); i <= static_cast<uint32_t>(last) && i < lastcharInstanceCount_; ++i) {
			Rectangle::InstanceData out{};
			charHitboxes->getInstance(i, out);
			out.color = Colors::Transparent(0.0f);
			out.outlineColor = Colors::Transparent(0.0f);
			charHitboxes->upsertInstance(i, out);
		}
	}
	selectionRanges.clear();
}

void Text::prewarmBasicLatinAndBox() {
	if (!features.prewarm) {
		return;
	}
	auto &SA = getSharedAtlas();
	std::lock_guard<std::mutex> lock(SA.mtx);
	auto &fa = SA.fonts[fontPath];
	if (!fa.prewarmGlyphs.empty())
		return;

	for (uint32_t cp = 0x20; cp <= 0x7E; ++cp)
		fa.prewarmGlyphs.push_back(cp); // ASCII
	for (uint32_t cp = 0x2500; cp <= 0x257F; ++cp)
		fa.prewarmGlyphs.push_back(cp); // box-drawing

	needAtlas = true; // force rebuild once with this superset
}

vec4 Text::ansiIndexToColor(int idx, const vec4 &fallback) {
	static const vec4 ansi[8] = {{0, 0, 0, 1}, {0.5, 0, 0, 1}, {0, 0.5, 0, 1}, {0.5, 0.5, 0, 1}, {0, 0, 0.65, 1}, {0.5, 0, 0.5, 1}, {0, 0.5, 0.5, 1}, {1, 1, 1, 1}};
	return (idx >= 30 && idx <= 37) ? ansi[idx - 30] : fallback;
}

std::u32string Text::parseAnsiToRuns(std::vector<ColorRun> &runs) const {
	const std::u32string src = utf8_to_u32(text);
	runs.clear();

	std::u32string clean;
	clean.reserve(src.size());

	vec4 current = baseColor;
	size_t runStart = 0; // index in 'clean' (not in 'src')

	auto push_run = [&](size_t from, size_t to, const vec4 &col) {
		if (to > from)
			runs.push_back(ColorRun{from, to, col});
	};

	// Swallow OSC/DCS/SOS/PM/APC “string-to-ST” families and stray control chars.
	// CSI is handled minimally (m colors); others are consumed silently.
	enum State { NORMAL, ESC_SEEN, CSI, OSC, ST_STRING } state = NORMAL;

	int acc = 0;
	bool haveNum = false; // for collecting CSI parameters

	for (size_t i = 0; i < src.size(); ++i) {
		char32_t c = src[i];

		if (state == NORMAL) {
			// IMPORTANT: handle ESC before generic C0 filtering,
			// otherwise ESC would be swallowed and we'd print "[1m" etc.
			if (c == U'\033') { // ESC
				push_run(runStart, clean.size(), current);
				state = ESC_SEEN;
				acc = 0;
				haveNum = false;
				continue; // do NOT emit ESC
			}
			// swallow CR and other C0 controls (except \n and \t)
			if (c == U'\r')
				continue;
			if (c < 0x20 && c != U'\n' && c != U'\t')
				continue;
			clean.push_back(c);
			continue;
		}

		if (state == ESC_SEEN) {
			if (c == U'[') {
				state = CSI;
				continue;
			} // CSI
			if (c == U']') {
				state = OSC;
				continue;
			} // OSC (to BEL/ST)
			// DCS/SOS/PM/APC → consume until ST (ESC \)
			if (c == U'P' || c == U'X' || c == U'^' || c == U'_') {
				state = ST_STRING;
				continue;
			}
			// Unknown single-ESC sequence → drop it
			state = NORMAL;
			continue;
		}

		// --- CSI: ESC [ ... final (0x40..0x7E) ---
		if (state == CSI) {
			if (c >= U'0' && c <= U'9') {
				acc = acc * 10 + int(c - U'0');
				haveNum = true;
				continue;
			}
			if (c == U';') {
				acc = 0;
				haveNum = false;
				continue;
			}
			// Handle SGR minimally; consume any CSI final
			if (c == U'm') {
				int code = haveNum ? acc : -1;
				runStart = clean.size();
				if (code == 0 || code == 39) {
					// 0 = full reset, 39 = default foreground
					current = baseColor;
				} else if (code >= 30 && code <= 37) {
					current = ansiIndexToColor(code, baseColor);
				} else if (code >= 90 && code <= 97) {
					// bright versions; map as you like
					current = ansiIndexToColor(code, baseColor); // or a brighter palette
				} else if (code == 22) {
					// normal intensity (undo bold); if you track bold, clear it here
					// (no color change needed)
				}
			}
			state = NORMAL;
			acc = 0;
			haveNum = false;
			continue;
		}

		// --- OSC: ESC ] ... terminated by BEL (0x07) or ST (ESC \) ---
		if (state == OSC) {
			if (c == U'\a') {
				state = NORMAL;
				continue;
			} // BEL
			if (c == U'\033') { // possible ST
				// peek next char if present
				if (i + 1 < src.size() && src[i + 1] == U'\\') {
					++i;
					state = NORMAL;
					continue;
				} else {
					// not ST yet; keep swallowing, stay in OSC
					continue;
				}
			}
			continue; // swallow payload
		}

		// --- DCS/SOS/PM/APC or OSC finishing with ST: consume until ESC \ ---
		if (state == ST_STRING) {
			if (c == U'\033') {
				if (i + 1 < src.size() && src[i + 1] == U'\\') {
					++i;
					state = NORMAL;
				}
				// else remain in ST_STRING (payload continues)
			}
			continue;
		}

		// Any other CSI terminator/garbage: drop it and leave CSI
		state = NORMAL;
	}

	// close trailing run
	push_run(runStart, clean.size(), current);
	return clean;
}

// ---------------- FT / SDF / Atlas ----------------
void Text::ensureFT() {
	if (ft)
		return;

	auto &S = getSharedFT();

	// 1) Initialize FT library once
	if (!S.lib) {
		if (FT_Init_FreeType(&S.lib))
			throw std::runtime_error("FT_Init_FreeType failed");
	}

	// 2) Look up (or create) shared FT_Face for this fontPath
	FT_Face face = nullptr;
	auto it = S.faces.find(fontPath);
	if (it != S.faces.end()) {
		face = it->second;
	} else {
		if (FT_New_Face(S.lib, fontPath.c_str(), 0, &face))
			throw std::runtime_error("FT_New_Face failed: " + fontPath);

		// Force Unicode charmap once when we first create this face
		FT_CharMap unicode = findUnicodeCharmap(face);
		if (!unicode) {
			throw std::runtime_error("Font has no Unicode charmap: " + fontPath);
		}
		FT_Set_Charmap(face, unicode);

		S.faces.emplace(fontPath, face);
	}

	// 3) Per-Text FTData just stores pointers to shared lib/face
	ft = std::make_unique<FTData>();
	ft->lib = S.lib;
	ft->face = face;

	// 4) Set per-instance size & SDF properties
	FT_Set_Pixel_Sizes(ft->face, 0, ft->pixelHeight);

	FT_UInt spreadPx = (FT_UInt)ft->sdfSpread;
	FT_Property_Set(ft->lib, "sdf", "spread", &spreadPx);
}

std::vector<uint8_t> Text::bitmapToSDF(const uint8_t *alpha, int w, int h, int spread) {
	auto idx = [&](int x, int y) { return y * w + x; };
	std::vector<float> din(w * h, 1e9f), dout(w * h, 1e9f);
	auto inside = [&](int x, int y) { return alpha[idx(x, y)] > 127; };
	for (int y = 1; y < h - 1; ++y)
		for (int x = 1; x < w - 1; ++x) {
			bool in = inside(x, y), edge = false;
			for (int dy = -1; dy <= 1 && !edge; ++dy)
				for (int dx = -1; dx <= 1 && !edge; ++dx)
					if (inside(x + dx, y + dy) != in)
						edge = true;
			if (edge) {
				(in ? din : dout)[idx(x, y)] = 0;
			}
		}
	auto relax = [&](std::vector<float> &d) {
		bool change = true;
		while (change) {
			change = false;
			for (int y = 1; y < h - 1; ++y)
				for (int x = 1; x < w - 1; ++x) {
					float v = d[idx(x, y)];
					float n = std::min({d[idx(x - 1, y)] + 1, d[idx(x + 1, y)] + 1, d[idx(x, y - 1)] + 1, d[idx(x, y + 1)] + 1, d[idx(x - 1, y - 1)] + 1.4142f, d[idx(x + 1, y - 1)] + 1.4142f, d[idx(x - 1, y + 1)] + 1.4142f, d[idx(x + 1, y + 1)] + 1.4142f});
					if (n + 1e-6f < v) {
						d[idx(x, y)] = n;
						change = true;
					}
				}
		}
	};
	relax(din);
	relax(dout);
	std::vector<uint8_t> out(w * h, 0);
	for (int y = 0; y < h; ++y)
		for (int x = 0; x < w; ++x) {
			float sd = din[idx(x, y)] - dout[idx(x, y)];
			float v = 0.5f + sd / float(spread);
			out[idx(x, y)] = (uint8_t)std::round(clampf(v, 0.f, 1.f) * 255.f);
		}
	return out;
}

// We now use a shared per-font atlas. This method only ensures that
// the shared atlas for this font has all glyphs that this Text needs.
void Text::ensureAtlas() {
	if (!ft || !ft->face)
		return;

	auto &SA = getSharedAtlas();
	std::lock_guard<std::mutex> lock(SA.mtx);
	auto &fa = SA.fonts[fontPath];

	// First time this Text touches this font's atlas: bump refcount
	if (!registeredInSharedAtlas_) {
		++fa.refCount;
		fa.users.insert(this);
		registeredInSharedAtlas_ = true;
	}

	// Point this instance to the shared Atlas struct.
	atlas = &fa.atlas;

	// Parse text & collect glyphs we need
	std::vector<ColorRun> runs;
	std::u32string u32 = parseAnsiToRuns(runs);
	std::unordered_set<uint32_t> need;

	for (auto c : u32) {
		if (c == U'\n')
			continue;
		if ((uint32_t)c < 0x20u)
			continue;
		need.insert((uint32_t)c);
	}
	need.insert((uint32_t)U' ');
	need.insert((uint32_t)U'_');

	// Include shared prewarm glyphs for this font (if any)
	for (uint32_t cp : fa.prewarmGlyphs)
		need.insert(cp);

	// If atlas already exists and we can append, try the incremental path.
	if (fa.atlasReady && fa.atlas.texW > 0 && fa.atlas.texH > 0 && !fa.host.empty()) {
		bool needFullRepack = false;

		for (uint32_t cp : need) {
			if (fa.atlas.glyphs.find(cp) != fa.atlas.glyphs.end())
				continue;
			if (!appendGlyphShared(fa, cp, ft.get(), this)) {
				needFullRepack = true;
				break;
			}
		}

		if (!needFullRepack) {
			// Nothing else to do; GPU already updated for appended glyphs.
			return;
		}

		// Out of room → fall back to a full repack with the superset.
		for (const auto &kv : fa.atlas.glyphs)
			need.insert(kv.first);
	}

	// Full build (initial or after repack), with a deterministic glyph order.
	rebuildFontAtlas(fa, ft.get(), need, this);
	atlas = &fa.atlas;
}

void Text::writeAtlasDescriptor() {
	if (!pipeline)
		return;
	auto &sets = pipeline->descriptorSets.descriptorSets;
	if (sets.empty() || sets[0] == VK_NULL_HANDLE)
		return;
	if (!atlas || !atlas->sampler || !atlas->view)
		return;

	VkDescriptorImageInfo ii{};
	ii.sampler = atlas->sampler;
	ii.imageView = atlas->view;
	ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	w.dstSet = sets[0];
	w.dstBinding = 2; // matches your FS: layout(set=0, binding=2)
	w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	w.descriptorCount = 1;
	w.pImageInfo = &ii;

	vkUpdateDescriptorSets(pipeline->device, 1, &w, 0, nullptr);
}

// ---------------- Layout & geometry ----------------
void Text::buildCharVisualAndHitboxes(const std::vector<glm::vec4> &chars) {
	if (!charHitboxes) {
		charHitboxes = std::make_unique<Rectangle>(scene);
		charHitboxes->setMaxInstances(65535);
		charHitboxes->init();
		charHitboxes->enableRayPicking();

		charHitboxes->onScreenResize = [&](Model *m, float, float, float, float) {
			charHitboxes->setView(vp.view);
			charHitboxes->setProj(vp.proj);
			charHitboxes->setViewport(viewport.width, viewport.height, viewport.x, viewport.y);
		};

		charHitboxes->onTick = [&](Model *m, double, double t) {
			if (!selectionRanges.empty()) {
				auto [first, last] = selectionRanges[0];
				for (uint32_t i = static_cast<uint32_t>(first); i <= static_cast<uint32_t>(last) && i < lastcharInstanceCount_; ++i) {
					Rectangle::InstanceData out{};
					m->getInstance(i, out);
					out.color = selectionColor;
					out.outlineColor = selectionColor;
					m->upsertInstance(i, out);
				}
			}
		};

		charHitboxesInited_ = true;
	}

	// Convert “pixels” to local units so hitboxes have a non-zero size
	const float sx = std::max(1e-6f, modelScaleX(pc.model));
	const float sy = std::max(1e-6f, modelScaleY(pc.model));
	const float onePxLocalX = 1.0f / sx;
	const float onePxLocalY = 1.0f / sy;
	const float minHitboxLocalW = 8.0f * onePxLocalX; // ~8 px wide
	const float minHeightLocal = 1.0f * onePxLocalY;  // ≥1 px tall

	size_t i = 0;
	for (; i < chars.size(); ++i) {
		const glm::vec4 r = chars[i]; // x0,y0,x1,y1 (y may be inverted)
		const float x0 = r.x;
		const float x1 = r.z;
		const float yA = std::min(r.y, r.w);
		const float yB = std::max(r.y, r.w);
		const float y0 = yA;

		const float w = std::max(minHitboxLocalW, x1 - x0);
		const float h = std::max(minHeightLocal, yB - yA);

		const float cx = 0.5f * (x0 + x1);
		const float cy = 0.5f * (yA + yB);

		// Build instance transform placing/scaling the unit quad into [x0..x1]x[y0..y1]
		Rectangle::InstanceData id{};
		id.model = pc.model;
		id.model = glm::translate(id.model, glm::vec3(cx, cy, 0.0f));
		id.model = glm::scale(id.model, glm::vec3(w, h, 1.0f));

		id.color = Colors::Transparent(0.0f);
		id.outlineColor = Colors::Transparent(0.0f);
		id.outlineWidth = 0.0f;
		charHitboxes->upsertInstance(uint32_t(i), id);
	}

	// 3) Hide any stale instances from a previous frame by zeroing alpha
	for (size_t j = i; j < lastcharInstanceCount_; ++j) {
		Rectangle::InstanceData id{};
		id.model = glm::mat4(1.0f);
		id.color = vec4(0, 0, 0, 0); // fully transparent → effectively disabled
		id.outlineColor = vec4(0);
		id.outlineWidth = 0.0f;
		charHitboxes->upsertInstance(uint32_t(j), id);
	}
	lastcharInstanceCount_ = chars.size();
}

void Text::buildCaretVisualAndHitboxes(const std::vector<glm::vec4> &carets) {
	textLength_ = static_cast<uint32_t>(carets.size());

	if (!caretHitboxes) {
		caretHitboxes = std::make_unique<Rectangle>(scene);
		caretHitboxes->setMaxInstances(65535);
		caretHitboxes->init();
		caretHitboxes->enableRayPicking();

		caretHitboxes->onScreenResize = [&](Model *m, float, float, float, float) {
			caretHitboxes->setView(vp.view);
			caretHitboxes->setProj(vp.proj);
			caretHitboxes->setViewport(viewport.width, viewport.height, viewport.x, viewport.y);
		};

		caretHitboxes->onTick = [&](Model *m, double, double t) {
			const auto &hitInfo = m->picking->hitInfo;
			if (hitInfo.hit) {
				caretHoverPosition = hitInfo.primId;
			}
			pc.time = t;
		};

		caretHitboxesInited_ = true;
	}

	// Convert “pixels” to local units so hitboxes have a non-zero size
	const float sx = std::max(1e-6f, modelScaleX(pc.model));
	const float sy = std::max(1e-6f, modelScaleY(pc.model));
	const float onePxLocalX = 1.0f / sx;
	const float onePxLocalY = 1.0f / sy;
	const float minHitboxLocalW = 8.0f * onePxLocalX; // ~8 px wide
	const float minHeightLocal = 1.0f * onePxLocalY;  // ≥1 px tall

	size_t i = 0;
	for (; i < carets.size(); ++i) {
		const glm::vec4 r = carets[i]; // x0,y0,x1,y1 (y may be inverted)
		const float x0 = r.x;
		const float x1 = r.z;
		const float yA = std::min(r.y, r.w);
		const float yB = std::max(r.y, r.w);
		const float y0 = yA;

		const float w = std::max(minHitboxLocalW, x1 - x0);
		const float h = std::max(minHeightLocal, yB - yA);

		const float cx = 0.5f * (x0 + x1);
		const float cy = 0.5f * (yA + yB);

		// Build instance transform placing/scaling the unit quad into [x0..x1]x[y0..y1]
		Rectangle::InstanceData id{};
		id.model = pc.model;
		id.model = glm::translate(id.model, glm::vec3(cx, cy, 0.0f));
		id.model = glm::scale(id.model, glm::vec3(w, h, 1.0f));

		id.color = Colors::Transparent(0.0f);
		id.outlineColor = Colors::Transparent(0.0f);
		id.outlineWidth = 0.0f;

		caretHitboxes->upsertInstance(uint32_t(i), id);
	}

	// 3) Hide any stale instances from a previous frame by zeroing alpha
	for (size_t j = i; j < lastCaretInstanceCount_; ++j) {
		Rectangle::InstanceData id{};
		id.model = glm::mat4(1.0f);
		id.color = vec4(0, 0, 0, 0); // fully transparent → effectively disabled
		id.outlineColor = vec4(0);
		id.outlineWidth = 0.0f;
		caretHitboxes->upsertInstance(uint32_t(j), id);
	}
	lastCaretInstanceCount_ = carets.size();
}

vec2 Text::measureTextBox(std::u32string &u32) {
	if (!atlas)
		return {0, 0};

	float x = 0, y = 0, maxX = 0;
	float lh = (float)ft->pixelHeight + lineSpacing;
	for (auto c : u32) {
		if (c == U'\n') {
			y += lh;
			x = 0;
			continue;
		}
		auto it = atlas->glyphs.find((uint32_t)c);
		if (it == atlas->glyphs.end())
			continue;
		x += (float)it->second.advanceX;
		if (x > maxTextWidthPx && maxTextWidthPx > 0) {
			y += lh;
			x = 0;
		}
		maxX = std::max(maxX, x);
	}
	return {maxX, y + lh};
}

void Text::layoutAndBuild() {
	// --- Clear CPU geometry ---
	cpuVerts.clear();
	cpuIdx.clear();

	charRects.clear();

	// Global bounds of all glyph quads in LOCAL space.
	// Used by billboard mode to normalize inPos to [-0.5, 0.5].
	float textMinX = std::numeric_limits<float>::infinity();
	float textMinY = std::numeric_limits<float>::infinity();
	float textMaxX = -std::numeric_limits<float>::infinity();
	float textMaxY = -std::numeric_limits<float>::infinity();

	if (!atlas || atlas->glyphs.empty()) {
		// Nothing to render yet; keep geometry empty.
		pc.textOriginX = 0.0f;
		pc.textOriginY = 0.0f;
		pc.textExtentX = 1.0f;
		pc.textExtentY = 1.0f;
		return;
	}

	// --- Scale & unit helpers ---
	const float sx = std::max(1e-6f, modelScaleX(pc.model));
	const float sy = std::max(1e-6f, modelScaleY(pc.model));
	const float onePxLocalX = 1.0f / sx;
	const float onePxLocalY = 1.0f / sy;
	const float wrapLocal = maxTextWidthPx > 0.f ? (maxTextWidthPx / sx) : 0.f;

	// Vertical “trim” so boxes don’t graze glyph pixels
	const float yTightPad = 1.0f * onePxLocalY;

	// --- Parse text & ANSI color runs ---
	std::vector<ColorRun> runs;
	std::u32string u32 = parseAnsiToRuns(runs);

	contentHeightPx_ = measureTextBox(u32).y;

	std::vector<vec4> colorOf(u32.size(), baseColor);
	for (const auto &r : runs)
		for (size_t i = r.start; i < r.end && i < colorOf.size(); ++i)
			colorOf[i] = r.color;

	// --- Line metrics ---
	const float lh = (float)ft->pixelHeight + lineSpacing;
	const float ascent = (float)ft->face->size->metrics.ascender / 64.0f;
	const float descent = (float)std::abs(ft->face->size->metrics.descender / 64.0f);

	// --- Accumulate geometry in two buckets (decorations first) ---
	std::vector<Vertex> decoVerts, glyphVerts;
	std::vector<uint32_t> decoIdx, glyphIdx;

	auto addQuad = [](std::vector<Vertex> &V, std::vector<uint32_t> &I, const Vertex q[4]) {
		uint32_t base = (uint32_t)V.size();
		V.insert(V.end(), q, q + 4);
		I.insert(I.end(), {base + 0, base + 1, base + 2, base + 0, base + 2, base + 3});
	};

	auto pushCaretQuad = [&](glm::vec4 r, const vec4 &color) {
		Vertex q[4]{{{r.x, r.y}, {0, 0}, color, 0.0f}, {{r.z, r.y}, {0, 0}, color, 0.0f}, {{r.z, r.w}, {0, 0}, color, 0.0f}, {{r.x, r.w}, {0, 0}, color, 0.0f}};
		addQuad(decoVerts, decoIdx, q);
	};

	std::vector<glm::vec4> caretSlots;
	std::vector<float> caretCentersOnLine;
	std::vector<float> caretBaselines;
	std::vector<float> caretHeightsPx;
	std::vector<float> caretHeightsOnLine;

	// Per-line bitmap bounds (loose = SDF bitmap; tight = bitmap minus SDF spread)
	float lineTopBound = std::numeric_limits<float>::infinity();
	float lineBotBound = -std::numeric_limits<float>::infinity();
	float lineTopBoundTight = std::numeric_limits<float>::infinity();
	float lineBotBoundTight = -std::numeric_limits<float>::infinity();

	auto snapX = [](float x) { return std::round(x); };
	auto pushCaretCenter = [&](float cx) { caretCentersOnLine.push_back(snapX(cx)); };
	auto pushCaretHeight = [&](float h) { caretHeightsOnLine.push_back(h); };

	auto pushGlyph = [&](const Glyph &g, vec2 pos, const vec4 &col) {
		// Pixel-snap baseline position
		const float px = std::round(pos.x);
		const float py = std::round(pos.y);

		const float x0 = px + (float)g.bearingX;
		const float y0 = py - (float)g.bearingY;
		const float x1 = x0 + g.width;
		const float y1 = y0 + g.height;

		// Track full text bounds for billboard mode
		if (g.width > 0 && g.height > 0) {
			textMinX = std::min(textMinX, x0);
			textMinY = std::min(textMinY, y0);
			textMaxX = std::max(textMaxX, x1);
			textMaxY = std::max(textMaxY, y1);
		}

		// Track loose & tight bounds for this line
		if (g.width > 0 && g.height > 0) {
			lineTopBound = std::min(lineTopBound, y0);
			lineBotBound = std::max(lineBotBound, y1);

			const float spread = g.sdfSpreadPx; // SDF distance in *bitmap* pixels
			const float yt0 = y0 + spread;
			const float yt1 = y1 - spread;
			if (yt1 > yt0) {
				lineTopBoundTight = std::min(lineTopBoundTight, yt0);
				lineBotBoundTight = std::max(lineBotBoundTight, yt1);
			}
		}

		// UVs slightly inset to avoid sampling seams
		const float insetU = 0.5f / float(atlas->texW);
		const float insetV = 0.5f / float(atlas->texH);
		const float u0 = g.u0 + insetU, v0 = g.v0 + insetV;
		const float u1 = g.u1 - insetU, v1 = g.v1 - insetV;

		Vertex q[4]{
			{{x0, y0}, {u0, v0}, col, g.sdfSpreadPx},
			{{x1, y0}, {u1, v0}, col, g.sdfSpreadPx},
			{{x1, y1}, {u1, v1}, col, g.sdfSpreadPx},
			{{x0, y1}, {u0, v1}, col, g.sdfSpreadPx},
		};
		uint32_t base = (uint32_t)glyphVerts.size();
		glyphVerts.insert(glyphVerts.end(), q, q + 4);
		glyphIdx.insert(glyphIdx.end(), {base + 0, base + 1, base + 2, base + 0, base + 2, base + 3});
	};

	auto finalizeLine = [&](float baseY) {
		if (caretCentersOnLine.size() < 2) {
			caretCentersOnLine.clear();
			lineTopBound = std::numeric_limits<float>::infinity();
			lineBotBound = -std::numeric_limits<float>::infinity();
			lineTopBoundTight = std::numeric_limits<float>::infinity();
			lineBotBoundTight = -std::numeric_limits<float>::infinity();
			return;
		}

		// Compute a single max height-above-baseline for the entire line
		float maxH = 0.0f;
		for (float h : caretHeightsOnLine)
			maxH = std::max(maxH, h);

		// Prefer tight union; fallback to metrics trimmed by spread
		float y0, y1;
		if (lineTopBoundTight < lineBotBoundTight) {
			y0 = lineTopBoundTight + yTightPad;
			y1 = lineBotBoundTight - yTightPad;
			if (y1 < y0)
				std::swap(y0, y1);
		} else {
			const float spread = ft ? float(ft->sdfSpread) : 0.0f; // bitmap px
			y0 = (baseY - ascent) + spread + yTightPad;
			y1 = (baseY + descent) - spread - yTightPad;
		}

		const float minWLocal = 8.0f * onePxLocalX;
		const float minHLocal = 1.0f * onePxLocalY;
		if (y1 - y0 < minHLocal) {
			const float mid = 0.5f * (y0 + y1);
			y0 = mid - 0.5f * minHLocal;
			y1 = mid + 0.5f * minHLocal;
		}

		const float lineLeft = caretCentersOnLine.front(); // usually 0
		const float lineRight = caretCentersOnLine.back(); // line width in local units

		// -----------------------------
		// build per-character cells
		// -----------------------------
		if (features.selection) {
			const size_t numCarets = caretCentersOnLine.size();
			const size_t numChars = (numCarets > 0) ? numCarets - 1 : 0;

			if (numChars > 0) {
				const float totalWidth = lineRight - lineLeft;
				const float cellWidth = (numChars > 0) ? (totalWidth / float(numChars)) : 0.0f;

				for (size_t i = 0; i < numChars; ++i) {
					const float cx = lineLeft + (float(i) + 0.5f) * cellWidth; // centered
					const float left = cx - 0.5f * cellWidth;
					const float right = cx + 0.5f * cellWidth;
					charRects.emplace_back(left, y0, right, y1);
				}
			}
		}

		// -----------------------------
		// caret hitbox logic
		// -----------------------------
		if (features.caret) {
			for (size_t i = 0; i < caretCentersOnLine.size(); ++i) {
				const float c = caretCentersOnLine[i];

				// Midpoint to neighbors; clamp first/last to line bounds
				float left = (i == 0) ? lineLeft : 0.5f * (caretCentersOnLine[i - 1] + c);
				float right = (i + 1 == caretCentersOnLine.size()) ? lineRight : 0.5f * (c + caretCentersOnLine[i + 1]);

				// Enforce minimum clickable size without creating gaps
				if (right - left < minWLocal) {
					const float mid = 0.5f * (left + right);
					left = mid - 0.5f * minWLocal;
					right = mid + 0.5f * minWLocal;
				}

				caretSlots.emplace_back(left, y0, right, y1);
				caretBaselines.push_back(baseY);
				caretHeightsPx.push_back(maxH);
			}
		}

		// Reset for next line
		caretCentersOnLine.clear();
		caretHeightsOnLine.clear();

		lineTopBound = std::numeric_limits<float>::infinity();
		lineBotBound = -std::numeric_limits<float>::infinity();
		lineTopBoundTight = std::numeric_limits<float>::infinity();
		lineBotBoundTight = -std::numeric_limits<float>::infinity();
	};

	float x = 0.f, y = ascent - scrollOffsetPx_;
	pushCaretCenter(0.f);
	pushCaretHeight(0.0f);

	// Selection accumulator
	for (size_t i = 0; i < u32.size(); ++i) {
		const char32_t c = u32[i];

		if (c == U'\n') {
			pushCaretCenter(x);
			finalizeLine(y);
			x = 0.f;
			y += lh;
			pushCaretCenter(0.f);
			pushCaretHeight(0.0f);
			continue;
		}

		auto itg = atlas->glyphs.find((uint32_t)c);
		if (itg == atlas->glyphs.end())
			continue;
		const Glyph &g = itg->second;

		// Optional wrapping
		if (wrapLocal > 0.f && (x + g.advanceX) > wrapLocal && c != U' ') {
			pushCaretCenter(x);
			finalizeLine(y);
			x = 0.f;
			y += lh;
			pushCaretCenter(0.f);
			pushCaretHeight(0.0f);
		}

		// Draw glyph quad if it has an image
		if (g.width > 0 && g.height > 0)
			pushGlyph(g, vec2(x, y), colorOf[i]);

		// Next caret center at advance boundary (pixel-snapped)
		const float nx = x + (float)g.advanceX;
		// If this is the first glyph on the line, backfill the height for the leading caret
		if (!caretHeightsOnLine.empty() && caretHeightsOnLine[0] == 0.0f) {
			caretHeightsOnLine[0] = std::max(0.0f, float(g.bearingY) - g.sdfSpreadPx);
		}
		// height for the caret AFTER this glyph uses this glyph's bearingY
		pushCaretCenter(nx);
		pushCaretHeight(std::max(0.0f, float(g.bearingY) - g.sdfSpreadPx));
		x = nx;
	}
	finalizeLine(y);

	// Active caret visual: 1 px wide, shifted left by its width and up by (origin - yMin)
	if (caretOn && caretPosition < caretSlots.size()) {
		const auto r = caretSlots[caretPosition];
		const float cx = 0.5f * (r.x + r.z);
		// baseline and per-slot height (pixels)
		const float baseline = (caretPosition < caretBaselines.size()) ? caretBaselines[caretPosition] : y;
		const float hPx = (caretPosition < caretHeightsPx.size()) ? std::max(0.0f, caretHeightsPx[caretPosition]) : 0.0f;
		// caret spans [baseline - hPx, baseline], shifted left by its width
		const float yTop = baseline - hPx;
		const float yBot = baseline;
		const float halfW = 1.0f * onePxLocalX;
		pushCaretQuad({cx - halfW + onePxLocalX, yTop, cx + halfW + onePxLocalX, yBot}, caretColor);
	}

	// Concatenate: decorations first, then glyphs
	cpuVerts = std::move(decoVerts);
	cpuIdx = std::move(decoIdx);
	const uint32_t base = (uint32_t)cpuVerts.size();
	cpuVerts.insert(cpuVerts.end(), glyphVerts.begin(), glyphVerts.end());
	for (uint32_t i : glyphIdx)
		cpuIdx.push_back(base + i);

	// Store text center/extent in push constants (reusing TextPC::_pad[]):
	//   textOriginX/Y = origin, textExtentX/Y = extents
	if (textMinX == std::numeric_limits<float>::infinity()) {
		// No visible glyphs; pick a small dummy quad so math is well-defined.
		pc.textOriginX = 0.0f;
		pc.textOriginY = 0.0f;
		pc.textExtentX = 1.0f;
		pc.textExtentY = 1.0f;
	} else {
		const float cx = 0.5f * (textMinX + textMaxX);
		const float cy = 0.5f * (textMinY + textMaxY);
		const float w = std::max(1e-3f, textMaxX - textMinX);
		const float h = std::max(1e-3f, textMaxY - textMinY);
		pc.textOriginX = cx;
		pc.textOriginY = cy;
		pc.textExtentX = w;
		pc.textExtentY = h;
	}

	// Update caret hitboxes for interaction
	if (features.caret) {
		buildCaretVisualAndHitboxes(caretSlots);
	}
	if (features.selection) {
		buildCharVisualAndHitboxes(charRects);
	}
}

// ---------------- Upload VB/IB (host visible) ----------------
void Text::uploadVBIB() {
	using F = VkFormat;

	initInfo.mesh.vsrc.data = nullptr;
	initInfo.mesh.vsrc.bytes = 0;
	initInfo.mesh.vsrc.stride = sizeof(Vertex);
	initInfo.mesh.isrc.data = nullptr;
	initInfo.mesh.isrc.count = 0;

	initInfo.mesh.vertexAttrs = {
		{0, 0, F::VK_FORMAT_R32G32_SFLOAT, (uint32_t)offsetof(Vertex, pos)},
		{1, 0, F::VK_FORMAT_R32G32_SFLOAT, (uint32_t)offsetof(Vertex, uv)},
		{2, 0, F::VK_FORMAT_R32G32B32A32_SFLOAT, (uint32_t)offsetof(Vertex, color)},
		{3, 0, F::VK_FORMAT_R32_SFLOAT, (uint32_t)offsetof(Vertex, sdfPX)},
	};
}

// ========================= Init (ties into Model) =========================
void Text::init() {
	engine = scene->getScenes().getEngine();
	pipeline->device = engine->getDevice();
	pipeline->physicalDevice = engine->getPhysicalDevice();
	pipeline->graphicsPipeline.colorFormat = engine->getGraphicsBuffer().getSceneColorFormat();
	pipeline->graphicsPipeline.depthFormat = engine->getGraphicsBuffer().getDepthFormat();

	auto ext = engine->getSwapchain().getExtent();
	setViewport((float)ext.width, (float)ext.height);

	// Build the shared atlas BEFORE descriptor allocation
	ensureFT();
	prewarmBasicLatinAndBox();
	needAtlas = true; // first build
	ensureAtlas();	  // performs full build / append + createAtlasGPUForFont()

	layoutAndBuild();
	uploadVBIB();

	initInfo.shaders = Assets::compileShaderProgram(Assets::shaderRootPath + "/text", pipeline->device);
	initInfo.maxInstances = 1;
	initInfo.instanceStrideBytes = 0;

	pipeline->graphicsPipeline.pushConstantRangeCount = 1;
	pipeline->graphicsPipeline.pushContantRanges.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pipeline->graphicsPipeline.pushContantRanges.offset = 0;
	pipeline->graphicsPipeline.pushContantRanges.size = sizeof(TextPC);

	// Allocates descriptor sets
	Model::init();

	// NOW sets exist → bind the atlas
	writeAtlasDescriptor();
}

void Text::setSelection(const std::vector<pair<size_t, size_t>> &ranges) { selectionRanges = ranges; }

// ========================= Overridden hooks =========================
uint32_t Text::createDescriptorPool() {
	// First let Model add UBO + SSBO pool sizes
	uint32_t baseSets = Model::createDescriptorPool();

	// We need capacity for the atlas sampler at set=0, binding=2
	pipeline->descriptorPoolSizes.push_back(VkDescriptorPoolSize{
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		1 // we only have one sampler in this set
	});

	// We only actually use set=0, so 1 is enough,
	// but returning baseSets (2) is also fine.
	return baseSets; // or just return 1;
}

void Text::createDescriptors() {
	// Add binding=2 in set=0 exactly once (Model owns binding=0 UBO)
	if (!addedBinding2) {
		pipeline->createDescriptorSetLayoutBinding(
			/*binding*/ 2,
			/*type*/ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			/*stages*/ VK_SHADER_STAGE_FRAGMENT_BIT,
			/*count*/ 1,
			/*set*/ 0);
		addedBinding2 = true;
	}

	// Let the base allocate pool+sets
	Model::createDescriptors();

	// Now it’s safe to write binding=2 if the atlas exists.
	if (!pipeline)
		return;
	auto &sets = pipeline->descriptorSets.descriptorSets;
	if (sets.empty() || sets[0] == VK_NULL_HANDLE)
		return;
	if (!atlas || !atlas->sampler || !atlas->view)
		return;

	// simple safety for engines without per-frame descriptor mgmt
	vkDeviceWaitIdle(pipeline->device);

	VkDescriptorImageInfo ii{};
	ii.sampler = atlas->sampler;
	ii.imageView = atlas->view;
	ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	w.dstSet = sets[0];
	w.dstBinding = 2;
	w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	w.descriptorCount = 1;
	w.pImageInfo = &ii;

	vkUpdateDescriptorSets(pipeline->device, 1, &w, 0, nullptr);
}

void Text::createGraphicsPipeline() {
	// Build the pipeline as usual
	Model::createGraphicsPipeline();

	if (!cull_) {
		pipeline->graphicsPipeline.rasterizationStateCI.cullMode = VK_CULL_MODE_NONE;
		pipeline->graphicsPipeline.depthStencilStateCI.depthTestEnable = VK_FALSE;
		pipeline->graphicsPipeline.depthStencilStateCI.depthWriteEnable = VK_FALSE;
	} else {
		pipeline->graphicsPipeline.rasterizationStateCI.cullMode = VK_CULL_MODE_FRONT_BIT;
    }
}

void Text::syncPickingInstances() { /* text is not pickable */ }

void Text::updateBuffersGPU() {
	const VkDeviceSize vbytes = cpuVerts.size() * sizeof(Vertex);
	const VkDeviceSize ibytes = cpuIdx.size() * sizeof(uint32_t);

	// 1) Ensure shared arena exists
	SharedTextArena::inst().ensureInit(engine.get());

	// 2) Reserve (or reuse) our slice
	//    - If we already have enough capacity, keep offsets and just upload into them.
	//    - Else, free the old slice and grab a new one.
	auto needV = vbytes;
	auto needI = ibytes;
	bool needNew = (!arenaAllocated_) || (needV > vbCapacity_) || (needI > ibCapacity_);
	if (needNew && arenaAllocated_) {
		SharedTextArena::inst().free(vbOffset_, vbCapacity_, ibOffset_, ibCapacity_);
		arenaAllocated_ = false;
		vbOffset_ = ibOffset_ = vbCapacity_ = ibCapacity_ = 0;
	}
	if (needNew) {
		if (!SharedTextArena::inst().alloc(needV, needI, vbOffset_, ibOffset_, vbCapacity_, ibCapacity_)) {
			std::cerr << "TextArenaOutOfMemory" << std::endl;
			exit(0);
		}
		arenaAllocated_ = true;
	}

	// 3) Upload into our slice
	SharedTextArena::inst().upload(engine.get(), cpuVerts.data(), vbytes, vbOffset_, cpuIdx.data(), ibytes, ibOffset_);

	indexCount = (uint32_t)cpuIdx.size();
}

// ========================= Public API =========================
void Text::setFont(const string &fontPath) {
	this->fontPath = fontPath;
	atlas = nullptr; // will be rebound to the shared atlas for the new font
	needAtlas = true;
	needRebuild = true;
}
void Text::setSize(int size) { fontSize = size; }
void Text::setLocation(const vec3 &location) {
	const float s = 1.0f / ft->pixelHeight * fontSize;
	glm::mat4 model(1.0f);
	model = glm::translate(model, location);
	model = glm::scale(model, {s, s, 1.0f});
	setModel(model);
}
void Text::setText(const std::string &utf8) {
	text = utf8;
	// We recompute needed glyphs lazily in ensureAtlas().
	needAtlas = true;
	needRebuild = true;
}
void Text::setMaxTextWidthPx(float w) {
	maxTextWidthPx = std::max(1.f, w);
	needRebuild = true;
}
void Text::setCaret(size_t pos) {
	caretPosition = (uint32_t)pos;
	needRebuild = true;
}
void Text::setSelectionColor(const glm::vec4 &color) {
	selectionColor = color;
	needRebuild = true;
}
void Text::setLineSpacing(float px) {
	lineSpacing = px;
	needRebuild = true;
}
void Text::setColor(const vec4 &rgba) {
	baseColor = rgba;
	needRebuild = true;
}

void Text::showCaret() {
	caretOn = true;
	needRebuild = true;
}
void Text::hideCaret() {
	caretOn = false;
	needRebuild = true;
}
void Text::setCaretColor(const glm::vec4 &color) { caretColor = color; }

float Text::getContentHeightScreenPx(float bottomPadding) const {
	if (charRects.empty())
		return 0.0f;

	float minY = std::numeric_limits<float>::max();
	float maxY = std::numeric_limits<float>::lowest();

	// charRects[i] = (x0, y0, x1, y1) in LOCAL space
	for (const glm::vec4 &r : charRects) {
		minY = std::min(minY, std::min(r.y, r.w));
		maxY = std::max(maxY, std::max(r.y, r.w));
	}

	const float heightLocal = std::max(0.0f, maxY - minY);

	// Convert local units -> screen pixels using model scale
	const float scaleY = modelScaleY(pc.model);
	const float heightPx = heightLocal * scaleY;

	return heightPx + bottomPadding;
}

void Text::enableTextSelection(bool enable) {
	if (enable == enableTextSelection_)
		return;

	if (enable) {
		enableTextSelection_ = true;

		// Clear any existing selection
		clearSelectionBox();

		// --- Mouse button events (start / end drag) ---
		textSelectMouseClickEventId = Events::registerMouseClick([this](int button, int action, int mods) {
			if (button != Events::MOUSE_BUTTON_LEFT)
				return;

			float mx = 0.0f, my = 0.0f;
			Mouse::getPixel(mx, my);

			glm::vec2 vpPx = windowToViewportPx(mx, my);

			if (action == Events::ACTION_PRESS) {
				// Start potential selection, but don't actually create one yet
				clearSelectionBox();
				selecting = true;

				dragSelectionStarted = false; // <--- NEW
				dragStartPx_ = vpPx;
				lastMousePx_ = dragStartPx_;

				// IMPORTANT: do NOT call setSelectionBoxPx here.
				// We wait until the mouse moves enough.
			} else if (action == Events::ACTION_RELEASE) {
				// End drag; if the user never moved past the threshold,
				// there is simply no selection.
				selecting = false;
				dragSelectionStarted = false;
			}
		});

		textSelectMouseMoveEventId = Events::registerCursor([this](GLFWwindow* win, double mx, double my) {
			glm::vec2 vpPx = windowToViewportPx((float)mx, (float)my);
			lastMousePx_ = vpPx;

			if (!selecting)
				return;

			// Check if we've moved enough to actually start a selection
			if (!dragSelectionStarted) {
				glm::vec2 delta = vpPx - dragStartPx_;
				if (std::abs(delta.x) < selectionMinDragDistancePx_ && std::abs(delta.y) < selectionMinDragDistancePx_) {
					// Not enough movement yet → still "click", no selection
					return;
				}
				// Threshold crossed: real drag selection starts now
				dragSelectionStarted = true;
			}

			// Once selection has started, update the selection box normally
			setSelectionBoxPx(dragStartPx_, lastMousePx_);
		});

	} else {
		// Disable selection: clear state & unregister events
		enableTextSelection_ = false;
		selecting = false;
		dragSelectionStarted = false;
		clearSelectionBox();

		if (!textSelectMouseClickEventId.empty()) {
			Events::unregisterMouseClick(textSelectMouseClickEventId);
			textSelectMouseClickEventId.clear();
		}
		if (!textSelectMouseMoveEventId.empty()) {
			Events::unregisterCursor(textSelectMouseMoveEventId);
			textSelectMouseMoveEventId.clear();
		}
	}
}

void Text::rebuild() {
	if (needAtlas) {
		// Make sure the shared atlas has all glyphs we need.
		// This may append glyphs and upload sub-rects,
		// but it does NOT change the sampler or image view.
		ensureAtlas();
		needAtlas = false;
	}

	layoutAndBuild();
	updateBuffersGPU();
	needRebuild = false;
}

void Text::record(VkCommandBuffer cmd) {
	const auto &dev = pipeline->device;
	const auto &pipe = pipeline->pipeline;
	const auto &pipeLayout = pipeline->pipelineLayout;
	const auto &dsets = pipeline->descriptorSets.descriptorSets;
	const auto &dynOffsets = pipeline->descriptorSets.dynamicOffsets;

	// SAFETY: only map if UBO memory actually exists
	if (uboDirty && umem != VK_NULL_HANDLE) {
		void *up = nullptr;
		VK_CHECK(vkMapMemory(dev, umem, 0, VK_WHOLE_SIZE, 0, &up));
		std::memcpy(up, &vp, sizeof(VPMatrix));
		vkUnmapMemory(dev, umem);
		uboDirty = false;
	}

	// Bail if we don't have geometry yet
	if (!SharedTextArena::inst().inited || indexCount == 0)
		return;

	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
	if (!dsets.empty()) {
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeLayout, 0, static_cast<uint32_t>(dsets.size()), dsets.data(), static_cast<uint32_t>(dynOffsets.size()), dynOffsets.empty() ? nullptr : dynOffsets.data());
	}

	vkCmdPushConstants(cmd, pipeLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(TextPC), &pc);

	// Bind shared arena buffers at our slice offsets
	VkBuffer vbs[1] = {SharedTextArena::inst().vbuf};
	VkDeviceSize offs[1] = {vbOffset_};
	vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offs);

	vkCmdBindIndexBuffer(cmd, SharedTextArena::inst().ibuf, ibOffset_, VK_INDEX_TYPE_UINT32);

	// draw exactly one instance (text is not instanced)
	vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
}
