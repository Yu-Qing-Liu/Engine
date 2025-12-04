#include "asset.hpp"

#include "assets.hpp"
#include "engine.hpp"
#include "memory.hpp"
#include "scenes.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstddef>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>

// ---------------------------------------------
// Helpers (tiny Assimp → glm converters)
// ---------------------------------------------
static inline glm::mat4 toGlm(const aiMatrix4x4 &m) { return glm::mat4(m.a1, m.b1, m.c1, m.d1, m.a2, m.b2, m.c2, m.d2, m.a3, m.b3, m.c3, m.d3, m.a4, m.b4, m.c4, m.d4); }

static inline glm::vec3 toGlm(const aiVector3D &v) { return glm::vec3(v.x, v.y, v.z); }

static inline glm::mat3 normalMat(const glm::mat4 &M) { return glm::transpose(glm::inverse(glm::mat3(M))); }

static aiNode *findNodeByNameRecursive(aiNode *n, const std::string &name) {
	if (!n)
		return nullptr;
	if (name == n->mName.C_Str())
		return n;
	for (unsigned i = 0; i < n->mNumChildren; ++i) {
		if (aiNode *found = findNodeByNameRecursive(n->mChildren[i], name))
			return found;
	}
	return nullptr;
}

static aiNode *findNodeByName(aiNode *root, const std::string &name) { return findNodeByNameRecursive(root, name); }

using F = VkFormat;

// --------------------------------------------------
// Construction / destruction
// --------------------------------------------------
Asset::Asset(Scene *scene) : Model(scene) {}
Asset::~Asset() { destroyBonesSsbo_(); }

// --------------------------------------------------
// Public API
// --------------------------------------------------
void Asset::init() {
	engine = scene->getScenes().getEngine();

	// Vertex layout (binding 0)
	Model::Mesh m{};
	m.vsrc.data = nullptr; // filled after first upsertInstance(path)
	m.vsrc.bytes = 0;
	m.vsrc.stride = sizeof(Vertex);

	// Index layout
	m.isrc.data = nullptr;
	m.isrc.count = 0;

	// Per-vertex attributes (no baked bones, only ids/weights)
	m.vertexAttrs = {
		{0, 0, F::VK_FORMAT_R32G32B32_SFLOAT, uint32_t(offsetof(Vertex, pos))},		  {1, 0, F::VK_FORMAT_R32G32B32_SFLOAT, uint32_t(offsetof(Vertex, normal))}, {2, 0, F::VK_FORMAT_R32G32B32A32_SFLOAT, uint32_t(offsetof(Vertex, color))},  {3, 0, F::VK_FORMAT_R32G32_SFLOAT, uint32_t(offsetof(Vertex, uv))},
		{4, 0, F::VK_FORMAT_R32G32B32A32_SFLOAT, uint32_t(offsetof(Vertex, tanSgn))}, {5, 0, F::VK_FORMAT_R32_UINT, uint32_t(offsetof(Vertex, matId))},			 {10, 0, F::VK_FORMAT_R32G32B32A32_UINT, uint32_t(offsetof(Vertex, boneIds))}, {11, 0, F::VK_FORMAT_R32G32B32A32_SFLOAT, uint32_t(offsetof(Vertex, weights))},
	};

	// Per-instance attributes (binding 1)
	m.vertexAttrs.push_back({6, 1, F::VK_FORMAT_R32G32B32A32_SFLOAT, uint32_t(offsetof(InstanceData, model) + sizeof(glm::vec4) * 0)});
	m.vertexAttrs.push_back({7, 1, F::VK_FORMAT_R32G32B32A32_SFLOAT, uint32_t(offsetof(InstanceData, model) + sizeof(glm::vec4) * 1)});
	m.vertexAttrs.push_back({8, 1, F::VK_FORMAT_R32G32B32A32_SFLOAT, uint32_t(offsetof(InstanceData, model) + sizeof(glm::vec4) * 2)});
	m.vertexAttrs.push_back({9, 1, F::VK_FORMAT_R32G32B32A32_SFLOAT, uint32_t(offsetof(InstanceData, model) + sizeof(glm::vec4) * 3)});
	m.vertexAttrs.push_back({13, 1, F::VK_FORMAT_R32G32B32A32_SFLOAT, uint32_t(offsetof(InstanceData, outlineColor))});
	m.vertexAttrs.push_back({14, 1, F::VK_FORMAT_R32_SFLOAT, uint32_t(offsetof(InstanceData, outlineWidth))});
	m.vertexAttrs.push_back({12, 1, F::VK_FORMAT_R32_UINT, uint32_t(offsetof(InstanceData, bonesBase))});

	initInfo.mesh = m;
	initInfo.instanceStrideBytes = sizeof(InstanceData);

	// Shaders (place SPIR-V under <shaderRootPath>/asset)
	initInfo.shaders = Assets::compileShaderProgram(Assets::shaderRootPath + "/asset", engine->getDevice());

	// Base init creates set=0, buffers (empty now), descriptor pool, etc.
	Model::init();

	// Create bones SSBO and write set=1
	createBonesSsbo_();
	ensureSet1Ready();

	// Placeholder instance (identity, no outline)
	InstanceData inst{};
	inst.model = glm::mat4(1.0f);
	inst.outlineColor = glm::vec4(0.0f);
	inst.outlineWidth = 0.0f;
	inst.bonesBase = 0; // will be overwritten by ensureBonesBaseFor
	upsertInstance(0, inst);
	ensureBonesBaseFor(0);

	createOutlinePipeline();
}

