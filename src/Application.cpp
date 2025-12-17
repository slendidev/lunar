#include "Application.h"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <fcntl.h>
#include <iostream>
#include <optional>
#include <print>
#include <stdexcept>
#include <unistd.h>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>

#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

#include <libinput.h>
#include <libudev.h>
#include <linux/input-event-codes.h>

#include <smath.hpp>

#include "Util.h"
#include "VulkanRenderer.h"

namespace {

int open_restricted(char const *path, int flags, void * /*user_data*/)
{
	int fd { open(path, flags | O_CLOEXEC) };
	return fd < 0 ? -errno : fd;
}

void close_restricted(int fd, void * /*user_data*/) { close(fd); }

libinput_interface const g_libinput_interface {
	.open_restricted = open_restricted,
	.close_restricted = close_restricted,
};

auto linux_key_to_imgui(uint32_t keycode) -> std::optional<ImGuiKey>
{
	switch (keycode) {
	case KEY_ESC:
		return ImGuiKey_Escape;
	case KEY_TAB:
		return ImGuiKey_Tab;
	case KEY_ENTER:
		return ImGuiKey_Enter;
	case KEY_BACKSPACE:
		return ImGuiKey_Backspace;
	case KEY_SPACE:
		return ImGuiKey_Space;
	case KEY_LEFTSHIFT:
		return ImGuiKey_LeftShift;
	case KEY_RIGHTSHIFT:
		return ImGuiKey_RightShift;
	case KEY_LEFTCTRL:
		return ImGuiKey_LeftCtrl;
	case KEY_RIGHTCTRL:
		return ImGuiKey_RightCtrl;
	case KEY_LEFTALT:
		return ImGuiKey_LeftAlt;
	case KEY_RIGHTALT:
		return ImGuiKey_RightAlt;
	case KEY_LEFTMETA:
		return ImGuiKey_LeftSuper;
	case KEY_RIGHTMETA:
		return ImGuiKey_RightSuper;
	case KEY_CAPSLOCK:
		return ImGuiKey_CapsLock;
	case KEY_NUMLOCK:
		return ImGuiKey_NumLock;
	case KEY_SCROLLLOCK:
		return ImGuiKey_ScrollLock;
	case KEY_UP:
		return ImGuiKey_UpArrow;
	case KEY_DOWN:
		return ImGuiKey_DownArrow;
	case KEY_LEFT:
		return ImGuiKey_LeftArrow;
	case KEY_RIGHT:
		return ImGuiKey_RightArrow;
	case KEY_HOME:
		return ImGuiKey_Home;
	case KEY_END:
		return ImGuiKey_End;
	case KEY_PAGEUP:
		return ImGuiKey_PageUp;
	case KEY_PAGEDOWN:
		return ImGuiKey_PageDown;
	case KEY_INSERT:
		return ImGuiKey_Insert;
	case KEY_DELETE:
		return ImGuiKey_Delete;
	case KEY_F1:
		return ImGuiKey_F1;
	case KEY_F2:
		return ImGuiKey_F2;
	case KEY_F3:
		return ImGuiKey_F3;
	case KEY_F4:
		return ImGuiKey_F4;
	case KEY_F5:
		return ImGuiKey_F5;
	case KEY_F6:
		return ImGuiKey_F6;
	case KEY_F7:
		return ImGuiKey_F7;
	case KEY_F8:
		return ImGuiKey_F8;
	case KEY_F9:
		return ImGuiKey_F9;
	case KEY_F10:
		return ImGuiKey_F10;
	case KEY_F11:
		return ImGuiKey_F11;
	case KEY_F12:
		return ImGuiKey_F12;
	case KEY_0:
		return ImGuiKey_0;
	case KEY_1:
		return ImGuiKey_1;
	case KEY_2:
		return ImGuiKey_2;
	case KEY_3:
		return ImGuiKey_3;
	case KEY_4:
		return ImGuiKey_4;
	case KEY_5:
		return ImGuiKey_5;
	case KEY_6:
		return ImGuiKey_6;
	case KEY_7:
		return ImGuiKey_7;
	case KEY_8:
		return ImGuiKey_8;
	case KEY_9:
		return ImGuiKey_9;
	case KEY_A:
		return ImGuiKey_A;
	case KEY_B:
		return ImGuiKey_B;
	case KEY_C:
		return ImGuiKey_C;
	case KEY_D:
		return ImGuiKey_D;
	case KEY_E:
		return ImGuiKey_E;
	case KEY_F:
		return ImGuiKey_F;
	case KEY_G:
		return ImGuiKey_G;
	case KEY_H:
		return ImGuiKey_H;
	case KEY_I:
		return ImGuiKey_I;
	case KEY_J:
		return ImGuiKey_J;
	case KEY_K:
		return ImGuiKey_K;
	case KEY_L:
		return ImGuiKey_L;
	case KEY_M:
		return ImGuiKey_M;
	case KEY_N:
		return ImGuiKey_N;
	case KEY_O:
		return ImGuiKey_O;
	case KEY_P:
		return ImGuiKey_P;
	case KEY_Q:
		return ImGuiKey_Q;
	case KEY_R:
		return ImGuiKey_R;
	case KEY_S:
		return ImGuiKey_S;
	case KEY_T:
		return ImGuiKey_T;
	case KEY_U:
		return ImGuiKey_U;
	case KEY_V:
		return ImGuiKey_V;
	case KEY_W:
		return ImGuiKey_W;
	case KEY_X:
		return ImGuiKey_X;
	case KEY_Y:
		return ImGuiKey_Y;
	case KEY_Z:
		return ImGuiKey_Z;
	default:
		return std::nullopt;
	}
}

} // namespace

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

	init_input();

	mouse_captured(true);

	m_logger.info("App init done!");
}

