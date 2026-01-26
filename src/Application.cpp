#include "Application.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <format>
#include <iostream>
#include <numbers>
#include <optional>
#include <print>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unistd.h>
#include <vulkan/vulkan.h>

#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr_platform.h>

#include "wayland-server-protocol.h"

#if defined(__clang__)
#	pragma clang diagnostic push
#	pragma clang diagnostic ignored "-Wreserved-identifier"
#	pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#	pragma clang diagnostic ignored "-Wcast-qual"
#	pragma clang diagnostic ignored "-Wmissing-field-initializers"
#	pragma clang diagnostic ignored "-Wused-but-marked-unused"
#	pragma clang diagnostic ignored "-Wmissing-prototypes"
#	pragma clang diagnostic ignored "-Wextra-semi-stmt"
#	pragma clang diagnostic ignored "-Wimplicit-int-conversion"
#endif
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../thirdparty/stb/stb_image_write.h"
#if defined(__clang__)
#	pragma clang diagnostic pop
#endif

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_filesystem.h>
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
#include "wayland/Shm.h"
#include "wayland/Surface.h"
#include "wayland/WaylandServer.h"

#if defined(TRACY_ENABLE)
#	include <tracy/Tracy.hpp>
#endif

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
	case KEY_KP0:
		return ImGuiKey_Keypad0;
	case KEY_KP1:
		return ImGuiKey_Keypad1;
	case KEY_KP2:
		return ImGuiKey_Keypad2;
	case KEY_KP3:
		return ImGuiKey_Keypad3;
	case KEY_KP4:
		return ImGuiKey_Keypad4;
	case KEY_KP5:
		return ImGuiKey_Keypad5;
	case KEY_KP6:
		return ImGuiKey_Keypad6;
	case KEY_KP7:
		return ImGuiKey_Keypad7;
	case KEY_KP8:
		return ImGuiKey_Keypad8;
	case KEY_KP9:
		return ImGuiKey_Keypad9;
	case KEY_KPDOT:
		return ImGuiKey_KeypadDecimal;
	case KEY_KPENTER:
		return ImGuiKey_KeypadEnter;
	case KEY_KPPLUS:
		return ImGuiKey_KeypadAdd;
	case KEY_KPMINUS:
		return ImGuiKey_KeypadSubtract;
	case KEY_KPASTERISK:
		return ImGuiKey_KeypadMultiply;
	case KEY_KPSLASH:
		return ImGuiKey_KeypadDivide;
	case KEY_KPEQUAL:
		return ImGuiKey_KeypadEqual;
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

auto linux_key_to_char(uint32_t keycode, bool shift) -> std::optional<char32_t>
{
	switch (keycode) {
	case KEY_0:
		return shift ? U')' : U'0';
	case KEY_1:
		return shift ? U'!' : U'1';
	case KEY_2:
		return shift ? U'@' : U'2';
	case KEY_3:
		return shift ? U'#' : U'3';
	case KEY_4:
		return shift ? U'$' : U'4';
	case KEY_5:
		return shift ? U'%' : U'5';
	case KEY_6:
		return shift ? U'^' : U'6';
	case KEY_7:
		return shift ? U'&' : U'7';
	case KEY_8:
		return shift ? U'*' : U'8';
	case KEY_9:
		return shift ? U'(' : U'9';
	case KEY_KP0:
		return U'0';
	case KEY_KP1:
		return U'1';
	case KEY_KP2:
		return U'2';
	case KEY_KP3:
		return U'3';
	case KEY_KP4:
		return U'4';
	case KEY_KP5:
		return U'5';
	case KEY_KP6:
		return U'6';
	case KEY_KP7:
		return U'7';
	case KEY_KP8:
		return U'8';
	case KEY_KP9:
		return U'9';
	case KEY_Q:
		return shift ? U'Q' : U'q';
	case KEY_W:
		return shift ? U'W' : U'w';
	case KEY_E:
		return shift ? U'E' : U'e';
	case KEY_R:
		return shift ? U'R' : U'r';
	case KEY_T:
		return shift ? U'T' : U't';
	case KEY_Y:
		return shift ? U'Y' : U'y';
	case KEY_U:
		return shift ? U'U' : U'u';
	case KEY_I:
		return shift ? U'I' : U'i';
	case KEY_O:
		return shift ? U'O' : U'o';
	case KEY_P:
		return shift ? U'P' : U'p';
	case KEY_A:
		return shift ? U'A' : U'a';
	case KEY_S:
		return shift ? U'S' : U's';
	case KEY_D:
		return shift ? U'D' : U'd';
	case KEY_F:
		return shift ? U'F' : U'f';
	case KEY_G:
		return shift ? U'G' : U'g';
	case KEY_H:
		return shift ? U'H' : U'h';
	case KEY_J:
		return shift ? U'J' : U'j';
	case KEY_K:
		return shift ? U'K' : U'k';
	case KEY_L:
		return shift ? U'L' : U'l';
	case KEY_Z:
		return shift ? U'Z' : U'z';
	case KEY_X:
		return shift ? U'X' : U'x';
	case KEY_C:
		return shift ? U'C' : U'c';
	case KEY_V:
		return shift ? U'V' : U'v';
	case KEY_B:
		return shift ? U'B' : U'b';
	case KEY_N:
		return shift ? U'N' : U'n';
	case KEY_M:
		return shift ? U'M' : U'm';
	case KEY_SPACE:
		return U' ';
	case KEY_MINUS:
		return shift ? U'_' : U'-';
	case KEY_EQUAL:
		return shift ? U'+' : U'=';
	case KEY_LEFTBRACE:
		return shift ? U'{' : U'[';
	case KEY_RIGHTBRACE:
		return shift ? U'}' : U']';
	case KEY_BACKSLASH:
		return shift ? U'|' : U'\\';
	case KEY_SEMICOLON:
		return shift ? U':' : U';';
	case KEY_APOSTROPHE:
		return shift ? U'"' : U'\'';
	case KEY_GRAVE:
		return shift ? U'~' : U'`';
	case KEY_COMMA:
		return shift ? U'<' : U',';
	case KEY_DOT:
		return shift ? U'>' : U'.';
	case KEY_SLASH:
		return shift ? U'?' : U'/';
	case KEY_KPDOT:
		return U'.';
	case KEY_KPPLUS:
		return U'+';
	case KEY_KPMINUS:
		return U'-';
	case KEY_KPASTERISK:
		return U'*';
	case KEY_KPSLASH:
		return U'/';
	case KEY_KPEQUAL:
		return U'=';
	default:
		return std::nullopt;
	}
}

auto split_extension_list(std::string_view list) -> std::vector<std::string>
{
	std::vector<std::string> extensions;
	std::size_t start = 0;
	while (start < list.size()) {
		while (start < list.size() && list[start] == ' ') {
			++start;
		}
		if (start >= list.size()) {
			break;
		}
		auto end { list.find(' ', start) };
		if (end == std::string_view::npos) {
			end = list.size();
		}
		auto token { list.substr(start, end - start) };
		if (!token.empty()) {
			extensions.emplace_back(token);
		}
		start = end + 1;
	}
	return extensions;
}

auto xr_rotate_vector(XrQuaternionf q, smath::Vec3 v) -> smath::Vec3
{
	smath::Vec3 u { q.x, q.y, q.z };
	float const s = q.w;
	auto const dot { u.dot(v) };
	auto const u_dot { u.dot(u) };
	auto const cross { u.cross(v) };
	return (u * (2.0f * dot)) + (v * (s * s - u_dot)) + (cross * (2.0f * s));
}

[[maybe_unused]] auto xr_result_to_string(XrInstance instance, XrResult result)
    -> std::string
{
	std::array<char, XR_MAX_RESULT_STRING_SIZE> buffer {};
	xrResultToString(instance, result, buffer.data());
	return std::string { buffer.data() };
}

auto make_shm_texture(Lunar::Wayland::ShmBuffer const &buffer)
    -> std::optional<Lunar::CPUTexture>
{
	if (!buffer.pool || !buffer.data()) {
		return {};
	}
	if (buffer.width <= 0 || buffer.height <= 0 || buffer.stride <= 0) {
		return {};
	}
	if (buffer.format != WL_SHM_FORMAT_ARGB8888
	    && buffer.format != WL_SHM_FORMAT_XRGB8888) {
		return {};
	}

	auto const width { static_cast<uint32_t>(buffer.width) };
	auto const height { static_cast<uint32_t>(buffer.height) };
	auto const row_bytes { static_cast<size_t>(width) * 4 };
	if (buffer.stride < static_cast<std::int32_t>(row_bytes)) {
		return {};
	}

	std::vector<uint8_t> pixels(row_bytes * height);
	auto const *src { reinterpret_cast<uint8_t const *>(buffer.data()) };
	for (uint32_t y = 0; y < height; ++y) {
		auto const *row_src { src + static_cast<size_t>(buffer.stride) * y };
		auto const dst_y { height - y - 1 };
		auto *row_dst { pixels.data() + row_bytes * dst_y };
		std::memcpy(row_dst, row_src, row_bytes);
		if (buffer.format == WL_SHM_FORMAT_XRGB8888) {
			for (uint32_t x = 0; x < width; ++x) {
				row_dst[x * 4 + 3] = 0xff;
			}
		}
	}

	return Lunar::CPUTexture(
	    std::move(pixels), width, height, vk::Format::eB8G8R8A8Unorm);
}

} // namespace

