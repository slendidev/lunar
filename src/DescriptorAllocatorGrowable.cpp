#include "DescriptorAllocatorGrowable.h"

#include <algorithm>

#include "Logger.h"
#include "Util.h"

namespace Lunar {

auto DescriptorAllocatorGrowable::init(VkDevice dev, uint32_t max_sets,
    std::span<PoolSizeRatio> pool_ratios) -> void
{
	m_ratios.clear();
	m_current_pool = VK_NULL_HANDLE;
	m_full_pools.clear();
	m_used_pools.clear();
	m_ready_pools.clear();

	m_ratios.insert(m_ratios.begin(), pool_ratios.begin(), pool_ratios.end());

	auto const new_pool = create_pool(dev, max_sets, pool_ratios);

	m_sets_per_pool = static_cast<uint32_t>(max_sets * 1.5);
	if (m_sets_per_pool > 4092)
		m_sets_per_pool = 4092;

	m_ready_pools.emplace_back(new_pool);
}

auto DescriptorAllocatorGrowable::clear_pools(VkDevice dev) -> void
{
	std::vector<VkDescriptorPool> all_pools;
	all_pools.reserve(
	    m_ready_pools.size() + m_used_pools.size() + m_full_pools.size());
	all_pools.insert(
	    all_pools.end(), m_ready_pools.begin(), m_ready_pools.end());
	all_pools.insert(all_pools.end(), m_used_pools.begin(), m_used_pools.end());
	all_pools.insert(all_pools.end(), m_full_pools.begin(), m_full_pools.end());

	std::sort(all_pools.begin(), all_pools.end());
	all_pools.erase(
	    std::unique(all_pools.begin(), all_pools.end()), all_pools.end());

	for (auto const p : all_pools) {
		vkResetDescriptorPool(dev, p, 0);
	}

	m_ready_pools = std::move(all_pools);
	m_used_pools.clear();
	m_full_pools.clear();
	m_current_pool = VK_NULL_HANDLE;
}

auto DescriptorAllocatorGrowable::destroy_pools(VkDevice dev) -> void
{
	std::vector<VkDescriptorPool> all_pools;
	all_pools.reserve(
	    m_ready_pools.size() + m_used_pools.size() + m_full_pools.size());
	all_pools.insert(
	    all_pools.end(), m_ready_pools.begin(), m_ready_pools.end());
	all_pools.insert(all_pools.end(), m_used_pools.begin(), m_used_pools.end());
	all_pools.insert(all_pools.end(), m_full_pools.begin(), m_full_pools.end());

	std::sort(all_pools.begin(), all_pools.end());
	all_pools.erase(
	    std::unique(all_pools.begin(), all_pools.end()), all_pools.end());

	for (auto const p : all_pools) {
		vkDestroyDescriptorPool(dev, p, nullptr);
	}

	m_ready_pools.clear();
	m_used_pools.clear();
	m_full_pools.clear();
	m_current_pool = VK_NULL_HANDLE;
}

auto DescriptorAllocatorGrowable::allocate(Logger &logger, VkDevice dev,
    VkDescriptorSetLayout layout, void *p_next) -> VkDescriptorSet
{
	auto pool_to_use = get_pool(dev);

	VkDescriptorSetAllocateInfo alloci {};
	alloci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloci.pNext = p_next;
	alloci.descriptorPool = pool_to_use;
	alloci.descriptorSetCount = 1;
	alloci.pSetLayouts = &layout;

	VkDescriptorSet ds;
	auto const res = vkAllocateDescriptorSets(dev, &alloci, &ds);
	if (res == VK_ERROR_OUT_OF_POOL_MEMORY || res == VK_ERROR_FRAGMENTED_POOL) {
		m_full_pools.emplace_back(pool_to_use);
		if (m_current_pool == pool_to_use) {
			m_current_pool = VK_NULL_HANDLE;
		}
		pool_to_use = get_pool(dev);
		alloci.descriptorPool = pool_to_use;
		VK_CHECK(logger, vkAllocateDescriptorSets(dev, &alloci, &ds));
	}

	return ds;
}

auto DescriptorAllocatorGrowable::get_pool(VkDevice dev) -> VkDescriptorPool
{
	if (m_current_pool != VK_NULL_HANDLE) {
		return m_current_pool;
	}

	if (!m_ready_pools.empty()) {
		m_current_pool = m_ready_pools.back();
		m_ready_pools.pop_back();
	} else {
		m_current_pool = create_pool(dev, m_sets_per_pool, m_ratios);

		m_sets_per_pool = static_cast<uint32_t>(m_sets_per_pool * 1.5);
		if (m_sets_per_pool > 4092)
			m_sets_per_pool = 4092;
	}

	m_used_pools.emplace_back(m_current_pool);
	return m_current_pool;
}

auto DescriptorAllocatorGrowable::create_pool(VkDevice dev, uint32_t set_count,
    std::span<PoolSizeRatio> pool_ratios) -> VkDescriptorPool
{
	std::vector<VkDescriptorPoolSize> pool_sizes;
	for (auto const ratio : pool_ratios) {
		pool_sizes.emplace_back(VkDescriptorPoolSize {
		    .type = ratio.type,
		    .descriptorCount = static_cast<uint32_t>(ratio.ratio * set_count),
		});
	}

	VkDescriptorPoolCreateInfo pool_ci {};
	pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_ci.flags = 0;
	pool_ci.maxSets = set_count;
	pool_ci.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
	pool_ci.pPoolSizes = pool_sizes.data();

	VkDescriptorPool new_pool;
	vkCreateDescriptorPool(dev, &pool_ci, nullptr, &new_pool);
	return new_pool;
}

} // namespace Lunar
