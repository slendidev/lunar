#pragma once

#include <string_view>

#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

using namespace std::string_view_literals;

template<typename F> struct privDefer {
	F f;
	privDefer(F f)
	    : f(f)
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
		VkResult err { x }; \
		if (err) { \
			(logger).err("Detected Vulkan error: {}", string_VkResult(err)); \
			throw std::runtime_error("Vulkan error"); \
		} \
	} while (0)

namespace vkutil {

auto transition_image(VkCommandBuffer cmd, VkImage image,
    VkImageLayout current_layout, VkImageLayout new_layout) -> void;
auto copy_image_to_image(VkCommandBuffer cmd, VkImage source,
    VkImage destination, VkExtent2D src_size, VkExtent2D dst_size) -> void;

} // namespace vkutil

namespace vkinit {

auto image_create_info(VkFormat format, VkImageUsageFlags usage_flags,
    VkExtent3D extent) -> VkImageCreateInfo;
auto imageview_create_info(VkFormat format, VkImage image,
    VkImageAspectFlags aspect_flags) -> VkImageViewCreateInfo;

} // namespace vkinit