namespace Lunar {

struct OpenXrSwapchain {
	XrSwapchain handle { XR_NULL_HANDLE };
	vk::Extent2D extent {};
	std::vector<XrSwapchainImageVulkanKHR> images {};
};

struct OpenXrState {
	bool enabled { false };
	bool session_running { false };
	bool use_vulkan_enable2 { false };
	XrInstance instance { XR_NULL_HANDLE };
	XrSystemId system_id { XR_NULL_SYSTEM_ID };
	XrSession session { XR_NULL_HANDLE };
	XrSpace app_space { XR_NULL_HANDLE };
	XrSessionState session_state { XR_SESSION_STATE_UNKNOWN };
	XrViewConfigurationType view_type {
		XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO
	};
	int64_t color_format { 0 };
	std::vector<XrView> views {};
	std::vector<XrViewConfigurationView> view_configs {};
	std::vector<OpenXrSwapchain> swapchains {};
	std::vector<std::string> instance_extensions {};
	std::vector<std::string> device_extensions {};
	PFN_xrGetVulkanGraphicsDevice2KHR get_graphics_device2 { nullptr };
	PFN_xrGetVulkanGraphicsDeviceKHR get_graphics_device { nullptr };
	PFN_xrGetVulkanGraphicsRequirements2KHR get_requirements2 { nullptr };
	PFN_xrGetVulkanGraphicsRequirementsKHR get_requirements { nullptr };
	bool hand_tracking_supported { false };
	XrHandTrackerEXT left_hand_tracker { XR_NULL_HANDLE };
	XrHandTrackerEXT right_hand_tracker { XR_NULL_HANDLE };
	PFN_xrCreateHandTrackerEXT create_hand_tracker { nullptr };
	PFN_xrDestroyHandTrackerEXT destroy_hand_tracker { nullptr };
	PFN_xrLocateHandJointsEXT locate_hand_joints { nullptr };
};

Application::Application()
{
	auto const *display_env { getenv("DISPLAY") };
	auto const *wayland_env { getenv("WAYLAND_DISPLAY") };
	bool const has_display
	    = (display_env && *display_env) || (wayland_env && *wayland_env);
	m_backend = has_display ? Backend::SDL : Backend::KMS;
	m_wayland = std::make_unique<Wayland::WaylandServer>(m_logger);

	init_openxr();

	auto instance_extensions { std::span<std::string const> {} };
	auto device_extensions { std::span<std::string const> {} };
	if (m_openxr) {
		instance_extensions = m_openxr->instance_extensions;
		device_extensions = m_openxr->device_extensions;
	}

	if (m_backend == Backend::SDL) {
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

		m_renderer = std::make_unique<VulkanRenderer>(
		    m_window, m_logger, instance_extensions, device_extensions);

		m_window_focused
		    = (SDL_GetWindowFlags(m_window) & SDL_WINDOW_INPUT_FOCUS) != 0;
	} else {
		m_logger.info("No display server detected; using KMS backend");
		m_renderer = std::make_unique<VulkanRenderer>(
		    VulkanRenderer::KmsSurfaceConfig {}, m_logger, instance_extensions,
		    device_extensions);
		m_window_focused = true;
	}
	m_renderer->set_antialiasing_immediate(
	    VulkanRenderer::AntiAliasingKind::MSAA_4X);

	init_openxr_session();

	m_skybox.init(*m_renderer, asset_directory() / "cubemap.png");
	init_test_meshes();

	init_input();

	init_wayland();

	if (m_backend == Backend::SDL)
		mouse_captured(true);

	m_logger.info("App init done!");

	m_cursor = PolarCoordinate::from_vec3(m_camera.target - m_camera.position);
}

Application::~Application()
{
	shutdown_openxr();
	if (m_renderer) {
		m_renderer->device().waitIdle();
		m_skybox.destroy(*m_renderer);
		for (auto const &mesh : m_test_meshes) {
			m_renderer->destroy_buffer(mesh->mesh_buffers.index_buffer);
			m_renderer->destroy_buffer(mesh->mesh_buffers.vertex_buffer);
		}
	}
	m_test_meshes.clear();

	m_renderer.reset();

	shutdown_input();

	if (m_backend == Backend::SDL) {
		SDL_DestroyWindow(m_window);
		SDL_Quit();
	}

	m_logger.info("App destroy done!");
}

auto Application::binary_directory() const -> std::filesystem::path
{
	if (m_backend != Backend::SDL) {
		return std::filesystem::current_path();
	}

	auto const *base_path { SDL_GetBasePath() };
	if (!base_path) {
		return std::filesystem::current_path();
	}
	return std::filesystem::path { base_path };
}

auto Application::asset_directory() -> std::filesystem::path
{
	std::vector<std::filesystem::path> candidates;

	auto const add_xdg_path { [&](std::filesystem::path const &base) {
		candidates.emplace_back(base / "lunar" / "assets");
	} };

	if (auto const *xdg_data_home { getenv("XDG_DATA_HOME") };
	    xdg_data_home && *xdg_data_home) {
		add_xdg_path(xdg_data_home);
	}

	if (auto const *xdg_data_dirs { getenv("XDG_DATA_DIRS") };
	    xdg_data_dirs && *xdg_data_dirs) {
		std::string_view dirs_view { xdg_data_dirs };
		size_t start { 0 };
		while (start <= dirs_view.size()) {
			size_t end { dirs_view.find(':', start) };
			if (end == std::string_view::npos) {
				end = dirs_view.size();
			}
			auto segment { dirs_view.substr(start, end - start) };
			if (!segment.empty()) {
				add_xdg_path(std::filesystem::path { segment });
			}
			start = end + 1;
		}
	} else {
		add_xdg_path("/usr/local/share");
		add_xdg_path("/usr/share");
	}

	auto const base_dir { binary_directory() };
	candidates.emplace_back(base_dir / "assets");
	candidates.emplace_back(base_dir / "../assets");

	for (auto const &candidate : candidates) {
		if (std::filesystem::exists(candidate)
		    && std::filesystem::is_directory(candidate)) {
			return candidate;
		}
	}

	m_logger.warn(
	    "Assets directory not found, using {}", (base_dir / "assets").string());
	return base_dir / "assets";
}

auto Application::init_test_meshes() -> void
{
	auto const assets_dir { asset_directory() };
	auto const mesh_path { assets_dir / "basicmesh.glb" };
	auto const meshes { Mesh::load_gltf_meshes(*m_renderer, mesh_path) };
	if (!meshes) {
		m_logger.err("Failed to load test mesh: {}", mesh_path.string());
		return;
	}

	m_test_meshes = std::move(*meshes);
}

auto Application::init_wayland() -> void
{
	auto const socket_name { m_wayland->socket_name() };
	assert(setenv("WAYLAND_DISPLAY", socket_name.data(), true) == 0);
	m_logger.info("Started Wayland display socket on {}", socket_name);
	std::println("WAYLAND_DISPLAY={}", socket_name);
}

auto Application::run() -> void

{
	SDL_Event e;
	bool const use_sdl { (m_backend == Backend::SDL) };
	bool const openxr_enabled { m_openxr != nullptr };
	bool const use_imgui { use_sdl && !openxr_enabled };

	if (use_imgui) {
		ImGuiIO &io = ImGui::GetIO();
		io.IniFilename = nullptr;
	}

	uint64_t last { 0 };
	float fps { 0.0f };
	while (m_running) {
		GZoneScopedN("Frame");
		if (m_wayland) {
			m_wayland->dispatch();
			m_wayland->flush();
		}
		if (m_openxr) {
			poll_openxr_events();
		}
		uint64_t now { 0 };
		if (use_sdl) {
			now = SDL_GetTicks();
		} else {
			auto const now_tp { std::chrono::steady_clock::now() };
			now = static_cast<uint64_t>(
			    std::chrono::duration_cast<std::chrono::milliseconds>(
			        now_tp.time_since_epoch())
			        .count());
		}
		uint64_t dt { now - last };
		float dt_seconds { static_cast<float>(dt) / 1000.0f };
		last = now;

		if (dt > 0)
			fps = 1000.0f / (float)dt;

		bool const xr_active = m_openxr != nullptr;

		{
			GZoneScopedN("Input");
			m_key_state_previous = m_key_state;
			process_libinput_events();

			if (use_sdl) {
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
					} else if (e.type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
						m_window_focused = true;
						forward_to_imgui = true;
					} else if (e.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
						m_window_focused = false;
						m_ctrl_pressed_count = 0;
						m_key_state.fill(false);
						m_key_state_previous.fill(false);
						m_mouse_dx = 0.0;

						m_mouse_dy = 0.0;
						forward_to_imgui = true;
					} else if (e.type == SDL_EVENT_MOUSE_MOTION) {
						m_mouse_x = e.motion.x;
						m_mouse_y = e.motion.y;
						m_mouse_dx = e.motion.xrel;
						m_mouse_dy = e.motion.yrel;
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

					if (forward_to_imgui && use_imgui)
						ImGui_ImplSDL3_ProcessEvent(&e);
				}
			}
		}

		bool const ctrl_down { is_key_down(KEY_LEFTCTRL)
			|| is_key_down(KEY_RIGHTCTRL) };
		{
			bool const shift_down { is_key_down(KEY_LEFTSHIFT)
				|| is_key_down(KEY_RIGHTSHIFT) };
			if (ctrl_down && shift_down && is_key_pressed(KEY_Q))
				m_running = false;
		}

		if (!xr_active) {
			GZoneScopedN("CameraUpdate");

			auto const target_offset { m_camera.target - m_camera.position };
			auto const target_distance { target_offset.magnitude() };
			auto const target_polar { PolarCoordinate::from_vec3(
				target_offset) };
			if (target_distance > 0.0f) {
				m_cursor.r = target_distance;
				m_cursor.theta = target_polar.theta;
				m_cursor.phi = target_polar.phi;
			}

			bool rotated_this_frame { false };
			if (mouse_captured()) {
				constexpr float phi_epsilon { smath::deg(5.0f) };

				m_cursor.theta
				    += static_cast<float>(m_mouse_dx) * m_mouse_sensitivity;
				m_cursor.phi
				    += static_cast<float>(m_mouse_dy) * m_mouse_sensitivity;
				m_cursor.phi = std::clamp(m_cursor.phi, phi_epsilon,
				    std::numbers::pi_v<float> - phi_epsilon);
				rotated_this_frame = (m_mouse_dx != 0.0 || m_mouse_dy != 0.0);
			}

			auto look_dir { m_cursor.to_vec3().normalized_safe() };
			if (!rotated_this_frame && target_distance > 0.0f) {
				look_dir = target_offset.normalized_safe();
			}
			if (look_dir.magnitude() == 0.0f)
				look_dir = smath::Vec3 { 0.0f, 0.0f, -1.0f };
			smath::Vec3 const world_up { 0.0f, 1.0f, 0.0f };
			auto right { look_dir.cross(world_up).normalized_safe() };
			if (right.magnitude() == 0.0f)
				right = smath::Vec3 { 1.0f, 0.0f, 0.0f };
			auto camera_up { right.cross(look_dir).normalized_safe() };
			if (camera_up.magnitude() == 0.0f)
				camera_up = world_up;

			auto forward_dir { smath::Vec3 {
				look_dir.x(), 0.0f, look_dir.z() } };
			if (forward_dir.magnitude() > 0.0f)
				forward_dir = forward_dir.normalized_safe();
			smath::Vec3 move_dir {};

			if (!ctrl_down) {
				if (is_key_down(KEY_W))
					move_dir += forward_dir;
				if (is_key_down(KEY_S))
					move_dir -= forward_dir;
				if (is_key_down(KEY_D))
					move_dir += right;
				if (is_key_down(KEY_A))
					move_dir -= right;
				if (is_key_down(KEY_SPACE))
					move_dir += world_up;
				if (is_key_down(KEY_LEFTSHIFT))
					move_dir -= world_up;
			}

			if (move_dir.magnitude() > 0.0f) {
				constexpr float move_speed { 10.0f };
				move_dir = move_dir.normalized_safe();
				m_camera.position += move_dir * (move_speed * dt_seconds);
			}

			if (!m_show_imgui) {
				m_camera.up = camera_up;
				auto const distance { target_distance > 0.0f
					    ? target_distance
					    : std::max(1.0f, m_cursor.r) };
				m_camera.target = m_camera.position + look_dir * distance;
			}

			m_mouse_dx = 0.0;
			m_mouse_dy = 0.0;
		}

		if (use_imgui) {
			GZoneScopedN("ImGui");
			ImGui_ImplSDL3_NewFrame();
			ImGui_ImplVulkan_NewFrame();

			ImGui::NewFrame();

			ImGui::SetNextWindowSize({ 300, 100 });
			ImGui::SetNextWindowPos({ 0, 0 });
			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4 { 0, 0, 0, 0.5f });
			bool debug_open { ImGui::Begin("Debug Info", nullptr,
				ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize) };
			if (debug_open) {
				ImGui::Text("%s", std::format("FPS: {:.2f}", fps).c_str());
				ImGui::Text("%s",
				    std::format("Cam pos: ({:.2f}, {:.2f}, {:.2f})",
				        m_camera.position.x(), m_camera.position.y(),
				        m_camera.position.z())
				        .c_str());
				ImGui::Text("%s",
				    std::format("Cam tgt: ({:.2f}, {:.2f}, {:.2f})",
				        m_camera.target.x(), m_camera.target.y(),
				        m_camera.target.z())
				        .c_str());
				ImGui::Text("%s",
				    std::format("Cam up:  ({:.2f}, {:.2f}, {:.2f})",
				        m_camera.up.x(), m_camera.up.y(), m_camera.up.z())
				        .c_str());
				ImGui::Text("%s",
				    std::format("Cursor r/theta/phi: {:.2f}, {:.2f}, {:.2f}",
				        m_cursor.r, m_cursor.theta, m_cursor.phi)
				        .c_str());
			}
			ImGui::End();
			ImGui::PopStyleColor();

			if (m_show_imgui) {
				ImGui::SetNextWindowCollapsed(true, ImGuiCond_Once);
				ImGui::ShowDemoWindow();

				ImGui::SetNextWindowSize({ 300, -1 }, ImGuiCond_Once);
				bool fun_menu_open { ImGui::Begin("Fun menu") };
				if (fun_menu_open) {
					static std::array<char const *, 4> const aa_items {
						"None",
						"MSAA 2X",
						"MSAA 4X",
						"MSAA 8X",
					};
					int selected_item {
						static_cast<int>(m_renderer->antialiasing()),
					};
					if (ImGui::Combo("Antialiasing", &selected_item,
					        aa_items.data(), aa_items.size())) {
						m_renderer->set_antialiasing(
						    static_cast<VulkanRenderer::AntiAliasingKind>(
						        selected_item));
					}

					if (ImGui::CollapsingHeader("Camera",
					        ImGuiTreeNodeFlags_Framed
					            | ImGuiTreeNodeFlags_SpanAvailWidth
					            | ImGuiTreeNodeFlags_DefaultOpen)) {
						auto const camera_offset { m_camera.target
							- m_camera.position };
						auto const camera_distance {
							camera_offset.magnitude()
						};
						auto const camera_direction {
							camera_offset.normalized_safe()
						};

						ImGui::SliderFloat("Mouse sensitivity",
						    &m_mouse_sensitivity, 0.0001f, 0.01f, "%.4f");
						constexpr float position_step { 0.05f };
						constexpr float target_step { 0.05f };
						bool position_changed { ImGui::DragFloat3(
							"Pos", m_camera.position.data(), position_step) };
						bool target_changed { ImGui::DragFloat3(
							"Target", m_camera.target.data(), target_step) };
						ImGui::DragFloat3("Up", m_camera.up.data());

						if (position_changed && !target_changed) {
							auto offset { m_cursor.to_vec3() };
							auto const preserve_distance {
								camera_distance > 0.0f ? camera_distance
								                       : offset.magnitude()
							};

							if (offset.magnitude() == 0.0f
							    && camera_direction.magnitude() > 0.0f) {
								offset = camera_direction * preserve_distance;
							}

							if (offset.magnitude() > 0.0f) {
								m_camera.target = m_camera.position + offset;
							}
						}

						if (target_changed && !position_changed) {
							auto const new_offset { m_camera.target
								- m_camera.position };
							auto new_direction { new_offset.normalized_safe() };
							auto const preserve_distance {
								camera_distance > 0.0f ? camera_distance
								                       : new_offset.magnitude()
							};

							if (new_direction.magnitude() == 0.0f) {
								new_direction = camera_direction;
							}

							if (new_direction.magnitude() > 0.0f
							    && preserve_distance > 0.0f) {
								m_camera.position = m_camera.target
								    - new_direction * preserve_distance;
							}
						}
					}
				}
				ImGui::End();
			}

			ImGui::Render();
		}

