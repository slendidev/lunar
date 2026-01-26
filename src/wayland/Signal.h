#pragma once

#include <cassert>

#include <wayland-server-core.h>

namespace Lunar::Wayland {

struct Signal {
	Signal(wl_signal *signal)
	    : m_signal { std::move(signal) }
	{
		assert(m_signal);
	}
	~Signal() = default;

	inline auto c_ptr() const -> wl_signal * { return m_signal; }

	template<typename T = void> auto flush(T *data)
	{
		wl_signal_emit_mutable(m_signal, (void *)data);
	}

private:
	wl_signal *m_signal {};
};

} // namespace Lunar::Wayland
