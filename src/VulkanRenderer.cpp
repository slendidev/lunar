#include "VulkanRenderer.h"

#include <cmath>
#include <format>
#include <iostream>
#include <print>
#include <stdexcept>

#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>
#include <VkBootstrap.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include <vulkan/vulkan_core.h>

#include "DescriptorLayoutBuilder.h"
#include "Util.h"

namespace Lunar {

VulkanRenderer::VulkanRenderer(SDL_Window *window, Logger &logger)
    : m_window(window)
    , m_logger(logger)
{
	if (m_window == nullptr) {
		throw std::runtime_error("VulkanRenderer requires a valid window");
	}

	vk_init();
	swapchain_init();
	commands_init();
	sync_init();
	descriptors_init();
	pipelines_init();
	imgui_init();
}

VulkanRenderer::~VulkanRenderer()
{
	vkDeviceWaitIdle(m_vkb.dev);

	for (auto &frame_data : m_vk.frames) {
		vkDestroyCommandPool(m_vkb.dev, frame_data.command_pool, nullptr);

		vkDestroyFence(m_vkb.dev, frame_data.render_fence, nullptr);
		vkDestroySemaphore(m_vkb.dev, frame_data.swapchain_semaphore, nullptr);

		frame_data.deletion_queue.flush();
	}

	destroy_swapchain();
	destroy_draw_image();

	m_vk.deletion_queue.flush();

	SDL_Vulkan_DestroySurface(m_vkb.instance, m_vk.surface, nullptr);

	vkb::destroy_device(m_vkb.dev);
	vkb::destroy_instance(m_vkb.instance);
}

auto VulkanRenderer::resize(uint32_t width, uint32_t height) -> void
{
	recreate_swapchain(width, height);
}

auto VulkanRenderer::immediate_submit(
    std::function<void(VkCommandBuffer cmd)> &&function) -> void
{
	VK_CHECK(m_logger, vkResetFences(m_vkb.dev, 1, &m_vk.imm_fence));
	VK_CHECK(m_logger, vkResetCommandBuffer(m_vk.imm_command_buffer, 0));

	auto cmd { m_vk.imm_command_buffer };
	VkCommandBufferBeginInfo cmd_begin_info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = nullptr,
	};
	VK_CHECK(m_logger, vkBeginCommandBuffer(cmd, &cmd_begin_info));

	function(cmd);

	VK_CHECK(m_logger, vkEndCommandBuffer(cmd));

	auto cmd_info { vkinit::command_buffer_submit_info(cmd) };
	auto submit { vkinit::submit_info2(&cmd_info, nullptr, nullptr) };
	VK_CHECK(m_logger,
	    vkQueueSubmit2(m_vk.graphics_queue, 1, &submit, m_vk.imm_fence));

	VK_CHECK(m_logger,
	    vkWaitForFences(m_vkb.dev, 1, &m_vk.imm_fence, true, 9999999999));
}

