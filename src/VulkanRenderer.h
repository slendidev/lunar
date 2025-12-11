#pragma once

#include <array>
#include <vector>

#include <SDL3/SDL_video.h>
#include <VkBootstrap.h>
#include <smath.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include "DeletionQueue.h"
#include "DescriptorAllocator.h"
#include "Loader.h"
#include "Logger.h"
#include "Pipeline.h"
#include "Types.h"

namespace Lunar {

struct GPUDrawPushConstants {
	smath::Mat4 world_matrix;
	vk::DeviceAddress vertex_buffer;
};

constexpr unsigned FRAME_OVERLAP = 2;

struct VulkanRenderer {
	VulkanRenderer(SDL_Window *window, Logger &logger);
	~VulkanRenderer();

	auto render() -> void;
	auto resize(uint32_t width, uint32_t height) -> void;

	auto immediate_submit(std::function<void(vk::CommandBuffer cmd)> &&function)
	    -> void;
	auto upload_mesh(std::span<uint32_t> indices, std::span<Vertex> vertices)
	    -> GPUMeshBuffers;

	auto logger() const -> Logger & { return m_logger; }

private:
	auto vk_init() -> void;
	auto swapchain_init() -> void;
	auto commands_init() -> void;
	auto sync_init() -> void;
	auto descriptors_init() -> void;
	auto pipelines_init() -> void;
	auto background_pipelines_init() -> void;
	auto triangle_pipeline_init() -> void;
	auto mesh_pipeline_init() -> void;
	auto imgui_init() -> void;
	auto default_data_init() -> void;

	auto draw_background(vk::CommandBuffer cmd) -> void;
	auto draw_geometry(vk::CommandBuffer cmd) -> void;
	auto draw_imgui(vk::CommandBuffer cmd, vk::ImageView target_image_view)
	    -> void;

	auto create_swapchain(uint32_t width, uint32_t height) -> void;
	auto create_draw_image(uint32_t width, uint32_t height) -> void;
	auto update_draw_image_descriptor() -> void;
	auto destroy_draw_image() -> void;
	auto create_depth_image(uint32_t width, uint32_t height) -> void;
	auto destroy_depth_image() -> void;
	auto recreate_swapchain(uint32_t width, uint32_t height) -> void;
	auto destroy_swapchain() -> void;
	auto create_image(vk::Extent3D size, vk::Format format,
	    vk::ImageUsageFlags flags, bool mipmapped = false) -> AllocatedImage;
	auto create_image(void const *data, vk::Extent3D size, vk::Format format,
	    vk::ImageUsageFlags flags, bool mipmapped = false) -> AllocatedImage;
	auto destroy_image(AllocatedImage const &img) -> void;

	auto create_buffer(size_t alloc_size, vk::BufferUsageFlags usage,
	    VmaMemoryUsage memory_usage) -> AllocatedBuffer;
	auto destroy_buffer(AllocatedBuffer const &buffer) -> void;

	vk::Instance m_instance {};
	vk::PhysicalDevice m_physical_device {};
	vk::Device m_device {};

	struct {
		vkb::Instance instance;
		vkb::PhysicalDevice phys_dev;
		vkb::Device dev;
		vkb::Swapchain swapchain;
	} m_vkb;

	struct {
		auto get_current_frame() -> FrameData &
		{
			return frames.at(frame_number % frames.size());
		}

		vk::SwapchainKHR swapchain {};
		vk::SurfaceKHR surface {};
		vk::Format swapchain_image_format {};

		uint32_t graphics_queue_family { 0 };
		vk::Queue graphics_queue {};

		std::vector<vk::Image> swapchain_images;
		std::vector<vk::UniqueImageView> swapchain_image_views;
		std::vector<vk::UniqueSemaphore> present_semaphores;
		vk::Extent2D swapchain_extent;

		std::array<FrameData, FRAME_OVERLAP> frames;
		AllocatedImage draw_image {};
		AllocatedImage depth_image {};
		vk::Extent2D draw_extent {};

		VmaAllocator allocator;
		DescriptorAllocator descriptor_allocator;

		VkDescriptorSet draw_image_descriptors {};
		vk::DescriptorSetLayout draw_image_descriptor_layout {};

		GPUSceneData scene_data {};
		vk::DescriptorSetLayout gpu_scene_data_descriptor_layout {};

		vk::DescriptorSetLayout single_image_descriptor_layout {};

		Pipeline gradient_pipeline;
		Pipeline triangle_pipeline;
		Pipeline mesh_pipeline;

		GPUMeshBuffers rectangle;

		vk::UniqueDescriptorPool imgui_descriptor_pool;

		DeletionQueue deletion_queue;

		vk::UniqueFence imm_fence;
		vk::UniqueCommandBuffer imm_command_buffer;
		vk::UniqueCommandPool imm_command_pool;

		uint64_t frame_number { 0 };

		std::vector<std::shared_ptr<Mesh>> test_meshes;

		AllocatedImage white_image {};
		AllocatedImage black_image {};
		AllocatedImage gray_image {};
		AllocatedImage error_image {};

		vk::UniqueSampler default_sampler_linear;
		vk::UniqueSampler default_sampler_nearest;
	} m_vk;

	SDL_Window *m_window { nullptr };
	Logger &m_logger;
};

} // namespace Lunar
