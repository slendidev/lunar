#include "Util.h"

#include <span>

namespace vkutil {

auto transition_image(VkCommandBuffer cmd, VkImage image,
    VkImageLayout current_layout, VkImageLayout new_layout) -> void
{
	VkImageAspectFlags aspect_mask
	    = (new_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
	    ? VK_IMAGE_ASPECT_DEPTH_BIT
	    : VK_IMAGE_ASPECT_COLOR_BIT;

	VkImageMemoryBarrier image_barrier {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.pNext = nullptr,

		.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
		.dstAccessMask
		= VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
		.oldLayout = current_layout,
		.newLayout = new_layout,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = image,
		.subresourceRange = {
			.aspectMask = aspect_mask,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
	    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1,
	    &image_barrier);
}

auto copy_image_to_image(VkCommandBuffer cmd, VkImage source,
    VkImage destination, VkExtent2D src_size, VkExtent2D dst_size) -> void
{
	VkImageBlit2 blit_region {};
	blit_region.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
	blit_region.pNext = nullptr;

	blit_region.srcOffsets[0] = { 0, 0, 0 };
	blit_region.srcOffsets[1] = { static_cast<int32_t>(src_size.width),
		static_cast<int32_t>(src_size.height), 1 };

	blit_region.dstOffsets[0] = { 0, 0, 0 };
	blit_region.dstOffsets[1] = { static_cast<int32_t>(dst_size.width),
		static_cast<int32_t>(dst_size.height), 1 };

	blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit_region.srcSubresource.baseArrayLayer = 0;
	blit_region.srcSubresource.layerCount = 1;
	blit_region.srcSubresource.mipLevel = 0;

	blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit_region.dstSubresource.baseArrayLayer = 0;
	blit_region.dstSubresource.layerCount = 1;
	blit_region.dstSubresource.mipLevel = 0;

	VkBlitImageInfo2 blit_info {};
	blit_info.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
	blit_info.pNext = nullptr;
	blit_info.dstImage = destination;
	blit_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	blit_info.srcImage = source;
	blit_info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	blit_info.filter = VK_FILTER_LINEAR;
	blit_info.regionCount = 1;
	blit_info.pRegions = &blit_region;

	vkCmdBlitImage2(cmd, &blit_info);
}

auto load_shader_module(std::span<uint8_t> spirv_data, VkDevice device,
    VkShaderModule *out_shader_module) -> bool
{
	if (!device || !out_shader_module)
		return false;

	if (spirv_data.empty() || (spirv_data.size() % 4) != 0)
		return false;

	VkShaderModuleCreateInfo create_info {};
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.pNext = nullptr;
	create_info.flags = 0;
	create_info.codeSize = spirv_data.size();
	create_info.pCode = reinterpret_cast<uint32_t const *>(spirv_data.data());

	VkResult const res = vkCreateShaderModule(
	    device, &create_info, nullptr, out_shader_module);
	if (res != VK_SUCCESS) {
		*out_shader_module = VK_NULL_HANDLE;
		return false;
	}

	return true;
}

} // namespace vkutil

namespace vkinit {

auto image_create_info(VkFormat format, VkImageUsageFlags usage_flags,
    VkExtent3D extent) -> VkImageCreateInfo
{
	VkImageCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.pNext = nullptr;

	info.imageType = VK_IMAGE_TYPE_2D;

	info.format = format;
	info.extent = extent;

	info.mipLevels = 1;
	info.arrayLayers = 1;

	info.samples = VK_SAMPLE_COUNT_1_BIT;

	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.usage = usage_flags;

	return info;
}

auto imageview_create_info(VkFormat format, VkImage image,
    VkImageAspectFlags aspect_flags) -> VkImageViewCreateInfo
{
	VkImageViewCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	info.pNext = nullptr;

	info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	info.image = image;
	info.format = format;
	info.subresourceRange.baseMipLevel = 0;
	info.subresourceRange.levelCount = 1;
	info.subresourceRange.baseArrayLayer = 0;
	info.subresourceRange.layerCount = 1;
	info.subresourceRange.aspectMask = aspect_flags;

	return info;
}

auto command_buffer_submit_info(VkCommandBuffer cmd)
    -> VkCommandBufferSubmitInfo
{
	VkCommandBufferSubmitInfo info {};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	info.pNext = nullptr;
	info.commandBuffer = cmd;
	info.deviceMask = 0;
	return info;
}

auto semaphore_submit_info(VkPipelineStageFlags2 stage_mask,
    VkSemaphore semaphore) -> VkSemaphoreSubmitInfo
{
	VkSemaphoreSubmitInfo info {};
	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	info.pNext = nullptr;
	info.semaphore = semaphore;
	info.value = 0;
	info.stageMask = stage_mask;
	info.deviceIndex = 0;
	return info;
}

auto submit_info2(VkCommandBufferSubmitInfo *cmd_info,
    VkSemaphoreSubmitInfo *wait_semaphore_info,
    VkSemaphoreSubmitInfo *signal_semaphore_info) -> VkSubmitInfo2
{
	VkSubmitInfo2 info {};
	info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	info.pNext = nullptr;
	info.flags = 0;
	info.waitSemaphoreInfoCount = wait_semaphore_info ? 1u : 0u;
	info.pWaitSemaphoreInfos = wait_semaphore_info;
	info.commandBufferInfoCount = cmd_info ? 1u : 0u;
	info.pCommandBufferInfos = cmd_info;
	info.signalSemaphoreInfoCount = signal_semaphore_info ? 1u : 0u;
	info.pSignalSemaphoreInfos = signal_semaphore_info;
	return info;
}

auto attachment_info(VkImageView view, VkClearValue *clear,
    VkImageLayout layout) -> VkRenderingAttachmentInfo
{
	VkRenderingAttachmentInfo color_at {};
	color_at.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	color_at.pNext = nullptr;

	color_at.imageView = view;
	color_at.imageLayout = layout;
	color_at.loadOp
	    = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
	color_at.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	if (clear) {
		color_at.clearValue = *clear;
	}

	return color_at;
}

} // namespace vkinit
