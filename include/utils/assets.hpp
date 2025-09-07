#pragma once

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

#if !ANDROID_VK
#include <openssl/sha.h>
#include <shaderc/shaderc.h>
#include <shaderc/shaderc.hpp>
using namespace shaderc;
#endif

#include <vulkan/vulkan_core.h>

#include "engine.hpp"
#include "platform.hpp"

namespace Assets {

namespace fs = std::filesystem;
using namespace Engine;

// ===================== Paths =====================
// Desktop defaults; on Android you'll call setResourceDirectories(...) to map to <files>/...
inline std::string shaderRootPath = std::string(PROJECT_ROOT_DIR) + "/src/shaders";
inline std::string textureRootPath = std::string(PROJECT_ROOT_DIR) + "/src/textures";
#if ANDROID_VK
inline std::string shaderCachePath; // set at runtime to "<files>/shaders"
#else
inline std::string shaderCachePath = std::string(PROJECT_ROOT_DIR) + "/src/shaders/cache";
#endif
inline std::string modelRootPath = std::string(PROJECT_ROOT_DIR) + "/src/meshes";
inline std::string fontRootPath = std::string(PROJECT_ROOT_DIR) + "/src/fonts";

// ---- Simple join + mkdir helpers
inline std::string joinPath(const std::string &a, const std::string &b) {
	if (a.empty())
		return b;
	return a.back() == '/' ? (a + b) : (a + "/" + b);
}
inline void ensureDir(const std::string &p) {
	if (p.empty())
		return;
	std::error_code ec;
	fs::create_directories(p, ec);
}

// ---- One call to set ALL Android resource dirs to <files> roots (or use this on desktop if you want)
inline void setResourceDirectories(const std::string &filesRoot) {
	shaderCachePath = joinPath(filesRoot, "shaders");
	modelRootPath = joinPath(filesRoot, "meshes");
	fontRootPath = joinPath(filesRoot, "fonts");
	textureRootPath = joinPath(filesRoot, "textures");
	ensureDir(shaderCachePath);
	ensureDir(modelRootPath);
	ensureDir(fontRootPath);
	ensureDir(textureRootPath);
}
#if ANDROID_VK
// Convenience overload if you have android_app*
inline void setResourceDirectories(android_app *app) {
	setResourceDirectories(app->activity->internalDataPath); // "<files>"
}
#endif

// Minimal init (keeps desktop behavior; Android dirs are set by setResourceDirectories)
inline void initialize() {
#if !ANDROID_VK
	ensureDir(shaderRootPath);
	ensureDir(textureRootPath);
#endif
	ensureDir(shaderCachePath);
	ensureDir(modelRootPath);
	ensureDir(fontRootPath);
	ensureDir(textureRootPath);
}

// ---- Path helpers for your loaders elsewhere
inline std::string texturePath(const std::string &rel) { return joinPath(textureRootPath, rel); }
inline std::string meshPath(const std::string &rel) { return joinPath(modelRootPath, rel); }
inline std::string fontPath(const std::string &rel) { return joinPath(fontRootPath, rel); }

// ===================== Small utils =====================
inline bool fileExists(const std::string &p) {
	std::error_code ec;
	return fs::exists(p, ec) && fs::is_regular_file(p, ec);
}
inline std::vector<uint8_t> readAllBytes(const std::string &p) {
	std::ifstream f(p, std::ios::binary);
	if (!f)
		return {};
	return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}
inline std::vector<uint32_t> readBinaryFileU32(const std::string &path) {
	auto bytes = readAllBytes(path);
	if (bytes.empty() || (bytes.size() % 4) != 0)
		return {};
	std::vector<uint32_t> out(bytes.size() / 4);
	std::memcpy(out.data(), bytes.data(), bytes.size());
	return out;
}

inline std::unordered_map<std::string, int> shaderExtensions = {{".vert", 0}, {".tesc", 1}, {".tese", 2}, {".geom", 3}, {".frag", 4}, {".comp", 5}};

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

// ===================== Desktop path (shaderc + cache) =====================
#if !ANDROID_VK

inline Compiler compiler;
inline CompileOptions options;

inline std::string readTextFile(const std::string &filePath) {
	std::ifstream f(filePath);
	if (!f.is_open())
		return {};
	std::stringstream ss;
	ss << f.rdbuf();
	return ss.str();
}
inline std::string computeHash(const std::string &input) {
	unsigned char hash[SHA_DIGEST_LENGTH];
	SHA1(reinterpret_cast<const unsigned char *>(input.data()), input.size(), hash);
	std::stringstream ss;
	for (int i = 0; i < SHA_DIGEST_LENGTH; ++i)
		ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
	return ss.str();
}
inline void writeBinaryFile(const std::string &path, const std::vector<uint32_t> &data) {
	std::error_code ec;
	fs::create_directories(fs::path(path).parent_path(), ec);
	std::ofstream file(path, std::ios::binary);
	if (!file.is_open()) {
		std::cerr << "Failed to write cache file: " << path << std::endl;
		return;
	}
	file.write(reinterpret_cast<const char *>(data.data()), data.size() * sizeof(uint32_t));
}
inline void deleteOldBinaries(const fs::path &cache_dir, const std::string &basenameSpv) {
	std::error_code ec;
	if (!fs::exists(cache_dir, ec))
		return;
	for (const auto &entry : fs::directory_iterator(cache_dir, ec)) {
		if (ec)
			break;
		if (!entry.is_regular_file())
			continue;
		const std::string name = entry.path().filename().string();
		if (name.size() >= basenameSpv.size() && name.compare(name.size() - basenameSpv.size(), basenameSpv.size(), basenameSpv) == 0) {
			std::error_code ec2;
			fs::remove(entry.path(), ec2);
		}
	}
}
inline std::vector<uint32_t> compileShader(const std::string &shaderPath) {
	std::string shaderCode = readTextFile(shaderPath);
	if (shaderCode.empty())
		return {};
	fs::path p(shaderPath);
	const std::string ext = p.extension().string();
	const auto it = shaderExtensions.find(ext);
	if (it == shaderExtensions.end()) {
		std::cerr << "Unsupported shader ext: " << ext << "\n";
		return {};
	}

	// content-addressed cache filename
	const std::string hash_str = computeHash(ext + shaderCode);
	constexpr const char *kSep = "--";
	const std::string basenameSpv = p.filename().string() + ".spv";
	const fs::path cache_dir(shaderCachePath);
	const fs::path cached_path = cache_dir / (hash_str + kSep + basenameSpv);

	if (fileExists(cached_path.string())) {
		auto cached_binary = readBinaryFileU32(cached_path.string());
		if (!cached_binary.empty())
			return cached_binary;
	}

	// miss/corrupt: purge old variants for this basename, then compile
	deleteOldBinaries(cache_dir, basenameSpv);

	shaderc_shader_kind kind = shaderc_glsl_infer_from_source;
	switch (it->second) {
	case 0:
		kind = shaderc_glsl_vertex_shader;
		break;
	case 1:
		kind = shaderc_glsl_tess_control_shader;
		break;
	case 2:
		kind = shaderc_glsl_tess_evaluation_shader;
		break;
	case 3:
		kind = shaderc_glsl_geometry_shader;
		break;
	case 4:
		kind = shaderc_glsl_fragment_shader;
		break;
	case 5:
		kind = shaderc_glsl_compute_shader;
		break;
	}

	SpvCompilationResult result = compiler.CompileGlslToSpv(shaderCode, kind, shaderPath.c_str(), options);
	if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
		std::cerr << "Failed to compile shader: " << shaderPath << "\nError: " << result.GetErrorMessage() << std::endl;
		return {};
	}

