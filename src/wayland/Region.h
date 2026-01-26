#pragma once

#include <cstdint>
#include <vector>

#include <wayland-server-core.h>

namespace Lunar::Wayland {

struct Region {
	struct Box {
		std::int32_t x {};
		std::int32_t y {};
		std::int32_t width {};
		std::int32_t height {};
	};

	explicit Region(wl_resource *resource)
	    : m_resource(resource)
	{
	}

	auto resource() const -> wl_resource * { return m_resource; }

	auto add(std::int32_t x, std::int32_t y, std::int32_t width,
	    std::int32_t height) -> void
	{
		m_boxes.push_back(Box { x, y, width, height });
	}

	auto subtract(std::int32_t x, std::int32_t y, std::int32_t width,
	    std::int32_t height) -> void
	{
		m_subtract_boxes.push_back(Box { x, y, width, height });
	}

private:
	wl_resource *m_resource {};
	std::vector<Box> m_boxes {};
	std::vector<Box> m_subtract_boxes {};
};

} // namespace Lunar::Wayland