void Asset::syncPickingInstances() { Model::syncPickingInstances<InstanceData>(); }

void Asset::upsertInstance(int id, const std::string &assetPath) {
	// 1) Load mesh with Assimp into cpuVerts_/cpuIdx_
	Assimp::Importer importer;
	const unsigned flags = aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace | aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality | aiProcess_SortByPType | aiProcess_LimitBoneWeights;

	const aiScene *as = importer.ReadFile(assetPath.c_str(), flags);
	if (!as || (as->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !as->mRootNode) {
		throw std::runtime_error(std::string("Assimp load failed: ") + importer.GetErrorString());
	}

	// Reserve rough capacity
	size_t vCap = 0, iCap = 0;
	for (unsigned i = 0; i < as->mNumMeshes; ++i) {
		vCap += as->mMeshes[i]->mNumVertices;
		iCap += as->mMeshes[i]->mNumFaces * 3;
	}
	cpuVerts_.clear();
	cpuIdx_.clear();
	cpuVerts_.reserve(vCap);
	cpuIdx_.reserve(iCap);

	// Gather a mapping for bone names (needed to fill boneIds/weights)
	uint32_t nextBoneId = 0;

	// MSVC FIX: Use custom struct instead of std::array for local type
	struct Infl {
		uint32_t id;
		float w;
	};

	struct VertexInfluences {
		Infl influences[4];
		
		VertexInfluences() {
			for (int i = 0; i < 4; i++) {
				influences[i] = Infl{0, 0};
			}
		}
		
		// Provide array-like access
		Infl& operator[](size_t index) { return influences[index]; }
		const Infl& operator[](size_t index) const { return influences[index]; }
	};

	// DFS through nodes, baking transforms
	std::function<void(aiNode *, const aiMatrix4x4 &)> processNode = [&](aiNode *node, const aiMatrix4x4 &parent) {
		aiMatrix4x4 accum = parent * node->mTransformation;
		glm::mat4 M = toGlm(accum);
		glm::mat3 N = normalMat(M);

		for (unsigned im = 0; im < node->mNumMeshes; ++im) {
			aiMesh *m = as->mMeshes[node->mMeshes[im]];
			const uint32_t base = static_cast<uint32_t>(cpuVerts_.size());
			const uint32_t matId = m->mMaterialIndex;

			std::vector<VertexInfluences> tempInf(m->mNumVertices);

			// collect bone weights for this mesh
			for (unsigned b = 0; b < m->mNumBones; ++b) {
				const aiBone *ab = m->mBones[b];
				const std::string name(ab->mName.C_Str());
				uint32_t boneId;
				auto it = boneMap.find(name);
				if (it == boneMap.end()) {
					boneId = nextBoneId++;
					boneMap[name] = boneId;
					if (boneBase.size() <= boneId) {
						boneBase.resize(boneId + 1, glm::mat4(1.0f));
						boneOffset.resize(boneId + 1, glm::mat4(1.0f));
						boneOffsetInv.resize(boneId + 1, glm::mat4(1.0f));
					}
				} else {
					boneId = it->second;
				}

				mat4 offset = toGlm(ab->mOffsetMatrix);
				mat4 globalNode = M;

				boneBase[boneId] = globalNode * offset;
				boneOffset[boneId] = offset;
				boneOffsetInv[boneId] = inverse(offset);

				for (unsigned w = 0; w < ab->mNumWeights; ++w) {
					const aiVertexWeight &vw = ab->mWeights[w];
					auto &arr = tempInf[vw.mVertexId];

					// insert by weight (descending), keep top 4
					Infl cand{boneId, vw.mWeight};
					for (int k = 0; k < 4; ++k) {
						if (cand.w > arr[k].w) {
							// shift down
							for (int j = 3; j > k; --j)
								arr[j] = arr[j - 1];
							arr[k] = cand;
							break;
						}
					}
				}
			}

			// vertices
			for (unsigned v = 0; v < m->mNumVertices; ++v) {
				Vertex out{};

				// position
				glm::vec4 p = glm::vec4(m->mVertices[v].x, m->mVertices[v].y, m->mVertices[v].z, 1.0f);
				out.pos = glm::vec3(p);

				// normal
				if (m->HasNormals()) {
					glm::vec3 nn = glm::normalize(glm::vec3(m->mNormals[v].x, m->mNormals[v].y, m->mNormals[v].z));
					out.normal = nn;
				} else {
					out.normal = glm::vec3(0, 0, 1);
				}

				// color
				if (m->HasVertexColors(0)) {
					const aiColor4D c = m->mColors[0][v];
					out.color = glm::vec4(c.r, c.g, c.b, c.a);
				} else {
					out.color = glm::vec4(1.0f);
				}

				// uv
				if (m->HasTextureCoords(0)) {
					out.uv = glm::vec2(m->mTextureCoords[0][v].x, m->mTextureCoords[0][v].y);
				} else {
					out.uv = glm::vec2(0.0f);
				}

				// tangent + sign
				if (m->HasTangentsAndBitangents()) {
					glm::vec3 T = glm::normalize(N * glm::vec3(m->mTangents[v].x, m->mTangents[v].y, m->mTangents[v].z));
					glm::vec3 B = glm::normalize(N * glm::vec3(m->mBitangents[v].x, m->mBitangents[v].y, m->mBitangents[v].z));
					glm::vec3 NN = out.normal;
					float sgn = (glm::dot(glm::cross(NN, T), B) < 0.0f) ? -1.0f : 1.0f;
					out.tanSgn = glm::vec4(T, sgn);
				} else {
					out.tanSgn = glm::vec4(1, 0, 0, 1);
				}

				out.matId = matId;

				// bone ids/weights (normalize)
				auto &arr = tempInf[v];
				float sum = arr[0].w + arr[1].w + arr[2].w + arr[3].w;
				if (sum > 0.0f) {
					out.weights = glm::vec4(arr[0].w, arr[1].w, arr[2].w, arr[3].w) / sum;
				} else {
					out.weights = glm::vec4(0.0f);
				}
				out.boneIds = glm::uvec4(arr[0].id, arr[1].id, arr[2].id, arr[3].id);

				cpuVerts_.push_back(out);
			}

			// indices
			for (unsigned f = 0; f < m->mNumFaces; ++f) {
				const aiFace &face = m->mFaces[f];
				for (unsigned j = 0; j < face.mNumIndices; ++j) {
					cpuIdx_.push_back(base + face.mIndices[j]);
				}
			}
		}

		for (unsigned c = 0; c < node->mNumChildren; ++c) {
			processNode(node->mChildren[c], accum);
		}
	};

	processNode(as->mRootNode, aiMatrix4x4());

	// ---- Build bone hierarchy (parent/children) so transforms propagate ----
	boneParent_.assign(boneMap.size(), -1);
	boneChildren_.assign(boneMap.size(), {});

	aiNode *root = as->mRootNode;

	for (const auto &pair : boneMap) {
		const std::string &boneName = pair.first;
		uint32_t boneId = pair.second;

		aiNode *node = findNodeByName(root, boneName);
		if (!node)
			continue;

		aiNode *parentNode = node->mParent;
		// Walk up until we find a parent that is also a bone (or hit root)
		while (parentNode) {
			auto itParent = boneMap.find(parentNode->mName.C_Str());
			if (itParent != boneMap.end()) {
				uint32_t parentId = itParent->second;
				boneParent_[boneId] = static_cast<int>(parentId);
				boneChildren_[parentId].push_back(boneId);
				break;
			}
			parentNode = parentNode->mParent;
		}
		// If no bone parent found, boneParent_[boneId] stays -1 (root bone)
	}

	// 1.5) Update Model::mesh vsrc/isrc so enableRayPicking can see CPU data
	mesh.vsrc.data = cpuVerts_.empty() ? nullptr : cpuVerts_.data();
	mesh.vsrc.bytes = cpuVerts_.size() * sizeof(Vertex);
	mesh.vsrc.stride = sizeof(Vertex); // just to be explicit

	mesh.isrc.data = cpuIdx_.empty() ? nullptr : cpuIdx_.data();
	mesh.isrc.count = static_cast<uint32_t>(cpuIdx_.size());

	// 2) (Re)build GPU vertex / index buffers to match new CPU arrays
	const auto &dev = pipeline->device;

	// Destroy old buffers if any (mirror Model::destroy() parts for v/i buffers only)
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

	// Create & upload vertex buffer (HOST_VISIBLE is fine for now)
	if (!cpuVerts_.empty()) {
		VkDeviceSize vbytes = static_cast<VkDeviceSize>(cpuVerts_.size() * sizeof(Vertex));
		pipeline->createBuffer(vbytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vbuf, vmem);

		void *p = nullptr;
		VK_CHECK(vkMapMemory(dev, vmem, 0, VK_WHOLE_SIZE, 0, &p));
		std::memcpy(p, cpuVerts_.data(), static_cast<size_t>(vbytes));
		vkUnmapMemory(dev, vmem);
	}

	// Create & upload index buffer
	if (!cpuIdx_.empty()) {
		VkDeviceSize ibytes = static_cast<VkDeviceSize>(cpuIdx_.size() * sizeof(uint32_t));
		pipeline->createBuffer(ibytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, ibuf, imem);

		void *p = nullptr;
		VK_CHECK(vkMapMemory(dev, imem, 0, VK_WHOLE_SIZE, 0, &p));
		std::memcpy(p, cpuIdx_.data(), static_cast<size_t>(ibytes));
		vkUnmapMemory(dev, imem);

		indexCount = static_cast<uint32_t>(cpuIdx_.size());
	} else {
		indexCount = 0;
	}

	// 3) Ensure a default instance exists/updates (transform + outline)
	InstanceData data{};
	data.model = glm::mat4(1.0f);
	data.outlineColor = glm::vec4(0.0f);
	data.outlineWidth = 0.0f;
	data.bonesBase = 0;
	upsertInstance(id, data); // <- uses Model::upsertBytes
	ensureBonesBaseFor(id);

	// If swapchain resize recreated sets recently, descriptors might need a rewrite
	if (set1Dirty_)
		ensureSet1Ready();
}

void Asset::upsertInstance(int id, const InstanceData &data) {
	std::span<const uint8_t> bytes(reinterpret_cast<const uint8_t *>(&data), sizeof(InstanceData));
	upsertBytes(id, bytes);
	ensureBonesBaseFor(id);
	if (set1Dirty_)
		ensureSet1Ready();
}

void Asset::setBones(int id, std::span<const glm::mat4> palette) {
	auto it = idToSlot.find(id);
	if (it == idToSlot.end())
		return;
	const uint32_t slot = it->second;

	const size_t n = std::min<size_t>(palette.size(), MAX_BONES);
	glm::mat4 *dst = bonesCPU_.data() + size_t(slot) * MAX_BONES;
	if (n > 0)
		std::memcpy(dst, palette.data(), n * sizeof(glm::mat4));
	for (size_t i = n; i < MAX_BONES; ++i)
		dst[i] = glm::mat4(1.0f);

	bonesDirty_[slot] = true;
}

mat4 Asset::getBoneTransform(int id, string boneName) {
	auto instance = idToSlot.find(id);
	if (instance == idToSlot.end()) {
		std::cerr << "instance not found" << std::endl;
		return mat4(1.0f);
	}

	const uint32_t slot = instance->second;

	auto bone = boneMap.find(boneName);
	if (bone == boneMap.end()) {
		std::cerr << "bone not found" << std::endl;
		return mat4(1.0f);
	}

	const uint32_t boneId = bone->second;

	if (bonesCPU_.empty()) {
		std::cerr << "bone palette empty" << std::endl;
		return mat4(1.0f);
	}

	glm::mat4 *palette = bonesCPU_.data() + size_t(slot) * MAX_BONES;

	return palette[boneId];
}

void Asset::applyBoneTransform(int id, string boneName, mat4 model, bool override) {
	auto instance = idToSlot.find(id);
	if (instance == idToSlot.end()) {
		std::cerr << "instance not found" << std::endl;
		return;
	}

	const uint32_t slot = instance->second;

	auto bone = boneMap.find(boneName);
	if (bone == boneMap.end()) {
		std::cerr << "bone not found" << std::endl;
		return;
	}

	const uint32_t boneId = bone->second;

	if (bonesCPU_.empty()) {
		std::cerr << "bone palette empty" << std::endl;
		return;
	}

	glm::mat4 *palette = bonesCPU_.data() + size_t(slot) * MAX_BONES;

	glm::mat4 base = (boneBase.size() > boneId) ? boneBase[boneId] : glm::mat4(1.0f);
	glm::mat4 offset = (boneOffset.size() > boneId) ? boneOffset[boneId] : glm::mat4(1.0f);
	glm::mat4 offsetInv = (boneOffsetInv.size() > boneId) ? boneOffsetInv[boneId] : glm::inverse(offset);

	// --- compute new parent bone transform ---
	glm::mat4 parentOld = palette[boneId];

	glm::mat4 parentNew;
	if (override) {
		// 'model' is a replacement local transform
		parentNew = base * offsetInv * model * offset;
	} else {
		// 'model' is a delta in local space
		parentNew = parentOld * offsetInv * model * offset;
	}

	// Delta from old to new in GLOBAL space
	glm::mat4 delta = parentNew * glm::inverse(parentOld);

	// Apply to this bone
	palette[boneId] = parentNew;

	// Recursively apply same delta to all descendants so they stay attached
	if (boneId < boneChildren_.size()) {
		std::function<void(uint32_t)> applyToChildren = [&](uint32_t parent) {
			if (parent >= boneChildren_.size())
				return;
			for (uint32_t child : boneChildren_[parent]) {
				if (child >= MAX_BONES)
					continue;
				glm::mat4 childOld = palette[child];
				glm::mat4 childNew = delta * childOld;
				palette[child] = childNew;
				applyToChildren(child);
			}
		};
		applyToChildren(boneId);
	}

	bonesDirty_[slot] = true;
}

// --------------------------------------------------
// Descriptor set=1 management (SSBO)
// --------------------------------------------------
uint32_t Asset::createDescriptorPool() {
	uint32_t baseSets = Model::createDescriptorPool();

	// capacity for set=1 (SSBO with bone palettes)
	pipeline->descriptorPoolSizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1});

	return (std::max)(baseSets, 2u);
}

