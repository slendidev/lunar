#include "DescriptorWriter.h"

namespace Lunar {

auto DescriptorWriter::write_image(int binding, VkImageView image_view,
    VkSampler sampler, VkImageLayout layout, VkDescriptorType type)
    -> DescriptorWriter &
{
	auto const &info = image_infos.emplace_back(VkDescriptorImageInfo {
	    .sampler = sampler,
	    .imageView = image_view,
	    .imageLayout = layout,
	});

	VkWriteDescriptorSet write {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.pNext = nullptr;
	write.dstBinding = binding;
	write.dstSet = VK_NULL_HANDLE;
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pImageInfo = &info;

	writes.emplace_back(write);

	return *this;
}

auto DescriptorWriter::write_buffer(int binding, VkBuffer buffer, size_t size,
    size_t offset, VkDescriptorType type) -> DescriptorWriter &
{
	auto const &info = buffer_infos.emplace_back(VkDescriptorBufferInfo {
	    .buffer = buffer,
	    .offset = offset,
	    .range = size,
	});

	VkWriteDescriptorSet write {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.pNext = nullptr;
	write.dstBinding = binding;
	write.dstSet = VK_NULL_HANDLE;
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pBufferInfo = &info;

	writes.emplace_back(write);

	return *this;
}

auto DescriptorWriter::clear() -> DescriptorWriter &
{
	image_infos.clear();
	writes.clear();
	buffer_infos.clear();

	return *this;
}

auto DescriptorWriter::update_set(VkDevice dev, VkDescriptorSet set) -> void
{
	for (auto &write : writes) {
		write.dstSet = set;
	}
	vkUpdateDescriptorSets(
	    dev, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

} // namespace Lunar
