#include <algorithm>
#include <cstdint>
#include <memory>
#include <utility>

#include <wayland-server-core.h>

#include "wayland-server-protocol.h"

#include "../Region.h"
#include "../Shm.h"
#include "../Surface.h"
#include "../WaylandServer.h"

namespace Lunar::Wayland {
namespace {

constexpr std::uint32_t COMPOSITOR_VERSION = 4;

auto resource_version(wl_resource *resource) -> std::uint32_t
{
	return std::min<std::uint32_t>(
	    wl_resource_get_version(resource), COMPOSITOR_VERSION);
}

auto region_from_resource(wl_resource *resource) -> std::shared_ptr<Region>
{
	if (!resource) {
		return {};
	}
	auto *handle { static_cast<std::shared_ptr<Region> *>(
		wl_resource_get_user_data(resource)) };
	if (!handle) {
		return {};
	}
	return *handle;
}

void region_destroy(wl_resource *resource)
{
	auto *handle { static_cast<std::shared_ptr<Region> *>(
		wl_resource_get_user_data(resource)) };
	delete handle;
}

void region_handle_destroy(wl_client *, wl_resource *resource)
{
	wl_resource_destroy(resource);
}

void region_handle_add(wl_client *, wl_resource *resource, std::int32_t x,
    std::int32_t y, std::int32_t width, std::int32_t height)
{
	if (auto region { region_from_resource(resource) }) {
		region->add(x, y, width, height);
	}
}

void region_handle_subtract(wl_client *, wl_resource *resource, std::int32_t x,
    std::int32_t y, std::int32_t width, std::int32_t height)
{
	if (auto region { region_from_resource(resource) }) {
		region->subtract(x, y, width, height);
	}
}

struct wl_region_interface const REGION_INTERFACE = {
	.destroy = region_handle_destroy,
	.add = region_handle_add,
	.subtract = region_handle_subtract,
};

auto surface_from_resource(wl_resource *resource) -> Surface *
{
	return static_cast<Surface *>(wl_resource_get_user_data(resource));
}

void surface_destroy_resource(wl_resource *resource)
{
	auto *surface { surface_from_resource(resource) };
	delete surface;
}

void surface_handle_destroy(wl_client *, wl_resource *resource)
{
	wl_resource_destroy(resource);
}

void surface_handle_attach(wl_client *, wl_resource *resource,
    wl_resource *buffer_resource, std::int32_t x, std::int32_t y)
{
	auto *surface { surface_from_resource(resource) };
	if (!surface) {
		return;
	}
	surface->attach(shm_buffer_from_resource(buffer_resource), x, y);
}

void surface_handle_damage(wl_client *, wl_resource *resource, std::int32_t x,
    std::int32_t y, std::int32_t width, std::int32_t height)
{
	if (auto *surface { surface_from_resource(resource) }) {
		surface->damage(x, y, width, height);
	}
}

void surface_handle_frame(
    wl_client *client, wl_resource *resource, std::uint32_t callback_id)
{
	auto *surface { surface_from_resource(resource) };
	if (!surface) {
		return;
	}
	auto version { wl_resource_get_version(resource) };
	auto *callback_resource { wl_resource_create(
		client, &wl_callback_interface, version, callback_id) };
	if (!callback_resource) {
		return;
	}
	wl_resource_set_implementation(
	    callback_resource, nullptr, nullptr, nullptr);
	surface->frame(callback_resource);
}

void surface_handle_set_opaque_region(
    wl_client *, wl_resource *resource, wl_resource *region_resource)
{
	if (auto *surface { surface_from_resource(resource) }) {
		surface->set_opaque_region(region_from_resource(region_resource));
	}
}

void surface_handle_set_input_region(
    wl_client *, wl_resource *resource, wl_resource *region_resource)
{
	if (auto *surface { surface_from_resource(resource) }) {
		surface->set_input_region(region_from_resource(region_resource));
	}
}

void surface_handle_commit(wl_client *, wl_resource *resource)
{
	if (auto *surface { surface_from_resource(resource) }) {
		surface->commit();
	}
}

void surface_handle_set_buffer_transform(
    wl_client *, wl_resource *resource, std::int32_t transform)
{
	if (auto *surface { surface_from_resource(resource) }) {
		surface->set_buffer_transform(transform);
	}
}

void surface_handle_set_buffer_scale(
    wl_client *, wl_resource *resource, std::int32_t scale)
{
	if (auto *surface { surface_from_resource(resource) }) {
		surface->set_buffer_scale(scale);
	}
}

void surface_handle_damage_buffer(wl_client *, wl_resource *resource,
    std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height)
{
	if (auto *surface { surface_from_resource(resource) }) {
		surface->damage_buffer(x, y, width, height);
	}
}

void surface_handle_offset(
    wl_client *, wl_resource *resource, std::int32_t x, std::int32_t y)
{
	(void)resource;
	(void)x;
	(void)y;
}

struct wl_surface_interface const SURFACE_INTERFACE = {
	.destroy = surface_handle_destroy,
	.attach = surface_handle_attach,
	.damage = surface_handle_damage,
	.frame = surface_handle_frame,
	.set_opaque_region = surface_handle_set_opaque_region,
	.set_input_region = surface_handle_set_input_region,
	.commit = surface_handle_commit,
	.set_buffer_transform = surface_handle_set_buffer_transform,
	.set_buffer_scale = surface_handle_set_buffer_scale,
	.damage_buffer = surface_handle_damage_buffer,
	.offset = surface_handle_offset,
};

void compositor_handle_create_surface(
    wl_client *client, wl_resource *resource, std::uint32_t id)
{
	auto *server { static_cast<WaylandServer *>(
		wl_resource_get_user_data(resource)) };
	if (!server) {
		return;
	}
	auto version { resource_version(resource) };
	auto *surface_resource { wl_resource_create(
		client, &wl_surface_interface, version, id) };
	if (!surface_resource) {
		return;
	}
	auto *surface { new Surface(*server, surface_resource) };
	wl_resource_set_implementation(surface_resource, &SURFACE_INTERFACE,
	    surface, surface_destroy_resource);
}

void compositor_handle_create_region(
    wl_client *client, wl_resource *resource, std::uint32_t id)
{
	auto version { wl_resource_get_version(resource) };
	auto *region_resource { wl_resource_create(
		client, &wl_region_interface, version, id) };
	if (!region_resource) {
		return;
	}
	auto region { std::make_shared<Region>(region_resource) };
	auto *handle { new std::shared_ptr<Region>(std::move(region)) };
	wl_resource_set_implementation(
	    region_resource, &REGION_INTERFACE, handle, region_destroy);
}

struct wl_compositor_interface const COMPOSITOR_INTERFACE = {
	.create_surface = compositor_handle_create_surface,
	.create_region = compositor_handle_create_region,
};

void bind_compositor(
    wl_client *client, void *data, std::uint32_t version, std::uint32_t id)
{
	auto *server { static_cast<WaylandServer *>(data) };
	auto *resource { wl_resource_create(client, &wl_compositor_interface,
		std::min(version, COMPOSITOR_VERSION), id) };
	if (!resource) {
		return;
	}
	wl_resource_set_implementation(
	    resource, &COMPOSITOR_INTERFACE, server, nullptr);
}

} // namespace

auto WaylandServer::create_compositor_global() -> std::unique_ptr<Global>
{
	return std::make_unique<Global>(display(), &wl_compositor_interface,
	    COMPOSITOR_VERSION, this, bind_compositor);
}

} // namespace Lunar::Wayland