	std::vector<uint32_t> spirv(result.cbegin(), result.cend());
	writeBinaryFile(cached_path.string(), spirv);
	return spirv;
}
#else
// ===================== Android path (select-only) =====================

// Try exact: <files>/shaders/<basename>.spv, else pick newest "*<basename>.spv"
inline std::optional<fs::path> selectCachedBinaryPath(const fs::path &cache_dir, const std::string &basenameSpv) {
	std::error_code ec;

	fs::path exact = cache_dir / basenameSpv;
	if (fs::exists(exact, ec) && fs::is_regular_file(exact, ec))
		return exact;

	if (!fs::exists(cache_dir, ec))
		return std::nullopt;

	std::optional<fs::path> best;
	fs::file_time_type bestTime{};

	for (const auto &entry : fs::directory_iterator(cache_dir, ec)) {
		if (ec)
			break;
		if (!entry.is_regular_file())
			continue;
		const auto &p = entry.path();
		const std::string name = p.filename().string();
		if (name.size() >= basenameSpv.size() && name.compare(name.size() - basenameSpv.size(), basenameSpv.size(), basenameSpv) == 0) {
			auto t = fs::last_write_time(p, ec);
			if (ec)
				continue;
			if (!best || t > bestTime) {
				best = p;
				bestTime = t;
			}
		}
	}
	return best;
}
inline std::vector<uint32_t> compileShader(const std::string &shaderPath /*logical*/) {
	fs::path p(shaderPath);
	const std::string basenameSpv = p.filename().string() + ".spv";
	const fs::path cache_dir(shaderCachePath);

	auto chosen = selectCachedBinaryPath(cache_dir, basenameSpv);
	if (!chosen) {
		std::cerr << "ERROR: No SPIR-V for " << basenameSpv << " in " << cache_dir << " (Android select-only)\n";
		return {};
	}
	return readBinaryFileU32(chosen->string());
}
#endif // ANDROID_VK

