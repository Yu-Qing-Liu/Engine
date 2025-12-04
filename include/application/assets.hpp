#pragma once

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <openssl/sha.h>
#include <shaderc/shaderc.hpp>
#include <vulkan/vulkan_core.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace Assets {

namespace fs = std::filesystem;

// -------------------- Public types --------------------
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

// -------------------- Configurable roots --------------------
#ifndef PROJECT_ROOT_DIR
#define PROJECT_ROOT_DIR "."
#endif

inline std::string shaderRootPath = std::string(PROJECT_ROOT_DIR) + "/assets/shaders";
inline std::string textureRootPath = std::string(PROJECT_ROOT_DIR) + "/assets/textures";
inline std::string modelRootPath = std::string(PROJECT_ROOT_DIR) + "/assets/meshes";
inline std::string fontRootPath = std::string(PROJECT_ROOT_DIR) + "/assets/fonts";
inline std::string shaderCachePath = std::string(PROJECT_ROOT_DIR) + "/assets/spirv";
inline std::string appdataPath = std::string(PROJECT_ROOT_DIR) + "/appdata";

// -------------------- Path helpers --------------------
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
inline std::string texturePath(const std::string &rel) { return joinPath(textureRootPath, rel); }
inline std::string meshPath(const std::string &rel) { return joinPath(modelRootPath, rel); }
inline std::string fontPath(const std::string &rel) { return joinPath(fontRootPath, rel); }

// -------------------- Desktop compiler & cache --------------------
inline shaderc::Compiler gCompiler;
inline shaderc::CompileOptions gOptions;

inline std::string readTextFile(const std::string &filePath) {
	std::ifstream f(filePath);
	if (!f.is_open())
		return {};
	std::stringstream ss;
	ss << f.rdbuf();
	return ss.str();
}
inline std::string computeHashHex(const std::string &input) {
	unsigned char hash[SHA_DIGEST_LENGTH];
	SHA1(reinterpret_cast<const unsigned char *>(input.data()), input.size(), hash);
	std::ostringstream oss;
	for (int i = 0; i < SHA_DIGEST_LENGTH; ++i)
		oss << std::hex << std::setw(2) << std::setfill('0') << int(hash[i]);
	return oss.str();
}
inline void writeBinaryFile(const std::string &path, const std::vector<uint32_t> &data) {
	std::error_code ec;
	fs::create_directories(fs::path(path).parent_path(), ec);
	std::ofstream file(path, std::ios::binary);
	if (!file.is_open()) {
		std::cerr << "Failed to write: " << path << std::endl;
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
	std::string source = readTextFile(shaderPath);
	if (source.empty())
		return {};

	const fs::path p(shaderPath);
	const std::string ext = p.extension().string();
	auto it = shaderExtensions.find(ext);
	if (it == shaderExtensions.end()) {
		std::cerr << "Unsupported shader ext: " << ext << "\n";
		return {};
	}

	// cache hit?
	const std::string hashStr = computeHashHex(ext + source);
	const std::string basenameSpv = p.filename().string() + ".spv";
	const fs::path cacheDir(shaderCachePath);
	const fs::path cachedPath = cacheDir / (hashStr + "--" + basenameSpv);

	if (fileExists(cachedPath.string())) {
		auto cached = readBinaryFileU32(cachedPath.string());
		if (!cached.empty())
			return cached;
	}

	// miss: drop other versions of the same basename
	deleteOldBinaries(cacheDir, basenameSpv);

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

	auto result = gCompiler.CompileGlslToSpv(source, kind, shaderPath.c_str(), gOptions);
	if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
		std::cerr << "Failed to compile: " << shaderPath << "\n" << result.GetErrorMessage() << std::endl;
		return {};
	}

	std::vector<uint32_t> spirv(result.cbegin(), result.cend());
	writeBinaryFile(cachedPath.string(), spirv);
	return spirv;
}

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

