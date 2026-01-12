#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <SDL3/SDL_video.h>
#include <imgui.h>
#include <linux/input-event-codes.h>
#include <openxr/openxr.h>

#include "Loader.h"
#include "Logger.h"
#include "Skybox.h"
#include "Types.h"

struct libinput;
struct libinput_event_keyboard;
struct libinput_event_pointer;
struct udev;

namespace Lunar {

struct VulkanRenderer;
struct OpenXrState;

struct Application {
	auto run() -> void;
	auto binary_directory() const -> std::filesystem::path;

	auto mouse_captured(bool new_state) -> void;
	auto mouse_captured() const -> bool { return m_mouse_captured; }
	auto toggle_mouse_captured() -> void { mouse_captured(!m_mouse_captured); }
	auto is_key_down(uint32_t key) const -> bool;
	auto is_key_up(uint32_t key) const -> bool;
	auto is_key_pressed(uint32_t key) const -> bool;
	auto is_key_released(uint32_t key) const -> bool;

	static auto the() -> Application &;

private:
	enum class Backend {
		SDL,
		KMS,
	};

	Application();
	~Application();

	auto init_input() -> void;
	auto init_test_meshes() -> void;
	auto asset_directory() -> std::filesystem::path;
	auto shutdown_input() -> void;
	auto process_libinput_events() -> void;
	auto handle_keyboard_event(libinput_event_keyboard *event) -> void;
	auto handle_pointer_motion(libinput_event_pointer *event) -> void;
	auto handle_pointer_button(libinput_event_pointer *event) -> void;
	auto handle_pointer_axis(libinput_event_pointer *event) -> void;
	auto handle_pointer_frame() -> void;
	auto handle_pointer_end_frame() -> void;
	auto handle_pointer_cancel() -> void;
	auto handle_keyboard_key(std::optional<uint32_t> key, bool pressed) -> void;
	auto clamp_mouse_to_window(int width, int height) -> void;
	auto init_openxr() -> void;
	auto init_openxr_session() -> void;
	auto shutdown_openxr() -> void;
	auto poll_openxr_events() -> void;
	auto render_openxr_frame(
	    std::function<void(VulkanRenderer::GL &)> const &record,
	    float dt_seconds) -> bool;
	auto update_camera_from_xr_view(XrView const &view) -> void;

	SDL_Window *m_window { nullptr };
	Backend m_backend { Backend::SDL };
	Logger m_logger { "Lunar" };
	std::unique_ptr<VulkanRenderer> m_renderer;
	Skybox m_skybox;
	std::vector<std::shared_ptr<Mesh>> m_test_meshes;

	udev *m_udev { nullptr };
	libinput *m_libinput { nullptr };

	std::unique_ptr<OpenXrState> m_openxr {};

	bool m_running { true };
	bool m_mouse_captured { false };
	bool m_show_imgui { false };
	bool m_window_focused { true };
	int m_ctrl_pressed_count { 0 };
	std::uint32_t m_screenshot_index { 0 };

	double m_mouse_x { 0.0 };
	double m_mouse_y { 0.0 };
	double m_mouse_dx { 0.0 };
	double m_mouse_dy { 0.0 };
	float m_mouse_sensitivity { 0.001f };

	std::array<bool, KEY_MAX + 1> m_key_state {};
	std::array<bool, KEY_MAX + 1> m_key_state_previous {};

	Camera m_camera;
	PolarCoordinate m_cursor;
};

} // namespace Lunar