// ===================== Bundling & modules =====================
struct ShaderBinaries;
inline ShaderBinaries compileShader(const std::vector<std::string> &shaderPaths) {
	ShaderBinaries bins;
	for (const auto &sp : shaderPaths) {
		const std::string ext = fs::path(sp).extension().string();
		if (ext == ".vert")
			bins.vertexShader = compileShader(sp);
		else if (ext == ".tesc")
			bins.tessellationControlShader = compileShader(sp);
		else if (ext == ".tese")
			bins.tessellationEvaluationShader = compileShader(sp);
		else if (ext == ".geom")
			bins.geometryShader = compileShader(sp);
		else if (ext == ".frag")
			bins.fragmentShader = compileShader(sp);
		else if (ext == ".comp")
			bins.computeShader = compileShader(sp);
		else {
			std::cerr << "Unsupported shader type: " << ext << std::endl;
			std::exit(1);
		}
	}
	return bins;
}

inline VkShaderModule createShaderModule(const std::vector<uint32_t> &binary) {
	if (binary.empty())
		throw std::runtime_error("empty SPIR-V blob");
	VkShaderModuleCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ci.codeSize = binary.size() * sizeof(uint32_t);
	ci.pCode = binary.data();
	VkShaderModule m{};
	if (vkCreateShaderModule(device, &ci, nullptr, &m) != VK_SUCCESS)
		throw std::runtime_error("failed to create shader module!");
	return m;
}

// Desktop: shaderRootDir is a real folder with GLSL files.
// Android: shaderRootDir is the base id (e.g. "basic" or "pbr/uber") — we synthesize stage names.
inline ShaderModules compileShaderProgram(const std::string &shaderRootDir) {
	ShaderModules modules;

#if !ANDROID_VK
	std::vector<std::string> shader_paths;
	try {
		for (const auto &entry : fs::directory_iterator(shaderRootDir)) {
			if (entry.is_regular_file()) {
				const std::string ext = entry.path().extension().string();
				if (shaderExtensions.contains(ext))
					shader_paths.push_back(entry.path().string());
			}
		}
	} catch (const fs::filesystem_error &e) {
		std::cerr << "Failed to read shader directory: " << e.what() << "\n";
		std::exit(1);
	}

	if (shader_paths.empty()) {
		std::cerr << "No shader files found in directory: " << shaderRootDir << "\n";
		std::exit(1);
	}

	auto bins = compileShader(shader_paths);
#else
	// Android: try known stages using the base id
	std::vector<std::string> shader_paths;
	auto add = [&](const char *ext) { shader_paths.emplace_back(shaderRootDir + std::string(ext)); };
	add(".vert");
	add(".tesc");
	add(".tese");
	add(".geom");
	add(".frag");
	add(".comp");
	auto bins = compileShader(shader_paths);
#endif

	auto mk = [&](const std::vector<uint32_t> &bin) -> VkShaderModule { return bin.empty() ? VK_NULL_HANDLE : createShaderModule(bin); };

	modules.vertexShader = mk(bins.vertexShader);
	modules.tessellationControlShader = mk(bins.tessellationControlShader);
	modules.tessellationEvaluationShader = mk(bins.tessellationEvaluationShader);
	modules.geometryShader = mk(bins.geometryShader);
	modules.fragmentShader = mk(bins.fragmentShader);
	modules.computeShader = mk(bins.computeShader);

	return modules;
}

} // namespace Assets
