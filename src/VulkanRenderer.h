#pragma once

#include <array>
#include <vector>

#include <SDL3/SDL_video.h>
#include <VkBootstrap.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include "AllocatedImage.h"
#include "DeletionQueue.h"
#include "Logger.h"
#include "src/DescriptorAllocator.h"

namespace Lunar {

struct FrameData {
	VkCommandPool command_pool;
	VkCommandBuffer main_command_buffer;
	VkSemaphore swapchain_semaphore;
	VkFence render_fence;

	DeletionQueue deletion_queue;
};

constexpr unsigned FRAME_OVERLAP = 2;

class VulkanRenderer {
public:
	VulkanRenderer(SDL_Window *window, Logger &logger);
	~VulkanRenderer();

	auto render() -> void;
	auto resize(uint32_t width, uint32_t height) -> void;

	auto immediate_submit(std::function<void(VkCommandBuffer cmd)> &&function)
	    -> void;

private:
	auto vk_init() -> void;
	auto swapchain_init() -> void;
	auto commands_init() -> void;
	auto sync_init() -> void;
	auto descriptors_init() -> void;
	auto pipelines_init() -> void;
	auto background_pipelines_init() -> void;
	auto imgui_init() -> void;

	auto draw_background(VkCommandBuffer cmd) -> void;
	auto draw_imgui(VkCommandBuffer cmd, VkImageView target_image_view) -> void;

	auto create_swapchain(uint32_t width, uint32_t height) -> void;
	auto create_draw_image(uint32_t width, uint32_t height) -> void;
	auto update_draw_image_descriptor() -> void;
	auto destroy_draw_image() -> void;
	auto recreate_swapchain(uint32_t width, uint32_t height) -> void;
	auto destroy_swapchain() -> void;

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

		VkSwapchainKHR swapchain { VK_NULL_HANDLE };
		VkSurfaceKHR surface { nullptr };
		VkFormat swapchain_image_format;

		uint32_t graphics_queue_family { 0 };
		VkQueue graphics_queue { nullptr };

		std::vector<VkImage> swapchain_images;
		std::vector<VkImageView> swapchain_image_views;
		std::vector<VkSemaphore> present_semaphores;
		VkExtent2D swapchain_extent;

		std::array<FrameData, FRAME_OVERLAP> frames;
		AllocatedImage draw_image {};
		VkExtent2D draw_extent {};

		VmaAllocator allocator;
		DescriptorAllocator descriptor_allocator;

		VkDescriptorSet draw_image_descriptors;
		VkDescriptorSetLayout draw_image_descriptor_layout;

		VkPipeline gradient_pipeline {};
		VkPipelineLayout gradient_pipeline_layout {};

		DeletionQueue deletion_queue;

		VkFence imm_fence {};
		VkCommandBuffer imm_command_buffer {};
		VkCommandPool imm_command_pool {};

		uint64_t frame_number { 0 };
	} m_vk;

	SDL_Window *m_window { nullptr };
	Logger &m_logger;
};

} // namespace Lunar