void Asset::createDescriptors() {
	// Declare set=1 layout BEFORE allocation so base includes it
	pipeline->createDescriptorSetLayoutBinding(
		/*binding*/ 0,
		/*type*/ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // SSBO
		/*stages*/ VK_SHADER_STAGE_VERTEX_BIT,
		/*count*/ 1,
		/*set*/ 1);

	// Allocate sets (set=0 + set=1)
	Model::createDescriptors();

	// Try to write set=1 now
	ensureSet1Ready();
}

void Asset::createGraphicsPipeline() {
	// Recreate pipeline (e.g., on swapchain resize)
	Model::createGraphicsPipeline();
	if (set1Dirty_)
		ensureSet1Ready();
}

void Asset::writeSet1Descriptors() {
	if (!pipeline)
		return;
	if (pipeline->descriptorSets.descriptorSets.size() <= 1) {
		set1Dirty_ = true;
		return;
	}

	VkDescriptorSet set1 = pipeline->descriptorSets.descriptorSets[1];
	if (set1 == VK_NULL_HANDLE) {
		set1Dirty_ = true;
		return;
	}
	if (bonesSsbo_ == VK_NULL_HANDLE) {
		set1Dirty_ = true;
		return;
	}

	// safest: stall before mutating a set possibly in flight
	vkDeviceWaitIdle(pipeline->device);

	VkDescriptorBufferInfo bi{};
	bi.buffer = bonesSsbo_;
	bi.offset = 0;
	bi.range = VK_WHOLE_SIZE;

	VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	w.dstSet = set1;
	w.dstBinding = 0;
	w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	w.descriptorCount = 1;
	w.pBufferInfo = &bi;

	vkUpdateDescriptorSets(pipeline->device, 1, &w, 0, nullptr);
	set1Dirty_ = false;
}

