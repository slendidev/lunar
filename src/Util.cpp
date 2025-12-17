#include "Util.h"

#include <span>

namespace vkutil {

auto transition_image(vk::CommandBuffer cmd, vk::Image image,
    vk::ImageLayout current_layout, vk::ImageLayout new_layout) -> void
{
	auto aspect_mask = (new_layout == vk::ImageLayout::eDepthAttachmentOptimal)
	    ? vk::ImageAspectFlagBits::eDepth
	    : vk::ImageAspectFlagBits::eColor;

	vk::ImageMemoryBarrier image_barrier {};
	image_barrier.srcAccessMask = vk::AccessFlagBits::eMemoryWrite;
	image_barrier.dstAccessMask
	    = vk::AccessFlagBits::eMemoryWrite | vk::AccessFlagBits::eMemoryRead;
	image_barrier.oldLayout = current_layout;
	image_barrier.newLayout = new_layout;
	image_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	image_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	image_barrier.image = image;
	image_barrier.subresourceRange.aspectMask = aspect_mask;
	image_barrier.subresourceRange.baseMipLevel = 0;
	image_barrier.subresourceRange.levelCount = 1;
	image_barrier.subresourceRange.baseArrayLayer = 0;
	image_barrier.subresourceRange.layerCount = 1;

	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
	    vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, image_barrier);
}

auto copy_image_to_image(vk::CommandBuffer cmd, vk::Image source,
    vk::Image destination, vk::Extent2D src_size, vk::Extent2D dst_size) -> void
{
	vk::ImageBlit2 blit_region {};
	blit_region.srcOffsets[0] = vk::Offset3D { 0, 0, 0 };
	blit_region.srcOffsets[1]
	    = vk::Offset3D { static_cast<int32_t>(src_size.width),
		      static_cast<int32_t>(src_size.height), 1 };

	blit_region.dstOffsets[0] = vk::Offset3D { 0, 0, 0 };
	blit_region.dstOffsets[1]
	    = vk::Offset3D { static_cast<int32_t>(dst_size.width),
		      static_cast<int32_t>(dst_size.height), 1 };

	blit_region.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
	blit_region.srcSubresource.baseArrayLayer = 0;
	blit_region.srcSubresource.layerCount = 1;
	blit_region.srcSubresource.mipLevel = 0;

	blit_region.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
	blit_region.dstSubresource.baseArrayLayer = 0;
	blit_region.dstSubresource.layerCount = 1;
	blit_region.dstSubresource.mipLevel = 0;

	vk::BlitImageInfo2 blit_info {};
	blit_info.dstImage = destination;
	blit_info.dstImageLayout = vk::ImageLayout::eTransferDstOptimal;
	blit_info.srcImage = source;
	blit_info.srcImageLayout = vk::ImageLayout::eTransferSrcOptimal;
	blit_info.filter = vk::Filter::eLinear;
	blit_info.regionCount = 1;
	blit_info.pRegions = &blit_region;

	cmd.blitImage2(blit_info);
}

auto load_shader_module(std::span<uint8_t> spirv_data, vk::Device device)
    -> vk::UniqueShaderModule
{
	if (!device || spirv_data.empty() || (spirv_data.size() % 4) != 0) {
		return {};
	}

	vk::ShaderModuleCreateInfo create_info {};
	create_info.codeSize = spirv_data.size();
	create_info.pCode = reinterpret_cast<uint32_t const *>(spirv_data.data());

	try {
		return device.createShaderModuleUnique(create_info);
	} catch (vk::SystemError const &) {
		return {};
	}
}

} // namespace vkutil

