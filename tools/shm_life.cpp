#define _GNU_SOURCE
#include <algorithm>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <print>
#include <random>
#include <sys/mman.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "xdg-shell-client-protocol.h"
#include <wayland-client.h>

static wl_compositor *g_compositor;
static wl_shm *g_shm;
static xdg_wm_base *g_xdg_wm_base;
static xdg_surface *g_xdg_surface;
static xdg_toplevel *g_xdg_toplevel;
static bool g_configured;
static std::sig_atomic_t volatile g_running { 1 };

static auto handle_signal(int) -> void { g_running = 0; }

static auto registry_global(void *data, wl_registry *registry, uint32_t name,
    char const *interface, uint32_t version) -> void
{
	(void)data;
	if (std::strcmp(interface, "wl_compositor") == 0) {
		g_compositor = static_cast<wl_compositor *>(wl_registry_bind(registry,
		    name, &wl_compositor_interface, version < 4 ? version : 4));
	} else if (std::strcmp(interface, "wl_shm") == 0) {
		g_shm = static_cast<wl_shm *>(wl_registry_bind(
		    registry, name, &wl_shm_interface, version < 1 ? version : 1));
	} else if (std::strcmp(interface, "xdg_wm_base") == 0) {
		g_xdg_wm_base = static_cast<xdg_wm_base *>(wl_registry_bind(
		    registry, name, &xdg_wm_base_interface, version < 7 ? version : 7));
	}
}

static auto registry_global_remove(
    void *data, wl_registry *registry, uint32_t name) -> void
{
	(void)data;
	(void)registry;
	(void)name;
}

static wl_registry_listener const registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};

static auto xdg_wm_base_handle_ping(
    void *, xdg_wm_base *wm_base, uint32_t serial) -> void
{
	xdg_wm_base_pong(wm_base, serial);
}

static xdg_wm_base_listener const xdg_wm_base_listener = {
	.ping = xdg_wm_base_handle_ping,
};

static auto xdg_surface_handle_configure(
    void *, xdg_surface *surface, uint32_t serial) -> void
{
	xdg_surface_ack_configure(surface, serial);
	g_configured = true;
}

static xdg_surface_listener const xdg_surface_listener = {
	.configure = xdg_surface_handle_configure,
};

static auto xdg_toplevel_handle_configure(
    void *, xdg_toplevel *, int32_t, int32_t, wl_array *) -> void
{
}

static auto xdg_toplevel_handle_close(void *, xdg_toplevel *) -> void { }

static auto xdg_toplevel_handle_configure_bounds(
    void *, xdg_toplevel *, int32_t, int32_t) -> void
{
}

static auto xdg_toplevel_handle_wm_capabilities(
    void *, xdg_toplevel *, wl_array *) -> void
{
}

static xdg_toplevel_listener const xdg_toplevel_listener = {
	.configure = xdg_toplevel_handle_configure,
	.close = xdg_toplevel_handle_close,
	.configure_bounds = xdg_toplevel_handle_configure_bounds,
	.wm_capabilities = xdg_toplevel_handle_wm_capabilities,
};

static auto create_shm_file(size_t size) -> int
{
	int fd { memfd_create("shm_life", 0) };
	if (fd < 0) {
		return -1;
	}
	if (ftruncate(fd, static_cast<off_t>(size)) != 0) {
		close(fd);
		return -1;
	}
	return fd;
}

