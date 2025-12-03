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

private:
	SDL_Window *m_window { nullptr };
	Logger m_logger { "Lunar" };
	std::unique_ptr<VulkanRenderer> m_renderer;

	ImGuiContext *m_imgui_context;

	bool m_running { true };
};

} // namespace Lunar
