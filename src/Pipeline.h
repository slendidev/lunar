#pragma once

#include <functional>
#include <span>
#include <vector>

#include <vulkan/vulkan.hpp>

#include "GraphicsPipelineBuilder.h"
#include "Logger.h"

namespace Lunar {

struct Pipeline {
	vk::UniquePipeline pipeline {};
	vk::UniquePipelineLayout layout {};

	auto get() const -> vk::Pipeline { return pipeline.get(); }
	auto get_layout() const -> vk::PipelineLayout { return layout.get(); }
	auto reset() -> void
	{
		pipeline.reset();
		layout.reset();
	}

	struct Builder {
		Builder(vk::Device device, Logger &logger);

		auto set_descriptor_set_layouts(
		    std::span<vk::DescriptorSetLayout const> layouts) -> Builder &;
		auto set_push_constant_ranges(
		    std::span<vk::PushConstantRange const> ranges) -> Builder &;

		auto build_compute(vk::PipelineShaderStageCreateInfo const &stage)
		    -> Pipeline;
		auto build_graphics(std::function<GraphicsPipelineBuilder &(
		        GraphicsPipelineBuilder &)> const &configure) -> Pipeline;

	private:
		auto build_layout() -> vk::UniquePipelineLayout;

		vk::Device m_device {};
		Logger &m_logger;
		std::vector<vk::DescriptorSetLayout> m_set_layouts {};
		std::vector<vk::PushConstantRange> m_push_constant_ranges {};
	};
};

} // namespace Lunar
