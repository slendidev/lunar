#pragma once

#include <cassert>
#include <format>
#include <functional>
#include <optional>
#include <tuple>

#include <wayland-server-core.h>

#include "Display.h"
#include "List.h"

namespace Lunar::Wayland {

struct Client {
	Client(wl_client *client)
	    : m_client { std::move(client) }
	{
		assert(m_client);
	}
	~Client() = default;

	inline auto c_ptr() const -> wl_client * { return m_client; }

	static auto from_link(wl_list *link) -> Client
	{
		return Client { wl_client_from_link(link) };
	}

	auto flush() { wl_client_flush(m_client); }

	auto get_display() -> Display
	{
		return Display { wl_client_get_display(m_client) };
	}

	auto get_credentials() noexcept -> std::tuple<pid_t, uid_t, gid_t>
	{
		std::tuple<pid_t, uid_t, gid_t> ret {};
		auto &[pid, uid, gid] { ret };
		wl_client_get_credentials(m_client, &pid, &uid, &gid);
		return ret;
	}

	auto get_fd() noexcept -> int { return wl_client_get_fd(m_client); }

	auto get_object(uint32_t id) -> std::optional<wl_resource *>
	{
		if (auto *res { wl_client_get_object(m_client, id) }; res != NULL) {
			return res;
		} else {
			return {};
		}
	}

	auto post_implementation_error(std::string_view string)
	{
		wl_client_post_implementation_error(
		    m_client, "%.*s", static_cast<int>(string.size()), string.data());
	}

	template<typename... Args>
	auto post_implementation_error(
	    std::format_string<Args...> fmt, Args &&...args)
	{
		post_implementation_error(
		    std::format(fmt, std::forward<Args>(args)...));
	}

	auto add_destroy_listener(wl_listener *listener)
	{
		wl_client_add_destroy_listener(m_client, listener);
	}

	auto add_destroy_late_listener(wl_listener *listener)
	{
		wl_client_add_destroy_late_listener(m_client, listener);
	}

	auto get_link() -> wl_list * { return wl_client_get_link(m_client); }

	auto add_resource_created_listener(wl_listener *listener)
	{
		wl_client_add_resource_created_listener(m_client, listener);
	}

	auto for_each_resource(
	    std::function<wl_iterator_result(wl_resource *)> const &fn) -> void
	{
		wl_client_for_each_resource(
		    m_client,
		    (wl_client_for_each_resource_iterator_func_t)[](
		        wl_resource * res, void *user_data)
		        ->wl_iterator_result {
			        auto *f { static_cast<
				        std::function<wl_iterator_result(wl_resource *)> *>(
				        user_data) };
			        return (*f)(res);
		        },
		    const_cast<void *>(static_cast<void const *>(&fn)));
	}

	auto set_max_buffer_size(size_t max_buffer_size)
	{
		wl_client_set_max_buffer_size(m_client, max_buffer_size);
	}

private:
	wl_client *m_client {};
};

} // namespace Lunar::Wayland
