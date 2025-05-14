#include "shaderutils.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

using namespace glm;

class Model {
  public:
	Model(VkDevice &device, std::string &model_root);
	Model(Model &&) = default;
	Model(const Model &) = delete;
	Model &operator=(Model &&) = delete;
	Model &operator=(const Model &) = delete;
	~Model() = default;

  private:
	VkDevice &device;
	std::string &model_root;
	ShaderUtils *shader_utils;
	ShaderUtils::ShaderModules shader_program;

	virtual void draw(const vec3 &position = vec3(0.0f, 0.0f, 0.0f), const quat &rotation = quat(0.0f, 0.0f, 0.0f, 0.0f), const vec3 &scale = vec3(1.0f, 1.0f, 1.0f), const vec3 &color = vec3(1.0f, 1.0f, 1.0f));
};