Application::~Application()
{
	m_renderer.reset();

	shutdown_input();

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

		process_libinput_events();

		while (SDL_PollEvent(&e)) {
			bool forward_to_imgui { false };
			if (e.type == SDL_EVENT_QUIT) {
				m_running = false;
			} else if (e.type == SDL_EVENT_WINDOW_RESIZED) {
				int width {}, height {};
				SDL_GetWindowSize(m_window, &width, &height);
				m_renderer->resize(static_cast<uint32_t>(width),
				    static_cast<uint32_t>(height));
				clamp_mouse_to_window(width, height);
				forward_to_imgui = true;
			} else if (e.type == SDL_EVENT_MOUSE_MOTION) {
				m_mouse_x = e.motion.x;
				m_mouse_y = e.motion.y;
				forward_to_imgui = true;
			} else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN
			    || e.type == SDL_EVENT_MOUSE_BUTTON_UP) {
				m_mouse_x = e.button.x;
				m_mouse_y = e.button.y;
				forward_to_imgui = true;
			} else if (e.type == SDL_EVENT_MOUSE_WHEEL) {
				m_mouse_x = e.wheel.mouse_x;
				m_mouse_y = e.wheel.mouse_y;
				forward_to_imgui = true;
			}

			if (forward_to_imgui)
				ImGui_ImplSDL3_ProcessEvent(&e);
		}

		ImGui_ImplSDL3_NewFrame();
		ImGui_ImplVulkan_NewFrame();

		ImGui::NewFrame();

		if (m_show_imgui) {
			ImGui::SetNextWindowCollapsed(true, ImGuiCond_Once);
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

			if (ImGui::Begin("Fun menu")) {
				defer(ImGui::End());

				static std::array<char const *, 4> const aa_items {
					"None",
					"MSAA 2X",
					"MSAA 4X",
					"MSAA 8X",
				};
				int selected_item {
					static_cast<int>(m_renderer->antialiasing()),
				};
				int prev_selected_item { selected_item };
				ImGui::Combo("Antialiasing", &selected_item, aa_items.data(),
				    aa_items.size());
				if (prev_selected_item != selected_item) {
					m_renderer->set_antialiasing(
					    static_cast<VulkanRenderer::AntiAliasingKind>(
					        selected_item));
				}
			}
		}

		ImGui::Render();

		m_renderer->set_antialiasing(VulkanRenderer::AntiAliasingKind::MSAA_8X);

		m_renderer->render([&](VulkanRenderer::GL &gl) {
			auto view { smath::matrix_look_at(smath::Vec3 { 0.0f, 0.0f, 3.0f },
				smath::Vec3 { 0.0f, 0.0f, 0.0f },
				smath::Vec3 { 0.0f, 1.0f, 0.0f }, false) };
			auto const draw_extent = m_renderer->draw_extent();
			auto const aspect = draw_extent.height == 0
			    ? 1.0f
			    : static_cast<float>(draw_extent.width)
			        / static_cast<float>(draw_extent.height);
			auto projection { smath::matrix_perspective(
				smath::deg(70.0f), aspect, 0.1f, 10000.0f) };
			projection[1][1] *= -1;
			auto view_projection { projection * view };

			auto rect_model { smath::scale(
				smath::translate(smath::Vec3 { 0.0f, 0.0f, -5.0f }),
				smath::Vec3 { 5.0f, 5.0f, 1.0f }) };

			gl.set_transform(view_projection * rect_model);

			gl.set_texture();
			auto const &meshes { m_renderer->test_meshes() };
			if (meshes.size() > 2 && !meshes[2]->surfaces.empty()) {
				auto const &surface = meshes[2]->surfaces[0];
				gl.draw_mesh(meshes[2]->mesh_buffers, view_projection,
				    surface.count, surface.start_index);
			}

			gl.set_texture(&m_renderer->white_texture());

			gl.begin(VulkanRenderer::GL::GeometryKind::Quads);

			gl.color(smath::Vec3 { 0.0f, 0.0f, 0.0f });
			gl.uv(smath::Vec2 { 1.0f, 1.0f });
			gl.vert(smath::Vec3 { 0.5f, -0.5f, 0.0f });

			gl.color(smath::Vec3 { 0.5f, 0.5f, 0.5f });
			gl.uv(smath::Vec2 { 1.0f, 0.0f });
			gl.vert(smath::Vec3 { 0.5f, 0.5f, 0.0f });

			gl.color(smath::Vec3 { 1.0f, 0.0f, 0.0f });
			gl.uv(smath::Vec2 { 0.0f, 1.0f });
			gl.vert(smath::Vec3 { -0.5f, -0.5f, 0.0f });

			gl.color(smath::Vec3 { 0.0f, 1.0f, 0.0f });
			gl.uv(smath::Vec2 { 0.0f, 0.0f });
			gl.vert(smath::Vec3 { -0.5f, 0.5f, 0.0f });

			gl.end();

			gl.draw_rectangle({ -0.5f, 0.5f }, { 0.5f, 0.5f });
			gl.draw_rectangle(
			    { 0, 0.5f }, { 0.5f, 0.5f }, { Colors::TEAL, 1.0f });
		});
	}
}

