#include "Application.h"

#include <iostream>
#include <print>
#include <stdexcept>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>

#include "VulkanRenderer.h"

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

	m_renderer = std::make_unique<VulkanRenderer>(m_window, m_logger);

	m_logger.info("App init done!");
}

Application::~Application()
{
	m_renderer.reset();

	SDL_DestroyWindow(m_window);
	SDL_Quit();

	m_logger.info("App destroy done!");
}

auto Application::run() -> void
{
	SDL_Event e;

	while (m_running) {
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_EVENT_QUIT) {
				m_running = false;
			} else if (e.type == SDL_EVENT_WINDOW_RESIZED) {
				int width {}, height {};
				SDL_GetWindowSize(m_window, &width, &height);
				m_renderer->resize(static_cast<uint32_t>(width),
				    static_cast<uint32_t>(height));
			}
		}

		m_renderer->render();
	}
}

} // namespace Lunar
