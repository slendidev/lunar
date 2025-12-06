#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "Types.h"

namespace Lunar {

struct VulkanRenderer;

struct Mesh {
	struct Surface {
		uint32_t start_index;
		uint32_t count;
	};

	std::string name;
	std::vector<Surface> surfaces;
	GPUMeshBuffers mesh_buffers;

	static auto load_gltf_meshes(
	    VulkanRenderer &renderer, std::filesystem::path const path)
	    -> std::optional<std::vector<std::shared_ptr<Mesh>>>;
};

} // namespace Lunar
