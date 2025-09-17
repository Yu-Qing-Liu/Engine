#include "instancedobject.hpp"
#include "assets.hpp"
#include "object.hpp"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <stb_image.hpp>

InstancedObject::InstancedObject(Scene* scene, const UBO &ubo, ScreenParams &screenParams, const std::string &objPath, shared_ptr<unordered_map<int, InstancedObjectData>> instances, uint32_t maxInstances) : objPath(objPath), InstancedModel(scene, ubo, screenParams, Assets::shaderRootPath + "/instanced/instancedobject", std::move(instances), maxInstances) {
	loadModel();

	createDescriptorSetLayout();
	createMaterialDescriptorSetLayout();

	createUniformBuffers();
	createDescriptorPool();
	createDescriptorSets();

	createVertexBuffer<Vertex>(this->vertices);
	createIndexBuffer();

	createMaterialResources();
	createMaterialDescriptorSets();

	createBindingDescriptions();
	createGraphicsPipeline();

	createComputeDescriptorSetLayout();
	createShaderStorageBuffers();
	createComputeDescriptorSets();
	createComputePipeline();
}

InstancedObject::~InstancedObject() {
	if (materialsBuf) {
		vkDestroyBuffer(Engine::device, materialsBuf, nullptr);
		materialsBuf = VK_NULL_HANDLE;
	}
	if (materialsMem) {
		vkFreeMemory(Engine::device, materialsMem, nullptr);
		materialsMem = VK_NULL_HANDLE;
	}
	if (materialPool) {
		vkDestroyDescriptorPool(Engine::device, materialPool, nullptr);
		materialPool = VK_NULL_HANDLE;
	}
	if (materialDSL) {
		vkDestroyDescriptorSetLayout(Engine::device, materialDSL, nullptr);
		materialDSL = VK_NULL_HANDLE;
	}
	destroyLoadedTextures();
}

void InstancedObject::buildBVH() {
    Model::buildBVH<Vertex>(vertices);
}

// ---------- small helpers ----------
VkFormat InstancedObject::formatFor(aiTextureType type) {
	switch (type) {
	case aiTextureType_BASE_COLOR:
	case aiTextureType_DIFFUSE:
	case aiTextureType_EMISSIVE:
		return VK_FORMAT_R8G8B8A8_SRGB; // color-like
	default:
		return VK_FORMAT_R8G8B8A8_UNORM; // data-like
	}
}

std::string InstancedObject::cacheKeyWithFormat(const std::string &raw, VkFormat fmt) { return raw + (fmt == VK_FORMAT_R8G8B8A8_SRGB ? "|SRGB" : "|LIN"); }

// ---------- loading ----------
void InstancedObject::loadModel() {
	Assimp::Importer import;
	unsigned flags = aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace | aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality | aiProcess_SortByPType;

	directory = objPath.substr(0, objPath.find_last_of('/'));

#if ANDROID_VK
	import.SetIOHandler(new Platform::AAssetIOSystem(g_app->activity->assetManager, /*base=*/""));
	std::string rel = Assets::toAssetRel(objPath);
	if (rel.empty()) {
		rel = std::filesystem::path(objPath).filename().string();
	}
	const aiScene *scene = import.ReadFile(rel.c_str(), flags);
#else
	const aiScene *scene = import.ReadFile(objPath.c_str(), flags);
#endif
	if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
		std::cout << "ERROR::ASSIMP::" << import.GetErrorString() << std::endl;
		throw std::runtime_error("Assimp load failed");
	}

	size_t vCap = 0, iCap = 0;
	for (unsigned i = 0; i < scene->mNumMeshes; ++i) {
		vCap += scene->mMeshes[i]->mNumVertices;
		iCap += scene->mMeshes[i]->mNumFaces * 3;
	}
	vertices.reserve(vCap);
	indices.reserve(iCap);

	processNode(scene->mRootNode, scene);
	bakeTexturesAndMaterials(scene);
}

