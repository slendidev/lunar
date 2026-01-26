#include "Surface.h"

#include <algorithm>
#include <utility>

#include "WaylandServer.h"

#include "wayland-server-protocol.h"

namespace Lunar::Wayland {

Surface::Surface(WaylandServer &server, wl_resource *resource)
    : m_server(server)
    , m_resource(resource)
{
	m_server.register_surface(this);
}

Surface::~Surface()
{
	m_server.unregister_surface(this);
	for (auto *callback : m_frame_callbacks) {
		if (callback) {
			wl_callback_send_done(callback, m_server.now_ms());
			wl_resource_destroy(callback);
		}
	}
}

auto Surface::attach(
    std::shared_ptr<ShmBuffer> buffer, std::int32_t x, std::int32_t y) -> void
{
	m_pending_buffer = std::move(buffer);
	m_pending_offset_x = x;
	m_pending_offset_y = y;
}

auto Surface::damage(std::int32_t, std::int32_t, std::int32_t, std::int32_t)
    -> void
{
}

auto Surface::damage_buffer(
    std::int32_t, std::int32_t, std::int32_t, std::int32_t) -> void
{
}

auto Surface::frame(wl_resource *callback) -> void
{
	if (!callback) {
		return;
	}
	m_frame_callbacks.push_back(callback);
}

auto Surface::commit() -> void
{
	auto previous { m_current_buffer };
	m_current_buffer = m_pending_buffer;
	m_pending_buffer.reset();

	if (previous && previous != m_current_buffer && previous->resource) {
		wl_buffer_send_release(previous->resource);
	}

	if (!m_frame_callbacks.empty()) {
		auto callbacks { std::move(m_frame_callbacks) };
		auto done_time { m_server.now_ms() };
		for (auto *callback : callbacks) {
			if (callback) {
				wl_callback_send_done(callback, done_time);
				wl_resource_destroy(callback);
			}
		}
	}
}

auto Surface::set_opaque_region(std::shared_ptr<Region> region) -> void
{
	m_opaque_region = std::move(region);
}

auto Surface::set_input_region(std::shared_ptr<Region> region) -> void
{
	m_input_region = std::move(region);
}

auto Surface::set_buffer_transform(std::int32_t transform) -> void
{
	m_buffer_transform = transform;
}

auto Surface::set_buffer_scale(std::int32_t scale) -> void
{
	m_buffer_scale = std::max(1, scale);
}

} // namespace Lunar::Wayland