void Asset::ensureSet1Ready() {
	if (!pipeline) {
		set1Dirty_ = true;
		return;
	}
	if (pipeline->descriptorSets.descriptorSets.size() <= 1) {
		set1Dirty_ = true;
		return;
	}
	if (pipeline->descriptorSets.descriptorSets[1] == VK_NULL_HANDLE) {
		set1Dirty_ = true;
		return;
	}
	if (bonesSsbo_ == VK_NULL_HANDLE) {
		set1Dirty_ = true;
		return;
	}
	writeSet1Descriptors();
}

// --------------------------------------------------
// Bones SSBO helpers
// --------------------------------------------------
void Asset::createBonesSsbo_() {
	if (bonesSsbo_)
		return;

	const uint32_t cap = (std::max)(1u, initInfo.maxInstances);
	bonesCPU_.assign(size_t(cap) * MAX_BONES, glm::mat4(1.0f));
	bonesDirty_.assign(cap, true);

	for (uint32_t slot = 0; slot < cap; ++slot) {
		glm::mat4 *palette = bonesCPU_.data() + size_t(slot) * MAX_BONES;
		for (uint32_t b = 0; b < boneBase.size() && b < MAX_BONES; ++b) {
			palette[b] = boneBase[b];
		}
	}

	VkDeviceSize bytes = VkDeviceSize(bonesCPU_.size() * sizeof(glm::mat4));
	pipeline->createBuffer(bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, bonesSsbo_, bonesMem_);

	VK_CHECK(vkMapMemory(pipeline->device, bonesMem_, 0, VK_WHOLE_SIZE, 0, reinterpret_cast<void **>(&bonesMapped_)));
	std::memcpy(bonesMapped_, bonesCPU_.data(), size_t(bytes));
}

