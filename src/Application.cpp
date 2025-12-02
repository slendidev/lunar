#include "Application.h"

#include <cmath>
#include <iostream>
#include <print>

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>
#include <VkBootstrap.h>
#include <openxr/openxr.h>

#include "Util.h"
#include "vulkan/vulkan_core.h"

namespace Lunar {

Application::Application()
{
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		std::println(std::cerr, "Failed to initialize SDL.");
		throw std::runtime_error("App init fail");
	}

	m_window = SDL_CreateWindow(
	    "Lunar", 1280, 720, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	if (!m_window) {
		m_logger.err("Failed to create SDL window");
		throw std::runtime_error("App init fail");
	}

	vk_init();
	swapchain_init();
	commands_init();
	sync_init();

	m_logger.info("App init done!");
}

auto Application::vk_init() -> void
{
	vkb::InstanceBuilder instance_builder {};
	instance_builder
	    .enable_extension(VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME)
	    .request_validation_layers()
	    .set_app_name("Lunar")
	    .set_engine_name("Lunar")
	    .require_api_version(1, 0, 0)
	    .set_debug_callback_user_data_pointer(this)
	    .set_debug_callback(
	        [](VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
	            VkDebugUtilsMessageTypeFlagsEXT message_type,
	            VkDebugUtilsMessengerCallbackDataEXT const *callback_data,
	            void *user_data) {
		        auto app { reinterpret_cast<Application *>(user_data) };

		        auto level = Logger::Level::Debug;
		        if (message_severity
		            & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
			        level = Logger::Level::Error;
		        } else if (message_severity
		            & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
			        level = Logger::Level::Warning;
		        } else if (message_severity
		            & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
			        level = Logger::Level::Info;
		        }

		        app->m_logger.log(level,
		            std::format("[Vulkan] [{}] {}",
		                vkb::to_string_message_type(message_type),
		                callback_data->pMessage));

		        return VK_FALSE;
	        });
	auto const instance_builder_ret { instance_builder.build() };
	if (!instance_builder_ret) {
		std::println(std::cerr, "Failed to create Vulkan instance. Error: {}",
		    instance_builder_ret.error().message());
		throw std::runtime_error("App init fail");
	}

	m_vkb.instance = instance_builder_ret.value();

	if (!SDL_Vulkan_CreateSurface(
	        m_window, m_vkb.instance, nullptr, &m_vk.surface)) {
		m_logger.err("Failed to create vulkan surface");
		throw std::runtime_error("App init fail");
	}

	vkb::PhysicalDeviceSelector phys_device_selector { m_vkb.instance };
	phys_device_selector.set_surface(m_vk.surface)
	    .add_desired_extensions({
	        VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
	        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
	        VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
	        VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME,
	        VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME,
	        VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,
	        VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
	        VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
	        VK_KHR_MAINTENANCE1_EXTENSION_NAME,
	        VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,
	    });
	auto physical_device_selector_return { phys_device_selector.select() };
	if (!physical_device_selector_return) {
		std::println(std::cerr,
		    "Failed to find Vulkan physical device. Error: {}",
		    physical_device_selector_return.error().message());
		throw std::runtime_error("App init fail");
	}
	m_vkb.phys_dev = physical_device_selector_return.value();

	m_logger.info("Chosen Vulkan physical device: {}",
	    m_vkb.phys_dev.properties.deviceName);

	vkb::DeviceBuilder device_builder { m_vkb.phys_dev };
	auto dev_ret { device_builder.build() };
	if (!dev_ret) {
		std::println(std::cerr, "Failed to create Vulkan device. Error: {}",
		    dev_ret.error().message());
		throw std::runtime_error("App init fail");
	}
	m_vkb.dev = dev_ret.value();

	auto queue_ret { m_vkb.dev.get_queue(vkb::QueueType::graphics) };
	if (!queue_ret) {
		std::println(std::cerr, "Failed to get graphics queue. Error: {}",
		    queue_ret.error().message());
		throw std::runtime_error("App init fail");
	}
	m_vk.graphics_queue = queue_ret.value();

	auto queue_family_ret { m_vkb.dev.get_queue_index(
		vkb::QueueType::graphics) };
	if (!queue_family_ret) {
		std::println(std::cerr, "Failed to get graphics queue. Error: {}",
		    queue_family_ret.error().message());
		throw std::runtime_error("App init fail");
	}
	m_vk.graphics_queue_family = queue_family_ret.value();
}

auto Application::swapchain_init() -> void
{
	int w, h;
	SDL_GetWindowSize(m_window, &w, &h);
	create_swapchain(w, h);
}

auto Application::commands_init() -> void
{
	VkCommandPoolCreateInfo ci {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = m_vk.graphics_queue_family,
	};
	for (auto &frame_data : m_vk.frames) {
		VK_CHECK(m_logger,
		    vkCreateCommandPool(
		        m_vkb.dev, &ci, nullptr, &frame_data.command_pool));

		VkCommandBufferAllocateInfo ai {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.pNext = nullptr,
			.commandPool = frame_data.command_pool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		};

		VK_CHECK(m_logger,
		    vkAllocateCommandBuffers(
		        m_vkb.dev, &ai, &frame_data.main_command_buffer));
	}
}

auto Application::sync_init() -> void
{
	VkFenceCreateInfo fence_ci {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.pNext = nullptr,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT,
	};
	VkSemaphoreCreateInfo semaphore_ci {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
	};
	for (auto &frame_data : m_vk.frames) {
		VK_CHECK(m_logger,
		    vkCreateFence(
		        m_vkb.dev, &fence_ci, nullptr, &frame_data.render_fence));

		VK_CHECK(m_logger,
		    vkCreateSemaphore(m_vkb.dev, &semaphore_ci, nullptr,
		        &frame_data.swapchain_semaphore));
	}
}

auto Application::render() -> void
{
	defer(m_vk.frame_number++);

	VK_CHECK(m_logger,
	    vkWaitForFences(m_vkb.dev, 1, &m_vk.get_current_frame().render_fence,
	        true, 1'000'000'000));
	VK_CHECK(m_logger,
	    vkResetFences(m_vkb.dev, 1, &m_vk.get_current_frame().render_fence));

	uint32_t swapchain_image_idx;
	VK_CHECK(m_logger,
	    vkAcquireNextImageKHR(m_vkb.dev, m_vk.swapchain, 1000000000,
	        m_vk.get_current_frame().swapchain_semaphore, nullptr,
	        &swapchain_image_idx));

	auto cmd { m_vk.get_current_frame().main_command_buffer };
	VK_CHECK(m_logger, vkResetCommandBuffer(cmd, 0));
	VkCommandBufferBeginInfo cmd_begin_info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = nullptr,
	};
	VK_CHECK(m_logger, vkBeginCommandBuffer(cmd, &cmd_begin_info));

	vkutil::transition_image(cmd, m_vk.swapchain_images.at(swapchain_image_idx),
	    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	VkClearColorValue clear_value;
	float flash { std::abs(std::sin(m_vk.frame_number / 60.f)) };
	clear_value = { { 0x64 / 255.0f * flash, 0x95 / 255.0f * flash,
		0xED / 255.0f * flash, 1.0f } };

	VkImageSubresourceRange clear_range {
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.baseMipLevel = 0,
		.levelCount = 1,
		.baseArrayLayer = 0,
		.layerCount = 1,
	};
	vkCmdClearColorImage(cmd, m_vk.swapchain_images[swapchain_image_idx],
	    VK_IMAGE_LAYOUT_GENERAL, &clear_value, 1, &clear_range);

	vkutil::transition_image(cmd, m_vk.swapchain_images[swapchain_image_idx],
	    VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	VK_CHECK(m_logger, vkEndCommandBuffer(cmd));

	VkSemaphore render_semaphore
	    = m_vk.present_semaphores.at(swapchain_image_idx);
	VkPipelineStageFlags wait_stage
	    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submit_info {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pNext = nullptr,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &m_vk.get_current_frame().swapchain_semaphore,
		.pWaitDstStageMask = &wait_stage,
		.commandBufferCount = 1,
		.pCommandBuffers = &cmd,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &render_semaphore,
	};

	VK_CHECK(m_logger,
	    vkQueueSubmit(m_vk.graphics_queue, 1, &submit_info,
	        m_vk.get_current_frame().render_fence));

	VkPresentInfoKHR present_info = {};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.pNext = nullptr;
	present_info.pSwapchains = &m_vk.swapchain;
	present_info.swapchainCount = 1;

	present_info.pWaitSemaphores = &render_semaphore;
	present_info.waitSemaphoreCount = 1;

	present_info.pImageIndices = &swapchain_image_idx;

	VK_CHECK(m_logger, vkQueuePresentKHR(m_vk.graphics_queue, &present_info));
}

auto Application::create_swapchain(uint32_t width, uint32_t height) -> void
{
	vkb::SwapchainBuilder builder { m_vkb.phys_dev, m_vkb.dev, m_vk.surface };
	m_vk.swapchain_image_format = VK_FORMAT_B8G8R8A8_UNORM;
	auto const swapchain_ret { builder
		    .set_desired_format({
		        .format = m_vk.swapchain_image_format,
		        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
		    })
		    .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		    .set_desired_extent(width, height)
		    .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		    .build() };
	if (!swapchain_ret) {
		std::println(std::cerr, "Failed to create swapchain. Error: {}",
		    swapchain_ret.error().message());
		throw std::runtime_error("App init fail");
	}
	m_vkb.swapchain = swapchain_ret.value();

	m_vk.swapchain = m_vkb.swapchain.swapchain;
	m_vk.swapchain_extent = m_vkb.swapchain.extent;
	m_vk.swapchain_images = m_vkb.swapchain.get_images().value();
	m_vk.swapchain_image_views = m_vkb.swapchain.get_image_views().value();

	VkSemaphoreCreateInfo semaphore_ci {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
	};
	m_vk.present_semaphores.resize(m_vk.swapchain_images.size());
	for (auto &semaphore : m_vk.present_semaphores) {
		VK_CHECK(m_logger,
		    vkCreateSemaphore(m_vkb.dev, &semaphore_ci, nullptr, &semaphore));
	}
}

auto Application::destroy_swapchain() -> void
{
	if (m_vk.swapchain == VK_NULL_HANDLE)
		return;

	for (auto const semaphore : m_vk.present_semaphores) {
		vkDestroySemaphore(m_vkb.dev, semaphore, nullptr);
	}
	for (auto const &iv : m_vk.swapchain_image_views) {
		vkDestroyImageView(m_vkb.dev, iv, nullptr);
	}
	vkDestroySwapchainKHR(m_vkb.dev, m_vk.swapchain, nullptr);

	m_vk.swapchain = VK_NULL_HANDLE;
	m_vk.swapchain_image_views.clear();
	m_vk.swapchain_images.clear();
	m_vk.present_semaphores.clear();
}

Application::~Application()
{
	vkDeviceWaitIdle(m_vkb.dev);

	for (auto &frame_data : m_vk.frames) {
		vkDestroyCommandPool(m_vkb.dev, frame_data.command_pool, nullptr);

		vkDestroyFence(m_vkb.dev, frame_data.render_fence, nullptr);
		vkDestroySemaphore(m_vkb.dev, frame_data.swapchain_semaphore, nullptr);
	}

	destroy_swapchain();

	SDL_Vulkan_DestroySurface(m_vkb.instance, m_vk.surface, nullptr);
	SDL_DestroyWindow(m_window);
	SDL_Quit();

	vkb::destroy_device(m_vkb.dev);
	vkb::destroy_instance(m_vkb.instance);

	m_logger.info("App destroy done!");
}

auto Application::run() -> void
{
	SDL_Event e;

	while (m_running) {
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_EVENT_QUIT)
				m_running = false;
		}

		render();
	}
}

} // namespace Lunar
