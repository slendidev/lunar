#include "Application.h"

#include <iostream>
#include <print>
#include <stdexcept>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>

#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

#include "Util.h"
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

	mouse_captured(true);

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

	ImGuiIO &io = ImGui::GetIO();
	io.IniFilename = nullptr;

	uint64_t last { 0 };
	float fps { 0.0f };
	while (m_running) {
		uint64_t now { SDL_GetTicks() };
		uint64_t dt { now - last };
		last = now;

		if (dt > 0)
			fps = 1000.0f / (float)dt;

		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_EVENT_QUIT) {
				m_running = false;
			} else if (e.type == SDL_EVENT_WINDOW_RESIZED) {
				int width {}, height {};
				SDL_GetWindowSize(m_window, &width, &height);
				m_renderer->resize(static_cast<uint32_t>(width),
				    static_cast<uint32_t>(height));
			} else if (e.type == SDL_EVENT_KEY_DOWN && e.key.repeat == false) {
				if (e.key.key == SDLK_F11 && e.key.mod & SDL_KMOD_LCTRL) {
					mouse_captured(!mouse_captured());
					m_show_imgui = mouse_captured();
				}
			}

			ImGui_ImplSDL3_ProcessEvent(&e);
		}

		ImGui_ImplSDL3_NewFrame();
		ImGui_ImplVulkan_NewFrame();

		ImGui::NewFrame();

		if (m_show_imgui) {
			ImGui::ShowDemoWindow();

			ImGui::SetNextWindowSize({ 100, 50 });
			ImGui::SetNextWindowPos({ 0, 0 });
			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4 { 0, 0, 0, 0.5f });
			if (ImGui::Begin("Debug Info", nullptr,
			        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
				defer(ImGui::End());

				ImGui::Text("%s", std::format("FPS: {:.2f}", fps).c_str());
			}
			ImGui::PopStyleColor();
		}

		ImGui::Render();

		m_renderer->render();
	}
}

auto Application::mouse_captured(bool new_state) -> void
{
	SDL_CaptureMouse(new_state);

	m_mouse_captured = new_state;
}

} // namespace Lunar
