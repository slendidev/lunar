#include "DescriptorAllocator.h"

#include <vector>

#include "Util.h"

namespace Lunar {

auto DescriptorAllocator::init_pool(VkDevice dev, uint32_t max_sets,
    std::span<PoolSizeRatio> pool_ratios) -> void
{
	std::vector<VkDescriptorPoolSize> pool_sizes;
	for (auto const &ratio : pool_ratios) {
		pool_sizes.emplace_back(VkDescriptorPoolSize {
		    .type = ratio.type,
		    .descriptorCount = static_cast<uint32_t>(ratio.ratio * max_sets),
		});
	}

	VkDescriptorPoolCreateInfo ci {};
	ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	ci.pNext = nullptr;
	ci.flags = 0;
	ci.maxSets = max_sets;
	ci.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
	ci.pPoolSizes = pool_sizes.data();

	vkCreateDescriptorPool(dev, &ci, nullptr, &pool);
}

auto DescriptorAllocator::clear_descriptors(VkDevice dev) -> void
{
	vkResetDescriptorPool(dev, pool, 0);
}

auto DescriptorAllocator::destroy_pool(VkDevice dev) -> void
{
	vkDestroyDescriptorPool(dev, pool, nullptr);
}

auto DescriptorAllocator::allocate(Logger &logger, VkDevice dev,
    VkDescriptorSetLayout layout) -> VkDescriptorSet
{
	VkDescriptorSetAllocateInfo ai {};
	ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	ai.pNext = nullptr;
	ai.descriptorPool = pool;
	ai.descriptorSetCount = 1;
	ai.pSetLayouts = &layout;

	VkDescriptorSet ds;
	VK_CHECK(logger, vkAllocateDescriptorSets(dev, &ai, &ds));

	return ds;
}

} // namespace Lunar
