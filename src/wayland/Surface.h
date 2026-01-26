#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <wayland-server-core.h>

#include "Region.h"
#include "Shm.h"

namespace Lunar::Wayland {

struct WaylandServer;

struct Surface {
	explicit Surface(WaylandServer &server, wl_resource *resource);
	~Surface();

	auto resource() const -> wl_resource * { return m_resource; }
	auto current_buffer() const -> std::shared_ptr<ShmBuffer> const &
	{
		return m_current_buffer;
	}

	auto attach(std::shared_ptr<ShmBuffer> buffer, std::int32_t x,
	    std::int32_t y) -> void;
	auto damage(std::int32_t x, std::int32_t y, std::int32_t width,
	    std::int32_t height) -> void;
	auto damage_buffer(std::int32_t x, std::int32_t y, std::int32_t width,
	    std::int32_t height) -> void;
	auto frame(wl_resource *callback) -> void;
	auto commit() -> void;
	auto set_opaque_region(std::shared_ptr<Region> region) -> void;
	auto set_input_region(std::shared_ptr<Region> region) -> void;
	auto set_buffer_transform(std::int32_t transform) -> void;
	auto set_buffer_scale(std::int32_t scale) -> void;

private:
	WaylandServer &m_server;
	wl_resource *m_resource {};
	std::shared_ptr<ShmBuffer> m_pending_buffer {};
	std::shared_ptr<ShmBuffer> m_current_buffer {};
	std::shared_ptr<Region> m_opaque_region {};
	std::shared_ptr<Region> m_input_region {};
	std::vector<wl_resource *> m_frame_callbacks {};
	std::int32_t m_buffer_transform { 0 };
	std::int32_t m_buffer_scale { 1 };
	std::int32_t m_pending_offset_x { 0 };
	std::int32_t m_pending_offset_y { 0 };
};

} // namespace Lunar::Wayland
