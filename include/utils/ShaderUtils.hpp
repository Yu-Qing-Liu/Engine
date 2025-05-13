#pragma once

#include <string>
#include <vector>

class ShaderUtils {
  public:
	ShaderUtils() = default;
	ShaderUtils(ShaderUtils &&) = default;
	ShaderUtils(const ShaderUtils &) = default;
	ShaderUtils &operator=(ShaderUtils &&) = default;
	ShaderUtils &operator=(const ShaderUtils &) = default;
	~ShaderUtils() = default;

  private:
	std::vector<char> readFile(const std::string &filename);
};
