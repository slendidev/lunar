#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

namespace Lunar {

struct AllocatedImage {
	VkImage image;
	VkImageView image_view;
	VmaAllocation allocation;
	VkExtent3D extent;
	VkFormat format;
};

} // namespace Lunar