void InstancedObject::processNode(aiNode *node, const aiScene *scene) {
	for (unsigned int i = 0; i < node->mNumMeshes; i++) {
		aiMesh *m = scene->mMeshes[node->mMeshes[i]];

		const uint32_t base = static_cast<uint32_t>(vertices.size());
		const uint32_t matId = m->mMaterialIndex;

		for (unsigned v = 0; v < m->mNumVertices; ++v) {
			Vertex vert{};
			vert.pos = {m->mVertices[v].x, m->mVertices[v].y, m->mVertices[v].z};
			vert.nrm = m->HasNormals() ? glm::vec3(m->mNormals[v].x, m->mNormals[v].y, m->mNormals[v].z) : glm::vec3(0, 0, 1);
			vert.col = m->HasVertexColors(0) ? glm::vec4(m->mColors[0][v].r, m->mColors[0][v].g, m->mColors[0][v].b, m->mColors[0][v].a) : glm::vec4(1.0f);

			if (m->HasTextureCoords(0)) {
				vert.uv = {m->mTextureCoords[0][v].x, m->mTextureCoords[0][v].y}; // already flipped by Assimp
			} else {
				vert.uv = glm::vec2(0.0f);
			}

			if (m->HasTangentsAndBitangents()) {
				glm::vec3 T(m->mTangents[v].x, m->mTangents[v].y, m->mTangents[v].z);
				glm::vec3 B(m->mBitangents[v].x, m->mBitangents[v].y, m->mBitangents[v].z);
				glm::vec3 N = vert.nrm;
				float sgn = (glm::dot(glm::cross(N, T), B) < 0.0f) ? -1.0f : 1.0f;
				vert.tan_sgn = glm::vec4(glm::normalize(T), sgn);
			} else {
				vert.tan_sgn = glm::vec4(1, 0, 0, 1);
			}

			vert.materialId = matId;
			vertices.push_back(vert);
		}

		for (unsigned f = 0; f < m->mNumFaces; ++f) {
			const aiFace &face = m->mFaces[f];
			for (unsigned j = 0; j < face.mNumIndices; ++j)
				indices.push_back(base + face.mIndices[j]);
		}
	}
	for (unsigned int i = 0; i < node->mNumChildren; i++)
		processNode(node->mChildren[i], scene);
}

static inline uint32_t SetFlag(uint32_t flags, bool cond, uint32_t bit) { return cond ? (flags | (1u << bit)) : flags; }