		auto record_scene { [&](VulkanRenderer::GL &gl) {
			GZoneScopedN("Render");
			auto view { smath::matrix_look_at(
				m_camera.position, m_camera.target, m_camera.up) };
			auto const draw_extent { m_renderer->draw_extent() };
			auto const aspect { draw_extent.height == 0
				    ? 1.0f
				    : static_cast<float>(draw_extent.width)
				        / static_cast<float>(draw_extent.height) };
			auto projection { smath::matrix_perspective(
				m_camera.fovy, aspect, 0.1f, 10000.0f) };
			projection[1][1] *= -1;
			auto view_projection { projection * view };

			auto skybox_view { view };
			skybox_view[3][0] = 0.0f;
			skybox_view[3][1] = 0.0f;
			skybox_view[3][2] = 0.0f;
			m_skybox.draw(gl, *m_renderer, projection * skybox_view);

			gl.set_transform(view_projection);
			gl.set_texture();
			auto const &meshes { m_test_meshes };
			if (meshes.size() > 2 && !meshes[2]->surfaces.empty()) {
				auto const &surface { meshes[2]->surfaces[0] };
				gl.draw_mesh(meshes[2]->mesh_buffers,
				    view_projection
				        * smath::translate(smath::Vec3 { 0.0f, 0.0f, -5.0f }),
				    surface.count, surface.start_index);
			}

			gl.push_transform();
			gl.set_transform(view_projection
			    * smath::translate(smath::Vec3 { 0.0f, 0.0f, 5.0f })
			    * smath::Quaternion<float>::from_axis_angle(
			        smath::Vec3 { 0.0f, 1.0f, 0.0f }, smath::deg(180))
			        .as_matrix());

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

			gl.set_transform(view_projection);
			gl.draw_sphere(m_camera.target, 0.01f);

			if (m_openxr && m_openxr->hand_tracking_supported) {
				render_hands(gl, view_projection);
			}

			if (m_wayland) {
				auto const wayland_draw_extent { m_renderer->draw_extent() };
				auto const draw_width { static_cast<float>(
					wayland_draw_extent.width) };
				auto const draw_height { static_cast<float>(
					wayland_draw_extent.height) };
				if (draw_width > 0.0f && draw_height > 0.0f) {
					gl.set_transform(smath::Mat4::identity());
					gl.set_culling(false);
					gl.use_pipeline(m_renderer->wayland_pipeline());
					for (auto *surface : m_wayland->surfaces()) {
						auto buffer { surface->current_buffer() };
						if (!buffer) {
							continue;
						}
						auto texture { make_shm_texture(*buffer) };
						if (!texture) {
							continue;
						}
						auto const width { static_cast<float>(buffer->width) };
						auto const height { static_cast<float>(
							buffer->height) };
						auto const size { smath::Vec2 {
							(width / draw_width) * 2.0f,
							(height / draw_height) * 2.0f,
						} };
						auto const pos { smath::Vec2 {
							-1.0f, 1.0f - size.y() } };
						auto image { m_renderer->create_image(
							*texture, vk::ImageUsageFlagBits::eSampled) };
						gl.set_texture(&image);
						gl.draw_rectangle(pos, size);
						gl.flush();
						gl.set_texture(std::nullopt);
						m_renderer->destroy_image_later(image);
					}
					gl.use_pipeline(m_renderer->mesh_pipeline());
					gl.set_culling(true);
				}
			}
		} };

