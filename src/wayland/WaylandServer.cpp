#include "WaylandServer.h"

#include <algorithm>
#include <chrono>
#include <span>
#include <stdexcept>

namespace Lunar::Wayland {

WaylandServer::WaylandServer(Logger &logger)
    : m_logger(logger)
{
	m_loop = wl_display_get_event_loop(m_display.c_ptr());
	if (!m_loop) {
		throw std::runtime_error("Failed to get Wayland event loop");
	}

	auto *socket_name { wl_display_add_socket_auto(m_display.c_ptr()) };
	if (!socket_name) {
		throw std::runtime_error("Failed to create Wayland socket");
	}
	if (socket_name) {
		m_socket_name = socket_name;
	}

	m_logger.info("Wayland listening on {}", m_socket_name);

	m_compositor_global = create_compositor_global();
	m_shm_global = create_shm_global();
	m_xdg_wm_base_global = create_xdg_wm_base_global();
}

WaylandServer::~WaylandServer() = default;

auto WaylandServer::dispatch() -> void
{
	if (!m_loop) {
		return;
	}
	wl_event_loop_dispatch(m_loop, 0);
}

auto WaylandServer::flush() -> void
{
	wl_display_flush_clients(m_display.c_ptr());
}

auto WaylandServer::now_ms() const -> std::uint32_t
{
	using Clock = std::chrono::steady_clock;
	auto now { Clock::now().time_since_epoch() };
	auto ms {
		std::chrono::duration_cast<std::chrono::milliseconds>(now).count()
	};
	return static_cast<std::uint32_t>(ms);
}

auto WaylandServer::register_surface(Surface *surface) -> void
{
	if (!surface) {
		return;
	}
	m_surfaces.push_back(surface);
}

auto WaylandServer::unregister_surface(Surface *surface) -> void
{
	if (!surface) {
		return;
	}
	auto it { std::remove(m_surfaces.begin(), m_surfaces.end(), surface) };
	m_surfaces.erase(it, m_surfaces.end());
}

auto WaylandServer::surfaces() const -> std::span<Surface *const>
{
	return { m_surfaces.data(), m_surfaces.size() };
}

} // namespace Lunar::Wayland
#include "Surface.h"
