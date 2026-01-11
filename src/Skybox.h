#pragma once

#include <cstdint>
#include <filesystem>

#include <vulkan/vulkan.hpp>

#include "Pipeline.h"
#include "Types.h"
#include "VulkanRenderer.h"

namespace Lunar {

struct Skybox {
	bool ok { false };

	auto init(VulkanRenderer &renderer, std::filesystem::path const &path)
	    -> void;
	auto destroy(VulkanRenderer &renderer) -> void;
	auto draw(VulkanRenderer::GL &gl, smath::Mat4 const &mvp) -> void;

private:
	Pipeline m_pipeline {};
	GPUMeshBuffers m_cube_mesh {};
	AllocatedImage m_cubemap {};
	vk::UniqueSampler m_sampler {};
	vk::UniqueDescriptorPool m_descriptor_pool {};
	vk::DescriptorSet m_descriptor_set {};
	uint32_t m_index_count { 0 };
};

} // namespace Lunar