void Asset::destroyBonesSsbo_() {
	if (!pipeline)
		return;
	if (bonesMapped_) {
		vkUnmapMemory(pipeline->device, bonesMem_);
		bonesMapped_ = nullptr;
	}
	if (bonesSsbo_) {
		vkDestroyBuffer(pipeline->device, bonesSsbo_, nullptr);
		bonesSsbo_ = VK_NULL_HANDLE;
	}
	if (bonesMem_) {
		vkFreeMemory(pipeline->device, bonesMem_, nullptr);
		bonesMem_ = VK_NULL_HANDLE;
	}
}

// --------------------------------------------------
// Drawing: flush dirty bones, guard null geometry
// --------------------------------------------------
void Asset::record(VkCommandBuffer cmd) {
	// Upload any dirty palettes (per-slot)
	if (bonesMapped_ && !bonesDirty_.empty()) {
		for (uint32_t slot = 0; slot < count && slot < bonesDirty_.size(); ++slot) {
			if (!bonesDirty_[slot])
				continue;
			const size_t offs = size_t(slot) * MAX_BONES * sizeof(glm::mat4);
			std::memcpy(bonesMapped_ + offs, bonesCPU_.data() + size_t(slot) * MAX_BONES, MAX_BONES * sizeof(glm::mat4));
			bonesDirty_[slot] = false;
		}
	}

	// If no geometry yet, skip to avoid VUID 04001
	if (vbuf == VK_NULL_HANDLE || indexCount == 0) {
		return;
	}

	// Normal draw path
	Model::record(cmd);
	recordOutline(cmd);
}

