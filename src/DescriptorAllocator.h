#pragma once

#include <span>

#include <vulkan/vulkan_core.h>

#include "Logger.h"

namespace Lunar {

struct DescriptorAllocator {
	struct PoolSizeRatio {
		VkDescriptorType type;
		float ratio;
	};

	VkDescriptorPool pool;

	auto init_pool(VkDevice dev, uint32_t max_sets,
	    std::span<PoolSizeRatio> pool_ratios) -> void;
	auto clear_descriptors(VkDevice dev) -> void;
	auto destroy_pool(VkDevice dev) -> void;

	auto allocate(Logger &logger, VkDevice dev, VkDescriptorSetLayout layout)
	    -> VkDescriptorSet;
};

} // namespace Lunar
