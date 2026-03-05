#pragma once

#include <cstddef>

struct Logger;

namespace Lunar {

auto alloc_tracker_enabled() -> bool;
auto log_top_allocators(::Logger &logger, std::size_t max_entries = 10) -> void;

} // namespace Lunar
