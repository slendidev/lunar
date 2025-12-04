#include "GraphicsPipelineBuilder.h"

#include <vulkan/vulkan_core.h>

#include "Util.h"

namespace Lunar {

auto GraphicsPipelineBuilder::clear() -> GraphicsPipelineBuilder &
{
	m_input_assembly = {};
	m_input_assembly.sType
	    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;

	m_rasterizer = {};
	m_rasterizer.sType
	    = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

	m_color_blend_attachment = {};

	m_multisampling = {};
	m_multisampling.sType
	    = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;

	m_pipeline_layout = {};

	m_depth_stencil = {};
	m_depth_stencil.sType
	    = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

	m_render_info = {};
	m_render_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;

	m_shader_stages.clear();

	return *this;
}

auto GraphicsPipelineBuilder::set_shaders(VkShaderModule vs, VkShaderModule fs)
    -> GraphicsPipelineBuilder &
{
	m_shader_stages.clear();

	m_shader_stages.emplace_back(
	    vkinit::pipeline_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, vs));
	m_shader_stages.emplace_back(
	    vkinit::pipeline_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, fs));

	return *this;
}

auto GraphicsPipelineBuilder::set_input_topology(VkPrimitiveTopology topology,
    VkBool32 primitive_restart_enable) -> GraphicsPipelineBuilder &
{
	m_input_assembly.topology = topology;
	m_input_assembly.primitiveRestartEnable = primitive_restart_enable;

	return *this;
}

auto GraphicsPipelineBuilder::set_polygon_mode(VkPolygonMode mode)
    -> GraphicsPipelineBuilder &
{
	m_rasterizer.polygonMode = mode;
	m_rasterizer.lineWidth = 1.0f;

	return *this;
}

auto GraphicsPipelineBuilder::set_cull_mode(VkCullModeFlags cull_mode,
    VkFrontFace front_face) -> GraphicsPipelineBuilder &
{
	m_rasterizer.cullMode = cull_mode;
	m_rasterizer.frontFace = front_face;

	return *this;
}

auto GraphicsPipelineBuilder::set_multisampling_none()
    -> GraphicsPipelineBuilder &
{
	m_multisampling.sampleShadingEnable = VK_FALSE;
	m_multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	m_multisampling.minSampleShading = 1.0f;
	m_multisampling.pSampleMask = nullptr;
	m_multisampling.alphaToCoverageEnable = VK_FALSE;
	m_multisampling.alphaToOneEnable = VK_FALSE;

	return *this;
}

auto GraphicsPipelineBuilder::disable_blending() -> GraphicsPipelineBuilder &
{
	m_color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT
	    | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
	    | VK_COLOR_COMPONENT_A_BIT;
	m_color_blend_attachment.blendEnable = VK_FALSE;

	return *this;
}

auto GraphicsPipelineBuilder::set_color_attachment_format(VkFormat format)
    -> GraphicsPipelineBuilder &
{
	m_color_attachment_format = format;

	m_render_info.colorAttachmentCount = 1;
	m_render_info.pColorAttachmentFormats = &m_color_attachment_format;

	return *this;
}

auto GraphicsPipelineBuilder::set_depth_format(VkFormat format)
    -> GraphicsPipelineBuilder &
{
	m_render_info.depthAttachmentFormat = format;

	return *this;
}

auto GraphicsPipelineBuilder::set_pipeline_layout(VkPipelineLayout layout)
    -> GraphicsPipelineBuilder &
{
	m_pipeline_layout = layout;

	return *this;
}

auto GraphicsPipelineBuilder::disable_depth_testing()
    -> GraphicsPipelineBuilder &
{
	m_depth_stencil.depthTestEnable = VK_FALSE;
	m_depth_stencil.depthWriteEnable = VK_FALSE;
	m_depth_stencil.depthCompareOp = VK_COMPARE_OP_NEVER;
	m_depth_stencil.depthBoundsTestEnable = VK_FALSE;
	m_depth_stencil.stencilTestEnable = VK_FALSE;
	m_depth_stencil.front = {};
	m_depth_stencil.back = {};
	m_depth_stencil.minDepthBounds = 0.f;
	m_depth_stencil.maxDepthBounds = 1.f;

	return *this;
}

auto GraphicsPipelineBuilder::build(VkDevice dev) -> VkPipeline
{
	VkPipelineViewportStateCreateInfo viewport_state_ci {};
	viewport_state_ci.sType
	    = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state_ci.pNext = nullptr;

	viewport_state_ci.viewportCount = 1;
	viewport_state_ci.scissorCount = 1;

	VkPipelineColorBlendStateCreateInfo color_blending_ci {};
	color_blending_ci.sType
	    = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blending_ci.pNext = nullptr;

	color_blending_ci.logicOpEnable = VK_FALSE;
	color_blending_ci.logicOp = VK_LOGIC_OP_COPY;
	color_blending_ci.attachmentCount = 1;
	color_blending_ci.pAttachments = &m_color_blend_attachment;

	VkPipelineVertexInputStateCreateInfo vertex_input_ci {};
	vertex_input_ci.sType
	    = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	VkGraphicsPipelineCreateInfo pipeline_ci {};
	pipeline_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_ci.pNext = &m_render_info;

	pipeline_ci.stageCount = static_cast<uint32_t>(m_shader_stages.size());
	pipeline_ci.pStages = m_shader_stages.data();
	pipeline_ci.pVertexInputState = &vertex_input_ci;
	pipeline_ci.pInputAssemblyState = &m_input_assembly;
	pipeline_ci.pViewportState = &viewport_state_ci;
	pipeline_ci.pRasterizationState = &m_rasterizer;
	pipeline_ci.pMultisampleState = &m_multisampling;
	pipeline_ci.pColorBlendState = &color_blending_ci;
	pipeline_ci.pDepthStencilState = &m_depth_stencil;
	pipeline_ci.layout = m_pipeline_layout;

	std::array<VkDynamicState, 2> state {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};

	VkPipelineDynamicStateCreateInfo dynamic_ci {};
	dynamic_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_ci.pDynamicStates = state.data();
	dynamic_ci.dynamicStateCount = state.size();

	pipeline_ci.pDynamicState = &dynamic_ci;

	VkPipeline pip {};
	if (vkCreateGraphicsPipelines(
	        dev, VK_NULL_HANDLE, 1, &pipeline_ci, nullptr, &pip)
	    != VK_SUCCESS) {
		m_logger.err("Failed to create graphics pipeline");
		return VK_NULL_HANDLE;
	}

	return pip;
}

} // namespace Lunar