auto main(int argc, char **argv) -> int
{
	auto *display { wl_display_connect(nullptr) };
	if (!display) {
		std::println(std::cerr, "Failed to connect to Wayland display.");
		return 1;
	}

	auto *registry { wl_display_get_registry(display) };
	if (!registry) {
		std::println(std::cerr, "Failed to fetch Wayland registry.");
		wl_display_disconnect(display);
		return 1;
	}

	if (wl_registry_add_listener(registry, &registry_listener, nullptr) < 0) {
		std::println(std::cerr, "Failed to add registry listener.");
		wl_display_disconnect(display);
		return 1;
	}
	if (wl_display_roundtrip(display) < 0) {
		std::println(std::cerr, "Failed to sync Wayland registry.");
		wl_display_disconnect(display);
		return 1;
	}

	if (!g_compositor || !g_shm || !g_xdg_wm_base) {
		std::println(std::cerr, "Missing compositor or xdg globals.");
		wl_display_disconnect(display);
		return 1;
	}

	std::signal(SIGINT, handle_signal);
	std::signal(SIGTERM, handle_signal);

	if (xdg_wm_base_add_listener(g_xdg_wm_base, &xdg_wm_base_listener, nullptr)
	    < 0) {
		std::println(std::cerr, "Failed to add xdg_wm_base listener.");
		wl_display_disconnect(display);
		return 1;
	}

	constexpr int width { 640 };
	constexpr int height { 480 };
	int cell_size { 5 };
	if (argc > 1) {
		long requested { std::strtol(argv[1], nullptr, 10) };
		if (requested >= 2 && requested <= 80) {
			cell_size = static_cast<int>(requested);
		}
	}
	int grid_width { width / cell_size };
	int grid_height { height / cell_size };
	constexpr int stride { width * 4 };
	constexpr size_t size { static_cast<size_t>(stride) * height };
	int fd { create_shm_file(size) };
	if (fd < 0) {
		std::println(std::cerr, "Failed to create shm file.");
		wl_display_disconnect(display);
		return 1;
	}

	auto *data { static_cast<uint32_t *>(
		mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) };
	if (data == MAP_FAILED) {
		std::println(std::cerr, "Failed to mmap shm.");
		close(fd);
		wl_display_disconnect(display);
		return 1;
	}

	std::vector<uint8_t> grid(static_cast<size_t>(grid_width * grid_height), 0);
	std::vector<uint8_t> next(static_cast<size_t>(grid_width * grid_height), 0);
	std::minstd_rand rng(std::random_device {}());
	std::bernoulli_distribution alive_dist(0.45);
	std::vector<uint64_t> history;
	float hue { 0.0f };
	float hue_step { 1.5f };

	auto hue_to_rgb { [](float hue_degrees) -> uint32_t {
		float h { std::fmod(hue_degrees, 360.0f) / 360.0f };
		float s { 1.0f };
		float l { 0.5f };
		auto hue_to_channel { [](float p, float q, float t) -> float {
			if (t < 0.0f) {
				t += 1.0f;
			}
			if (t > 1.0f) {
				t -= 1.0f;
			}
			if (t < 1.0f / 6.0f) {
				return p + (q - p) * 6.0f * t;
			}
			if (t < 1.0f / 2.0f) {
				return q;
			}
			if (t < 2.0f / 3.0f) {
				return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
			}
			return p;
		} };
		float r { l };
		float g { l };
		float b { l };
		if (s > 0.0f) {
			float q { l < 0.5f ? l * (1.0f + s) : l + s - l * s };
			float p { 2.0f * l - q };
			r = hue_to_channel(p, q, h + 1.0f / 3.0f);
			g = hue_to_channel(p, q, h);
			b = hue_to_channel(p, q, h - 1.0f / 3.0f);
		}
		uint8_t rr { static_cast<uint8_t>(r * 255.0f) };
		uint8_t gg { static_cast<uint8_t>(g * 255.0f) };
		uint8_t bb { static_cast<uint8_t>(b * 255.0f) };
		return 0xff000000u | (uint32_t)rr << 16 | (uint32_t)gg << 8
		    | (uint32_t)bb;
	} };

	auto randomize_grid { [&]() -> void {
		for (int y = 0; y < grid_height; ++y) {
			for (int x = 0; x < grid_width; ++x) {
				grid[y * grid_width + x] = alive_dist(rng) ? 1 : 0;
			}
		}
		std::fill(next.begin(), next.end(), 0);
		history.clear();
	} };

	auto hash_grid { [&]() -> uint64_t {
		uint64_t hash { 1469598103934665603ull };
		for (uint8_t cell : grid) {
			hash ^= static_cast<uint64_t>(cell);
			hash *= 1099511628211ull;
		}
		return hash;
	} };

	randomize_grid();

	auto *pool { wl_shm_create_pool(g_shm, fd, static_cast<int>(size)) };
	auto *buffer { wl_shm_pool_create_buffer(
		pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888) };
	auto *surface { wl_compositor_create_surface(g_compositor) };
	if (!pool || !buffer || !surface) {
		std::println(std::cerr, "Failed to create shm objects.");
		if (surface) {
			wl_surface_destroy(surface);
		}
		if (buffer) {
			wl_buffer_destroy(buffer);
		}
		if (pool) {
			wl_shm_pool_destroy(pool);
		}
		munmap(data, size);
		close(fd);
		wl_display_disconnect(display);
		return 1;
	}

	g_xdg_surface = xdg_wm_base_get_xdg_surface(g_xdg_wm_base, surface);
	if (!g_xdg_surface) {
		std::println(std::cerr, "Failed to create xdg_surface.");
		wl_surface_destroy(surface);
		wl_buffer_destroy(buffer);
		wl_shm_pool_destroy(pool);
		munmap(data, size);
		close(fd);
		wl_display_disconnect(display);
		return 1;
	}
	if (xdg_surface_add_listener(g_xdg_surface, &xdg_surface_listener, nullptr)
	    < 0) {
		std::println(std::cerr, "Failed to add xdg_surface listener.");
		xdg_surface_destroy(g_xdg_surface);
		wl_surface_destroy(surface);
		wl_buffer_destroy(buffer);
		wl_shm_pool_destroy(pool);
		munmap(data, size);
		close(fd);
		wl_display_disconnect(display);
		return 1;
	}

	g_xdg_toplevel = xdg_surface_get_toplevel(g_xdg_surface);
	if (g_xdg_toplevel) {
		xdg_toplevel_add_listener(
		    g_xdg_toplevel, &xdg_toplevel_listener, nullptr);
		xdg_toplevel_set_title(g_xdg_toplevel, "life");
		xdg_toplevel_set_app_id(g_xdg_toplevel, "lunar.life");
	}

	wl_surface_commit(surface);
	if (wl_display_roundtrip(display) < 0 || !g_configured) {
		std::println(std::cerr, "Failed to receive initial configure.");
		if (g_xdg_toplevel) {
			xdg_toplevel_destroy(g_xdg_toplevel);
		}
		xdg_surface_destroy(g_xdg_surface);
		wl_surface_destroy(surface);
		wl_buffer_destroy(buffer);
		wl_shm_pool_destroy(pool);
		munmap(data, size);
		close(fd);
		wl_display_disconnect(display);
		return 1;
	}

	wl_surface_attach(surface, buffer, 0, 0);
	wl_surface_commit(surface);
	wl_display_flush(display);

	auto last_tick { std::chrono::steady_clock::now() };
	while (g_running) {
		wl_display_dispatch_pending(display);

		auto now { std::chrono::steady_clock::now() };
		auto dt { now - last_tick };
		if (dt < std::chrono::milliseconds(16)) {
			std::this_thread::sleep_for(std::chrono::milliseconds(4));
			continue;
		}
		last_tick = now;

		for (int y = 0; y < grid_height; ++y) {
			for (int x = 0; x < grid_width; ++x) {
				int neighbors { 0 };
				for (int oy = -1; oy <= 1; ++oy) {
					for (int ox = -1; ox <= 1; ++ox) {
						if (ox == 0 && oy == 0) {
							continue;
						}
						int ny { (y + oy + grid_height) % grid_height };
						int nx { (x + ox + grid_width) % grid_width };
						neighbors += grid[ny * grid_width + nx];
					}
				}
				uint8_t alive { grid[y * grid_width + x] };
				if (alive) {
					next[y * grid_width + x]
					    = (neighbors == 2 || neighbors == 3) ? 1 : 0;
				} else {
					next[y * grid_width + x] = (neighbors == 3) ? 1 : 0;
				}
			}
		}

		grid.swap(next);
		std::fill(next.begin(), next.end(), 0);

		uint64_t current_hash { hash_grid() };
		bool repeating { std::find(history.begin(), history.end(), current_hash)
			!= history.end() };
		if (repeating) {
			randomize_grid();
			current_hash = hash_grid();
		}
		history.push_back(current_hash);
		if (history.size() > 5) {
			history.erase(history.begin());
		}

		hue += hue_step;
		if (hue >= 360.0f) {
			hue -= 360.0f;
		}
		uint32_t alive_color { hue_to_rgb(hue) };
		for (int y = 0; y < height; ++y) {
			int gy { y / cell_size };
			for (int x = 0; x < width; ++x) {
				int gx { x / cell_size };
				uint8_t alive { grid[gy * grid_width + gx] };
				if (alive) {
					data[y * width + x] = alive_color;
				} else {
					data[y * width + x] = 0x00000000u;
				}
			}
		}

		wl_surface_attach(surface, buffer, 0, 0);
		wl_surface_damage_buffer(surface, 0, 0, width, height);
		wl_surface_commit(surface);
		wl_display_flush(display);
	}

	if (g_xdg_toplevel) {
		xdg_toplevel_destroy(g_xdg_toplevel);
	}
	if (g_xdg_surface) {
		xdg_surface_destroy(g_xdg_surface);
	}
	wl_buffer_destroy(buffer);
	wl_shm_pool_destroy(pool);
	wl_surface_destroy(surface);
	munmap(data, size);
	close(fd);
	wl_display_disconnect(display);
}
