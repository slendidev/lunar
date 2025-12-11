#pragma once

#include <deque>
#include <vector>

#include <vulkan/vulkan_core.h>

namespace Lunar {

struct DescriptorWriter {
	std::deque<VkDescriptorImageInfo> image_infos;
	std::deque<VkDescriptorBufferInfo> buffer_infos;
	std::vector<VkWriteDescriptorSet> writes;

	auto write_image(int binding, VkImageView image_view, VkSampler sampler,
	    VkImageLayout layout, VkDescriptorType type) -> DescriptorWriter &;
	auto write_buffer(int binding, VkBuffer buffer, size_t size, size_t offset,
	    VkDescriptorType type) -> DescriptorWriter &;

	auto clear() -> DescriptorWriter &;
	auto update_set(VkDevice dev, VkDescriptorSet set) -> void;
};

} // namespace Lunar
