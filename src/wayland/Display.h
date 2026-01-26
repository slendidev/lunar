#pragma once

#include <cassert>
#include <format>
#include <stdexcept>
#include <vector>

#include <wayland-server-core.h>

namespace Lunar::Wayland {

struct Display {
	Display()
	    : m_display(wl_display_create())
	{
	}
	Display(wl_display *display)
	    : m_display { std::move(display) }
	    , m_should_cleanup { false }
	{
	}
	~Display()
	{
		if (!m_should_cleanup)
			return;
		wl_display_destroy_clients(m_display);
		wl_display_destroy(m_display);
	}

	inline auto c_ptr() const -> wl_display * { return m_display; }

	auto set_global_filter(
	    wl_display_global_filter_func_t filter, void *data) noexcept
	{
		wl_display_set_global_filter(m_display, filter, data);
	}

	auto next_serial() noexcept -> uint32_t
	{
		return wl_display_next_serial(m_display);
	}

	auto set_default_max_buffer_size(size_t max_buffer_size) noexcept
	{
		wl_display_set_default_max_buffer_size(m_display, max_buffer_size);
	}

	auto add_socket_fd(int fd)
	{
		if (wl_display_add_socket_fd(m_display, fd) == -1) {
			throw std::runtime_error(
			    "Failed to add socket fd to Wayland display");
		}
	}

	auto add_socket(char const *name)
	{
		if (wl_display_add_socket(m_display, name) == -1) {
			throw std::runtime_error(std::format(
			    "Failed to add socket `{}` to Wayland display", name));
		}
	}

	auto add_protocol_logger(wl_protocol_logger_func_t func, void *user_data)
	    -> wl_protocol_logger *
	{
		if (auto *logger {
		        wl_display_add_protocol_logger(m_display, func, user_data) };
		    logger != NULL) {
			return logger;
		} else {
			throw std::runtime_error(
			    "Failed to add protocol logger to Wayland display");
		}
	}

	auto add_shm_format(uint32_t format) -> uint32_t *
	{
		if (auto *fmt { wl_display_add_shm_format(m_display, format) };
		    fmt != NULL) {
			return fmt;
		} else {
			throw std::runtime_error(
			    "Failed to add SHM format to Wayland display");
		}
	}

	auto get_client_list() -> std::vector<wl_client *>
	{
		std::vector<wl_client *> ret {};
		auto const list { wl_display_get_client_list(m_display) };
		assert(list);
		wl_client *client {};
		wl_client_for_each(client, list) { ret.push_back(client); }
		return ret;
	}

private:
	wl_display *m_display {};
	bool m_should_cleanup { true };
};

} // namespace Lunar::Wayland