		if (xr_active) {
			if (m_openxr && m_openxr->session_running) {
				render_openxr_frame(record_scene, dt_seconds);
			}
		} else {
			m_renderer->render(record_scene);
		}
#if defined(TRACY_ENABLE)
		FrameMark;
#endif
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

	if (m_backend == Backend::SDL) {
		int width {}, height {};
		SDL_GetWindowSize(m_window, &width, &height);
		float mouse_x {}, mouse_y {};
		SDL_GetMouseState(&mouse_x, &mouse_y);
		m_mouse_x = mouse_x;
		m_mouse_y = mouse_y;
		ImGui::GetIO().AddMousePosEvent(
		    static_cast<float>(m_mouse_x), static_cast<float>(m_mouse_y));
	} else {
		m_mouse_x = 0.0;
		m_mouse_y = 0.0;
	}
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

auto Application::init_openxr() -> void
{
	if (auto const *no_xr { getenv("LUNAR_NO_XR") }; no_xr && *no_xr) {
		return;
	}
	m_openxr = std::make_unique<OpenXrState>();

	XrInstanceCreateInfo create_info {};
	create_info.type = XR_TYPE_INSTANCE_CREATE_INFO;
	create_info.next = nullptr;
	std::strncpy(create_info.applicationInfo.applicationName, "Lunar",
	    XR_MAX_APPLICATION_NAME_SIZE - 1);
	std::strncpy(create_info.applicationInfo.engineName, "Lunar",
	    XR_MAX_ENGINE_NAME_SIZE - 1);
	create_info.applicationInfo.applicationVersion = 1;
	create_info.applicationInfo.engineVersion = 1;
	create_info.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

	uint32_t extension_count = 0;
	auto extension_count_result {
		xrEnumerateInstanceExtensionProperties(
		    nullptr, 0, &extension_count, nullptr),
	};
	if (XR_FAILED(extension_count_result) || extension_count == 0) {
		m_logger.warn("OpenXR instance extensions unavailable: {}",
		    xr_result_to_string(XR_NULL_HANDLE, extension_count_result));
		m_openxr.reset();
		return;
	}

	std::vector<XrExtensionProperties> available_extensions(extension_count);
	for (auto &extension : available_extensions) {
		extension = XrExtensionProperties {};
		extension.type = XR_TYPE_EXTENSION_PROPERTIES;
		extension.next = nullptr;
	}
	auto enumerate_result {
		xrEnumerateInstanceExtensionProperties(nullptr, extension_count,
		    &extension_count, available_extensions.data()),
	};
	if (XR_FAILED(enumerate_result)) {
		m_logger.warn("Failed to enumerate OpenXR extensions: {}",
		    xr_result_to_string(XR_NULL_HANDLE, enumerate_result));
		m_openxr.reset();
		return;
	}

	auto const has_extension { [&](char const *name) {
		return std::any_of(available_extensions.begin(),
		    available_extensions.end(), [&](auto const &extension) {
			    return std::strcmp(extension.extensionName, name) == 0;
		    });
	} };

	bool const has_enable2 {
		has_extension(XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME),
	};
	bool const has_enable1 = has_extension(XR_KHR_VULKAN_ENABLE_EXTENSION_NAME);
	if (!has_enable2 && !has_enable1) {
		m_logger.warn("OpenXR Vulkan extensions missing");
		m_openxr.reset();
		return;
	}

	std::vector<char const *> extensions {};
	if (has_enable1) {
		extensions.push_back(XR_KHR_VULKAN_ENABLE_EXTENSION_NAME);
		m_openxr->use_vulkan_enable2 = false;
	} else if (has_enable2) {
		extensions.push_back(XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME);
		m_openxr->use_vulkan_enable2 = true;
	}

	bool const has_hand_tracking {
		has_extension(XR_EXT_HAND_TRACKING_EXTENSION_NAME),
	};
	if (has_hand_tracking) {
		extensions.push_back(XR_EXT_HAND_TRACKING_EXTENSION_NAME);
		m_openxr->hand_tracking_supported = true;
	}

	create_info.enabledExtensionCount
	    = static_cast<uint32_t>(extensions.size());
	create_info.enabledExtensionNames = extensions.data();

	auto const instance_result {
		xrCreateInstance(&create_info, &m_openxr->instance),
	};
	if (XR_FAILED(instance_result)) {
		m_logger.info("OpenXR not available (no instance)");
		m_openxr.reset();
		return;
	}

	XrSystemGetInfo system_info {};
	system_info.type = XR_TYPE_SYSTEM_GET_INFO;
	system_info.next = nullptr;
	system_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
	auto const system_result {
		xrGetSystem(m_openxr->instance, &system_info, &m_openxr->system_id),
	};
	if (XR_FAILED(system_result)) {
		m_logger.info("OpenXR system not detected");
		xrDestroyInstance(m_openxr->instance);
		m_openxr.reset();
		return;
	}

	PFN_xrGetVulkanInstanceExtensionsKHR get_instance_exts {};
	auto instance_ext_result {
		xrGetInstanceProcAddr(m_openxr->instance,
		    "xrGetVulkanInstanceExtensionsKHR",
		    reinterpret_cast<PFN_xrVoidFunction *>(&get_instance_exts)),
	};
	if (XR_FAILED(instance_ext_result) || !get_instance_exts) {
		m_logger.warn("OpenXR missing Vulkan instance extensions (proc)");
		xrDestroyInstance(m_openxr->instance);
		m_openxr.reset();
		return;
	}

	PFN_xrGetVulkanDeviceExtensionsKHR get_device_exts {};
	auto device_ext_result {
		xrGetInstanceProcAddr(m_openxr->instance,
		    "xrGetVulkanDeviceExtensionsKHR",
		    reinterpret_cast<PFN_xrVoidFunction *>(&get_device_exts)),
	};
	if (XR_FAILED(device_ext_result) || !get_device_exts) {
		m_logger.warn("OpenXR missing Vulkan device extensions");
		xrDestroyInstance(m_openxr->instance);
		m_openxr.reset();
		return;
	}

	uint32_t instance_ext_size = 0;
	instance_ext_result = get_instance_exts(m_openxr->instance,
	    m_openxr->system_id, 0, &instance_ext_size, nullptr);
	if (XR_FAILED(instance_ext_result) || instance_ext_size == 0) {
		m_logger.warn("Failed to query OpenXR instance extensions");
		xrDestroyInstance(m_openxr->instance);
		m_openxr.reset();
		return;
	}
	std::string instance_ext_string(instance_ext_size, '\0');
	instance_ext_result
	    = get_instance_exts(m_openxr->instance, m_openxr->system_id,
	        instance_ext_size, &instance_ext_size, instance_ext_string.data());
	if (XR_FAILED(instance_ext_result)) {
		m_logger.warn("Failed to read OpenXR instance extensions");
		xrDestroyInstance(m_openxr->instance);
		m_openxr.reset();
		return;
	}
	m_openxr->instance_extensions = split_extension_list(
	    std::string_view { instance_ext_string.c_str() });

	uint32_t device_ext_size { 0 };
	device_ext_result = get_device_exts(
	    m_openxr->instance, m_openxr->system_id, 0, &device_ext_size, nullptr);
	if (XR_FAILED(device_ext_result) || device_ext_size == 0) {
		m_logger.warn("Failed to query OpenXR device extensions");
		xrDestroyInstance(m_openxr->instance);
		m_openxr.reset();
		return;
	}
	std::string device_ext_string(device_ext_size, '\0');
	device_ext_result = get_device_exts(m_openxr->instance, m_openxr->system_id,
	    device_ext_size, &device_ext_size, device_ext_string.data());
	if (XR_FAILED(device_ext_result)) {
		m_logger.warn("Failed to read OpenXR device extensions");
		xrDestroyInstance(m_openxr->instance);
		m_openxr.reset();
		return;
	}
	m_openxr->device_extensions
	    = split_extension_list(std::string_view { device_ext_string.c_str() });
	m_openxr->enabled = true;

	if (m_openxr->use_vulkan_enable2) {
		auto const graphics_device_result {
			xrGetInstanceProcAddr(m_openxr->instance,
			    "xrGetVulkanGraphicsDevice2KHR",
			    reinterpret_cast<PFN_xrVoidFunction *>(
			        &m_openxr->get_graphics_device2)),
		};
		if (XR_FAILED(graphics_device_result)
		    || !m_openxr->get_graphics_device2) {
			m_logger.warn("OpenXR missing Vulkan graphics device hook");
			m_openxr->get_graphics_device2 = nullptr;
		}

		auto const requirements_result {
			xrGetInstanceProcAddr(m_openxr->instance,
			    "xrGetVulkanGraphicsRequirements2KHR",
			    reinterpret_cast<PFN_xrVoidFunction *>(
			        &m_openxr->get_requirements2)),
		};
		if (XR_FAILED(requirements_result) || !m_openxr->get_requirements2) {
			m_logger.warn("OpenXR missing Vulkan requirements hook");
			m_openxr->get_requirements2 = nullptr;
		}
	} else {
		auto const graphics_device_result {
			xrGetInstanceProcAddr(m_openxr->instance,
			    "xrGetVulkanGraphicsDeviceKHR",
			    reinterpret_cast<PFN_xrVoidFunction *>(
			        &m_openxr->get_graphics_device)),
		};
		if (XR_FAILED(graphics_device_result)
		    || !m_openxr->get_graphics_device) {
			m_logger.warn("OpenXR missing Vulkan graphics device hook");
			m_openxr->get_graphics_device = nullptr;
		}

		auto const requirements_result {
			xrGetInstanceProcAddr(m_openxr->instance,
			    "xrGetVulkanGraphicsRequirementsKHR",
			    reinterpret_cast<PFN_xrVoidFunction *>(
			        &m_openxr->get_requirements)),
		};
		if (XR_FAILED(requirements_result) || !m_openxr->get_requirements) {
			m_logger.warn("OpenXR missing Vulkan requirements hook");
			m_openxr->get_requirements = nullptr;
		}
	}

	if (m_openxr->hand_tracking_supported) {
		auto const create_result {
			xrGetInstanceProcAddr(m_openxr->instance, "xrCreateHandTrackerEXT",
			    reinterpret_cast<PFN_xrVoidFunction *>(
			        &m_openxr->create_hand_tracker)),
		};
		if (XR_FAILED(create_result) || !m_openxr->create_hand_tracker) {
			m_logger.warn("OpenXR hand tracking create function unavailable");
			m_openxr->hand_tracking_supported = false;
		}

		auto const destroy_result {
			xrGetInstanceProcAddr(m_openxr->instance, "xrDestroyHandTrackerEXT",
			    reinterpret_cast<PFN_xrVoidFunction *>(
			        &m_openxr->destroy_hand_tracker)),
		};
		if (XR_FAILED(destroy_result) || !m_openxr->destroy_hand_tracker) {
			m_logger.warn("OpenXR hand tracking destroy function unavailable");
			m_openxr->hand_tracking_supported = false;
		}

		auto const locate_result {
			xrGetInstanceProcAddr(m_openxr->instance, "xrLocateHandJointsEXT",
			    reinterpret_cast<PFN_xrVoidFunction *>(
			        &m_openxr->locate_hand_joints)),
		};
		if (XR_FAILED(locate_result) || !m_openxr->locate_hand_joints) {
			m_logger.warn("OpenXR hand tracking locate function unavailable");
			m_openxr->hand_tracking_supported = false;
		}
	}

	m_logger.info("OpenXR system detected");
}

auto Application::init_openxr_session() -> void
{
	if (!m_openxr || !m_renderer) {
		return;
	}
	VkPhysicalDevice xr_physical_device {};
	XrGraphicsBindingVulkan2KHR binding2 {};
	XrGraphicsBindingVulkanKHR binding1 {};
	void *binding_ptr = nullptr;

	if (m_openxr->use_vulkan_enable2) {
		if (!m_openxr->get_graphics_device2 || !m_openxr->get_requirements2) {
			m_logger.warn("OpenXR graphics device hook unavailable");
			shutdown_openxr();
			return;
		}

		XrGraphicsRequirementsVulkan2KHR requirements {};
		requirements.type = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR;
		requirements.next = nullptr;
		auto const requirements_result {
			m_openxr->get_requirements2(
			    m_openxr->instance, m_openxr->system_id, &requirements),
		};
		if (XR_FAILED(requirements_result)) {
			m_logger.warn("OpenXR Vulkan requirements failed: {}",
			    xr_result_to_string(m_openxr->instance, requirements_result));
			shutdown_openxr();
			return;
		}

		XrVulkanGraphicsDeviceGetInfoKHR device_info {};
		device_info.type = XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR;
		device_info.next = nullptr;
		device_info.systemId = m_openxr->system_id;
		device_info.vulkanInstance
		    = static_cast<VkInstance>(m_renderer->instance());
		auto const phys_result {
			m_openxr->get_graphics_device2(
			    m_openxr->instance, &device_info, &xr_physical_device),
		};
		if (XR_FAILED(phys_result)) {
			m_logger.warn("Failed to fetch OpenXR Vulkan device: {}",
			    xr_result_to_string(m_openxr->instance, phys_result));
			shutdown_openxr();
			return;
		}

		binding2.type = XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR;
		binding2.next = nullptr;
		binding2.instance = static_cast<VkInstance>(m_renderer->instance());
		binding2.physicalDevice
		    = static_cast<VkPhysicalDevice>(m_renderer->physical_device());
		binding2.device = static_cast<VkDevice>(m_renderer->device());
		binding2.queueFamilyIndex = m_renderer->graphics_queue_family();
		binding2.queueIndex = 0;
		binding_ptr = &binding2;
	} else {
		if (!m_openxr->get_graphics_device || !m_openxr->get_requirements) {
			m_logger.warn("OpenXR graphics device hook unavailable");
			shutdown_openxr();
			return;
		}

		XrGraphicsRequirementsVulkanKHR requirements {};
		requirements.type = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR;
		requirements.next = nullptr;
		auto const requirements_result {
			m_openxr->get_requirements(
			    m_openxr->instance, m_openxr->system_id, &requirements),
		};
		if (XR_FAILED(requirements_result)) {
			m_logger.warn("OpenXR Vulkan requirements failed: {}",
			    xr_result_to_string(m_openxr->instance, requirements_result));
			shutdown_openxr();
			return;
		}

		auto const phys_result {
			m_openxr->get_graphics_device(m_openxr->instance,
			    m_openxr->system_id,
			    static_cast<VkInstance>(m_renderer->instance()),
			    &xr_physical_device),
		};
		if (XR_FAILED(phys_result)) {
			m_logger.warn("Failed to fetch OpenXR Vulkan device: {}",
			    xr_result_to_string(m_openxr->instance, phys_result));
			shutdown_openxr();
			return;
		}

		binding1.type = XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR;
		binding1.next = nullptr;
		binding1.instance = static_cast<VkInstance>(m_renderer->instance());
		binding1.physicalDevice
		    = static_cast<VkPhysicalDevice>(m_renderer->physical_device());
		binding1.device = static_cast<VkDevice>(m_renderer->device());
		binding1.queueFamilyIndex = m_renderer->graphics_queue_family();
		binding1.queueIndex = 0;
		binding_ptr = &binding1;
	}

	if (xr_physical_device
	    != static_cast<VkPhysicalDevice>(m_renderer->physical_device())) {
		m_logger.warn("OpenXR device differs from selected Vulkan device");
	}

	XrSessionCreateInfo session_info {};
	session_info.type = XR_TYPE_SESSION_CREATE_INFO;
	session_info.next = binding_ptr;
	session_info.systemId = m_openxr->system_id;
	auto const session_result {
		xrCreateSession(m_openxr->instance, &session_info, &m_openxr->session),
	};
	if (XR_FAILED(session_result)) {
		m_logger.warn("Failed to create OpenXR session: {}",
		    xr_result_to_string(m_openxr->instance, session_result));
		shutdown_openxr();
		return;
	}

	XrReferenceSpaceCreateInfo space_info {};
	space_info.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
	space_info.next = nullptr;
	space_info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
	space_info.poseInReferenceSpace.orientation.w = 1.0f;
	auto const space_result {
		xrCreateReferenceSpace(
		    m_openxr->session, &space_info, &m_openxr->app_space),
	};
	if (XR_FAILED(space_result)) {
		m_logger.warn("Failed to create OpenXR space: {}",
		    xr_result_to_string(m_openxr->instance, space_result));
		shutdown_openxr();
		return;
	}

	uint32_t view_count = 0;
	auto const view_count_result {
		xrEnumerateViewConfigurationViews(m_openxr->instance,
		    m_openxr->system_id, m_openxr->view_type, 0, &view_count, nullptr),
	};
	if (XR_FAILED(view_count_result) || view_count == 0) {
		m_logger.warn("OpenXR view configuration missing: {}",
		    xr_result_to_string(m_openxr->instance, view_count_result));
		shutdown_openxr();
		return;
	}
	m_openxr->view_configs.resize(view_count);
	for (auto &config : m_openxr->view_configs) {
		config = XrViewConfigurationView {};
		config.type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
		config.next = nullptr;
	}
	auto const view_result {
		xrEnumerateViewConfigurationViews(m_openxr->instance,
		    m_openxr->system_id, m_openxr->view_type, view_count, &view_count,
		    m_openxr->view_configs.data()),
	};
	if (XR_FAILED(view_result)) {
		m_logger.warn("Failed to enumerate OpenXR views: {}",
		    xr_result_to_string(m_openxr->instance, view_result));
		shutdown_openxr();
		return;
	}
	m_openxr->views.resize(view_count);
	for (auto &view : m_openxr->views) {
		view = XrView {};
		view.type = XR_TYPE_VIEW;
		view.next = nullptr;
	}

	uint32_t format_count = 0;
	auto const format_count_result {
		xrEnumerateSwapchainFormats(
		    m_openxr->session, 0, &format_count, nullptr),
	};
	if (XR_FAILED(format_count_result) || format_count == 0) {
		m_logger.warn("OpenXR swapchain formats unavailable: {}",
		    xr_result_to_string(m_openxr->instance, format_count_result));
		shutdown_openxr();
		return;
	}
	std::vector<int64_t> formats(format_count);
	auto const format_result {
		xrEnumerateSwapchainFormats(
		    m_openxr->session, format_count, &format_count, formats.data()),
	};
	if (XR_FAILED(format_result)) {
		m_logger.warn("Failed to enumerate OpenXR swapchain formats: {}",
		    xr_result_to_string(m_openxr->instance, format_result));
		shutdown_openxr();
		return;
	}

	std::array<int64_t, 4> const preferred_formats {
		static_cast<int64_t>(VK_FORMAT_B8G8R8A8_UNORM),
		static_cast<int64_t>(VK_FORMAT_B8G8R8A8_SRGB),
		static_cast<int64_t>(VK_FORMAT_R8G8B8A8_UNORM),
		static_cast<int64_t>(VK_FORMAT_R8G8B8A8_SRGB),
	};
	m_openxr->color_format = formats.front();
	for (auto const preferred : preferred_formats) {
		auto const found { std::find(
			formats.begin(), formats.end(), preferred) };
		if (found != formats.end()) {
			m_openxr->color_format = *found;
			break;
		}
	}

	m_openxr->swapchains.clear();
	m_openxr->swapchains.resize(view_count);
	for (uint32_t i = 0; i < view_count; ++i) {
		auto const &view_config { m_openxr->view_configs[i] };
		XrSwapchainCreateInfo swapchain_info {};
		swapchain_info.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
		swapchain_info.next = nullptr;
		swapchain_info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT
		    | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
		swapchain_info.format = m_openxr->color_format;
		swapchain_info.sampleCount
		    = view_config.recommendedSwapchainSampleCount;
		swapchain_info.width = view_config.recommendedImageRectWidth;
		swapchain_info.height = view_config.recommendedImageRectHeight;
		swapchain_info.faceCount = 1;
		swapchain_info.arraySize = 1;
		swapchain_info.mipCount = 1;

		auto const swapchain_result {
			xrCreateSwapchain(m_openxr->session, &swapchain_info,
			    &m_openxr->swapchains[i].handle),
		};
		if (XR_FAILED(swapchain_result)) {
			m_logger.warn("Failed to create OpenXR swapchain: {}",
			    xr_result_to_string(m_openxr->instance, swapchain_result));
			shutdown_openxr();
			return;
		}

		m_openxr->swapchains[i].extent
		    = vk::Extent2D { swapchain_info.width, swapchain_info.height };

		uint32_t image_count { 0 };
		auto const image_count_result {
			xrEnumerateSwapchainImages(
			    m_openxr->swapchains[i].handle, 0, &image_count, nullptr),
		};
		if (XR_FAILED(image_count_result) || image_count == 0) {
			m_logger.warn("Failed to enumerate OpenXR swapchain images: {}",
			    xr_result_to_string(m_openxr->instance, image_count_result));
			shutdown_openxr();
			return;
		}
		m_openxr->swapchains[i].images.resize(image_count);
		for (auto &image : m_openxr->swapchains[i].images) {
			image = XrSwapchainImageVulkanKHR {};
			image.type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
			image.next = nullptr;
		}
		auto const image_result {
			xrEnumerateSwapchainImages(m_openxr->swapchains[i].handle,
			    image_count, &image_count,
			    reinterpret_cast<XrSwapchainImageBaseHeader *>(
			        m_openxr->swapchains[i].images.data())),
		};
		if (XR_FAILED(image_result)) {
			m_logger.warn("Failed to read OpenXR swapchain images: {}",
			    xr_result_to_string(m_openxr->instance, image_result));
			shutdown_openxr();
			return;
		}
	}

	if (!m_openxr->swapchains.empty()) {
		m_renderer->set_offscreen_extent(m_openxr->swapchains.front().extent);
	}

	if (m_openxr->hand_tracking_supported && m_openxr->create_hand_tracker) {
		XrHandTrackerCreateInfoEXT left_info {};
		left_info.type = XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT;
		left_info.next = nullptr;
		left_info.hand = XR_HAND_LEFT_EXT;
		left_info.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;
		auto const left_result { m_openxr->create_hand_tracker(
			m_openxr->session, &left_info, &m_openxr->left_hand_tracker) };
		if (XR_FAILED(left_result)) {
			m_logger.warn("Failed to create left hand tracker");
			m_openxr->left_hand_tracker = XR_NULL_HANDLE;
		}

		XrHandTrackerCreateInfoEXT right_info {};
		right_info.type = XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT;
		right_info.next = nullptr;
		right_info.hand = XR_HAND_RIGHT_EXT;
		right_info.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;
		auto const right_result { m_openxr->create_hand_tracker(
			m_openxr->session, &right_info, &m_openxr->right_hand_tracker) };
		if (XR_FAILED(right_result)) {
			m_logger.warn("Failed to create right hand tracker");
			m_openxr->right_hand_tracker = XR_NULL_HANDLE;
		}
	}

	m_logger.info("OpenXR session initialized");
}

auto Application::shutdown_openxr() -> void
{
	if (!m_openxr) {
		return;
	}

	for (auto &swapchain : m_openxr->swapchains) {
		if (swapchain.handle != XR_NULL_HANDLE) {
			xrDestroySwapchain(swapchain.handle);
			swapchain.handle = XR_NULL_HANDLE;
		}
	}
	m_openxr->swapchains.clear();

	if (m_openxr->left_hand_tracker != XR_NULL_HANDLE
	    && m_openxr->destroy_hand_tracker) {
		m_openxr->destroy_hand_tracker(m_openxr->left_hand_tracker);
		m_openxr->left_hand_tracker = XR_NULL_HANDLE;
	}

	if (m_openxr->right_hand_tracker != XR_NULL_HANDLE
	    && m_openxr->destroy_hand_tracker) {
		m_openxr->destroy_hand_tracker(m_openxr->right_hand_tracker);
		m_openxr->right_hand_tracker = XR_NULL_HANDLE;
	}

	if (m_openxr->app_space != XR_NULL_HANDLE) {
		xrDestroySpace(m_openxr->app_space);
		m_openxr->app_space = XR_NULL_HANDLE;
	}

	if (m_openxr->session != XR_NULL_HANDLE) {
		xrDestroySession(m_openxr->session);
		m_openxr->session = XR_NULL_HANDLE;
	}

	if (m_openxr->instance != XR_NULL_HANDLE) {
		xrDestroyInstance(m_openxr->instance);
		m_openxr->instance = XR_NULL_HANDLE;
	}

	m_openxr.reset();
}

auto Application::poll_openxr_events() -> void
{
	if (!m_openxr) {
		return;
	}

	XrEventDataBuffer event {};
	event.type = XR_TYPE_EVENT_DATA_BUFFER;
	event.next = nullptr;
	while (xrPollEvent(m_openxr->instance, &event) == XR_SUCCESS) {
		switch (event.type) {
		case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
			auto const &state_event {
				*reinterpret_cast<XrEventDataSessionStateChanged const *>(
				    &event)
			};
			m_openxr->session_state = state_event.state;
			if (state_event.state == XR_SESSION_STATE_READY
			    && !m_openxr->session_running) {
				XrSessionBeginInfo begin_info {};
				begin_info.type = XR_TYPE_SESSION_BEGIN_INFO;
				begin_info.next = nullptr;
				begin_info.primaryViewConfigurationType = m_openxr->view_type;
				if (XR_SUCCEEDED(
				        xrBeginSession(m_openxr->session, &begin_info))) {
					m_openxr->session_running = true;
				}
			} else if (state_event.state == XR_SESSION_STATE_STOPPING
			    && m_openxr->session_running) {
				xrEndSession(m_openxr->session);
				m_openxr->session_running = false;
			} else if (state_event.state == XR_SESSION_STATE_EXITING
			    || state_event.state == XR_SESSION_STATE_LOSS_PENDING) {
				m_running = false;
			}
			break;
		}
		default:
			break;
		}
		event = XrEventDataBuffer {};
		event.type = XR_TYPE_EVENT_DATA_BUFFER;
		event.next = nullptr;
	}
}