// --------------------------------------------------
// Instance -> bonesBase wiring
// --------------------------------------------------
void Asset::ensureBonesBaseFor(int id) {
	auto it = idToSlot.find(id);
	if (it == idToSlot.end())
		return;
	const uint32_t slot = it->second;

	if (iStride != sizeof(InstanceData))
		return; // safety

	// Mutate CPU shadow struct to set bonesBase
	uint8_t *dst = cpu.data() + size_t(slot) * iStride;
	InstanceData tmp{};
	std::memcpy(&tmp, dst, sizeof(InstanceData));
	tmp.bonesBase = slot * MAX_BONES; // in mat4 units
	std::memcpy(dst, &tmp, sizeof(InstanceData));

	ssboDirty = true;			  // instance buffer update needed
	pickingInstancesDirty = true; // ray picking depends on model
}

void Asset::recordOutline(VkCommandBuffer cmd) {
	if (!outline || outline->pipeline == VK_NULL_HANDLE)
		return;
	if (count == 0 || indexCount == 0)
		return;

	// Bind viewport & scissor same as base
	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	// Bind outline pipeline
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, outline->pipeline);

	// Bind its descriptor sets
	const auto &dsets = outline->descriptorSets.descriptorSets;
	const auto &dynOffsets = outline->descriptorSets.dynamicOffsets;
	if (!dsets.empty()) {
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, outline->pipelineLayout, 0, static_cast<uint32_t>(dsets.size()), dsets.data(), static_cast<uint32_t>(dynOffsets.size()), dynOffsets.empty() ? nullptr : dynOffsets.data());
	}

	// Bind vertex buffers: 0 = geometry, 1 = instance SSBO (same as base)
	VkBuffer vbs[2];
	VkDeviceSize offs[2] = {0, 0};
	uint32_t vbCount = 0;
	if (vbuf != VK_NULL_HANDLE)
		vbs[vbCount++] = vbuf;
	if (iStride > 0 && ssbo != VK_NULL_HANDLE)
		vbs[vbCount++] = ssbo;

	if (vbCount == 0)
		return;

	vkCmdBindVertexBuffers(cmd, 0, vbCount, vbs, offs);
	vkCmdBindIndexBuffer(cmd, ibuf, 0, VK_INDEX_TYPE_UINT32);

	vkCmdDrawIndexed(cmd, indexCount, count, 0, 0, 0);
}