void InstancedObject::bakeTexturesAndMaterials(const aiScene *scene) {
	textures.clear();
	textureCache.clear();
	texSlots.clear(); // harmless even if not used anymore

	materialsGPU.clear();
	materialsGPU.reserve(scene ? scene->mNumMaterials : 0);
	if (!scene)
		return;

	for (unsigned mi = 0; mi < scene->mNumMaterials; ++mi) {
		const aiMaterial *m = scene->mMaterials[mi];

		Material g{};
		// Indices default to -1 (no texture)
		g.baseColor = g.normal = g.roughness = g.metallic = g.specular = g.ao = g.emissive = g.opacity = g.displacement = -1;

		// Factors (std430-friendly vec4 packs)
		g.baseColorFactor = glm::vec4(1.0f);		  // rgba = 1
		g.emissiveFactor_roughness = glm::vec4(0.0f); // xyz emissive, w roughness
		g.emissiveFactor_roughness.w = 1.0f;		  // roughness default = very rough
		g.metallic_flags_pad = glm::vec4(0.0f);		  // x metallic scalar, y flags (as float)

		// --- Texture indices (Assimp PBR first, then classic) ---
		// Base color / Diffuse
		g.baseColor = getOrLoadTexture(scene, directory, m, aiTextureType_BASE_COLOR);
		if (g.baseColor < 0)
			g.baseColor = getOrLoadTexture(scene, directory, m, aiTextureType_DIFFUSE);

		// Normal (some assets use HEIGHT for TS normals)
		g.normal = getOrLoadTexture(scene, directory, m, aiTextureType_NORMALS);
		if (g.normal < 0)
			g.normal = getOrLoadTexture(scene, directory, m, aiTextureType_HEIGHT);

		// Roughness (or shininess as fallback)
		g.roughness = getOrLoadTexture(scene, directory, m, aiTextureType_DIFFUSE_ROUGHNESS);
		if (g.roughness < 0)
			g.roughness = getOrLoadTexture(scene, directory, m, aiTextureType_SHININESS);

		// Metallic
		g.metallic = getOrLoadTexture(scene, directory, m, aiTextureType_METALNESS);

		// Specular
		g.specular = getOrLoadTexture(scene, directory, m, aiTextureType_SPECULAR);

		// AO / Lightmap / Ambient
		g.ao = getOrLoadTexture(scene, directory, m, aiTextureType_AMBIENT);
		if (g.ao < 0)
			g.ao = getOrLoadTexture(scene, directory, m, aiTextureType_LIGHTMAP);

		// Emissive
		g.emissive = getOrLoadTexture(scene, directory, m, aiTextureType_EMISSIVE);

		// Opacity
		g.opacity = getOrLoadTexture(scene, directory, m, aiTextureType_OPACITY);

		// Displacement / Height
		g.displacement = getOrLoadTexture(scene, directory, m, aiTextureType_DISPLACEMENT);
		if (g.displacement < 0)
			g.displacement = getOrLoadTexture(scene, directory, m, aiTextureType_HEIGHT);

		// --- Constant factors from the material ---
		// Base color factor (diffuse)
		aiColor4D dcol;
		if (AI_SUCCESS == m->Get(AI_MATKEY_COLOR_DIFFUSE, dcol)) {
			g.baseColorFactor = glm::vec4(dcol.r, dcol.g, dcol.b, dcol.a);
		}

		// Emissive (xyz)
		aiColor3D emis;
		if (AI_SUCCESS == m->Get(AI_MATKEY_COLOR_EMISSIVE, emis)) {
			g.emissiveFactor_roughness.x = emis.r;
			g.emissiveFactor_roughness.y = emis.g;
			g.emissiveFactor_roughness.z = emis.b;
		}

		// Roughness fallback from shininess (Assimp shininess 0..128)
		float shininess = 0.0f;
		if (AI_SUCCESS == m->Get(AI_MATKEY_SHININESS, shininess)) {
			g.emissiveFactor_roughness.w = glm::clamp(1.0f - (shininess / 128.0f), 0.04f, 1.0f);
		}

		// Optional metallic scalar if you have a custom key; default 0.0
		g.metallic_flags_pad.x = 0.0f;

		// Presence flags -> pack as bits into a float
		uint32_t flags = 0;
		auto setFlagBit = [](uint32_t F, bool c, uint32_t b) { return c ? (F | (1u << b)) : F; };
		flags = setFlagBit(flags, g.baseColor >= 0, 0);
		flags = setFlagBit(flags, g.normal >= 0, 1);
		flags = setFlagBit(flags, g.roughness >= 0, 2);
		flags = setFlagBit(flags, g.metallic >= 0, 3);
		flags = setFlagBit(flags, g.specular >= 0, 4);
		flags = setFlagBit(flags, g.ao >= 0, 5);
		flags = setFlagBit(flags, g.emissive >= 0, 6);
		flags = setFlagBit(flags, g.opacity >= 0, 7);
		flags = setFlagBit(flags, g.displacement >= 0, 8);

		g.metallic_flags_pad.y = glm::uintBitsToFloat(flags);

		// Done
		materialsGPU.push_back(g);
	}

	// If scene had no materials, keep one default material
	if (materialsGPU.empty()) {
		Material g{};
		g.baseColor = g.normal = g.roughness = g.metallic = g.specular = g.ao = g.emissive = g.opacity = g.displacement = -1;

		g.baseColorFactor = glm::vec4(1.0f);
		g.emissiveFactor_roughness = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		g.metallic_flags_pad = glm::vec4(0.0f);

		materialsGPU.push_back(g);
	}
}

// Return texture index in textures[], or -1
int InstancedObject::getOrLoadTexture(const aiScene *scene, const std::string &directory, const aiMaterial *mat, aiTextureType type, unsigned slot) {
	aiString str;
	if (mat->GetTexture(type, slot, &str) != AI_SUCCESS)
		return -1;
	return loadTextureFromAssimpString(scene, directory, str, type);
}

