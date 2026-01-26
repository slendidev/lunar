#include <algorithm>
#include <cstdint>

#include <wayland-server-core.h>

#include "xdg-shell-server-protocol.h"

#include "../WaylandServer.h"

namespace Lunar::Wayland {
namespace {

constexpr std::uint32_t XDG_WM_BASE_VERSION = 7;

struct XdgSurface {
	WaylandServer &server;
	wl_resource *resource {};
	wl_resource *surface_resource {};
	wl_resource *toplevel_resource {};
	std::uint32_t last_serial { 0 };
};

struct XdgToplevel {
	WaylandServer &server;
	wl_resource *resource {};
	XdgSurface *surface {};
};

extern struct xdg_toplevel_interface const XDG_TOPLEVEL_INTERFACE_IMPL;

auto xdg_surface_from_resource(wl_resource *resource) -> XdgSurface *
{
	return static_cast<XdgSurface *>(wl_resource_get_user_data(resource));
}

auto xdg_toplevel_from_resource(wl_resource *resource) -> XdgToplevel *
{
	return static_cast<XdgToplevel *>(wl_resource_get_user_data(resource));
}

auto send_configure(XdgSurface &surface) -> void
{
	std::uint32_t serial = surface.server.display().next_serial();
	if (surface.toplevel_resource) {
		wl_array states;
		wl_array_init(&states);
		xdg_toplevel_send_configure(surface.toplevel_resource, 0, 0, &states);
		wl_array_release(&states);
	}
	surface.last_serial = serial;
	xdg_surface_send_configure(surface.resource, serial);
}

auto xdg_positioner_handle_destroy(wl_client *, wl_resource *resource) -> void
{
	wl_resource_destroy(resource);
}

auto xdg_positioner_handle_set_size(
    wl_client *, wl_resource *, std::int32_t, std::int32_t) -> void
{
}

auto xdg_positioner_handle_set_anchor_rect(wl_client *, wl_resource *,
    std::int32_t, std::int32_t, std::int32_t, std::int32_t) -> void
{
}

auto xdg_positioner_handle_set_anchor(wl_client *, wl_resource *, std::uint32_t)
    -> void
{
}

auto xdg_positioner_handle_set_gravity(
    wl_client *, wl_resource *, std::uint32_t) -> void
{
}

auto xdg_positioner_handle_set_constraint_adjustment(
    wl_client *, wl_resource *, std::uint32_t) -> void
{
}

auto xdg_positioner_handle_set_offset(
    wl_client *, wl_resource *, std::int32_t, std::int32_t) -> void
{
}

auto xdg_positioner_handle_set_reactive(wl_client *, wl_resource *) -> void { }

auto xdg_positioner_handle_set_parent_size(
    wl_client *, wl_resource *, std::int32_t, std::int32_t) -> void
{
}

auto xdg_positioner_handle_set_parent_configure(
    wl_client *, wl_resource *, std::uint32_t) -> void
{
}

struct xdg_positioner_interface const XDG_POSITIONER_INTERFACE_IMPL = {
	.destroy = xdg_positioner_handle_destroy,
	.set_size = xdg_positioner_handle_set_size,
	.set_anchor_rect = xdg_positioner_handle_set_anchor_rect,
	.set_anchor = xdg_positioner_handle_set_anchor,
	.set_gravity = xdg_positioner_handle_set_gravity,
	.set_constraint_adjustment
	= xdg_positioner_handle_set_constraint_adjustment,
	.set_offset = xdg_positioner_handle_set_offset,
	.set_reactive = xdg_positioner_handle_set_reactive,
	.set_parent_size = xdg_positioner_handle_set_parent_size,
	.set_parent_configure = xdg_positioner_handle_set_parent_configure,
};

auto xdg_surface_destroy_resource(wl_resource *resource) -> void
{
	auto *surface { xdg_surface_from_resource(resource) };
	delete surface;
}

auto xdg_surface_handle_destroy(wl_client *, wl_resource *resource) -> void
{
	wl_resource_destroy(resource);
}

auto xdg_surface_handle_get_toplevel(
    wl_client *client, wl_resource *resource, std::uint32_t id) -> void
{
	auto *surface { xdg_surface_from_resource(resource) };
	if (!surface) {
		return;
	}
	auto version { wl_resource_get_version(resource) };
	auto *toplevel_resource { wl_resource_create(
		client, &::xdg_toplevel_interface, version, id) };
	if (!toplevel_resource) {
		return;
	}
	auto *toplevel { new XdgToplevel {
		surface->server, toplevel_resource, surface } };
	surface->toplevel_resource = toplevel_resource;
	wl_resource_set_implementation(toplevel_resource,
	    &XDG_TOPLEVEL_INTERFACE_IMPL, toplevel, [](wl_resource *res) {
		    auto *tl { xdg_toplevel_from_resource(res) };
		    if (tl && tl->surface && tl->surface->toplevel_resource == res) {
			    tl->surface->toplevel_resource = nullptr;
		    }
		    delete tl;
	    });

	send_configure(*surface);
}

auto xdg_surface_handle_get_popup(wl_client *, wl_resource *, std::uint32_t,
    wl_resource *, wl_resource *) -> void
{
}

auto xdg_surface_handle_set_window_geometry(wl_client *, wl_resource *,
    std::int32_t, std::int32_t, std::int32_t, std::int32_t) -> void
{
}

auto xdg_surface_handle_ack_configure(
    wl_client *, wl_resource *resource, std::uint32_t serial) -> void
{
	if (auto *surface { xdg_surface_from_resource(resource) }) {
		surface->last_serial = serial;
	}
}

struct xdg_surface_interface const XDG_SURFACE_INTERFACE_IMPL = {
	.destroy = xdg_surface_handle_destroy,
	.get_toplevel = xdg_surface_handle_get_toplevel,
	.get_popup = xdg_surface_handle_get_popup,
	.set_window_geometry = xdg_surface_handle_set_window_geometry,
	.ack_configure = xdg_surface_handle_ack_configure,
};

auto xdg_toplevel_handle_destroy(wl_client *, wl_resource *resource) -> void
{
	wl_resource_destroy(resource);
}

auto xdg_toplevel_handle_set_parent(wl_client *, wl_resource *, wl_resource *)
    -> void
{
}

auto xdg_toplevel_handle_set_title(wl_client *, wl_resource *, char const *)
    -> void
{
}

auto xdg_toplevel_handle_set_app_id(wl_client *, wl_resource *, char const *)
    -> void
{
}

auto xdg_toplevel_handle_show_window_menu(wl_client *, wl_resource *,
    wl_resource *, std::uint32_t, std::int32_t, std::int32_t) -> void
{
}

auto xdg_toplevel_handle_move(
    wl_client *, wl_resource *, wl_resource *, std::uint32_t) -> void
{
}

auto xdg_toplevel_handle_resize(wl_client *, wl_resource *, wl_resource *,
    std::uint32_t, std::uint32_t) -> void
{
}

auto xdg_toplevel_handle_set_max_size(
    wl_client *, wl_resource *, std::int32_t, std::int32_t) -> void
{
}

auto xdg_toplevel_handle_set_min_size(
    wl_client *, wl_resource *, std::int32_t, std::int32_t) -> void
{
}

auto xdg_toplevel_handle_set_maximized(wl_client *, wl_resource *) -> void { }

auto xdg_toplevel_handle_unset_maximized(wl_client *, wl_resource *) -> void { }

auto xdg_toplevel_handle_set_fullscreen(
    wl_client *, wl_resource *, wl_resource *) -> void
{
}

auto xdg_toplevel_handle_unset_fullscreen(wl_client *, wl_resource *) -> void {
}

auto xdg_toplevel_handle_set_minimized(wl_client *, wl_resource *) -> void { }

struct xdg_toplevel_interface const XDG_TOPLEVEL_INTERFACE_IMPL = {
	.destroy = xdg_toplevel_handle_destroy,
	.set_parent = xdg_toplevel_handle_set_parent,
	.set_title = xdg_toplevel_handle_set_title,
	.set_app_id = xdg_toplevel_handle_set_app_id,
	.show_window_menu = xdg_toplevel_handle_show_window_menu,
	.move = xdg_toplevel_handle_move,
	.resize = xdg_toplevel_handle_resize,
	.set_max_size = xdg_toplevel_handle_set_max_size,
	.set_min_size = xdg_toplevel_handle_set_min_size,
	.set_maximized = xdg_toplevel_handle_set_maximized,
	.unset_maximized = xdg_toplevel_handle_unset_maximized,
	.set_fullscreen = xdg_toplevel_handle_set_fullscreen,
	.unset_fullscreen = xdg_toplevel_handle_unset_fullscreen,
	.set_minimized = xdg_toplevel_handle_set_minimized,
};

auto xdg_wm_base_handle_destroy(wl_client *, wl_resource *resource) -> void
{
	wl_resource_destroy(resource);
}

auto xdg_wm_base_handle_create_positioner(
    wl_client *client, wl_resource *resource, std::uint32_t id) -> void
{
	auto version { wl_resource_get_version(resource) };
	auto *positioner { wl_resource_create(
		client, &::xdg_positioner_interface, version, id) };
	if (!positioner) {
		return;
	}
	wl_resource_set_implementation(
	    positioner, &XDG_POSITIONER_INTERFACE_IMPL, nullptr, nullptr);
}

auto xdg_wm_base_handle_get_xdg_surface(wl_client *client,
    wl_resource *resource, std::uint32_t id, wl_resource *surface_resource)
    -> void
{
	auto *server
	    = static_cast<WaylandServer *>(wl_resource_get_user_data(resource));
	if (!server) {
		return;
	}
	auto version { wl_resource_get_version(resource) };
	auto *xdg_surface_resource { wl_resource_create(
		client, &::xdg_surface_interface, version, id) };
	if (!xdg_surface_resource) {
		return;
	}
	auto *surface { new XdgSurface {
		*server, xdg_surface_resource, surface_resource, nullptr, 0 } };
	wl_resource_set_implementation(xdg_surface_resource,
	    &XDG_SURFACE_INTERFACE_IMPL, surface, xdg_surface_destroy_resource);
}

auto xdg_wm_base_handle_pong(wl_client *, wl_resource *, std::uint32_t) -> void
{
}

struct xdg_wm_base_interface const XDG_WM_BASE_INTERFACE_IMPL = {
	.destroy = xdg_wm_base_handle_destroy,
	.create_positioner = xdg_wm_base_handle_create_positioner,
	.get_xdg_surface = xdg_wm_base_handle_get_xdg_surface,
	.pong = xdg_wm_base_handle_pong,
};

auto bind_xdg_wm_base(wl_client *client, void *data, std::uint32_t version,
    std::uint32_t id) -> void
{
	auto *server { static_cast<WaylandServer *>(data) };
	auto *resource { wl_resource_create(client, &::xdg_wm_base_interface,
		std::min(version, XDG_WM_BASE_VERSION), id) };
	if (!resource) {
		return;
	}
	wl_resource_set_implementation(
	    resource, &XDG_WM_BASE_INTERFACE_IMPL, server, nullptr);
}

} // namespace

auto WaylandServer::create_xdg_wm_base_global() -> std::unique_ptr<Global>
{
	return std::make_unique<Global>(display(), &::xdg_wm_base_interface,
	    XDG_WM_BASE_VERSION, this, bind_xdg_wm_base);
}

} // namespace Lunar::Wayland
