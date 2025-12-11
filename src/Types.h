#pragma once

#include <smath.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include "DeletionQueue.h"
#include "DescriptorAllocatorGrowable.h"

namespace Lunar {

struct AllocatedImage {
	vk::Image image;
	vk::ImageView image_view;
	VmaAllocation allocation;
	vk::Extent3D extent;
	vk::Format format;
};

struct AllocatedBuffer {
	vk::Buffer buffer;
	VmaAllocation allocation;
	VmaAllocationInfo info;
};

struct FrameData {
	vk::UniqueCommandPool command_pool;
	vk::UniqueCommandBuffer main_command_buffer;
	vk::UniqueSemaphore swapchain_semaphore;
	vk::UniqueFence render_fence;

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
	vk::DeviceAddress vertex_buffer_address;
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