int InstancedObject::loadTextureFromAssimpString(const aiScene *scene, const std::string &directory, const aiString &str, aiTextureType type) {
	std::string key = str.C_Str();
	if (key.empty())
		return -1;

	const VkFormat fmt = formatFor(type);

	if (key[0] == '*') {
		std::string cacheKey = cacheKeyWithFormat(key, fmt);
		auto it = textureCache.find(cacheKey);
		if (it != textureCache.end())
			return it->second;

		int idx = std::atoi(key.c_str() + 1);
		if (idx < 0 || (unsigned)idx >= scene->mNumTextures)
			return -1;
		const aiTexture *tex = scene->mTextures[idx];

		int texWidth = 0, texHeight = 0;
		std::vector<unsigned char> rgba;

		if (tex->mHeight == 0) {
			const unsigned char *bytes = reinterpret_cast<const unsigned char *>(tex->pcData);
			int w, h, ch;
			stbi_uc *data = stbi_load_from_memory(bytes, tex->mWidth, &w, &h, &ch, STBI_rgb_alpha);
			if (!data)
				return -1;
			rgba.assign(data, data + (w * h * 4));
			texWidth = w;
			texHeight = h;
			stbi_image_free(data);
		} else {
			texWidth = (int)tex->mWidth;
			texHeight = (int)tex->mHeight;
			rgba.resize(texWidth * texHeight * 4);
			for (int i = 0; i < texWidth * texHeight; ++i) {
				rgba[i * 4 + 0] = tex->pcData[i].r;
				rgba[i * 4 + 1] = tex->pcData[i].g;
				rgba[i * 4 + 2] = tex->pcData[i].b;
				rgba[i * 4 + 3] = tex->pcData[i].a;
			}
		}

		VkBuffer stg;
		VkDeviceMemory stgMem;
		VkDeviceSize imageSize = (VkDeviceSize)rgba.size();
		Engine::createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stg, stgMem);
		void *p = nullptr;
		vkMapMemory(Engine::device, stgMem, 0, imageSize, 0, &p);
		std::memcpy(p, rgba.data(), (size_t)imageSize);
		vkUnmapMemory(Engine::device, stgMem);

		Texture lt{};
		Engine::createImage(texWidth, texHeight, fmt, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, lt.image, lt.memory);

		Engine::transitionImageLayout(lt.image, fmt, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		Engine::copyBufferToImage(stg, lt.image, (uint32_t)texWidth, (uint32_t)texHeight);
		Engine::transitionImageLayout(lt.image, fmt, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		vkDestroyBuffer(Engine::device, stg, nullptr);
		vkFreeMemory(Engine::device, stgMem, nullptr);

		lt.view = Engine::createImageView(lt.image, fmt, VK_IMAGE_ASPECT_COLOR_BIT);
		lt.sampler = createDefaultSampler();
		lt.width = texWidth;
		lt.height = texHeight;

		int outIdx = (int)textures.size();
		if (outIdx >= OBJMODEL_MAX_TEXTURES)
			return -1;
		textures.push_back(lt);
		textureCache[cacheKey] = outIdx;
		return outIdx;
	} else {
		std::string path = directory + "/" + key;
		std::string cacheKey = cacheKeyWithFormat(path, fmt);
		auto it = textureCache.find(cacheKey);
		if (it != textureCache.end())
			return it->second;

		int w, h, ch;
		stbi_uc *pixels = nullptr;

		auto bytes = Assets::loadBytes(path);
		if (!bytes.empty()) {
			pixels = stbi_load_from_memory(bytes.data(), (int)bytes.size(), &w, &h, &ch, STBI_rgb_alpha);
		} else {
			// last-ditch try direct path (useful on desktop/dev)
			pixels = stbi_load(path.c_str(), &w, &h, &ch, STBI_rgb_alpha);
		}

		if (!pixels) {
			throw std::runtime_error("assimp load texture failed");
			return -1;
		}

		VkDeviceSize imageSize = (VkDeviceSize)w * h * 4;
		VkBuffer stg;
		VkDeviceMemory stgMem;
		Engine::createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stg, stgMem);

		void *data = nullptr;
		vkMapMemory(Engine::device, stgMem, 0, imageSize, 0, &data);
		std::memcpy(data, pixels, (size_t)imageSize);
		vkUnmapMemory(Engine::device, stgMem);
		stbi_image_free(pixels);

		Texture lt{};
		Engine::createImage(w, h, fmt, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, lt.image, lt.memory);

		Engine::transitionImageLayout(lt.image, fmt, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		Engine::copyBufferToImage(stg, lt.image, (uint32_t)w, (uint32_t)h);
		Engine::transitionImageLayout(lt.image, fmt, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		vkDestroyBuffer(Engine::device, stg, nullptr);
		vkFreeMemory(Engine::device, stgMem, nullptr);

		lt.view = Engine::createImageView(lt.image, fmt, VK_IMAGE_ASPECT_COLOR_BIT);
		lt.sampler = createDefaultSampler();
		lt.width = (uint32_t)w;
		lt.height = (uint32_t)h;

		int outIdx = (int)textures.size();
		if (outIdx >= OBJMODEL_MAX_TEXTURES)
			return -1;
		textures.push_back(lt);
		textureCache[cacheKey] = outIdx;
		return outIdx;
	}
}

// ---------- material GPU resources ----------
int InstancedObject::createSolidTexture(uint8_t r, uint8_t g, uint8_t b, uint8_t a, VkFormat fmt) {
	uint8_t px[4] = {r, g, b, a};
	VkBuffer stg;
	VkDeviceMemory stgMem;
	Engine::createBuffer(4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stg, stgMem);
	void *p = nullptr;
	vkMapMemory(Engine::device, stgMem, 0, 4, 0, &p);
	std::memcpy(p, px, 4);
	vkUnmapMemory(Engine::device, stgMem);

	Texture lt{};
	Engine::createImage(1, 1, fmt, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, lt.image, lt.memory);

	Engine::transitionImageLayout(lt.image, fmt, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	Engine::copyBufferToImage(stg, lt.image, 1, 1);
	Engine::transitionImageLayout(lt.image, fmt, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkDestroyBuffer(Engine::device, stg, nullptr);
	vkFreeMemory(Engine::device, stgMem, nullptr);

	lt.view = Engine::createImageView(lt.image, fmt, VK_IMAGE_ASPECT_COLOR_BIT);
	lt.sampler = createDefaultSampler();
	lt.width = 1;
	lt.height = 1;

	int idx = (int)textures.size();
	if (idx >= OBJMODEL_MAX_TEXTURES)
		return -1;
	textures.push_back(lt);
	return idx;
}

VkSampler InstancedObject::createDefaultSampler() const {
	VkPhysicalDeviceProperties props{};
	vkGetPhysicalDeviceProperties(Engine::physicalDevice, &props);

	VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
	si.magFilter = VK_FILTER_LINEAR;
	si.minFilter = VK_FILTER_LINEAR;
	si.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	si.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	si.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	si.anisotropyEnable = VK_TRUE;
	si.maxAnisotropy = props.limits.maxSamplerAnisotropy;
	si.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	si.unnormalizedCoordinates = VK_FALSE;
	si.compareEnable = VK_FALSE;
	si.compareOp = VK_COMPARE_OP_ALWAYS;
	si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

	VkSampler s{};
	if (vkCreateSampler(Engine::device, &si, nullptr, &s) != VK_SUCCESS) {
		throw std::runtime_error("OBJModel: sampler create failed");
	}
	return s;
}

void InstancedObject::destroyLoadedTextures() {
	for (auto &t : textures) {
		if (t.sampler)
			vkDestroySampler(Engine::device, t.sampler, nullptr);
		if (t.view)
			vkDestroyImageView(Engine::device, t.view, nullptr);
		if (t.image)
			vkDestroyImage(Engine::device, t.image, nullptr);
		if (t.memory)
			vkFreeMemory(Engine::device, t.memory, nullptr);
		t = {};
	}
	textures.clear();
}

void InstancedObject::createMaterialDescriptorSetLayout() {
	VkDescriptorSetLayoutBinding texArr{};
	texArr.binding = 0;
	texArr.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	texArr.descriptorCount = OBJMODEL_MAX_TEXTURES; // must match shader
	texArr.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutBinding mats{};
	mats.binding = 1;
	mats.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	mats.descriptorCount = 1;
	mats.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	std::array<VkDescriptorSetLayoutBinding, 2> bindings{texArr, mats};

	VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
	ci.bindingCount = (uint32_t)bindings.size();
	ci.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(Engine::device, &ci, nullptr, &materialDSL) != VK_SUCCESS)
		throw std::runtime_error("OBJModel: material DSL create failed");

	std::array<VkDescriptorPoolSize, 2> ps{};
	ps[0] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, OBJMODEL_MAX_TEXTURES};
	ps[1] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};

	VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
	pci.poolSizeCount = (uint32_t)ps.size();
	pci.pPoolSizes = ps.data();
	pci.maxSets = 1;

	if (vkCreateDescriptorPool(Engine::device, &pci, nullptr, &materialPool) != VK_SUCCESS)
		throw std::runtime_error("OBJModel: material descriptor pool create failed");
}

void InstancedObject::createMaterialResources() {
	// Ensure dummies exist FIRST so we can skip them in mapping
	if (dummyWhiteIndex < 0)
		dummyWhiteIndex = createSolidTexture(255, 255, 255, 255, VK_FORMAT_R8G8B8A8_SRGB);
	if (dummyFlatNormalIndex < 0)
		dummyFlatNormalIndex = createSolidTexture(128, 128, 255, 255, VK_FORMAT_R8G8B8A8_UNORM);

	// Build a stable mapping: textures[] index -> descriptor slot.
	// slot 0 is reserved WHITE. Skip dummies from real slots.
	texSlots.assign(textures.size(), 0);
	uint32_t slotCursor = 1; // first real slot

	for (size_t i = 0; i < textures.size() && slotCursor < OBJMODEL_MAX_TEXTURES; ++i) {
		if ((int)i == dummyWhiteIndex || (int)i == dummyFlatNormalIndex)
			continue; // never give them a real slot
		const auto &t = textures[i];
		if (!t.view || !t.sampler)
			continue;
		texSlots[i] = (int)slotCursor++;
	}

	auto toSlot = [&](int idx) -> int {
		if (idx < 0 || (size_t)idx >= texSlots.size())
			return 0;
		int s = texSlots[(size_t)idx];
		return (s > 0 && s < OBJMODEL_MAX_TEXTURES) ? s : 0;
	};

	// Remap materials: keep 0 for "no texture"; valid slots are >=1
	for (auto &m : materialsGPU) {
		m.baseColor = toSlot(m.baseColor);
		m.normal = toSlot(m.normal);
		m.roughness = toSlot(m.roughness);
		m.metallic = toSlot(m.metallic);
		m.specular = toSlot(m.specular);
		m.ao = toSlot(m.ao);
		m.emissive = toSlot(m.emissive);
		m.opacity = toSlot(m.opacity);
		m.displacement = toSlot(m.displacement);
	}

	// Upload materials SSBO
	VkDeviceSize sz = sizeof(Material) * materialsGPU.size();
	if (materialsBuf) {
		vkDestroyBuffer(Engine::device, materialsBuf, nullptr);
		materialsBuf = VK_NULL_HANDLE;
	}
	if (materialsMem) {
		vkFreeMemory(Engine::device, materialsMem, nullptr);
		materialsMem = VK_NULL_HANDLE;
	}

	VkBuffer staging = VK_NULL_HANDLE;
	VkDeviceMemory stagingMem = VK_NULL_HANDLE;
	Engine::createBuffer(sz, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging, stagingMem);

	void *mapped = nullptr;
	vkMapMemory(Engine::device, stagingMem, 0, sz, 0, &mapped);
	std::memcpy(mapped, materialsGPU.data(), (size_t)sz);
	vkUnmapMemory(Engine::device, stagingMem);

	Engine::createBuffer(sz, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, materialsBuf, materialsMem);

	Engine::copyBuffer(staging, materialsBuf, sz);

	vkDestroyBuffer(Engine::device, staging, nullptr);
	vkFreeMemory(Engine::device, stagingMem, nullptr);
}

void InstancedObject::createMaterialDescriptorSets() {
	VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
	ai.descriptorPool = materialPool;
	ai.descriptorSetCount = 1;
	ai.pSetLayouts = &materialDSL;
	if (vkAllocateDescriptorSets(Engine::device, &ai, &materialDS) != VK_SUCCESS)
		throw std::runtime_error("OBJModel: material DS alloc failed");

	// 1) Fill everything with WHITE (slot 0 stays the default)
	std::vector<VkDescriptorImageInfo> imageInfos(OBJMODEL_MAX_TEXTURES);
	const auto &white = textures[(size_t)dummyWhiteIndex];
	for (uint32_t i = 0; i < OBJMODEL_MAX_TEXTURES; ++i) {
		imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfos[i].imageView = white.view;
		imageInfos[i].sampler = white.sampler;
	}

	// 2) Place each REAL texture at its assigned SLOT
	//    (texSlots.size() == textures.size(); texSlots[i] in [0..255], 0 means white/dummy)
	for (size_t i = 0; i < textures.size(); ++i) {
		int slot = (i < texSlots.size()) ? texSlots[i] : 0;
		if (slot <= 0 || slot >= OBJMODEL_MAX_TEXTURES)
			continue; // 0 = default white, or out of range
		const auto &t = textures[i];
		if (!t.view || !t.sampler)
			continue;

		imageInfos[(uint32_t)slot].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfos[(uint32_t)slot].imageView = t.view;
		imageInfos[(uint32_t)slot].sampler = t.sampler;
	}

	VkDescriptorBufferInfo matInfo{materialsBuf, 0, VK_WHOLE_SIZE};

	std::array<VkWriteDescriptorSet, 2> w{};
	w[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	w[0].dstSet = materialDS;
	w[0].dstBinding = 0;
	w[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	w[0].descriptorCount = OBJMODEL_MAX_TEXTURES;
	w[0].pImageInfo = imageInfos.data();

	w[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	w[1].dstSet = materialDS;
	w[1].dstBinding = 1;
	w[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	w[1].descriptorCount = 1;
	w[1].pBufferInfo = &matInfo;

	vkUpdateDescriptorSets(Engine::device, (uint32_t)w.size(), w.data(), 0, nullptr);
}

void InstancedObject::createBindingDescriptions() {
	// binding 0: per-vertex
	vertexBD.binding = 0;
	vertexBD.stride = sizeof(Vertex);
	vertexBD.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	// binding 1: per-instance (mat4 model -> 4x vec4 = 4 attributes)
	instanceBD.binding = 1;
	instanceBD.stride = sizeof(InstancedObjectData);
	instanceBD.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

	bindings = {vertexBD, instanceBD};

	// Start with the mesh vertex attributes (locations 0..5)
	attributes = Vertex::getAttributeDescriptions();

	// Then append 4 attributes for the instance model matrix columns (locations 6..9)
	// NOTE: std140-style column-major mat4 as 4 vec4 attributes.
	const uint32_t locBase = 6;
	attributes.push_back({locBase + 0, 1, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(offsetof(InstancedObjectData, model) + sizeof(glm::vec4) * 0)});
	attributes.push_back({locBase + 1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(offsetof(InstancedObjectData, model) + sizeof(glm::vec4) * 1)});
	attributes.push_back({locBase + 2, 1, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(offsetof(InstancedObjectData, model) + sizeof(glm::vec4) * 2)});
	attributes.push_back({locBase + 3, 1, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(offsetof(InstancedObjectData, model) + sizeof(glm::vec4) * 3)});
}

void InstancedObject::setupGraphicsPipeline() {
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = (uint32_t)bindings.size();
	vertexInputInfo.pVertexBindingDescriptions = bindings.data();
	vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)attributes.size();
	vertexInputInfo.pVertexAttributeDescriptions = attributes.data();

	colorBlendAttachment.blendEnable = VK_FALSE;

	setLayouts = {descriptorSetLayout, materialDSL};
	pipelineLayoutInfo.setLayoutCount = (uint32_t)setLayouts.size();
	pipelineLayoutInfo.pSetLayouts = setLayouts.data();
}

void InstancedObject::bindExtraDescriptorSets(VkCommandBuffer cmd) { vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &materialDS, 0, nullptr); }