auto Application::render_openxr_frame(
    std::function<void(VulkanRenderer::GL &)> const &record,
    float /*dt_seconds*/) -> bool
{
	if (!m_openxr || !m_openxr->session_running) {
		return false;
	}

	XrFrameWaitInfo wait_info {};
	wait_info.type = XR_TYPE_FRAME_WAIT_INFO;
	wait_info.next = nullptr;
	XrFrameState frame_state {};
	frame_state.type = XR_TYPE_FRAME_STATE;
	frame_state.next = nullptr;
	if (XR_FAILED(xrWaitFrame(m_openxr->session, &wait_info, &frame_state))) {
		return false;
	}

	XrFrameBeginInfo begin_info {};
	begin_info.type = XR_TYPE_FRAME_BEGIN_INFO;
	begin_info.next = nullptr;
	if (XR_FAILED(xrBeginFrame(m_openxr->session, &begin_info))) {
		return false;
	}

	if (!frame_state.shouldRender) {
		XrFrameEndInfo end_info {};
		end_info.type = XR_TYPE_FRAME_END_INFO;
		end_info.next = nullptr;
		end_info.displayTime = frame_state.predictedDisplayTime;
		end_info.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
		end_info.layerCount = 0;
		end_info.layers = nullptr;
		xrEndFrame(m_openxr->session, &end_info);
		return true;
	}

	XrViewLocateInfo locate_info {};
	locate_info.type = XR_TYPE_VIEW_LOCATE_INFO;
	locate_info.next = nullptr;
	locate_info.viewConfigurationType = m_openxr->view_type;
	locate_info.displayTime = frame_state.predictedDisplayTime;
	locate_info.space = m_openxr->app_space;

	XrViewState view_state {};
	view_state.type = XR_TYPE_VIEW_STATE;
	view_state.next = nullptr;
	auto view_count { static_cast<uint32_t>(m_openxr->views.size()) };
	view_count = std::min(
	    view_count, static_cast<uint32_t>(m_openxr->swapchains.size()));
	if (XR_FAILED(xrLocateViews(m_openxr->session, &locate_info, &view_state,
	        view_count, &view_count, m_openxr->views.data()))) {
		return false;
	}

	update_hands(frame_state.predictedDisplayTime);

	std::vector<XrCompositionLayerProjectionView> projection_views(view_count);
	for (auto &projection_view : projection_views) {
		projection_view = XrCompositionLayerProjectionView {};
		projection_view.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
		projection_view.next = nullptr;
	}
	for (uint32_t i = 0; i < view_count; ++i) {
		auto const &swapchain { m_openxr->swapchains[i] };
		uint32_t image_index { 0 };
		XrSwapchainImageAcquireInfo acquire_info {};
		acquire_info.type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO;
		acquire_info.next = nullptr;
		if (XR_FAILED(xrAcquireSwapchainImage(
		        swapchain.handle, &acquire_info, &image_index))) {
			continue;
		}

		XrSwapchainImageWaitInfo swapchain_wait_info {};
		swapchain_wait_info.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
		swapchain_wait_info.next = nullptr;
		swapchain_wait_info.timeout = XR_INFINITE_DURATION;
		if (XR_FAILED(
		        xrWaitSwapchainImage(swapchain.handle, &swapchain_wait_info))) {
			continue;
		}

		update_camera_from_xr_view(m_openxr->views[i]);
		m_renderer->render_to_image(
		    vk::Image { swapchain.images[image_index].image }, swapchain.extent,
		    record);

		XrSwapchainImageReleaseInfo release_info {};
		release_info.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO;
		release_info.next = nullptr;
		xrReleaseSwapchainImage(swapchain.handle, &release_info);

		projection_views[i].pose = m_openxr->views[i].pose;
		projection_views[i].fov = m_openxr->views[i].fov;
		projection_views[i].subImage.swapchain = swapchain.handle;
		projection_views[i].subImage.imageRect.offset = { 0, 0 };
		projection_views[i].subImage.imageRect.extent
		    = { static_cast<int32_t>(swapchain.extent.width),
			      static_cast<int32_t>(swapchain.extent.height) };
		projection_views[i].subImage.imageArrayIndex = 0;
	}

	XrCompositionLayerProjection layer {};
	layer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
	layer.next = nullptr;
	layer.space = m_openxr->app_space;
	layer.viewCount = view_count;
	layer.views = projection_views.data();

	XrCompositionLayerBaseHeader const *layers[] {
		reinterpret_cast<XrCompositionLayerBaseHeader *>(&layer)
	};
	XrFrameEndInfo end_info {};
	end_info.type = XR_TYPE_FRAME_END_INFO;
	end_info.next = nullptr;
	end_info.displayTime = frame_state.predictedDisplayTime;
	end_info.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
	end_info.layerCount = view_count > 0 ? 1 : 0;
	end_info.layers = view_count > 0 ? layers : nullptr;
	return XR_SUCCEEDED(xrEndFrame(m_openxr->session, &end_info));
}

