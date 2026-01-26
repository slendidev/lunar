#pragma once

#include <cassert>
#include <optional>
#include <utility>

#include <wayland-server-core.h>

#include "Client.h"
#include "Display.h"

namespace Lunar::Wayland {

struct Global {
	Global() = delete;

	explicit Global(Display &display, wl_interface const *interface,
	    int version, void *data, wl_global_bind_func_t bind)
	    : m_global {
		    wl_global_create(display.c_ptr(), interface, version, data, bind),
	    }
	{
	}

	Global(wl_global *global)
	    : m_global { std::move(global) }
	    , m_should_cleanup { false }
	{
		assert(m_global);
	}

	~Global()
	{
		if (!m_should_cleanup)
			return;
		wl_global_destroy(m_global);
	}

	inline auto c_ptr() const -> wl_global * { return m_global; }

	auto get_name(Client &client) const -> std::optional<uint32_t>
	{
		if (auto const ret = wl_global_get_name(m_global, client.c_ptr());
		    ret != 0) {
			return ret;
		} else {
			return {};
		}
	}

	inline auto get_version() const -> uint32_t
	{
		return wl_global_get_version(m_global);
	}

	inline auto get_display() const -> Display
	{
		return wl_global_get_display(m_global);
	}

	inline auto get_interface() const -> wl_interface const *
	{
		return wl_global_get_interface(m_global);
	}

private:
	wl_global *m_global {};
	bool m_should_cleanup { true };
};

} // namespace Lunar::Wayland
