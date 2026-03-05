#include "AllocTracker.h"

#include "AllocTracker.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <thread>

#include "Logger.h"

namespace Lunar {
auto log_top_allocators(::Logger &logger, std::size_t max_entries) -> void;
}

namespace {

constexpr std::uintptr_t k_empty = 0;
constexpr std::uintptr_t k_tombstone = 1;
constexpr std::size_t k_ptr_table_size = 1 << 18;
constexpr std::size_t k_site_table_size = 1 << 14;

struct PtrEntry {
	std::uintptr_t ptr { k_empty };
	std::size_t size { 0 };
	std::uintptr_t site { 0 };
};

struct SiteEntry {
	std::uintptr_t site { 0 };
	std::size_t live_bytes { 0 };
	std::size_t alloc_bytes { 0 };
	std::size_t alloc_count { 0 };
	std::size_t free_count { 0 };
};

static PtrEntry g_ptr_table[k_ptr_table_size];
static SiteEntry g_site_table[k_site_table_size];
static std::atomic_flag g_lock = ATOMIC_FLAG_INIT;
static thread_local bool g_tracking_disabled = false;

auto tracker_enabled() -> bool
{
	static bool enabled = std::getenv("LUNAR_ALLOC_TRACKER") != nullptr;
	return enabled;
}

auto lock_table() -> void
{
	while (g_lock.test_and_set(std::memory_order_acquire)) {
		std::this_thread::yield();
	}
}

auto unlock_table() -> void { g_lock.clear(std::memory_order_release); }

auto hash_ptr(std::uintptr_t key) -> std::size_t
{
	return (key >> 4) ^ (key >> 9);
}

auto find_ptr_slot(std::uintptr_t ptr, bool for_insert) -> std::size_t
{
	std::size_t index = hash_ptr(ptr) & (k_ptr_table_size - 1);
	std::size_t first_tombstone = k_ptr_table_size;
	for (std::size_t probe = 0; probe < k_ptr_table_size; ++probe) {
		auto &entry = g_ptr_table[index];
		if (entry.ptr == ptr) {
			return index;
		}
		if (entry.ptr == k_empty) {
			return for_insert && first_tombstone != k_ptr_table_size
			    ? first_tombstone
			    : index;
		}
		if (entry.ptr == k_tombstone && first_tombstone == k_ptr_table_size) {
			first_tombstone = index;
		}
		index = (index + 1) & (k_ptr_table_size - 1);
	}
	return k_ptr_table_size;
}

auto find_site_slot(std::uintptr_t site, bool for_insert) -> std::size_t
{
	std::size_t index = hash_ptr(site) & (k_site_table_size - 1);
	for (std::size_t probe = 0; probe < k_site_table_size; ++probe) {
		auto &entry = g_site_table[index];
		if (entry.site == site) {
			return index;
		}
		if (entry.site == 0) {
			return for_insert ? index : k_site_table_size;
		}
		index = (index + 1) & (k_site_table_size - 1);
	}
	return k_site_table_size;
}

auto track_alloc(void *ptr, std::size_t size, std::uintptr_t site) -> void
{
	if (!ptr || !tracker_enabled() || g_tracking_disabled) {
		return;
	}

	lock_table();
	auto slot = find_ptr_slot(reinterpret_cast<std::uintptr_t>(ptr), true);
	if (slot < k_ptr_table_size) {
		g_ptr_table[slot].ptr = reinterpret_cast<std::uintptr_t>(ptr);
		g_ptr_table[slot].size = size;
		g_ptr_table[slot].site = site;
	}

	auto site_slot = find_site_slot(site, true);
	if (site_slot < k_site_table_size) {
		auto &entry = g_site_table[site_slot];
		if (entry.site == 0) {
			entry.site = site;
		}
		entry.live_bytes += size;
		entry.alloc_bytes += size;
		entry.alloc_count += 1;
	}
	unlock_table();
}

auto track_free(void *ptr) -> void
{
	if (!ptr || !tracker_enabled() || g_tracking_disabled) {
		return;
	}

	lock_table();
	auto slot = find_ptr_slot(reinterpret_cast<std::uintptr_t>(ptr), false);
	if (slot < k_ptr_table_size
	    && g_ptr_table[slot].ptr == reinterpret_cast<std::uintptr_t>(ptr)) {
		auto size = g_ptr_table[slot].size;
		auto site = g_ptr_table[slot].site;
		g_ptr_table[slot].ptr = k_tombstone;
		g_ptr_table[slot].size = 0;
		g_ptr_table[slot].site = 0;

		auto site_slot = find_site_slot(site, false);
		if (site_slot < k_site_table_size) {
			auto &entry = g_site_table[site_slot];
			if (entry.live_bytes >= size) {
				entry.live_bytes -= size;
			} else {
				entry.live_bytes = 0;
			}
			entry.free_count += 1;
		}
	}
	unlock_table();
}

} // namespace