auto VulkanRenderer::vk_init() -> void
{
	vkb::InstanceBuilder instance_builder {};
	instance_builder
	    .enable_extension(VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME)
	    .request_validation_layers()
	    .set_app_name("Lunar")
	    .set_engine_name("Lunar")
	    .require_api_version(1, 3, 0)
	    .set_debug_callback_user_data_pointer(this)
	    .set_debug_callback(
	        [](VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
	            VkDebugUtilsMessageTypeFlagsEXT message_type,
	            VkDebugUtilsMessengerCallbackDataEXT const *callback_data,
	            void *user_data) {
		        auto renderer { reinterpret_cast<VulkanRenderer *>(user_data) };

		        auto level = Logger::Level::Debug;
		        if (message_severity
		            & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
			        level = Logger::Level::Error;
		        } else if (message_severity
		            & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
			        level = Logger::Level::Warning;
		        } else if (message_severity
		            & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
			        level = Logger::Level::Info;
		        }

		        renderer->m_logger.log(level,
		            std::format("[Vulkan] [{}] {}",
		                vkb::to_string_message_type(message_type),
		                callback_data->pMessage));

		        return VK_FALSE;
	        });
	auto const instance_builder_ret { instance_builder.build() };
	if (!instance_builder_ret) {
		std::println(std::cerr, "Failed to create Vulkan instance. Error: {}",
		    instance_builder_ret.error().message());
		throw std::runtime_error("App init fail");
	}

	m_vkb.instance = instance_builder_ret.value();

	if (!SDL_Vulkan_CreateSurface(
	        m_window, m_vkb.instance, nullptr, &m_vk.surface)) {
		m_logger.err("Failed to create vulkan surface");
		throw std::runtime_error("App init fail");
	}

	vkb::PhysicalDeviceSelector phys_device_selector { m_vkb.instance };
	VkPhysicalDeviceVulkan13Features features_13 {};
	features_13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	features_13.pNext = nullptr;
	features_13.synchronization2 = VK_TRUE;
	features_13.dynamicRendering = VK_TRUE;
	phys_device_selector.set_surface(m_vk.surface)
	    .add_desired_extensions({
	        VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
	        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
	        VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
	        VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME,
	        VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME,
	        VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,
	        VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
	        VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
	        VK_KHR_MAINTENANCE1_EXTENSION_NAME,
	        VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,
	        VK_KHR_COPY_COMMANDS_2_EXTENSION_NAME,
	    })
	    .set_required_features_13(features_13);
	auto physical_device_selector_return { phys_device_selector.select() };
	if (!physical_device_selector_return) {
		std::println(std::cerr,
		    "Failed to find Vulkan physical device. Error: {}",
		    physical_device_selector_return.error().message());
		throw std::runtime_error("App init fail");
	}
	m_vkb.phys_dev = physical_device_selector_return.value();

	m_logger.info("Chosen Vulkan physical device: {}",
	    m_vkb.phys_dev.properties.deviceName);

	vkb::DeviceBuilder device_builder { m_vkb.phys_dev };
	auto dev_ret { device_builder.build() };
	if (!dev_ret) {
		std::println(std::cerr, "Failed to create Vulkan device. Error: {}",
		    dev_ret.error().message());
		throw std::runtime_error("App init fail");
	}
	m_vkb.dev = dev_ret.value();

	auto queue_ret { m_vkb.dev.get_queue(vkb::QueueType::graphics) };
	if (!queue_ret) {
		std::println(std::cerr, "Failed to get graphics queue. Error: {}",
		    queue_ret.error().message());
		throw std::runtime_error("App init fail");
	}
	m_vk.graphics_queue = queue_ret.value();

	auto queue_family_ret { m_vkb.dev.get_queue_index(
		vkb::QueueType::graphics) };
	if (!queue_family_ret) {
		std::println(std::cerr, "Failed to get graphics queue. Error: {}",
		    queue_family_ret.error().message());
		throw std::runtime_error("App init fail");
	}
	m_vk.graphics_queue_family = queue_family_ret.value();

	VmaAllocatorCreateInfo allocator_ci {};
	allocator_ci.physicalDevice = m_vkb.phys_dev;
	allocator_ci.device = m_vkb.dev;
	allocator_ci.instance = m_vkb.instance;
	vmaCreateAllocator(&allocator_ci, &m_vk.allocator);

	m_vk.deletion_queue.emplace(
	    [this]() { vmaDestroyAllocator(m_vk.allocator); });
}