// Create shader module (note: takes VkDevice; no globals)
inline VkShaderModule createShaderModule(VkDevice device, const std::vector<uint32_t> &binary) {
	if (binary.empty())
		throw std::runtime_error("empty SPIR-V blob");
	VkShaderModuleCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ci.codeSize = binary.size() * sizeof(uint32_t);
	ci.pCode = binary.data();
	VkShaderModule m{};
	if (vkCreateShaderModule(device, &ci, nullptr, &m) != VK_SUCCESS)
		throw std::runtime_error("vkCreateShaderModule failed");
	return m;
}

inline ShaderModules compileShaderProgram(const std::string &shaderRootDir, VkDevice device) {
	ShaderModules modules;
	std::vector<std::string> shader_paths;

	for (const auto &entry : fs::directory_iterator(shaderRootDir)) {
		if (!entry.is_regular_file())
			continue;
		const std::string ext = entry.path().extension().string();
		if (shaderExtensions.contains(ext))
			shader_paths.push_back(entry.path().string());
	}
	auto bins = compileShader(shader_paths);

	auto mk = [&](const std::vector<uint32_t> &bin) -> VkShaderModule { return bin.empty() ? VK_NULL_HANDLE : createShaderModule(device, bin); };
	modules.vertexShader = mk(bins.vertexShader);
	modules.tessellationControlShader = mk(bins.tessellationControlShader);
	modules.tessellationEvaluationShader = mk(bins.tessellationEvaluationShader);
	modules.geometryShader = mk(bins.geometryShader);
	modules.fragmentShader = mk(bins.fragmentShader);
	modules.computeShader = mk(bins.computeShader);
	return modules;
}

inline VkShaderModule compileShaderProgram(const std::string &shaderProgram, const shaderc_shader_kind &shaderKind, VkDevice device) {
	shaderc::SpvCompilationResult result = gCompiler.CompileGlslToSpv(shaderProgram, shaderKind, "inline_shader", "main", gOptions);

	if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
		throw std::runtime_error(std::string("Shader compilation failed: ") + result.GetErrorMessage());
	}

	// 2. Convert SPIR-V to a vector<uint32_t> as Vulkan expects
	std::vector<uint32_t> spirv(result.cbegin(), result.cend());

	// 3. Create Vulkan shader module
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = spirv.size() * sizeof(uint32_t);
	createInfo.pCode = spirv.data();

	VkShaderModule shaderModule = VK_NULL_HANDLE;
	VkResult vkRes = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
	if (vkRes != VK_SUCCESS) {
		throw std::runtime_error("Failed to create VkShaderModule.");
	}

	return shaderModule;
}

inline size_t shaderCount(const ShaderModules &shaders) {
	size_t count = 0;
	if (shaders.vertexShader != VK_NULL_HANDLE)
		count++;
	if (shaders.fragmentShader != VK_NULL_HANDLE)
		count++;
	if (shaders.computeShader != VK_NULL_HANDLE)
		count++;
	if (shaders.geometryShader != VK_NULL_HANDLE)
		count++;
	if (shaders.tessellationControlShader != VK_NULL_HANDLE)
		count++;
	if (shaders.tessellationEvaluationShader != VK_NULL_HANDLE)
		count++;
	return count;
}

inline void destroyShaderProgram(ShaderModules &p, VkDevice device) {
	if (p.vertexShader) {
		vkDestroyShaderModule(device, p.vertexShader, nullptr);
		p.vertexShader = VK_NULL_HANDLE;
	}
	if (p.fragmentShader) {
		vkDestroyShaderModule(device, p.fragmentShader, nullptr);
		p.fragmentShader = VK_NULL_HANDLE;
	}
	if (p.geometryShader) {
		vkDestroyShaderModule(device, p.geometryShader, nullptr);
		p.geometryShader = VK_NULL_HANDLE;
	}
	if (p.computeShader) {
		vkDestroyShaderModule(device, p.computeShader, nullptr);
		p.computeShader = VK_NULL_HANDLE;
	}
	if (p.tessellationControlShader) {
		vkDestroyShaderModule(device, p.tessellationControlShader, nullptr);
		p.tessellationControlShader = VK_NULL_HANDLE;
	}
	if (p.tessellationEvaluationShader) {
		vkDestroyShaderModule(device, p.tessellationEvaluationShader, nullptr);
		p.tessellationEvaluationShader = VK_NULL_HANDLE;
	}
}

