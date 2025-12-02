#pragma once

#include <SDL3/SDL_video.h>
#include <VkBootstrap.h>
#include <vulkan/vulkan_core.h>

#include "src/Logger.h"

namespace Lunar {

struct Application {
	Application();
	~Application();

	auto run() -> void;

private:
	vkb::Instance m_vkb_instance;
	VkSurfaceKHR m_vk_surface { nullptr };
	SDL_Window *m_window { nullptr };
	Logger m_logger { "Lunar" };

	bool m_running { true };
};

} // namespace Lunar
