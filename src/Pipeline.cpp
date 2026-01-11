#include "Pipeline.h"

#include "Util.h"

namespace Lunar {

Pipeline::Builder::Builder(vk::Device device, Logger &logger)
    : m_device(device)
    , m_logger(logger)
{
}

auto Pipeline::Builder::set_descriptor_set_layouts(
    std::span<vk::DescriptorSetLayout const> layouts) -> Builder &
{
	m_set_layouts.assign(layouts.begin(), layouts.end());
	return *this;
}

auto Pipeline::Builder::set_push_constant_ranges(
    std::span<vk::PushConstantRange const> ranges) -> Builder &
{
	m_push_constant_ranges.assign(ranges.begin(), ranges.end());
	return *this;
}

auto Pipeline::Builder::build_compute(
    vk::PipelineShaderStageCreateInfo const &stage) -> Pipeline
{
	auto pipeline_layout { build_layout() };

	vk::ComputePipelineCreateInfo pipeline_ci {};
	pipeline_ci.layout = pipeline_layout.get();
	pipeline_ci.stage = stage;

	auto pipeline_ret { m_device.createComputePipelineUnique({}, pipeline_ci) };
	VK_CHECK(m_logger, pipeline_ret.result);

	return Pipeline {
		.pipeline = std::move(pipeline_ret.value),
		.layout = std::move(pipeline_layout),
	};
}

auto Pipeline::Builder::build_graphics(
    std::function<GraphicsPipelineBuilder &(GraphicsPipelineBuilder &)> const
        &configure) -> Pipeline
{
	auto pipeline_layout { build_layout() };

	auto builder { GraphicsPipelineBuilder { m_logger } };
	builder.set_pipeline_layout(
	    static_cast<VkPipelineLayout>(pipeline_layout.get()));
	configure(builder);

	auto pipeline_handle { builder.build(static_cast<VkDevice>(m_device)) };
	vk::UniquePipeline pipeline_unique(pipeline_handle,
	    vk::detail::ObjectDestroy<vk::Device,
	        VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>(m_device));

	return Pipeline {
		.pipeline = std::move(pipeline_unique),
		.layout = std::move(pipeline_layout),
	};
}

auto Pipeline::Builder::build_layout() -> vk::UniquePipelineLayout
{
	vk::PipelineLayoutCreateInfo layout_ci {};
	layout_ci.setLayoutCount = static_cast<uint32_t>(m_set_layouts.size());
	layout_ci.pSetLayouts = m_set_layouts.data();
	layout_ci.pushConstantRangeCount
	    = static_cast<uint32_t>(m_push_constant_ranges.size());
	layout_ci.pPushConstantRanges = m_push_constant_ranges.data();

	return m_device.createPipelineLayoutUnique(layout_ci);
}

} // namespace Lunar