auto VulkanRenderer::swapchain_init() -> void
{
	int w, h;
	SDL_GetWindowSize(m_window, &w, &h);
	create_swapchain(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
	create_draw_image(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
}

auto VulkanRenderer::commands_init() -> void
{
	VkCommandPoolCreateInfo ci {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = m_vk.graphics_queue_family,
	};
	for (auto &frame_data : m_vk.frames) {
		VK_CHECK(m_logger,
		    vkCreateCommandPool(
		        m_vkb.dev, &ci, nullptr, &frame_data.command_pool));

		VkCommandBufferAllocateInfo ai {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.pNext = nullptr,
			.commandPool = frame_data.command_pool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		};

		VK_CHECK(m_logger,
		    vkAllocateCommandBuffers(
		        m_vkb.dev, &ai, &frame_data.main_command_buffer));
	}

	VK_CHECK(m_logger,
	    vkCreateCommandPool(m_vkb.dev, &ci, nullptr, &m_vk.imm_command_pool));

	VkCommandBufferAllocateInfo ai {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext = nullptr,
		.commandPool = m_vk.imm_command_pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};
	VK_CHECK(m_logger,
	    vkAllocateCommandBuffers(m_vkb.dev, &ai, &m_vk.imm_command_buffer));

	m_vk.deletion_queue.emplace([this]() {
		vkDestroyCommandPool(m_vkb.dev, m_vk.imm_command_pool, nullptr);
	});
}

auto VulkanRenderer::sync_init() -> void
{
	VkFenceCreateInfo fence_ci {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.pNext = nullptr,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT,
	};
	VkSemaphoreCreateInfo semaphore_ci {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
	};
	for (auto &frame_data : m_vk.frames) {
		VK_CHECK(m_logger,
		    vkCreateFence(
		        m_vkb.dev, &fence_ci, nullptr, &frame_data.render_fence));

		VK_CHECK(m_logger,
		    vkCreateSemaphore(m_vkb.dev, &semaphore_ci, nullptr,
		        &frame_data.swapchain_semaphore));
	}

	VK_CHECK(m_logger,
	    vkCreateFence(m_vkb.dev, &fence_ci, nullptr, &m_vk.imm_fence));
	m_vk.deletion_queue.emplace(
	    [this]() { vkDestroyFence(m_vkb.dev, m_vk.imm_fence, nullptr); });
}

auto VulkanRenderer::descriptors_init() -> void
{
	std::vector<DescriptorAllocator::PoolSizeRatio> sizes {
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
	};
	m_vk.descriptor_allocator.init_pool(m_vkb.dev, 10, sizes);

	m_vk.draw_image_descriptor_layout
	    = DescriptorLayoutBuilder()
	          .add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
	          .build(m_logger, m_vkb.dev, VK_SHADER_STAGE_COMPUTE_BIT);

	m_vk.draw_image_descriptors = m_vk.descriptor_allocator.allocate(
	    m_logger, m_vkb.dev, m_vk.draw_image_descriptor_layout);

	update_draw_image_descriptor();

	m_vk.deletion_queue.emplace([&]() {
		m_vk.descriptor_allocator.destroy_pool(m_vkb.dev);
		vkDestroyDescriptorSetLayout(
		    m_vkb.dev, m_vk.draw_image_descriptor_layout, nullptr);
	});
}

auto VulkanRenderer::pipelines_init() -> void { background_pipelines_init(); }

auto VulkanRenderer::background_pipelines_init() -> void
{
	VkPipelineLayoutCreateInfo layout_ci {};
	layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layout_ci.pNext = nullptr;
	layout_ci.pSetLayouts = &m_vk.draw_image_descriptor_layout;
	layout_ci.setLayoutCount = 1;

	VK_CHECK(m_logger,
	    vkCreatePipelineLayout(
	        m_vkb.dev, &layout_ci, nullptr, &m_vk.gradient_pipeline_layout));

	uint8_t compute_draw_shader_data[] {
#embed "gradient.spv"
	};
	VkShaderModule compute_draw_shader {};
	if (!vkutil::load_shader_module(std::span<uint8_t>(compute_draw_shader_data,
	                                    sizeof(compute_draw_shader_data)),
	        m_vkb.dev, &compute_draw_shader)) {
		m_logger.err("Failed to load gradient compute shader");
	}

	VkPipelineShaderStageCreateInfo stage_ci {};
	stage_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage_ci.pNext = nullptr;
	stage_ci.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage_ci.module = compute_draw_shader;
	stage_ci.pName = "main";

	VkComputePipelineCreateInfo compute_pip_ci {};
	compute_pip_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	compute_pip_ci.pNext = nullptr;
	compute_pip_ci.layout = m_vk.gradient_pipeline_layout;
	compute_pip_ci.stage = stage_ci;

	VK_CHECK(m_logger,
	    vkCreateComputePipelines(m_vkb.dev, VK_NULL_HANDLE, 1, &compute_pip_ci,
	        nullptr, &m_vk.gradient_pipeline));

	vkDestroyShaderModule(m_vkb.dev, compute_draw_shader, nullptr);
	m_vk.deletion_queue.emplace([&]() {
		vkDestroyPipelineLayout(
		    m_vkb.dev, m_vk.gradient_pipeline_layout, nullptr);
		vkDestroyPipeline(m_vkb.dev, m_vk.gradient_pipeline, nullptr);
	});
}

auto VulkanRenderer::imgui_init() -> void
{
	VkDescriptorPoolSize pool_sizes[] = {
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 },
	};

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;

	VkDescriptorPool imgui_pool;
	VK_CHECK(m_logger,
	    vkCreateDescriptorPool(m_vkb.dev, &pool_info, nullptr, &imgui_pool));

	ImGui::CreateContext();

	ImGui_ImplSDL3_InitForVulkan(m_window);

	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = m_vkb.instance;
	init_info.PhysicalDevice = m_vkb.phys_dev;
	init_info.Device = m_vkb.dev;
	init_info.Queue = m_vk.graphics_queue;
	init_info.DescriptorPool = imgui_pool;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.UseDynamicRendering = true;

	init_info.PipelineInfoMain.PipelineRenderingCreateInfo.sType
	    = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	init_info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount
	    = 1;
	init_info.PipelineInfoMain.PipelineRenderingCreateInfo
	    .pColorAttachmentFormats
	    = &m_vk.swapchain_image_format;

	init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&init_info);

	m_vk.deletion_queue.emplace([&]() {
		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(m_vkb.dev, imgui_pool, nullptr);
	});
}

