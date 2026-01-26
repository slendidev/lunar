#include "Shm.h"

#include <cerrno>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>
#include <utility>

namespace Lunar::Wayland {

ShmPool::ShmPool(int fd, std::size_t size, Logger &logger)
    : m_logger(logger)
    , m_fd(fd)
    , m_size(size)
{
	m_data = mmap(nullptr, m_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
	if (m_data == MAP_FAILED) {
		m_data = nullptr;
		m_logger.err("Failed to mmap shm pool: {}", std::strerror(errno));
	}
}

ShmPool::~ShmPool()
{
	if (m_data) {
		munmap(m_data, m_size);
	}
	if (m_fd >= 0) {
		close(m_fd);
	}
}

auto ShmPool::resize(std::size_t new_size) -> bool
{
	if (!m_data) {
		return false;
	}
	void *new_data = mremap(m_data, m_size, new_size, MREMAP_MAYMOVE);
	if (new_data == MAP_FAILED) {
		m_logger.err("Failed to resize shm pool: {}", std::strerror(errno));
		return false;
	}
	m_data = new_data;
	m_size = new_size;
	return true;
}

auto ShmPool::data() const -> std::byte *
{
	return static_cast<std::byte *>(m_data);
}

ShmBuffer::ShmBuffer(std::shared_ptr<ShmPool> pool, wl_resource *resource,
    std::int32_t offset, std::int32_t width, std::int32_t height,
    std::int32_t stride, std::uint32_t format)
    : pool(std::move(pool))
    , resource(resource)
    , offset(offset)
    , width(width)
    , height(height)
    , stride(stride)
    , format(format)
{
}

auto ShmBuffer::data() const -> std::byte *
{
	if (!pool || !pool->data()) {
		return nullptr;
	}
	return pool->data() + offset;
}

auto ShmBuffer::byte_size() const -> std::size_t
{
	if (height <= 0 || stride <= 0) {
		return 0;
	}
	return static_cast<std::size_t>(height) * static_cast<std::size_t>(stride);
}

auto shm_buffer_from_resource(wl_resource *resource)
    -> std::shared_ptr<ShmBuffer>
{
	if (!resource) {
		return {};
	}
	auto *handle { static_cast<std::shared_ptr<ShmBuffer> *>(
		wl_resource_get_user_data(resource)) };
	if (!handle) {
		return {};
	}
	return *handle;
}

} // namespace Lunar::Wayland
