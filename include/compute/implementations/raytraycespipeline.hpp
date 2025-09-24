#pragma once

#include "raytracingpipeline.hpp"

using std::vector;

class RayTraycesPipeline : public RayTracingPipeline {
  public:
	RayTraycesPipeline(RayTraycesPipeline &&) = default;
	RayTraycesPipeline(const RayTraycesPipeline &) = default;
	RayTraycesPipeline &operator=(RayTraycesPipeline &&) = delete;
	RayTraycesPipeline &operator=(const RayTraycesPipeline &) = delete;
	virtual ~RayTraycesPipeline();

	struct InstanceXformGPU {
		glm::mat4 model;	// object -> world
		glm::mat4 invModel; // world  -> object
	};

	RayTraycesPipeline(Model *model, void *instMapped, void *idMapped, vector<InstanceXformGPU> &instCPU, vector<int> &idsCPU, uint32_t &instanceCount, uint32_t maxInstances);

	vector<InstanceXformGPU> &instCPU; // sized to maxInstances
	vector<int> &idsCPU;			   // slot -> external id

	void updateComputeUniformBuffer() override;

  protected:
	void createComputeDescriptorSetLayout() override;
	void createShaderStorageBuffers() override;
	void createComputeDescriptorSets() override;
	void createComputePipeline() override;

  private:
	void *instMapped;
	void *idMapped;

	uint32_t &instanceCount;
	uint32_t maxInstances;

	VkBuffer instBuf = VK_NULL_HANDLE;
	VkDeviceMemory instMem = VK_NULL_HANDLE; // binding=5
	VkBuffer idBuf = VK_NULL_HANDLE;
	VkDeviceMemory idMem = VK_NULL_HANDLE; // binding=6
};
