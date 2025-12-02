#pragma once

#include <vector>

#include <SDL3/SDL_video.h>
#include <VkBootstrap.h>
#include <vulkan/vulkan_core.h>

#include "src/Logger.h"

namespace Lunar {

struct FrameData {
	VkCommandPool command_pool;
	VkCommandBuffer main_command_buffer;
	VkSemaphore swapchain_semaphore;
	VkFence render_fence;
};

constexpr unsigned FRAME_OVERLAP = 2;

struct Application {
	Application();
	~Application();

	auto run() -> void;

private:
	auto vk_init() -> void;
	auto swapchain_init() -> void;
	auto commands_init() -> void;
	auto sync_init() -> void;

	auto render() -> void;

	auto create_swapchain(uint32_t width, uint32_t height) -> void;
	auto destroy_swapchain() -> void;

	struct {
		vkb::Instance instance;
		vkb::PhysicalDevice phys_dev;
		vkb::Device dev;
		vkb::Swapchain swapchain;
	} m_vkb;

	struct {
		VkSwapchainKHR swapchain { VK_NULL_HANDLE };
		VkFormat swapchain_image_format;
		VkSurfaceKHR surface { nullptr };

		VkQueue graphics_queue { nullptr };
		uint32_t graphics_queue_family { 0 };

		std::vector<VkImage> swapchain_images;
		std::vector<VkImageView> swapchain_image_views;
		std::vector<VkSemaphore> present_semaphores;
		VkExtent2D swapchain_extent;

		std::array<FrameData, FRAME_OVERLAP> frames;
		auto get_current_frame() -> FrameData &
		{
			return frames.at(frame_number % frames.size());
		}

		uint64_t frame_number { 0 };
	} m_vk;

	SDL_Window *m_window { nullptr };
	Logger m_logger { "Lunar" };

	bool m_running { true };
};

} // namespace Lunar