auto Application::update_camera_from_xr_view(XrView const &view) -> void
{
	auto const &pose { view.pose };
	smath::Vec3 const position { pose.position.x, pose.position.y,
		pose.position.z };
	auto const forward { xr_rotate_vector(
		pose.orientation, smath::Vec3 { 0.0f, 0.0f, -1.0f }) };
	auto const up { xr_rotate_vector(
		pose.orientation, smath::Vec3 { 0.0f, 1.0f, 0.0f }) };
	m_camera.position = position;
	m_camera.target = position + forward;
	m_camera.up = up;
	m_camera.fovy = view.fov.angleUp - view.fov.angleDown;
	m_cursor = PolarCoordinate::from_vec3(m_camera.target - m_camera.position);
}

auto Application::update_hands(XrTime display_time) -> void
{
	if (!m_openxr || !m_openxr->hand_tracking_supported) {
		m_left_hand_valid = false;
		m_right_hand_valid = false;
		return;
	}

	if (m_openxr->left_hand_tracker != XR_NULL_HANDLE
	    && m_openxr->locate_hand_joints) {
		XrHandJointsLocateInfoEXT locate_info {};
		locate_info.type = XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT;
		locate_info.next = nullptr;
		locate_info.baseSpace = m_openxr->app_space;
		locate_info.time = display_time;

		XrHandJointLocationEXT locations[XR_HAND_JOINT_COUNT_EXT];
		XrHandJointLocationsEXT locations_info {};
		locations_info.type = XR_TYPE_HAND_JOINT_LOCATIONS_EXT;
		locations_info.next = nullptr;
		locations_info.jointCount = XR_HAND_JOINT_COUNT_EXT;
		locations_info.jointLocations = locations;

		if (XR_SUCCEEDED(m_openxr->locate_hand_joints(
		        m_openxr->left_hand_tracker, &locate_info, &locations_info))) {
			m_left_hand_valid = (locations[0].locationFlags
			                        & XR_SPACE_LOCATION_POSITION_VALID_BIT)
			    != 0;
			if (m_left_hand_valid) {
				for (uint32_t j { 0 }; j < XR_HAND_JOINT_COUNT_EXT; j++) {
					m_left_joints[j]
					    = smath::Vec3 { locations[j].pose.position.x,
						      locations[j].pose.position.y,
						      locations[j].pose.position.z };
				}
			}
		} else {
			m_left_hand_valid = false;
		}
	}

	if (m_openxr->right_hand_tracker != XR_NULL_HANDLE
	    && m_openxr->locate_hand_joints) {
		XrHandJointsLocateInfoEXT locate_info {};
		locate_info.type = XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT;
		locate_info.next = nullptr;
		locate_info.baseSpace = m_openxr->app_space;
		locate_info.time = display_time;

		XrHandJointLocationEXT locations[XR_HAND_JOINT_COUNT_EXT];
		XrHandJointLocationsEXT locations_info {};
		locations_info.type = XR_TYPE_HAND_JOINT_LOCATIONS_EXT;
		locations_info.next = nullptr;
		locations_info.jointCount = XR_HAND_JOINT_COUNT_EXT;
		locations_info.jointLocations = locations;

		if (XR_SUCCEEDED(m_openxr->locate_hand_joints(
		        m_openxr->right_hand_tracker, &locate_info, &locations_info))) {
			m_right_hand_valid = (locations[0].locationFlags
			                         & XR_SPACE_LOCATION_POSITION_VALID_BIT)
			    != 0;
			if (m_right_hand_valid) {
				for (uint32_t j { 0 }; j < XR_HAND_JOINT_COUNT_EXT; j++) {
					m_right_joints[j]
					    = smath::Vec3 { locations[j].pose.position.x,
						      locations[j].pose.position.y,
						      locations[j].pose.position.z };
				}
			}
		} else {
			m_right_hand_valid = false;
		}
	}
}