auto Application::init_input() -> void
{
	m_udev = udev_new();
	if (!m_udev) {
		m_logger.err("Failed to create udev context");
		throw std::runtime_error("App init fail");
	}

	m_libinput
	    = libinput_udev_create_context(&g_libinput_interface, this, m_udev);
	if (!m_libinput) {
		m_logger.err("Failed to create libinput context");
		shutdown_input();
		throw std::runtime_error("App init fail");
	}

	if (libinput_udev_assign_seat(m_libinput, "seat0") != 0) {
		m_logger.err("Failed to assign libinput seat");
		shutdown_input();
		throw std::runtime_error("App init fail");
	}

	int width {}, height {};
	SDL_GetWindowSize(m_window, &width, &height);
	float mouse_x {}, mouse_y {};
	SDL_GetMouseState(&mouse_x, &mouse_y);
	m_mouse_x = mouse_x;
	m_mouse_y = mouse_y;
	ImGui::GetIO().AddMousePosEvent(
	    static_cast<float>(m_mouse_x), static_cast<float>(m_mouse_y));
}

auto Application::shutdown_input() -> void
{
	if (m_libinput) {
		libinput_unref(m_libinput);
		m_libinput = nullptr;
	}

	if (m_udev) {
		udev_unref(m_udev);
		m_udev = nullptr;
	}
}

auto Application::process_libinput_events() -> void
{
	if (!m_libinput)
		return;

	if (int const rc { libinput_dispatch(m_libinput) }; rc != 0) {
		m_logger.err("libinput_dispatch failed ({})", rc);
		return;
	}

	for (libinput_event *event { libinput_get_event(m_libinput) };
	    event != nullptr; event = libinput_get_event(m_libinput)) {
		switch (libinput_event_get_type(event)) {
		case LIBINPUT_EVENT_KEYBOARD_KEY:
			handle_keyboard_event(libinput_event_get_keyboard_event(event));
			break;
		default:
			break;
		}

		libinput_event_destroy(event);
	}
}

auto Application::handle_keyboard_event(libinput_event_keyboard *event) -> void
{
	uint32_t const key { libinput_event_keyboard_get_key(event) };
	auto const state { libinput_event_keyboard_get_key_state(event) };
	bool const pressed { state == LIBINPUT_KEY_STATE_PRESSED };

	if (key == KEY_LEFTCTRL || key == KEY_RIGHTCTRL) {
		if (pressed) {
			++m_ctrl_pressed_count;
		} else if (m_ctrl_pressed_count > 0) {
			--m_ctrl_pressed_count;
		}
	}

	if (pressed && key == KEY_F11 && m_ctrl_pressed_count > 0) {
		mouse_captured(!mouse_captured());
		m_show_imgui = !mouse_captured();
	}

	if (auto imgui_key { linux_key_to_imgui(key) }) {
		ImGui::GetIO().AddKeyEvent(*imgui_key, pressed);
	}
}

auto Application::clamp_mouse_to_window(int width, int height) -> void
{
	double const max_x { std::max(0.0, static_cast<double>(width - 1)) };
	double const max_y { std::max(0.0, static_cast<double>(height - 1)) };

	m_mouse_x = std::clamp(m_mouse_x, 0.0, max_x);
	m_mouse_y = std::clamp(m_mouse_y, 0.0, max_y);

	ImGui::GetIO().AddMousePosEvent(
	    static_cast<float>(m_mouse_x), static_cast<float>(m_mouse_y));
}

auto Application::mouse_captured(bool new_state) -> void
{
	if (!SDL_SetWindowRelativeMouseMode(m_window, new_state)) {
		m_logger.err("Failed to capture mouse");
		return;
	}

	m_mouse_captured = new_state;
}

} // namespace Lunar