// -------------------- Executable dir & asset copy --------------------
inline std::string executableDir() {
#if defined(_WIN32)
	char buf[MAX_PATH];
	DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
	std::string p(buf, (len ? len : 0));
	auto pos = p.find_last_of("\\/");
	return (pos == std::string::npos) ? std::string(".") : p.substr(0, pos);
#elif defined(__APPLE__)
	char buf[4096];
	uint32_t size = sizeof(buf);
	std::string p;
	if (_NSGetExecutablePath(buf, &size) == 0)
		p.assign(buf);
	else {
		std::vector<char> dyn(size);
		if (_NSGetExecutablePath(dyn.data(), &size) == 0)
			p.assign(dyn.data());
	}
	auto pos = p.find_last_of('/');
	return (pos == std::string::npos) ? std::string(".") : p.substr(0, pos);
#else // Linux / BSD
	char buf[4096];
	ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
	if (n <= 0)
		return ".";
	buf[n] = '\0';
	std::string p(buf);
	auto pos = p.find_last_of('/');
	return (pos == std::string::npos) ? std::string(".") : p.substr(0, pos);
#endif
}

inline void copyDirRecursive(const fs::path &src, const fs::path &dst) {
	std::error_code ec;
	if (!fs::exists(src, ec) || !fs::is_directory(src, ec))
		return;
	fs::create_directories(dst, ec);

	for (auto it = fs::recursive_directory_iterator(src, ec); it != fs::recursive_directory_iterator(); ++it) {
		if (ec)
			break;
		const fs::path rel = fs::relative(it->path(), src, ec);
		const fs::path out = dst / rel;

		if (it->is_directory(ec)) {
			fs::create_directories(out, ec);
		} else if (it->is_regular_file(ec)) {
			bool copyNeeded = true;
			if (fs::exists(out, ec)) {
				auto tSrc = fs::last_write_time(it->path(), ec);
				auto tDst = fs::last_write_time(out, ec);
				if (!ec && tDst >= tSrc)
					copyNeeded = false;
			}
			if (copyNeeded) {
				fs::create_directories(out.parent_path(), ec);
				fs::copy_file(it->path(), out, fs::copy_options::overwrite_existing, ec);
			}
		}
	}
}

// Call once at startup (desktop)
inline void initialize() {
	fs::current_path(executableDir());

	// Ensure configured roots exist (ok if missing; copy step below can populate)
	ensureDir(shaderRootPath);
	ensureDir(textureRootPath);
	ensureDir(modelRootPath);
	ensureDir(fontRootPath);

	// Mirror assets next to the executable for easy runtime access
	const fs::path binDir = executableDir();
	const fs::path outRoot = binDir / "assets";
	const fs::path dstShaders = outRoot / "shaders";
	const fs::path dstTextures = outRoot / "textures";
	const fs::path dstMeshes = outRoot / "meshes";
	const fs::path dstFonts = outRoot / "fonts";
	const fs::path dstSpirv = outRoot / "spirv";
	const fs::path dstAppdata = binDir / "appdata";

	copyDirRecursive(shaderRootPath, dstShaders);
	copyDirRecursive(textureRootPath, dstTextures);
	copyDirRecursive(modelRootPath, dstMeshes);
	copyDirRecursive(fontRootPath, dstFonts);
	copyDirRecursive(shaderCachePath, dstSpirv);

	// Switch to local relative roots that your runtime will use
	shaderRootPath = "./assets/shaders";
	textureRootPath = "./assets/textures";
	modelRootPath = "./assets/meshes";
	fontRootPath = "./assets/fonts";
	shaderCachePath = "./assets/spirv";
	appdataPath = "./appdata";

	// Make sure the cache dir exists
	ensureDir(shaderCachePath);
}

} // namespace Assets
