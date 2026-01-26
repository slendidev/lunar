#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include <wayland-server-core.h>

#include "../Logger.h"

namespace Lunar::Wayland {

struct ShmPool {
	explicit ShmPool(int fd, std::size_t size, Logger &logger);
	~ShmPool();

	auto resize(std::size_t new_size) -> bool;
	auto data() const -> std::byte *;
	auto size() const -> std::size_t { return m_size; }

private:
	Logger &m_logger;
	int m_fd { -1 };
	std::size_t m_size { 0 };
	void *m_data { nullptr };
};

struct ShmBuffer {
	ShmBuffer(std::shared_ptr<ShmPool> pool, wl_resource *resource,
	    std::int32_t offset, std::int32_t width, std::int32_t height,
	    std::int32_t stride, std::uint32_t format);

	auto data() const -> std::byte *;
	auto byte_size() const -> std::size_t;

	std::shared_ptr<ShmPool> pool;
	wl_resource *resource {};
	std::int32_t offset {};
	std::int32_t width {};
	std::int32_t height {};
	std::int32_t stride {};
	std::uint32_t format {};
};

auto shm_buffer_from_resource(wl_resource *resource)
    -> std::shared_ptr<ShmBuffer>;

} // namespace Lunar::Wayland
