#pragma once

#include <deque>
#include <functional>

namespace Lunar {

struct DeletionQueue {
	std::deque<std::function<void()>> deletors;

	auto emplace(std::function<void()> &&fn) -> void
	{
		deletors.emplace_back(fn);
	}

	auto flush() -> void
	{
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
			(*it)();
		}
		deletors.clear();
	}
};

} // namespace Lunar
