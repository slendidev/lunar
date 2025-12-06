#pragma once

#include <memory>

#include <SDL3/SDL_video.h>

#include "Logger.h"

#include <imgui.h>

namespace Lunar {

class VulkanRenderer;

struct Application {
	Application();
	~Application();

	auto run() -> void;
	auto mouse_captured(bool new_state) -> void;
	auto mouse_captured() const -> bool { return m_mouse_captured; }
	auto toggle_mouse_captured() -> void { mouse_captured(!m_mouse_captured); }

private:
	SDL_Window *m_window { nullptr };
	Logger m_logger { "Lunar" };
	std::unique_ptr<VulkanRenderer> m_renderer;

	bool m_running { true };
	bool m_mouse_captured { false };
	bool m_show_imgui { false };
};

} // namespace Lunar
