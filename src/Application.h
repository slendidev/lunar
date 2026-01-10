#pragma once

#include <array>
#include <memory>

#include <SDL3/SDL_video.h>
#include <imgui.h>
#include <linux/input-event-codes.h>

#include "Logger.h"
#include "Types.h"

struct libinput;
struct libinput_event_keyboard;
struct libinput_event_pointer;
struct udev;

namespace Lunar {

struct VulkanRenderer;

struct Application {
	Application();
	~Application();

	auto run() -> void;

	auto mouse_captured(bool new_state) -> void;
	auto mouse_captured() const -> bool { return m_mouse_captured; }
	auto toggle_mouse_captured() -> void { mouse_captured(!m_mouse_captured); }
	auto is_key_pressed(uint32_t key) const -> bool;

private:
	auto init_input() -> void;
	auto shutdown_input() -> void;
	auto process_libinput_events() -> void;
	auto handle_keyboard_event(libinput_event_keyboard *event) -> void;
	auto clamp_mouse_to_window(int width, int height) -> void;

	SDL_Window *m_window { nullptr };
	Logger m_logger { "Lunar" };
	std::unique_ptr<VulkanRenderer> m_renderer;

	udev *m_udev { nullptr };
	libinput *m_libinput { nullptr };

	bool m_running { true };
	bool m_mouse_captured { false };
	bool m_show_imgui { false };
	int m_ctrl_pressed_count { 0 };

	double m_mouse_x { 0.0 };
	double m_mouse_y { 0.0 };
	double m_mouse_dx { 0.0 };
	double m_mouse_dy { 0.0 };
	float m_mouse_sensitivity { 0.001f };

	std::array<bool, KEY_MAX + 1> m_key_state {};

	Camera m_camera;
	PolarCoordinate m_cursor;
};

} // namespace Lunar
