#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include <vulkan/vulkan.hpp>

namespace Lunar {

struct CPUTexture {
	std::vector<uint8_t> pixels;
	uint32_t width { 0 };
	uint32_t height { 0 };
	vk::Format format { vk::Format::eR8G8B8A8Unorm };

	explicit CPUTexture(std::filesystem::path const &path);
	CPUTexture(std::vector<uint8_t> pixels, uint32_t width, uint32_t height,
	    vk::Format format);
};

} // namespace Lunar
