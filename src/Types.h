#pragma once

#include <smath.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include "DeletionQueue.h"
#include "DescriptorAllocatorGrowable.h"

namespace Lunar {

struct AllocatedImage {
	VkImage image;
	VkImageView image_view;
	VmaAllocation allocation;
	VkExtent3D extent;
	VkFormat format;
};

struct AllocatedBuffer {
	VkBuffer buffer;
	VmaAllocation allocation;
	VmaAllocationInfo info;
};

struct FrameData {
	VkCommandPool command_pool;
	VkCommandBuffer main_command_buffer;
	VkSemaphore swapchain_semaphore;
	VkFence render_fence;

	DeletionQueue deletion_queue;
	DescriptorAllocatorGrowable frame_descriptors;
};

struct Vertex {
	smath::Vec3 position;
	float u;
	smath::Vec3 normal;
	float v;
	smath::Vec4 color;
};

struct GPUMeshBuffers {
	AllocatedBuffer index_buffer, vertex_buffer;
	VkDeviceAddress vertex_buffer_address;
};

struct GPUSceneData {
	smath::Mat4 view;
	smath::Mat4 proj;
	smath::Mat4 viewport;
	smath::Vec4 ambient_color;
	smath::Vec4 sunlight_direction;
	smath::Vec4 sunlight_color;
};

} // namespace Lunar
