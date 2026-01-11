#pragma once

#include <span>

#include <vulkan/vulkan.hpp>

template<typename F> struct privDefer {
	F f;
	privDefer(F f_)
	    : f(f_)
	{
	}
	~privDefer() { f(); }
};

template<typename F> privDefer<F> defer_func(F f) { return privDefer<F>(f); }

#define DEFER_1(x, y) x##y
#define DEFER_2(x, y) DEFER_1(x, y)
#define DEFER_3(x) DEFER_2(x, __COUNTER__)
#define defer(code) auto DEFER_3(_defer_) = defer_func([&]() { code; })

#if defined(_MSC_VER)
#	define ALIGN(a) __declspec(align(a))
#else
#	define ALIGN(a) __attribute__((aligned(a)))
#endif

#define VK_CHECK(logger, x) \
	do { \
		auto err { x }; \
		auto result = vk::Result(err); \
		if (result != vk::Result::eSuccess) { \
			(logger).err("Detected Vulkan error: {}", vk::to_string(result)); \
			throw std::runtime_error("Vulkan error"); \
		} \
	} while (0)

#if defined(TRACY_ENABLE)
#	define GZoneScopedN(name) ZoneScopedN(name)
#else
#	define GZoneScopedN(name) \
		do { \
		} while (0)
#endif

namespace vkutil {

auto transition_image(vk::CommandBuffer cmd, vk::Image image,
    vk::ImageLayout current_layout, vk::ImageLayout new_layout) -> void;
auto copy_image_to_image(vk::CommandBuffer cmd, vk::Image source,
    vk::Image destination, vk::Extent2D src_size, vk::Extent2D dst_size)
    -> void;
auto load_shader_module(std::span<uint8_t> spirv_data, vk::Device device)
    -> vk::UniqueShaderModule;

} // namespace vkutil

namespace vkinit {

auto image_create_info(vk::Format format, vk::ImageUsageFlags usage_flags,
    vk::Extent3D extent,
    vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1)
    -> vk::ImageCreateInfo;
auto imageview_create_info(vk::Format format, vk::Image image,
    vk::ImageAspectFlags aspect_flags) -> vk::ImageViewCreateInfo;
auto command_buffer_submit_info(vk::CommandBuffer cmd)
    -> vk::CommandBufferSubmitInfo;
auto semaphore_submit_info(vk::PipelineStageFlags2 stage_mask,
    vk::Semaphore semaphore) -> vk::SemaphoreSubmitInfo;
auto submit_info2(vk::CommandBufferSubmitInfo *cmd_info,
    vk::SemaphoreSubmitInfo *wait_semaphore_info,
    vk::SemaphoreSubmitInfo *signal_semaphore_info) -> vk::SubmitInfo2;
auto attachment_info(vk::ImageView view, vk::ClearValue *clear,
    vk::ImageLayout layout = vk::ImageLayout::eColorAttachmentOptimal)
    -> vk::RenderingAttachmentInfo;
auto pipeline_shader_stage(vk::ShaderStageFlagBits stage,
    vk::ShaderModule module) -> vk::PipelineShaderStageCreateInfo;
auto render_info(vk::Extent2D extent,
    vk::RenderingAttachmentInfo const *color_att,
    vk::RenderingAttachmentInfo const *depth_att) -> vk::RenderingInfo;
auto depth_attachment_info(vk::ImageView view,
    vk::ImageLayout layout = vk::ImageLayout::eDepthAttachmentOptimal)
    -> vk::RenderingAttachmentInfo;

} // namespace vkinit
