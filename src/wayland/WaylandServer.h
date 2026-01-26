#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <wayland-server-core.h>

#include "../Logger.h"

#include "Display.h"
#include "Global.h"

namespace Lunar::Wayland {

struct Surface;

struct WaylandServer {
	explicit WaylandServer(Logger &logger);
	~WaylandServer();

	auto display() -> Display & { return m_display; }
	auto logger() -> Logger & { return m_logger; }
	auto socket_name() const -> std::string_view { return m_socket_name; }
	auto dispatch() -> void;
	auto flush() -> void;
	auto now_ms() const -> std::uint32_t;
	auto register_surface(Surface *surface) -> void;
	auto unregister_surface(Surface *surface) -> void;
	auto surfaces() const -> std::span<Surface *const>;

private:
	auto create_compositor_global() -> std::unique_ptr<Global>;
	auto create_shm_global() -> std::unique_ptr<Global>;
	auto create_xdg_wm_base_global() -> std::unique_ptr<Global>;

	Logger &m_logger;
	Display m_display {};
	wl_event_loop *m_loop { nullptr };
	std::string m_socket_name {};
	std::unique_ptr<Global> m_compositor_global {};
	std::unique_ptr<Global> m_shm_global {};
	std::unique_ptr<Global> m_xdg_wm_base_global {};
	std::vector<Surface *> m_surfaces {};
};

} // namespace Lunar::Wayland