void *operator new(std::size_t size)
{
	void *ptr = std::malloc(size);
	if (!ptr) {
		throw std::bad_alloc();
	}
	track_alloc(ptr, size,
	    reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)));
	return ptr;
}

void *operator new[](std::size_t size)
{
	void *ptr = std::malloc(size);
	if (!ptr) {
		throw std::bad_alloc();
	}
	track_alloc(ptr, size,
	    reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)));
	return ptr;
}

void operator delete(void *ptr) noexcept
{
	track_free(ptr);
	std::free(ptr);
}

void operator delete[](void *ptr) noexcept
{
	track_free(ptr);
	std::free(ptr);
}

void operator delete(void *ptr, std::size_t) noexcept
{
	track_free(ptr);
	std::free(ptr);
}

void operator delete[](void *ptr, std::size_t) noexcept
{
	track_free(ptr);
	std::free(ptr);
}

void *operator new(std::size_t size, std::nothrow_t const &) noexcept
{
	void *ptr = std::malloc(size);
	if (!ptr) {
		return nullptr;
	}
	track_alloc(ptr, size,
	    reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)));
	return ptr;
}

void *operator new[](std::size_t size, std::nothrow_t const &) noexcept
{
	void *ptr = std::malloc(size);
	if (!ptr) {
		return nullptr;
	}
	track_alloc(ptr, size,
	    reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)));
	return ptr;
}

void operator delete(void *ptr, std::nothrow_t const &) noexcept
{
	track_free(ptr);
	std::free(ptr);
}

void operator delete[](void *ptr, std::nothrow_t const &) noexcept
{
	track_free(ptr);
	std::free(ptr);
}

namespace Lunar {

auto alloc_tracker_enabled() -> bool { return tracker_enabled(); }

auto log_top_allocators(::Logger &logger, std::size_t max_entries) -> void
{
	if (!tracker_enabled()) {
		return;
	}

	struct Snapshot {
		std::uintptr_t site;
		std::size_t live_bytes;
		std::size_t alloc_bytes;
		std::size_t alloc_count;
		std::size_t free_count;
	};
	constexpr std::size_t max_slots = 64;
	Snapshot top[max_slots] {};
	std::size_t count = 0;

	g_tracking_disabled = true;
	lock_table();
	for (auto const &entry : g_site_table) {
		if (entry.site == 0 || entry.live_bytes == 0) {
			continue;
		}
		std::size_t insert_at = count;
		if (insert_at < max_entries && insert_at < max_slots) {
			count++;
		} else {
			insert_at
			    = max_entries < max_slots ? max_entries - 1 : max_slots - 1;
		}
		if (count > max_slots) {
			count = max_slots;
		}
		for (std::size_t i = 0; i < count; ++i) {
			if (entry.live_bytes > top[i].live_bytes) {
				insert_at = i;
				break;
			}
		}
		for (std::size_t i = count; i > insert_at + 1; --i) {
			top[i - 1] = top[i - 2];
		}
		if (insert_at < count) {
			top[insert_at] = Snapshot {
				entry.site,
				entry.live_bytes,
				entry.alloc_bytes,
				entry.alloc_count,
				entry.free_count,
			};
		}
	}
	unlock_table();

	std::size_t limit = std::min(count, max_entries);
	for (std::size_t i = 0; i < limit; ++i) {
		auto const &entry = top[i];
		logger.info("AllocTop[{}]: site=0x{:x} live={} KB allocs={} frees={} "
		            "total={} KB",
		    i, entry.site, entry.live_bytes / 1024, entry.alloc_count,
		    entry.free_count, entry.alloc_bytes / 1024);
	}
	g_tracking_disabled = false;
}

} // namespace Lunar
