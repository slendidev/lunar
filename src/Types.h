#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

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
	AllocatedBuffer frame_image_buffer {};
	vk::Extent2D frame_image_extent {};
	std::vector<std::uint8_t> frame_image_rgba;
	bool frame_image_ready { false };
	bool tracy_frame_ready { false };
	AllocatedBuffer screenshot_buffer {};
	vk::Extent2D screenshot_extent {};
	std::vector<std::uint8_t> screenshot_rgba;
	bool screenshot_ready { false };
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

struct Camera {
	smath::Vec3 position {};
	smath::Vec3 target { 0, 0, -1 };
	smath::Vec3 up { 0, 1, 0 };
	float fovy { smath::deg(70.0f) };
};

struct PolarCoordinate {
	float r, theta, phi;

	static PolarCoordinate from_vec3(smath::Vec3 const &v)
	{
		PolarCoordinate p;
		p.r = std::sqrt(v.x() * v.x() + v.y() * v.y() + v.z() * v.z());

		if (p.r == 0.0f) {
			p.theta = 0.0f;
			p.phi = 0.0f;
			return p;
		}

		p.theta = std::atan2(v.z(), v.x());
		p.phi = std::acos(v.y() / p.r);
		return p;
	}

	smath::Vec3 to_vec3() const
	{
		float sin_phi = std::sin(phi);

		return smath::Vec3 { r * sin_phi * std::cos(theta), r * std::cos(phi),
			r * sin_phi * std::sin(theta) };
	}
};

} // namespace Lunar