auto Application::render_hands(
    VulkanRenderer::GL &gl, smath::Mat4 const &view_projection) -> void
{
	gl.set_texture(&m_renderer->white_texture());
	auto constexpr radius { 0.005f };

	if (m_left_hand_valid) {
		for (uint32_t j { 0 }; j < XR_HAND_JOINT_COUNT_EXT; j++) {
			auto const &pos { m_left_joints[j] };
			gl.set_transform(view_projection * smath::translate(pos));
			gl.draw_sphere(smath::Vec3 { 0.0f, 0.0f, 0.0f }, radius, 8, 16,
			    smath::Vec4 { 1.0f, 0.0f, 0.0f, 1.0f });
		}
	}

	if (m_right_hand_valid) {
		for (uint32_t j { 0 }; j < XR_HAND_JOINT_COUNT_EXT; j++) {
			auto const &pos { m_right_joints[j] };
			gl.set_transform(view_projection * smath::translate(pos));
			gl.draw_sphere(smath::Vec3 { 0.0f, 0.0f, 0.0f }, radius, 8, 16,
			    smath::Vec4 { 1.0f, 0.0f, 0.0f, 1.0f });
		}
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

	if (!m_window_focused)
		return;

	if (key == KEY_LEFTCTRL || key == KEY_RIGHTCTRL) {
		if (pressed) {
			++m_ctrl_pressed_count;
		} else if (m_ctrl_pressed_count > 0) {
			--m_ctrl_pressed_count;
		}
	}

	if (m_backend == Backend::SDL && !m_openxr && pressed && key == KEY_F11
	    && m_ctrl_pressed_count > 0) {
		bool const new_show_imgui { !m_show_imgui };
		m_show_imgui = new_show_imgui;
		mouse_captured(!new_show_imgui);
	}

	if (pressed && key == KEY_F12) {
		auto screenshot { std::optional<VulkanRenderer::ScreenshotPixels> {} };
		if (m_renderer) {
			screenshot = m_renderer->get_screenshot_pixels();
		}

		if (!screenshot) {
			m_logger.warn("Screenshot not ready");
			return;
		}

		auto const extent { screenshot->extent };
		auto const stride { static_cast<int>(extent.width * 4) };
		auto const index { m_screenshot_index++ };
		auto const now { std::chrono::system_clock::now() };
		auto filename { std::format(
			"screenshot_{:%Y%m%d_%H%M%S}_{:04}.png", now, index) };
		int const result { stbi_write_png(filename.c_str(),
			static_cast<int>(extent.width), static_cast<int>(extent.height), 4,
			screenshot->pixels.data(), stride) };

		if (result == 0) {
			m_logger.err("Failed to write screenshot {}", filename);
		} else {
			m_logger.info("Saved screenshot {}", filename);
		}
	}

	if (m_backend == Backend::SDL) {
		if (auto imgui_key { linux_key_to_imgui(key) }) {
			if (m_show_imgui)
				ImGui::GetIO().AddKeyEvent(*imgui_key, pressed);
		}

		if (m_show_imgui && pressed) {
			bool const shift_pressed { is_key_down(KEY_LEFTSHIFT)
				|| is_key_down(KEY_RIGHTSHIFT)
				|| (key == KEY_LEFTSHIFT && pressed)
				|| (key == KEY_RIGHTSHIFT && pressed) };

			if (auto ch { linux_key_to_char(key, shift_pressed) })
				ImGui::GetIO().AddInputCharacter(*ch);
		}
	}

	if (key < m_key_state.size())
		m_key_state[key] = pressed;
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
	if (m_backend != Backend::SDL) {
		m_mouse_captured = false;
		return;
	}

	if (!SDL_SetWindowRelativeMouseMode(m_window, new_state)) {
		m_logger.err("Failed to capture mouse");
		return;
	}

	m_mouse_captured = new_state && !m_show_imgui;
}

auto Application::is_key_down(uint32_t key) const -> bool
{
	if (key >= m_key_state.size())
		return false;
	return m_key_state[key];
}

auto Application::is_key_up(uint32_t key) const -> bool
{
	if (key >= m_key_state.size())
		return true;
	return !m_key_state[key];
}

auto Application::is_key_pressed(uint32_t key) const -> bool
{
	if (key >= m_key_state.size())
		return false;
	return m_key_state[key] && !m_key_state_previous[key];
}

auto Application::is_key_released(uint32_t key) const -> bool
{
	if (key >= m_key_state.size())
		return false;
	return !m_key_state[key] && m_key_state_previous[key];
}

auto Application::the() -> Application &
{
	static Application self {};
	return self;
}

} // namespace Lunar