namespace vkinit {

auto image_create_info(vk::Format format, vk::ImageUsageFlags usage_flags,
    vk::Extent3D extent, vk::SampleCountFlagBits samples) -> vk::ImageCreateInfo
{
	vk::ImageCreateInfo info {};
	info.imageType = vk::ImageType::e2D;
	info.format = format;
	info.extent = extent;
	info.mipLevels = 1;
	info.arrayLayers = 1;
	info.samples = samples;
	info.tiling = vk::ImageTiling::eOptimal;
	info.usage = usage_flags;
	return info;
}

auto imageview_create_info(vk::Format format, vk::Image image,
    vk::ImageAspectFlags aspect_flags) -> vk::ImageViewCreateInfo
{
	vk::ImageViewCreateInfo info {};
	info.viewType = vk::ImageViewType::e2D;
	info.image = image;
	info.format = format;
	info.subresourceRange.baseMipLevel = 0;
	info.subresourceRange.levelCount = 1;
	info.subresourceRange.baseArrayLayer = 0;
	info.subresourceRange.layerCount = 1;
	info.subresourceRange.aspectMask = aspect_flags;
	return info;
}

auto command_buffer_submit_info(vk::CommandBuffer cmd)
    -> vk::CommandBufferSubmitInfo
{
	vk::CommandBufferSubmitInfo info {};
	info.commandBuffer = cmd;
	info.deviceMask = 0;
	return info;
}

auto semaphore_submit_info(vk::PipelineStageFlags2 stage_mask,
    vk::Semaphore semaphore) -> vk::SemaphoreSubmitInfo
{
	vk::SemaphoreSubmitInfo info {};
	info.semaphore = semaphore;
	info.value = 0;
	info.stageMask = stage_mask;
	info.deviceIndex = 0;
	return info;
}

auto submit_info2(vk::CommandBufferSubmitInfo *cmd_info,
    vk::SemaphoreSubmitInfo *wait_semaphore_info,
    vk::SemaphoreSubmitInfo *signal_semaphore_info) -> vk::SubmitInfo2
{
	vk::SubmitInfo2 info {};
	info.waitSemaphoreInfoCount = wait_semaphore_info ? 1u : 0u;
	info.pWaitSemaphoreInfos = wait_semaphore_info;
	info.commandBufferInfoCount = cmd_info ? 1u : 0u;
	info.pCommandBufferInfos = cmd_info;
	info.signalSemaphoreInfoCount = signal_semaphore_info ? 1u : 0u;
	info.pSignalSemaphoreInfos = signal_semaphore_info;
	return info;
}

auto attachment_info(vk::ImageView view, vk::ClearValue *clear,
    vk::ImageLayout layout) -> vk::RenderingAttachmentInfo
{
	vk::RenderingAttachmentInfo color_at {};
	color_at.imageView = view;
	color_at.imageLayout = layout;
	color_at.loadOp
	    = clear ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad;
	color_at.storeOp = vk::AttachmentStoreOp::eStore;
	if (clear) {
		color_at.clearValue = *clear;
	}

	return color_at;
}

auto pipeline_shader_stage(vk::ShaderStageFlagBits stage,
    vk::ShaderModule module) -> vk::PipelineShaderStageCreateInfo
{
	vk::PipelineShaderStageCreateInfo stage_ci {};
	stage_ci.stage = stage;
	stage_ci.module = module;
	stage_ci.pName = "main";
	return stage_ci;
}

auto render_info(vk::Extent2D extent,
    vk::RenderingAttachmentInfo const *color_att,
    vk::RenderingAttachmentInfo const *depth_att) -> vk::RenderingInfo
{
	vk::RenderingInfo render_info {};
	render_info.renderArea = vk::Rect2D { {}, extent };
	render_info.layerCount = 1;
	render_info.colorAttachmentCount = color_att ? 1 : 0;
	render_info.pColorAttachments = color_att;
	render_info.pDepthAttachment = depth_att;
	return render_info;
}

auto depth_attachment_info(vk::ImageView view, vk::ImageLayout layout)
    -> vk::RenderingAttachmentInfo
{
	vk::RenderingAttachmentInfo depth_att {};
	depth_att.imageView = view;
	depth_att.imageLayout = layout;
	depth_att.loadOp = vk::AttachmentLoadOp::eClear;
	depth_att.storeOp = vk::AttachmentStoreOp::eStore;
	depth_att.clearValue.depthStencil.depth = 1.f;
	return depth_att;
}

} // namespace vkinit
