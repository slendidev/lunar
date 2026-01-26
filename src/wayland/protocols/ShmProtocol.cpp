#include <algorithm>
#include <cstdint>
#include <memory>
#include <unistd.h>
#include <utility>

#include <wayland-server-core.h>

#include "wayland-server-protocol.h"

#include "../Shm.h"
#include "../WaylandServer.h"

namespace Lunar::Wayland {
namespace {

constexpr std::uint32_t SHM_VERSION = 2;

auto shm_pool_from_resource(wl_resource *resource) -> std::shared_ptr<ShmPool>
{
	auto *handle { static_cast<std::shared_ptr<ShmPool> *>(
		wl_resource_get_user_data(resource)) };
	if (!handle) {
		return {};
	}
	return *handle;
}

void shm_pool_destroy_resource(wl_resource *resource)
{
	auto *handle { static_cast<std::shared_ptr<ShmPool> *>(
		wl_resource_get_user_data(resource)) };
	delete handle;
}

void shm_buffer_destroy_resource(wl_resource *resource)
{
	auto *handle { static_cast<std::shared_ptr<ShmBuffer> *>(
		wl_resource_get_user_data(resource)) };
	delete handle;
}

void shm_pool_handle_destroy(wl_client *, wl_resource *resource)
{
	wl_resource_destroy(resource);
}

void shm_pool_handle_resize(
    wl_client *, wl_resource *resource, std::int32_t size)
{
	auto pool { shm_pool_from_resource(resource) };
	if (!pool) {
		return;
	}
	if (size <= 0) {
		return;
	}
	pool->resize(static_cast<std::size_t>(size));
}

void shm_handle_release(wl_client *, wl_resource *) { }

void shm_pool_handle_create_buffer(wl_client *client, wl_resource *resource,
    std::uint32_t id, std::int32_t offset, std::int32_t width,
    std::int32_t height, std::int32_t stride, std::uint32_t format)
{
	auto pool { shm_pool_from_resource(resource) };
	if (!pool) {
		return;
	}

	if (width <= 0 || height <= 0 || stride <= 0 || offset < 0) {
		wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_STRIDE,
		    "Invalid shm buffer geometry");
		return;
	}

	if (format != WL_SHM_FORMAT_XRGB8888 && format != WL_SHM_FORMAT_ARGB8888) {
		wl_resource_post_error(
		    resource, WL_SHM_ERROR_INVALID_FORMAT, "Unsupported shm format");
		return;
	}

	auto required { static_cast<std::size_t>(offset)
		+ static_cast<std::size_t>(height) * static_cast<std::size_t>(stride) };
	if (required > pool->size()) {
		wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_STRIDE,
		    "Shm buffer size out of bounds");
		return;
	}

	auto *buffer_resource { wl_resource_create(
		client, &wl_buffer_interface, 1, id) };
	if (!buffer_resource) {
		return;
	}

	auto buffer { std::make_shared<ShmBuffer>(
		pool, buffer_resource, offset, width, height, stride, format) };
	auto *handle { new std::shared_ptr<ShmBuffer>(std::move(buffer)) };
	wl_resource_set_implementation(
	    buffer_resource, nullptr, handle, shm_buffer_destroy_resource);
}

struct wl_shm_pool_interface const SHM_POOL_INTERFACE = {
	.create_buffer = shm_pool_handle_create_buffer,
	.destroy = shm_pool_handle_destroy,
	.resize = shm_pool_handle_resize,
};

void shm_handle_create_pool(wl_client *client, wl_resource *resource,
    std::uint32_t id, int fd, std::int32_t size)
{
	auto *server
	    = static_cast<WaylandServer *>(wl_resource_get_user_data(resource));
	if (!server) {
		return;
	}
	if (size <= 0) {
		wl_resource_post_error(
		    resource, WL_SHM_ERROR_INVALID_STRIDE, "Invalid shm pool size");
		close(fd);
		return;
	}

	auto pool { std::make_shared<ShmPool>(
		fd, static_cast<std::size_t>(size), server->logger()) };
	if (!pool->data()) {
		wl_resource_post_error(
		    resource, WL_SHM_ERROR_INVALID_FD, "Failed to mmap shm pool");
		return;
	}

	auto *pool_resource
	    = wl_resource_create(client, &wl_shm_pool_interface, 1, id);
	if (!pool_resource) {
		return;
	}
	auto *handle { new std::shared_ptr<ShmPool>(std::move(pool)) };
	wl_resource_set_implementation(
	    pool_resource, &SHM_POOL_INTERFACE, handle, shm_pool_destroy_resource);
}

struct wl_shm_interface const SHM_INTERFACE = {
	.create_pool = shm_handle_create_pool,
	.release = shm_handle_release,
};

void bind_shm(
    wl_client *client, void *data, std::uint32_t version, std::uint32_t id)
{
	auto *server { static_cast<WaylandServer *>(data) };
	auto *resource { wl_resource_create(
		client, &wl_shm_interface, std::min(version, SHM_VERSION), id) };
	if (!resource) {
		return;
	}
	wl_resource_set_implementation(resource, &SHM_INTERFACE, server, nullptr);
	wl_shm_send_format(resource, WL_SHM_FORMAT_XRGB8888);
	wl_shm_send_format(resource, WL_SHM_FORMAT_ARGB8888);
}

} // namespace

auto WaylandServer::create_shm_global() -> std::unique_ptr<Global>
{
	return std::make_unique<Global>(
	    display(), &wl_shm_interface, SHM_VERSION, this, bind_shm);
}

} // namespace Lunar::Wayland
