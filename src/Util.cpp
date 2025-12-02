#include "Util.h"

namespace vkutil {

void transition_image(VkCommandBuffer cmd, VkImage image,
    VkImageLayout current_layout, VkImageLayout new_layout)
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

	vkCmdPipelineBarrier(cmd,
	    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
	    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1,
	    &image_barrier);
}

} // namespace vkutil
