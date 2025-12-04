#pragma once

#include <vector>

#include <vulkan/vulkan_core.h>

#include "Logger.h"

namespace Lunar {

struct GraphicsPipelineBuilder {
	GraphicsPipelineBuilder(Logger &logger)
	    : m_logger(logger)
	{
		clear();
	}

	auto clear() -> GraphicsPipelineBuilder &;
	auto set_shaders(VkShaderModule vs, VkShaderModule fs)
	    -> GraphicsPipelineBuilder &;
	auto set_input_topology(VkPrimitiveTopology topology,
	    VkBool32 primitive_restart_enable = VK_FALSE)
	    -> GraphicsPipelineBuilder &;
	auto set_polygon_mode(VkPolygonMode mode) -> GraphicsPipelineBuilder &;
	auto set_cull_mode(VkCullModeFlags cull_mode, VkFrontFace front_face)
	    -> GraphicsPipelineBuilder &;
	auto set_multisampling_none() -> GraphicsPipelineBuilder &;
	auto disable_blending() -> GraphicsPipelineBuilder &;
	auto set_color_attachment_format(VkFormat format)
	    -> GraphicsPipelineBuilder &;
	auto set_depth_format(VkFormat format) -> GraphicsPipelineBuilder &;
	auto set_pipeline_layout(VkPipelineLayout layout)
	    -> GraphicsPipelineBuilder &;
	auto disable_depth_testing() -> GraphicsPipelineBuilder &;
	auto build(VkDevice dev) -> VkPipeline;

private:
	VkPipelineInputAssemblyStateCreateInfo m_input_assembly {};
	VkPipelineRasterizationStateCreateInfo m_rasterizer {};
	VkPipelineColorBlendAttachmentState m_color_blend_attachment {};
	VkPipelineMultisampleStateCreateInfo m_multisampling {};
	VkPipelineLayout m_pipeline_layout {};
	VkPipelineDepthStencilStateCreateInfo m_depth_stencil {};
	VkPipelineRenderingCreateInfo m_render_info {};
	VkFormat m_color_attachment_format {};

	std::vector<VkPipelineShaderStageCreateInfo> m_shader_stages {};

	Logger &m_logger;
};

} // namespace Lunar