void Asset::createOutlinePipeline() {
	// Create new Pipeline
	outline = std::make_unique<Pipeline>();
	outline->device = pipeline->device;
	outline->physicalDevice = pipeline->physicalDevice;

	// Share formats & MSAA settings
	outline->graphicsPipeline.colorFormat = pipeline->graphicsPipeline.colorFormat;
	outline->graphicsPipeline.depthFormat = pipeline->graphicsPipeline.depthFormat;
	outline->samplesCountFlagBits = pipeline->samplesCountFlagBits;

	// Load OUTLINE shaders (place SPIR-V under <shaderRootPath>/asset_outline)
	// e.g. asset_outline.vert + asset_outline.frag
	outline->shaders = Assets::compileShaderProgram(outlineShaderPath, outline->device);

	// --- Descriptor pool sizes (same kinds as base: UBO + instance SSBO + bones SSBO) ---
	outline->descriptorPoolSizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1});
	if (iStride > 0) {
		outline->descriptorPoolSizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2});
		// set0: instances SSBO, set1: bones SSBO
	} else {
		outline->descriptorPoolSizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1});
		// only bones SSBO
	}

	// We need 2 descriptor sets: set=0 (camera+instances) and set=1 (bones)
	outline->createDescriptorPool(/*setCount=*/2);

	// --- Descriptor set layouts & writes, mirroring Model + Asset ---

	// set=0, binding 0: UBO VP (VS only is enough, but VS|FS also fine)
	outline->createDescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1, /*setIndex=*/0);

	// set=0, binding 1: instance SSBO
	if (iStride > 0 && ssbo != VK_NULL_HANDLE) {
		outline->createDescriptorSetLayoutBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1, /*setIndex=*/0);
	}

	// set=1, binding 0: bones SSBO
	outline->createDescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1, /*setIndex=*/1);

	// write descriptors – share the SAME buffers as base pipeline
	// set 0, binding 0: VP UBO
	{
		VkDescriptorBufferInfo uboInfo{ubo, 0, sizeof(VPMatrix)};
		outline->createWriteDescriptorSet(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uboInfo, 1, /*setIndex=*/0);
	}

	// set 0, binding 1: instances SSBO
	if (iStride > 0 && ssbo != VK_NULL_HANDLE) {
		VkDescriptorBufferInfo sboInfo{ssbo, 0, maxInstances * iStride};
		outline->createWriteDescriptorSet(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sboInfo, 1, /*setIndex=*/0);
	}

	// set 1, binding 0: bones SSBO
	{
		VkDescriptorBufferInfo bonesInfo{bonesSsbo_, 0, VK_WHOLE_SIZE};
		outline->createWriteDescriptorSet(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bonesInfo, 1, /*setIndex=*/1);
	}

	// Allocate + update descriptor sets
	outline->createDescriptors();

	// --- Vertex input: exactly the same as base pipeline ---
	outline->graphicsPipeline.vertexInputAttributeDescription = pipeline->graphicsPipeline.vertexInputAttributeDescription;
	outline->graphicsPipeline.vertexInputBindingDescriptions = pipeline->graphicsPipeline.vertexInputBindingDescriptions;

	// Rasterization / depth state for silhouette outline:
	auto &rs = outline->graphicsPipeline.rasterizationStateCI;
	rs.cullMode = VK_CULL_MODE_FRONT_BIT; // inverted hull
	rs.frontFace = pipeline->graphicsPipeline.rasterizationStateCI.frontFace;

	auto &ds = outline->graphicsPipeline.depthStencilStateCI;
	ds.depthTestEnable = VK_TRUE;
	ds.depthWriteEnable = VK_FALSE; // don’t overwrite depth

	// Blending same as base (can be solid or alpha)
	outline->graphicsPipeline.colorBlendAttachmentState = pipeline->graphicsPipeline.colorBlendAttachmentState;

	// Finally create the outline graphics pipeline
	outline->createGraphicsPipeline();
}
