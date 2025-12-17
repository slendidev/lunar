#pragma once

#include <span>
#include <vector>

#include <vulkan/vulkan_core.h>

#include "Logger.h"

namespace Lunar {

struct DescriptorAllocatorGrowable {
	struct PoolSizeRatio {
		VkDescriptorType type;
		float ratio;
	};

	auto init(VkDevice dev, uint32_t max_sets,
	    std::span<PoolSizeRatio> pool_ratios) -> void;

	auto clear_pools(VkDevice dev) -> void;
	auto destroy_pools(VkDevice dev) -> void;

	auto allocate(Logger &logger, VkDevice dev, VkDescriptorSetLayout layout,
	    void *p_next = nullptr) -> VkDescriptorSet;

private:
	auto get_pool(VkDevice dev) -> VkDescriptorPool;
	auto create_pool(VkDevice dev, uint32_t set_count,
	    std::span<PoolSizeRatio> pool_ratios) -> VkDescriptorPool;

	std::vector<PoolSizeRatio> m_ratios;
	VkDescriptorPool m_current_pool { VK_NULL_HANDLE };
	std::vector<VkDescriptorPool> m_full_pools;
	std::vector<VkDescriptorPool> m_used_pools;
	std::vector<VkDescriptorPool> m_ready_pools;
	uint32_t m_sets_per_pool;
};

} // namespace Lunar
