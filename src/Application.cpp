#include "Application.h"

#include <SDL3/SDL_init.h>
#include <iostream>
#include <print>

#include <SDL3/SDL_vulkan.h>
#include <VkBootstrap.h>
#include <openxr/openxr.h>

namespace Lunar {

Application::Application()
{
	vkb::InstanceBuilder instance_builder {};
	instance_builder.request_validation_layers()
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
		            std::format("[{}] {}",
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

	m_vkb_instance = instance_builder_ret.value();

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

	if (!SDL_Vulkan_CreateSurface(
	        m_window, m_vkb_instance, nullptr, &m_vk_surface)) {
		m_logger.err("Failed to create vulkan surface");
		throw std::runtime_error("App init fail");
	}

	vkb::PhysicalDeviceSelector phys_device_selector { m_vkb_instance };
	phys_device_selector.set_surface(m_vk_surface)
	    .add_required_extensions({ VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
	        VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
	        VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME,
	        VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME,
	        VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME });
	auto physical_device_selector_return { phys_device_selector.select() };
	if (!physical_device_selector_return) {
		std::println(std::cerr, "Failed to find Vulkan device. Error: {}",
		    physical_device_selector_return.error().message());
		throw std::runtime_error("App init fail");
	}
	auto phys_device = physical_device_selector_return.value();

	m_logger.info("App init done!");
}

Application::~Application()
{
	SDL_Vulkan_DestroySurface(m_vkb_instance, m_vk_surface, nullptr);
	SDL_DestroyWindow(m_window);
	SDL_Quit();

	vkb::destroy_instance(m_vkb_instance);

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
		// nothing else
	}
}

} // namespace Lunar
