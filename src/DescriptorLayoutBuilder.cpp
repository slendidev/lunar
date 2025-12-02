#include "DescriptorLayoutBuilder.h"

#include "Util.h"

namespace Lunar {

auto DescriptorLayoutBuilder::add_binding(
    uint32_t binding, VkDescriptorType type) -> DescriptorLayoutBuilder &
{
	VkDescriptorSetLayoutBinding b {};
	b.binding = binding;
	b.descriptorCount = 1;
	b.descriptorType = type;

	bindings.emplace_back(b);

	return *this;
}

auto DescriptorLayoutBuilder::build(Logger &logger, VkDevice dev,
    VkShaderStageFlags shader_stages, void *pNext,
    VkDescriptorSetLayoutCreateFlags flags) -> VkDescriptorSetLayout
{
	for (auto &&b : bindings) {
		b.stageFlags |= shader_stages;
	}

	VkDescriptorSetLayoutCreateInfo ci {};
	ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	ci.pNext = pNext;

	ci.pBindings = bindings.data();
	ci.bindingCount = static_cast<uint32_t>(bindings.size());
	ci.flags = flags;

	VkDescriptorSetLayout set;
	VK_CHECK(logger, vkCreateDescriptorSetLayout(dev, &ci, nullptr, &set));

	return set;
}

} // namespace Lunar
