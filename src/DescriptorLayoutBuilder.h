#pragma once

#include <vector>

#include <vulkan/vulkan_core.h>

#include "Logger.h"

namespace Lunar {

struct DescriptorLayoutBuilder {
	std::vector<VkDescriptorSetLayoutBinding> bindings;

	auto add_binding(uint32_t binding, VkDescriptorType type)
	    -> DescriptorLayoutBuilder &;
	auto clear() -> void { bindings.clear(); }
	auto build(Logger &logger, VkDevice dev, VkShaderStageFlags shader_stages,
	    void *pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0)
	    -> VkDescriptorSetLayout;
};

} // namespace Lunar