auto VulkanRenderer::render() -> void
{
	defer(m_vk.frame_number++);

	if (m_vk.swapchain == VK_NULL_HANDLE || m_vk.swapchain_extent.width == 0
	    || m_vk.swapchain_extent.height == 0) {
		return;
	}

	VK_CHECK(m_logger,
	    vkWaitForFences(m_vkb.dev, 1, &m_vk.get_current_frame().render_fence,
	        true, 1'000'000'000));
	VK_CHECK(m_logger,
	    vkResetFences(m_vkb.dev, 1, &m_vk.get_current_frame().render_fence));

	uint32_t swapchain_image_idx;
	auto const acquire_result = vkAcquireNextImageKHR(m_vkb.dev, m_vk.swapchain,
	    1000000000, m_vk.get_current_frame().swapchain_semaphore, nullptr,
	    &swapchain_image_idx);
	if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR
	    || acquire_result == VK_SUBOPTIMAL_KHR) {
		int width {}, height {};
		SDL_GetWindowSize(m_window, &width, &height);
		recreate_swapchain(
		    static_cast<uint32_t>(width), static_cast<uint32_t>(height));
		return;
	}
	VK_CHECK(m_logger, acquire_result);

	auto cmd { m_vk.get_current_frame().main_command_buffer };
	VK_CHECK(m_logger, vkResetCommandBuffer(cmd, 0));

	m_vk.draw_extent.width = m_vk.draw_image.extent.width;
	m_vk.draw_extent.height = m_vk.draw_image.extent.height;

	VkCommandBufferBeginInfo cmd_begin_info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = nullptr,
	};
	VK_CHECK(m_logger, vkBeginCommandBuffer(cmd, &cmd_begin_info));

	vkutil::transition_image(cmd, m_vk.draw_image.image,
	    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	draw_background(cmd);

	vkutil::transition_image(cmd, m_vk.draw_image.image,
	    VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

	vkutil::transition_image(cmd, m_vk.swapchain_images.at(swapchain_image_idx),
	    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	vkutil::copy_image_to_image(cmd, m_vk.draw_image.image,
	    m_vk.swapchain_images.at(swapchain_image_idx), m_vk.draw_extent,
	    m_vk.swapchain_extent);

	vkutil::transition_image(cmd, m_vk.swapchain_images[swapchain_image_idx],
	    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	draw_imgui(cmd, m_vk.swapchain_image_views.at(swapchain_image_idx));

	vkutil::transition_image(cmd, m_vk.swapchain_images[swapchain_image_idx],
	    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	VK_CHECK(m_logger, vkEndCommandBuffer(cmd));

	VkSemaphore render_semaphore
	    = m_vk.present_semaphores.at(swapchain_image_idx);
	VkPipelineStageFlags2 wait_stage
	    = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	auto wait_info { vkinit::semaphore_submit_info(
		wait_stage, m_vk.get_current_frame().swapchain_semaphore) };
	auto command_buffer_info { vkinit::command_buffer_submit_info(cmd) };
	auto signal_info { vkinit::semaphore_submit_info(
		VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, render_semaphore) };
	auto submit_info { vkinit::submit_info2(
		&command_buffer_info, &wait_info, &signal_info) };

	VK_CHECK(m_logger,
	    vkQueueSubmit2(m_vk.graphics_queue, 1, &submit_info,
	        m_vk.get_current_frame().render_fence));

	VkPresentInfoKHR present_info = {};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.pNext = nullptr;
	present_info.pSwapchains = &m_vk.swapchain;
	present_info.swapchainCount = 1;

	present_info.pWaitSemaphores = &render_semaphore;
	present_info.waitSemaphoreCount = 1;

	present_info.pImageIndices = &swapchain_image_idx;

	auto const present_result
	    = vkQueuePresentKHR(m_vk.graphics_queue, &present_info);
	if (present_result == VK_ERROR_OUT_OF_DATE_KHR
	    || present_result == VK_SUBOPTIMAL_KHR) {
		int width {}, height {};
		SDL_GetWindowSize(m_window, &width, &height);
		recreate_swapchain(
		    static_cast<uint32_t>(width), static_cast<uint32_t>(height));
		return;
	}
	VK_CHECK(m_logger, present_result);
}

auto VulkanRenderer::draw_background(VkCommandBuffer cmd) -> void
{
	vkCmdBindPipeline(
	    cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_vk.gradient_pipeline);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
	    m_vk.gradient_pipeline_layout, 0, 1, &m_vk.draw_image_descriptors, 0,
	    nullptr);
	vkCmdDispatch(cmd,
	    static_cast<uint32_t>(std::ceil(m_vk.draw_extent.width / 16.0)),
	    static_cast<uint32_t>(std::ceil(m_vk.draw_extent.height / 16.0)), 1);
}

auto VulkanRenderer::draw_imgui(
    VkCommandBuffer cmd, VkImageView target_image_view) -> void
{
	auto const color_attachment { vkinit::attachment_info(
		target_image_view, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) };
	VkRenderingInfo render_info {};
	render_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	render_info.pNext = nullptr;
	render_info.flags = 0;
	render_info.renderArea = { {}, m_vk.draw_extent };
	render_info.layerCount = 1;
	render_info.colorAttachmentCount = 1;
	render_info.pColorAttachments = &color_attachment;

	vkCmdBeginRendering(cmd, &render_info);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	vkCmdEndRendering(cmd);
}

auto VulkanRenderer::create_swapchain(uint32_t width, uint32_t height) -> void
{
	vkb::SwapchainBuilder builder { m_vkb.phys_dev, m_vkb.dev, m_vk.surface };
	m_vk.swapchain_image_format = VK_FORMAT_B8G8R8A8_UNORM;
	auto const swapchain_ret { builder
		    .set_desired_format({
		        .format = m_vk.swapchain_image_format,
		        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
		    })
		    .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		    .set_desired_extent(width, height)
		    .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		    .build() };
	if (!swapchain_ret) {
		std::println(std::cerr, "Failed to create swapchain. Error: {}",
		    swapchain_ret.error().message());
		throw std::runtime_error("App init fail");
	}
	m_vkb.swapchain = swapchain_ret.value();

	m_vk.swapchain = m_vkb.swapchain.swapchain;
	m_vk.swapchain_extent = m_vkb.swapchain.extent;
	m_vk.swapchain_images = m_vkb.swapchain.get_images().value();
	m_vk.swapchain_image_views = m_vkb.swapchain.get_image_views().value();

	VkSemaphoreCreateInfo semaphore_ci {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
	};
	m_vk.present_semaphores.resize(m_vk.swapchain_images.size());
	for (auto &semaphore : m_vk.present_semaphores) {
		VK_CHECK(m_logger,
		    vkCreateSemaphore(m_vkb.dev, &semaphore_ci, nullptr, &semaphore));
	}
}

auto VulkanRenderer::create_draw_image(uint32_t width, uint32_t height) -> void
{
	destroy_draw_image();

	m_vk.draw_image.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	m_vk.draw_image.extent = {
		width,
		height,
		1,
	};

	VkImageCreateInfo rimg_ci { vkinit::image_create_info(
		m_vk.draw_image.format,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
		    | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		m_vk.draw_image.extent) };
	VmaAllocationCreateInfo rimg_alloci {};
	rimg_alloci.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	rimg_alloci.requiredFlags
	    = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	vmaCreateImage(m_vk.allocator, &rimg_ci, &rimg_alloci,
	    &m_vk.draw_image.image, &m_vk.draw_image.allocation, nullptr);

	VkImageViewCreateInfo rview_ci
	    = vkinit::imageview_create_info(m_vk.draw_image.format,
	        m_vk.draw_image.image, VK_IMAGE_ASPECT_COLOR_BIT);
	VK_CHECK(m_logger,
	    vkCreateImageView(
	        m_vkb.dev, &rview_ci, nullptr, &m_vk.draw_image.image_view));
}

auto VulkanRenderer::update_draw_image_descriptor() -> void
{
	VkDescriptorImageInfo img_info {};
	img_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	img_info.imageView = m_vk.draw_image.image_view;

	VkWriteDescriptorSet draw_img_write {};
	draw_img_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	draw_img_write.pNext = nullptr;
	draw_img_write.dstBinding = 0;
	draw_img_write.dstSet = m_vk.draw_image_descriptors;
	draw_img_write.descriptorCount = 1;
	draw_img_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	draw_img_write.pImageInfo = &img_info;

	vkUpdateDescriptorSets(m_vkb.dev, 1, &draw_img_write, 0, nullptr);
}

auto VulkanRenderer::destroy_draw_image() -> void
{
	if (m_vk.draw_image.image_view != VK_NULL_HANDLE) {
		vkDestroyImageView(m_vkb.dev, m_vk.draw_image.image_view, nullptr);
		m_vk.draw_image.image_view = VK_NULL_HANDLE;
	}
	if (m_vk.draw_image.image != VK_NULL_HANDLE) {
		vmaDestroyImage(
		    m_vk.allocator, m_vk.draw_image.image, m_vk.draw_image.allocation);
		m_vk.draw_image.image = VK_NULL_HANDLE;
		m_vk.draw_image.allocation = nullptr;
	}
	m_vk.draw_image.extent = { 0, 0, 0 };
}

auto VulkanRenderer::recreate_swapchain(uint32_t width, uint32_t height) -> void
{
	vkDeviceWaitIdle(m_vkb.dev);

	if (width == 0 || height == 0) {
		destroy_swapchain();
		destroy_draw_image();
		m_vk.swapchain_extent = { 0, 0 };
		return;
	}

	destroy_swapchain();
	destroy_draw_image();

	create_swapchain(width, height);
	create_draw_image(width, height);
	update_draw_image_descriptor();
}

auto VulkanRenderer::destroy_swapchain() -> void
{
	if (m_vk.swapchain == VK_NULL_HANDLE)
		return;

	for (auto const semaphore : m_vk.present_semaphores) {
		vkDestroySemaphore(m_vkb.dev, semaphore, nullptr);
	}
	for (auto const &iv : m_vk.swapchain_image_views) {
		vkDestroyImageView(m_vkb.dev, iv, nullptr);
	}
	vkDestroySwapchainKHR(m_vkb.dev, m_vk.swapchain, nullptr);

	m_vk.swapchain = VK_NULL_HANDLE;
	m_vk.swapchain_image_views.clear();
	m_vk.swapchain_images.clear();
	m_vk.present_semaphores.clear();
	m_vk.swapchain_extent = { 0, 0 };
}

} // namespace Lunar
