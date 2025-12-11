#define VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#include "VulkanRenderer.h"

#include <cassert>
#include <cmath>
#include <cstring>
#include <format>
#include <iostream>
#include <print>
#include <stdexcept>

#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>
#include <VkBootstrap.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

#include "DescriptorLayoutBuilder.h"
#include "DescriptorWriter.h"
#include "GraphicsPipelineBuilder.h"
#include "Util.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

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
	default_data_init();
	imgui_init();
}

VulkanRenderer::~VulkanRenderer()
{
	m_device.waitIdle();

	for (auto &frame_data : m_vk.frames) {
		frame_data.deletion_queue.flush();
		frame_data.main_command_buffer.reset();
		frame_data.command_pool.reset();
		frame_data.swapchain_semaphore.reset();
		frame_data.render_fence.reset();
	}
	m_vk.present_semaphores.clear();
	m_vk.swapchain_image_views.clear();
	m_vk.imm_command_buffer.reset();
	m_vk.imm_command_pool.reset();
	m_vk.imm_fence.reset();
	m_vk.gradient_pipeline.reset();
	m_vk.gradient_pipeline_layout.reset();
	if (m_vk.triangle_pipeline) {
		m_device.destroyPipeline(m_vk.triangle_pipeline);
		m_vk.triangle_pipeline = vk::Pipeline {};
	}
	m_vk.triangle_pipeline_layout.reset();
	if (m_vk.mesh_pipeline) {
		m_device.destroyPipeline(m_vk.mesh_pipeline);
		m_vk.mesh_pipeline = vk::Pipeline {};
	}
	m_vk.mesh_pipeline_layout.reset();
	m_vk.default_sampler_linear.reset();
	m_vk.default_sampler_nearest.reset();

	destroy_swapchain();
	destroy_draw_image();
	destroy_depth_image();

	m_vk.deletion_queue.flush();

	if (m_vk.allocator) {
		vmaDestroyAllocator(m_vk.allocator);
		m_vk.allocator = nullptr;
	}

	if (m_vk.surface) {
		SDL_Vulkan_DestroySurface(
		    m_vkb.instance, static_cast<VkSurfaceKHR>(m_vk.surface), nullptr);
		m_vk.surface = nullptr;
	}

	vkb::destroy_device(m_vkb.dev);
	vkb::destroy_instance(m_vkb.instance);
}

auto VulkanRenderer::resize(uint32_t width, uint32_t height) -> void
{
	recreate_swapchain(width, height);
}

auto VulkanRenderer::immediate_submit(
    std::function<void(vk::CommandBuffer cmd)> &&function) -> void
{
	m_device.resetFences(m_vk.imm_fence.get());
	m_vk.imm_command_buffer.get().reset();

	auto cmd { m_vk.imm_command_buffer.get() };
	vk::CommandBufferBeginInfo cmd_begin_info {};
	cmd_begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
	cmd.begin(cmd_begin_info);

	function(cmd);

	cmd.end();

	auto cmd_info { vkinit::command_buffer_submit_info(cmd) };
	auto submit { vkinit::submit_info2(&cmd_info, nullptr, nullptr) };
	m_vk.graphics_queue.submit2(submit, m_vk.imm_fence.get());

	VK_CHECK(m_logger,
	    m_device.waitForFences(m_vk.imm_fence.get(), true, 9'999'999'999));

	m_vk.get_current_frame().deletion_queue.flush();
	m_vk.get_current_frame().frame_descriptors.clear_pools(m_vkb.dev.device);
}

auto VulkanRenderer::vk_init() -> void
{
	VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

	vkb::InstanceBuilder instance_builder {};
	instance_builder
	    .enable_extension(VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME)
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
#ifndef NDEBUG
	instance_builder.request_validation_layers();
#endif
	auto const instance_builder_ret { instance_builder.build() };
	if (!instance_builder_ret) {
		std::println(std::cerr, "Failed to create Vulkan instance. Error: {}",
		    instance_builder_ret.error().message());
		throw std::runtime_error("App init fail");
	}

	m_vkb.instance = instance_builder_ret.value();
	m_instance = vk::Instance { m_vkb.instance.instance };
	VULKAN_HPP_DEFAULT_DISPATCHER.init(m_instance);

	VkSurfaceKHR raw_surface {};
	if (!SDL_Vulkan_CreateSurface(
	        m_window, m_vkb.instance, nullptr, &raw_surface)) {
		m_logger.err("Failed to create vulkan surface");
		throw std::runtime_error("App init fail");
	}
	m_vk.surface = vk::SurfaceKHR { raw_surface };

	vkb::PhysicalDeviceSelector phys_device_selector { m_vkb.instance };
	VkPhysicalDeviceVulkan13Features features_13 {};
	features_13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	features_13.pNext = nullptr;
	features_13.synchronization2 = VK_TRUE;
	features_13.dynamicRendering = VK_TRUE;
	VkPhysicalDeviceBufferDeviceAddressFeatures
	    buffer_device_address_features {};
	buffer_device_address_features.sType
	    = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
	buffer_device_address_features.bufferDeviceAddress = VK_TRUE;
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
	        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
	    })
	    .set_required_features_13(features_13)
	    .add_required_extension_features(buffer_device_address_features);
	auto physical_device_selector_return { phys_device_selector.select() };
	if (!physical_device_selector_return) {
		std::println(std::cerr,
		    "Failed to find Vulkan physical device. Error: {}",
		    physical_device_selector_return.error().message());
		throw std::runtime_error("App init fail");
	}
	m_vkb.phys_dev = physical_device_selector_return.value();
	m_physical_device = vk::PhysicalDevice { m_vkb.phys_dev.physical_device };

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
	m_device = vk::Device { m_vkb.dev.device };
	VULKAN_HPP_DEFAULT_DISPATCHER.init(m_device);

	auto queue_family_ret { m_vkb.dev.get_queue_index(
		vkb::QueueType::graphics) };
	if (!queue_family_ret) {
		std::println(std::cerr, "Failed to get graphics queue. Error: {}",
		    queue_family_ret.error().message());
		throw std::runtime_error("App init fail");
	}
	m_vk.graphics_queue_family = queue_family_ret.value();
	m_vk.graphics_queue = m_device.getQueue(m_vk.graphics_queue_family, 0);

	VmaAllocatorCreateInfo allocator_ci {};
	allocator_ci.physicalDevice = m_vkb.phys_dev.physical_device;
	allocator_ci.device = m_vkb.dev.device;
	allocator_ci.instance = m_vkb.instance.instance;
	allocator_ci.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	vmaCreateAllocator(&allocator_ci, &m_vk.allocator);
}

auto VulkanRenderer::swapchain_init() -> void
{
	int w, h;
	SDL_GetWindowSize(m_window, &w, &h);
	create_swapchain(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
	create_draw_image(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
	create_depth_image(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
}

auto VulkanRenderer::commands_init() -> void
{
	vk::CommandPoolCreateInfo ci {};
	ci.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
	ci.queueFamilyIndex = m_vk.graphics_queue_family;
	for (auto &frame_data : m_vk.frames) {
		frame_data.command_pool = m_device.createCommandPoolUnique(ci);

		vk::CommandBufferAllocateInfo ai {};
		ai.commandPool = frame_data.command_pool.get();
		ai.level = vk::CommandBufferLevel::ePrimary;
		ai.commandBufferCount = 1;

		frame_data.main_command_buffer
		    = std::move(m_device.allocateCommandBuffersUnique(ai).front());
	}

	m_vk.imm_command_pool = m_device.createCommandPoolUnique(ci);

	vk::CommandBufferAllocateInfo ai {};
	ai.commandPool = m_vk.imm_command_pool.get();
	ai.level = vk::CommandBufferLevel::ePrimary;
	ai.commandBufferCount = 1;
	m_vk.imm_command_buffer
	    = std::move(m_device.allocateCommandBuffersUnique(ai).front());
}

auto VulkanRenderer::sync_init() -> void
{
	vk::FenceCreateInfo fence_ci {};
	fence_ci.flags = vk::FenceCreateFlagBits::eSignaled;
	vk::SemaphoreCreateInfo semaphore_ci {};

	for (auto &frame_data : m_vk.frames) {
		frame_data.render_fence = m_device.createFenceUnique(fence_ci);
		frame_data.swapchain_semaphore
		    = m_device.createSemaphoreUnique(semaphore_ci);
	}

	m_vk.imm_fence = m_device.createFenceUnique(fence_ci);
}

auto VulkanRenderer::descriptors_init() -> void
{
	std::vector<DescriptorAllocator::PoolSizeRatio> sizes {
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
	};
	m_vk.descriptor_allocator.init_pool(m_vkb.dev.device, 10, sizes);

	auto draw_layout_raw
	    = DescriptorLayoutBuilder()
	          .add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
	          .build(m_logger, m_vkb.dev.device, VK_SHADER_STAGE_COMPUTE_BIT);
	m_vk.draw_image_descriptor_layout
	    = vk::DescriptorSetLayout { draw_layout_raw };

	m_vk.draw_image_descriptors = m_vk.descriptor_allocator.allocate(
	    m_logger, m_vkb.dev.device, m_vk.draw_image_descriptor_layout);

	update_draw_image_descriptor();

	m_vk.deletion_queue.emplace([&]() {
		m_vk.descriptor_allocator.destroy_pool(m_vkb.dev.device);
		m_device.destroyDescriptorSetLayout(m_vk.draw_image_descriptor_layout);
		m_device.destroyDescriptorSetLayout(
		    m_vk.gpu_scene_data_descriptor_layout);
		m_device.destroyDescriptorSetLayout(
		    m_vk.single_image_descriptor_layout);
	});

	for (unsigned int i = 0; i < FRAME_OVERLAP; i++) {
		std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_sizes = {
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
		};

		m_vk.frames[i].frame_descriptors = DescriptorAllocatorGrowable {};
		m_vk.frames[i].frame_descriptors.init(
		    m_vkb.dev.device, 1000, frame_sizes);

		m_vk.deletion_queue.emplace([&, i]() {
			m_vk.frames[i].frame_descriptors.destroy_pools(m_vkb.dev.device);
		});
	}

	auto scene_layout_raw
	    = DescriptorLayoutBuilder()
	          .add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
	          .build(m_logger, m_vkb.dev.device,
	              VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	m_vk.gpu_scene_data_descriptor_layout
	    = vk::DescriptorSetLayout { scene_layout_raw };

	auto single_layout_raw
	    = DescriptorLayoutBuilder()
	          .add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
	          .build(m_logger, m_vkb.dev.device, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_vk.single_image_descriptor_layout
	    = vk::DescriptorSetLayout { single_layout_raw };
}

auto VulkanRenderer::pipelines_init() -> void
{
	background_pipelines_init();
	triangle_pipeline_init();
	mesh_pipeline_init();
}

auto VulkanRenderer::background_pipelines_init() -> void
{
	vk::PipelineLayoutCreateInfo layout_ci {};
	auto draw_layout_handle = m_vk.draw_image_descriptor_layout;
	layout_ci.pSetLayouts = &draw_layout_handle;
	layout_ci.setLayoutCount = 1;

	m_vk.gradient_pipeline_layout
	    = m_device.createPipelineLayoutUnique(layout_ci);

	uint8_t compute_draw_shader_data[] {
#embed "gradient_comp.spv"
	};
	auto compute_draw_shader { vkutil::load_shader_module(
		std::span<uint8_t>(
		    compute_draw_shader_data, sizeof(compute_draw_shader_data)),
		m_device) };
	if (!compute_draw_shader) {
		m_logger.err("Failed to load gradient compute shader");
	}

	auto stage_ci { vkinit::pipeline_shader_stage(
		vk::ShaderStageFlagBits::eCompute, compute_draw_shader.get()) };

	vk::ComputePipelineCreateInfo compute_pip_ci {};
	compute_pip_ci.layout = m_vk.gradient_pipeline_layout.get();
	compute_pip_ci.stage = stage_ci;

	auto pipeline_ret
	    = m_device.createComputePipelineUnique({}, compute_pip_ci);
	VK_CHECK(m_logger, pipeline_ret.result);
	m_vk.gradient_pipeline = std::move(pipeline_ret.value);
}

auto VulkanRenderer::triangle_pipeline_init() -> void
{
	uint8_t triangle_vert_shader_data[] {
#embed "triangle_vert.spv"
	};
	auto triangle_vert_shader = vkutil::load_shader_module(
	    std::span<uint8_t>(
	        triangle_vert_shader_data, sizeof(triangle_vert_shader_data)),
	    m_device);
	if (!triangle_vert_shader) {
		m_logger.err("Failed to load triangle vert shader");
	}

	uint8_t triangle_frag_shader_data[] {
#embed "triangle_frag.spv"
	};
	auto triangle_frag_shader = vkutil::load_shader_module(
	    std::span<uint8_t>(
	        triangle_frag_shader_data, sizeof(triangle_frag_shader_data)),
	    m_device);
	if (!triangle_frag_shader) {
		m_logger.err("Failed to load triangle frag shader");
	}

	vk::PipelineLayoutCreateInfo layout_ci {};
	m_vk.triangle_pipeline_layout
	    = m_device.createPipelineLayoutUnique(layout_ci);

	auto pip
	    = GraphicsPipelineBuilder { m_logger }
	          .set_pipeline_layout(m_vk.triangle_pipeline_layout.get())
	          .set_shaders(
	              triangle_vert_shader.get(), triangle_frag_shader.get())
	          .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
	          .set_polygon_mode(VK_POLYGON_MODE_FILL)
	          .set_multisampling_none()
	          .enable_blending_additive()
	          .disable_depth_testing()
	          .set_color_attachment_format(
	              static_cast<VkFormat>(m_vk.draw_image.format))
	          .set_depth_format(static_cast<VkFormat>(m_vk.depth_image.format))
	          .build(m_vkb.dev.device);
	m_vk.triangle_pipeline = vk::Pipeline { pip };
	m_vk.deletion_queue.emplace(
	    [this]() { m_device.destroyPipeline(m_vk.triangle_pipeline); });
}

auto VulkanRenderer::mesh_pipeline_init() -> void
{
	uint8_t triangle_vert_shader_data[] {
#embed "triangle_mesh_vert.spv"
	};
	auto triangle_vert_shader = vkutil::load_shader_module(
	    std::span<uint8_t>(
	        triangle_vert_shader_data, sizeof(triangle_vert_shader_data)),
	    m_device);
	if (!triangle_vert_shader) {
		m_logger.err("Failed to load triangle vert shader");
	}

	uint8_t triangle_frag_shader_data[] {
#embed "tex_image_frag.spv"
	};
	auto triangle_frag_shader = vkutil::load_shader_module(
	    std::span<uint8_t>(
	        triangle_frag_shader_data, sizeof(triangle_frag_shader_data)),
	    m_device);
	if (!triangle_frag_shader) {
		m_logger.err("Failed to load triangle frag shader");
	}

	vk::PushConstantRange push_constant_range {};
	push_constant_range.stageFlags = vk::ShaderStageFlagBits::eVertex;
	push_constant_range.offset = 0;
	push_constant_range.size = sizeof(GPUDrawPushConstants);

	vk::PipelineLayoutCreateInfo layout_ci {};
	layout_ci.pushConstantRangeCount = 1;
	layout_ci.pPushConstantRanges = &push_constant_range;
	auto single_layout = m_vk.single_image_descriptor_layout;
	layout_ci.setLayoutCount = 1;
	layout_ci.pSetLayouts = &single_layout;

	m_vk.mesh_pipeline_layout = m_device.createPipelineLayoutUnique(layout_ci);

	auto pip
	    = GraphicsPipelineBuilder { m_logger }
	          .set_pipeline_layout(m_vk.mesh_pipeline_layout.get())
	          .set_shaders(
	              triangle_vert_shader.get(), triangle_frag_shader.get())
	          .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
	          .set_polygon_mode(VK_POLYGON_MODE_FILL)
	          .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
	          .set_multisampling_none()
	          .disable_blending()
	          .enable_depth_testing()
	          .set_color_attachment_format(
	              static_cast<VkFormat>(m_vk.draw_image.format))
	          .set_depth_format(static_cast<VkFormat>(m_vk.depth_image.format))
	          .build(m_vkb.dev.device);
	m_vk.mesh_pipeline = vk::Pipeline { pip };
	m_vk.deletion_queue.emplace(
	    [this]() { m_device.destroyPipeline(m_vk.mesh_pipeline); });
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

	m_vk.imgui_descriptor_pool = m_device.createDescriptorPoolUnique(pool_info);

	ImGui::CreateContext();

	ImGui_ImplSDL3_InitForVulkan(m_window);

	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = m_vkb.instance;
	init_info.PhysicalDevice = m_vkb.phys_dev.physical_device;
	init_info.Device = m_vkb.dev.device;
	init_info.Queue = static_cast<VkQueue>(m_vk.graphics_queue);
	init_info.DescriptorPool = m_vk.imgui_descriptor_pool.get();
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.UseDynamicRendering = true;

	init_info.PipelineInfoMain.PipelineRenderingCreateInfo.sType
	    = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	init_info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount
	    = 1;
	auto swapchain_format = static_cast<VkFormat>(m_vk.swapchain_image_format);
	init_info.PipelineInfoMain.PipelineRenderingCreateInfo
	    .pColorAttachmentFormats
	    = &swapchain_format;

	init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&init_info);

	m_vk.deletion_queue.emplace([this]() {
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext();
		m_vk.imgui_descriptor_pool.reset();
	});
}

auto VulkanRenderer::default_data_init() -> void
{
	std::array<Vertex, 4> rect_vertices;

	rect_vertices[0].position = { 0.5, -0.5, 0 };
	rect_vertices[1].position = { 0.5, 0.5, 0 };
	rect_vertices[2].position = { -0.5, -0.5, 0 };
	rect_vertices[3].position = { -0.5, 0.5, 0 };

	rect_vertices[0].u = 1.0f;
	rect_vertices[0].v = 1.0f;
	rect_vertices[1].u = 1.0f;
	rect_vertices[1].v = 0.0f;
	rect_vertices[2].u = 0.0f;
	rect_vertices[2].v = 1.0f;
	rect_vertices[3].u = 0.0f;
	rect_vertices[3].v = 0.0f;

	for (auto &v : rect_vertices) {
		v.normal = { 0.0f, 0.0f, 1.0f };
	}

	rect_vertices[0].color = { 0, 0, 0, 1 };
	rect_vertices[1].color = { 0.5, 0.5, 0.5, 1 };
	rect_vertices[2].color = { 1, 0, 0, 1 };
	rect_vertices[3].color = { 0, 1, 0, 1 };

	std::array<uint32_t, 6> rect_indices;

	rect_indices[0] = 0;
	rect_indices[1] = 1;
	rect_indices[2] = 2;

	rect_indices[3] = 2;
	rect_indices[4] = 1;
	rect_indices[5] = 3;

	m_vk.rectangle = upload_mesh(rect_indices, rect_vertices);

	m_vk.test_meshes
	    = Mesh::load_gltf_meshes(*this, "assets/basicmesh.glb").value();

	m_vk.deletion_queue.emplace([&]() {
		for (auto &mesh : m_vk.test_meshes) {
			destroy_buffer(mesh->mesh_buffers.index_buffer);
			destroy_buffer(mesh->mesh_buffers.vertex_buffer);
		}

		destroy_buffer(m_vk.rectangle.index_buffer);
		destroy_buffer(m_vk.rectangle.vertex_buffer);
	});

	{
		// Solid color images
		auto const white = smath::pack_unorm4x8(smath::Vec4 { 1, 1, 1, 1 });
		m_vk.white_image = create_image(&white, vk::Extent3D { 1, 1, 1 },
		    vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled);

		auto const black = smath::pack_unorm4x8(smath::Vec4 { 0, 0, 0, 1 });
		m_vk.black_image = create_image(&black, vk::Extent3D { 1, 1, 1 },
		    vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled);

		auto const gray
		    = smath::pack_unorm4x8(smath::Vec4 { 0.6f, 0.6f, 0.6f, 1 });
		m_vk.gray_image = create_image(&gray, vk::Extent3D { 1, 1, 1 },
		    vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled);

		// Error checkerboard image
		auto const magenta = smath::pack_unorm4x8(smath::Vec4 { 1, 0, 1, 1 });
		std::array<uint32_t, 16 * 16> checkerboard;
		for (int x = 0; x < 16; x++) {
			for (int y = 0; y < 16; y++) {
				checkerboard[y * 16 + x]
				    = ((x % 2) ^ (y % 2)) ? magenta : black;
			}
		}
		m_vk.error_image
		    = create_image(checkerboard.data(), vk::Extent3D { 16, 16, 1 },
		        vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled);
	}

	vk::SamplerCreateInfo sampler_ci {};

	sampler_ci.magFilter = vk::Filter::eNearest;
	sampler_ci.minFilter = vk::Filter::eNearest;
	m_vk.default_sampler_nearest = m_device.createSamplerUnique(sampler_ci);

	sampler_ci.magFilter = vk::Filter::eLinear;
	sampler_ci.minFilter = vk::Filter::eLinear;
	m_vk.default_sampler_linear = m_device.createSamplerUnique(sampler_ci);

	m_vk.deletion_queue.emplace([&]() {
		m_vk.default_sampler_linear.reset();
		m_vk.default_sampler_nearest.reset();
		destroy_image(m_vk.error_image);
		destroy_image(m_vk.gray_image);
		destroy_image(m_vk.black_image);
		destroy_image(m_vk.white_image);
	});
}

auto VulkanRenderer::render() -> void
{
	defer(m_vk.frame_number++);

	if (!m_vk.swapchain || m_vk.swapchain_extent.width == 0
	    || m_vk.swapchain_extent.height == 0) {
		return;
	}

	VK_CHECK(m_logger,
	    m_device.waitForFences(
	        m_vk.get_current_frame().render_fence.get(), true, 1'000'000'000));
	auto raw_fence
	    = static_cast<VkFence>(m_vk.get_current_frame().render_fence.get());
	VK_CHECK(m_logger, vkResetFences(m_vkb.dev.device, 1, &raw_fence));

	auto const acquire_result = m_device.acquireNextImageKHR(m_vk.swapchain,
	    1'000'000'000, m_vk.get_current_frame().swapchain_semaphore.get(), {});
	if (acquire_result.result == vk::Result::eErrorOutOfDateKHR
	    || acquire_result.result == vk::Result::eSuboptimalKHR) {
		int width {}, height {};
		SDL_GetWindowSize(m_window, &width, &height);
		recreate_swapchain(
		    static_cast<uint32_t>(width), static_cast<uint32_t>(height));
		return;
	}
	VK_CHECK(m_logger, acquire_result.result);
	uint32_t const swapchain_image_idx { acquire_result.value };

	auto cmd { m_vk.get_current_frame().main_command_buffer.get() };
	cmd.reset();

	m_vk.draw_extent.width = m_vk.draw_image.extent.width;
	m_vk.draw_extent.height = m_vk.draw_image.extent.height;

	vk::CommandBufferBeginInfo cmd_begin_info {};
	cmd_begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
	VK_CHECK(m_logger,
	    vkBeginCommandBuffer(static_cast<VkCommandBuffer>(cmd),
	        reinterpret_cast<VkCommandBufferBeginInfo *>(&cmd_begin_info)));

	vkutil::transition_image(cmd, m_vk.draw_image.image,
	    vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);

	draw_background(cmd);

	vkutil::transition_image(cmd, m_vk.draw_image.image,
	    vk::ImageLayout::eGeneral, vk::ImageLayout::eColorAttachmentOptimal);
	vkutil::transition_image(cmd, m_vk.depth_image.image,
	    vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthAttachmentOptimal);

	draw_geometry(cmd);

	vkutil::transition_image(cmd, m_vk.draw_image.image,
	    vk::ImageLayout::eColorAttachmentOptimal,
	    vk::ImageLayout::eTransferSrcOptimal);

	vkutil::transition_image(cmd, m_vk.swapchain_images.at(swapchain_image_idx),
	    vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

	vkutil::copy_image_to_image(cmd, m_vk.draw_image.image,
	    m_vk.swapchain_images.at(swapchain_image_idx), m_vk.draw_extent,
	    m_vk.swapchain_extent);

	vkutil::transition_image(cmd, m_vk.swapchain_images[swapchain_image_idx],
	    vk::ImageLayout::eTransferDstOptimal,
	    vk::ImageLayout::eColorAttachmentOptimal);

	draw_imgui(cmd, m_vk.swapchain_image_views.at(swapchain_image_idx).get());

	vkutil::transition_image(cmd, m_vk.swapchain_images[swapchain_image_idx],
	    vk::ImageLayout::eColorAttachmentOptimal,
	    vk::ImageLayout::ePresentSrcKHR);

	cmd.end();

	auto render_semaphore
	    = m_vk.present_semaphores.at(swapchain_image_idx).get();
	vk::PipelineStageFlags2 wait_stage
	    = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
	auto wait_info { vkinit::semaphore_submit_info(
		wait_stage, m_vk.get_current_frame().swapchain_semaphore.get()) };
	auto command_buffer_info { vkinit::command_buffer_submit_info(cmd) };
	auto signal_info { vkinit::semaphore_submit_info(
		vk::PipelineStageFlagBits2::eAllCommands, render_semaphore) };
	auto submit_info { vkinit::submit_info2(
		&command_buffer_info, &wait_info, &signal_info) };

	m_vk.graphics_queue.submit2(
	    submit_info, m_vk.get_current_frame().render_fence.get());

	vk::PresentInfoKHR present_info {};
	present_info.setSwapchains(m_vk.swapchain);
	present_info.setWaitSemaphores(render_semaphore);
	present_info.setImageIndices(swapchain_image_idx);

	auto const present_result = m_vk.graphics_queue.presentKHR(present_info);
	if (present_result == vk::Result::eErrorOutOfDateKHR
	    || present_result == vk::Result::eSuboptimalKHR) {
		int width {}, height {};
		SDL_GetWindowSize(m_window, &width, &height);
		recreate_swapchain(
		    static_cast<uint32_t>(width), static_cast<uint32_t>(height));
		return;
	}
	VK_CHECK(m_logger, present_result);
}

auto VulkanRenderer::draw_background(vk::CommandBuffer cmd) -> void
{
	cmd.bindPipeline(
	    vk::PipelineBindPoint::eCompute, m_vk.gradient_pipeline.get());
	auto compute_set = vk::DescriptorSet { m_vk.draw_image_descriptors };
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
	    m_vk.gradient_pipeline_layout.get(), 0, compute_set, {});
	cmd.dispatch(
	    static_cast<uint32_t>(std::ceil(m_vk.draw_extent.width / 16.0)),
	    static_cast<uint32_t>(std::ceil(m_vk.draw_extent.height / 16.0)), 1);
}

auto VulkanRenderer::draw_geometry(vk::CommandBuffer cmd) -> void
{
	auto gpu_scene_data_buffer { create_buffer(sizeof(GPUSceneData),
		vk::BufferUsageFlagBits::eUniformBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU) };
	m_vk.get_current_frame().deletion_queue.emplace(
	    [=, this]() { destroy_buffer(gpu_scene_data_buffer); });

	VmaAllocationInfo info {};
	vmaGetAllocationInfo(
	    m_vk.allocator, gpu_scene_data_buffer.allocation, &info);

	GPUSceneData *scene_uniform_data
	    = reinterpret_cast<GPUSceneData *>(info.pMappedData);
	if (!scene_uniform_data) {
		VkResult res = vmaMapMemory(m_vk.allocator,
		    gpu_scene_data_buffer.allocation, (void **)&scene_uniform_data);
		assert(res == VK_SUCCESS);
	}
	defer({
		if (info.pMappedData == nullptr) {
			vmaUnmapMemory(m_vk.allocator, gpu_scene_data_buffer.allocation);
		}
	});

	*scene_uniform_data = m_vk.scene_data;

	auto const global_desc {
		m_vk.get_current_frame().frame_descriptors.allocate(
		    m_logger, m_vkb.dev.device, m_vk.gpu_scene_data_descriptor_layout)
	};

	DescriptorWriter writer;
	writer.write_buffer(0, gpu_scene_data_buffer.buffer, sizeof(GPUSceneData),
	    0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.update_set(m_vkb.dev.device, global_desc);

	auto color_att { vkinit::attachment_info(m_vk.draw_image.image_view,
		nullptr, vk::ImageLayout::eColorAttachmentOptimal) };
	auto depth_att { vkinit::depth_attachment_info(m_vk.depth_image.image_view,
		vk::ImageLayout::eDepthAttachmentOptimal) };
	auto const render_info { vkinit::render_info(
		m_vk.draw_extent, &color_att, &depth_att) };

	cmd.beginRendering(render_info);

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_vk.triangle_pipeline);

	vk::Viewport viewport {};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = static_cast<float>(m_vk.draw_extent.width);
	viewport.height = static_cast<float>(m_vk.draw_extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	cmd.setViewport(0, viewport);

	vk::Rect2D scissor {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent = m_vk.draw_extent;
	cmd.setScissor(0, scissor);

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_vk.mesh_pipeline);

	auto const image_set { m_vk.get_current_frame().frame_descriptors.allocate(
		m_logger, m_vkb.dev.device, m_vk.single_image_descriptor_layout) };
	DescriptorWriter()
	    .write_image(0, m_vk.error_image.image_view,
	        m_vk.default_sampler_nearest.get(),
	        static_cast<VkImageLayout>(vk::ImageLayout::eShaderReadOnlyOptimal),
	        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
	    .update_set(m_vkb.dev.device, image_set);

	auto vk_image_set = vk::DescriptorSet { image_set };
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
	    m_vk.mesh_pipeline_layout.get(), 0, vk_image_set, {});

	auto view { smath::matrix_look_at(smath::Vec3 { 0.0f, 0.0f, 3.0f },
		smath::Vec3 { 0.0f, 0.0f, 0.0f }, smath::Vec3 { 0.0f, 1.0f, 0.0f },
		false) };
	auto projection {
		smath::matrix_perspective(smath::deg(70.0f),
		    static_cast<float>(m_vk.draw_extent.width)
		        / static_cast<float>(m_vk.draw_extent.height),
		    0.1f, 10000.0f),
	};
	projection[1][1] *= -1;
	auto view_projection { projection * view };

	GPUDrawPushConstants push_constants;
	auto rect_model { smath::scale(
		smath::translate(smath::Vec3 { 0.0f, 0.0f, -5.0f }),
		smath::Vec3 { 5.0f, 5.0f, 1.0f }) };
	push_constants.world_matrix = view_projection * rect_model;
	push_constants.vertex_buffer = m_vk.rectangle.vertex_buffer_address;

	cmd.pushConstants(m_vk.mesh_pipeline_layout.get(),
	    vk::ShaderStageFlagBits::eVertex, 0, sizeof(push_constants),
	    &push_constants);
	cmd.bindIndexBuffer(
	    m_vk.rectangle.index_buffer.buffer, 0, vk::IndexType::eUint32);

	cmd.drawIndexed(6, 1, 0, 0, 0);

	push_constants.vertex_buffer
	    = m_vk.test_meshes[2]->mesh_buffers.vertex_buffer_address;

	auto model { smath::Mat4::identity() };
	push_constants.world_matrix = view_projection * model;

	cmd.pushConstants(m_vk.mesh_pipeline_layout.get(),
	    vk::ShaderStageFlagBits::eVertex, 0, sizeof(push_constants),
	    &push_constants);
	cmd.bindIndexBuffer(m_vk.test_meshes[2]->mesh_buffers.index_buffer.buffer,
	    0, vk::IndexType::eUint32);

	cmd.drawIndexed(m_vk.test_meshes[2]->surfaces[0].count, 1,
	    m_vk.test_meshes[2]->surfaces[0].start_index, 0, 0);

	cmd.endRendering();
}

auto VulkanRenderer::draw_imgui(
    vk::CommandBuffer cmd, vk::ImageView target_image_view) -> void
{
	auto const color_attachment { vkinit::attachment_info(
		target_image_view, nullptr, vk::ImageLayout::eColorAttachmentOptimal) };
	auto const render_info { vkinit::render_info(
		m_vk.draw_extent, &color_attachment, nullptr) };

	cmd.beginRendering(render_info);

	ImGui_ImplVulkan_RenderDrawData(
	    ImGui::GetDrawData(), static_cast<VkCommandBuffer>(cmd));

	cmd.endRendering();
}

auto VulkanRenderer::create_swapchain(uint32_t width, uint32_t height) -> void
{
	vkb::SwapchainBuilder builder { m_vkb.phys_dev, m_vkb.dev, m_vk.surface };
	m_vk.swapchain_image_format = vk::Format::eB8G8R8A8Unorm;
	auto const swapchain_ret { builder
		    .set_desired_format({
		        .format = static_cast<VkFormat>(m_vk.swapchain_image_format),
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
	m_vk.swapchain_extent = vk::Extent2D { m_vkb.swapchain.extent.width,
		m_vkb.swapchain.extent.height };
	auto images = m_vkb.swapchain.get_images().value();
	m_vk.swapchain_images.assign(images.begin(), images.end());

	m_vk.swapchain_image_views.clear();
	for (auto img : m_vk.swapchain_images) {
		vk::ImageViewCreateInfo iv_ci {};
		iv_ci.image = img;
		iv_ci.viewType = vk::ImageViewType::e2D;
		iv_ci.format = m_vk.swapchain_image_format;
		iv_ci.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		iv_ci.subresourceRange.levelCount = 1;
		iv_ci.subresourceRange.layerCount = 1;
		m_vk.swapchain_image_views.emplace_back(
		    m_device.createImageViewUnique(iv_ci));
	}

	vk::SemaphoreCreateInfo semaphore_ci {};
	m_vk.present_semaphores.resize(m_vk.swapchain_images.size());
	for (auto &semaphore : m_vk.present_semaphores) {
		semaphore = m_device.createSemaphoreUnique(semaphore_ci);
	}
}

auto VulkanRenderer::create_draw_image(uint32_t width, uint32_t height) -> void
{
	destroy_draw_image();

	auto const flags { vk::ImageUsageFlagBits::eTransferSrc
		| vk::ImageUsageFlagBits::eTransferDst
		| vk::ImageUsageFlagBits::eStorage
		| vk::ImageUsageFlagBits::eColorAttachment };
	m_vk.draw_image = create_image(
	    { width, height, 1 }, vk::Format::eR16G16B16A16Sfloat, flags);
}

auto VulkanRenderer::create_depth_image(uint32_t width, uint32_t height) -> void
{
	destroy_depth_image();

	auto const flags { vk::ImageUsageFlagBits::eTransferSrc
		| vk::ImageUsageFlagBits::eTransferDst
		| vk::ImageUsageFlagBits::eDepthStencilAttachment };
	m_vk.depth_image
	    = create_image({ width, height, 1 }, vk::Format::eD32Sfloat, flags);
}

auto VulkanRenderer::destroy_depth_image() -> void
{
	if (m_vk.depth_image.image) {
		m_device.destroyImageView(m_vk.depth_image.image_view);
		m_vk.depth_image.image_view = vk::ImageView {};
		vmaDestroyImage(m_vk.allocator,
		    static_cast<VkImage>(m_vk.depth_image.image),
		    m_vk.depth_image.allocation);
		m_vk.depth_image.image = vk::Image {};
		m_vk.depth_image.allocation = nullptr;
		m_vk.depth_image.extent = vk::Extent3D { 0, 0, 0 };
	}
}

auto VulkanRenderer::update_draw_image_descriptor() -> void
{
	DescriptorWriter()
	    .write_image(0, m_vk.draw_image.image_view, VK_NULL_HANDLE,
	        VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
	    .update_set(m_vkb.dev.device, m_vk.draw_image_descriptors);
}

auto VulkanRenderer::destroy_draw_image() -> void
{
	if (m_vk.draw_image.image) {
		m_device.destroyImageView(m_vk.draw_image.image_view);
		m_vk.draw_image.image_view = vk::ImageView {};
		vmaDestroyImage(m_vk.allocator,
		    static_cast<VkImage>(m_vk.draw_image.image),
		    m_vk.draw_image.allocation);
		m_vk.draw_image.image = vk::Image {};
		m_vk.draw_image.allocation = nullptr;
		m_vk.draw_image.extent = vk::Extent3D { 0, 0, 0 };
	}
}

auto VulkanRenderer::recreate_swapchain(uint32_t width, uint32_t height) -> void
{
	m_device.waitIdle();

	if (width == 0 || height == 0) {
		destroy_swapchain();
		destroy_draw_image();
		destroy_depth_image();
		m_vk.swapchain_extent = vk::Extent2D { 0, 0 };
		return;
	}

	destroy_swapchain();
	destroy_draw_image();
	destroy_depth_image();

	create_swapchain(width, height);
	create_draw_image(width, height);
	create_depth_image(width, height);
	update_draw_image_descriptor();
}

auto VulkanRenderer::destroy_swapchain() -> void
{
	if (!m_vk.swapchain)
		return;

	m_vk.present_semaphores.clear();
	m_device.destroySwapchainKHR(m_vk.swapchain);

	m_vk.swapchain = vk::SwapchainKHR {};
	m_vk.swapchain_image_views.clear();
	m_vk.swapchain_images.clear();
	m_vk.present_semaphores.clear();
	m_vk.swapchain_extent = vk::Extent2D { 0, 0 };
}

auto VulkanRenderer::create_image(vk::Extent3D size, vk::Format format,
    vk::ImageUsageFlags flags, bool mipmapped) -> AllocatedImage
{
	AllocatedImage new_image;
	new_image.format = format;
	new_image.extent = size;

	auto img_ci { vkinit::image_create_info(format, flags, size) };
	if (mipmapped) {
		img_ci.mipLevels = static_cast<uint32_t>(std::floor(
		                       std::log2(std::max(size.width, size.height))))
		    + 1;
	}

	VmaAllocationCreateInfo alloc_ci {};
	alloc_ci.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	alloc_ci.requiredFlags
	    = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VK_CHECK(m_logger,
	    vmaCreateImage(m_vk.allocator,
	        reinterpret_cast<VkImageCreateInfo const *>(&img_ci), &alloc_ci,
	        reinterpret_cast<VkImage *>(&new_image.image),
	        &new_image.allocation, nullptr));

	vk::ImageAspectFlags aspect_flag { vk::ImageAspectFlagBits::eColor };
	if (format == vk::Format::eD32Sfloat) {
		aspect_flag = vk::ImageAspectFlagBits::eDepth;
	}

	auto const view_ci { vkinit::imageview_create_info(
		format, new_image.image, aspect_flag) };
	new_image.image_view = m_device.createImageView(view_ci);

	return new_image;
}

auto VulkanRenderer::create_image(void const *data, vk::Extent3D size,
    vk::Format format, vk::ImageUsageFlags flags, bool mipmapped)
    -> AllocatedImage
{
	size_t data_size {
		static_cast<uint32_t>(size.depth) * static_cast<uint32_t>(size.width)
		    * static_cast<uint32_t>(size.height) * 4,
	};
	auto const upload_buffer {
		create_buffer(data_size, vk::BufferUsageFlagBits::eTransferSrc,
		    VMA_MEMORY_USAGE_CPU_TO_GPU),
	};

	VmaAllocationInfo info {};
	vmaGetAllocationInfo(m_vk.allocator, upload_buffer.allocation, &info);

	void *mapped_data { reinterpret_cast<GPUSceneData *>(info.pMappedData) };
	bool mapped_here { false };
	if (!mapped_data) {
		VkResult res = vmaMapMemory(
		    m_vk.allocator, upload_buffer.allocation, (void **)&mapped_data);
		assert(res == VK_SUCCESS);
		mapped_here = true;
	}

	memcpy(mapped_data, data, data_size);

	auto const new_image {
		create_image(size, format,
		    flags | vk::ImageUsageFlagBits::eTransferDst
		        | vk::ImageUsageFlagBits::eTransferSrc,
		    mipmapped),
	};

	immediate_submit([&](vk::CommandBuffer cmd) {
		vkutil::transition_image(cmd, new_image.image,
		    vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

		vk::BufferImageCopy copy_region {};
		copy_region.imageSubresource.aspectMask
		    = vk::ImageAspectFlagBits::eColor;
		copy_region.imageSubresource.mipLevel = 0;
		copy_region.imageSubresource.baseArrayLayer = 0;
		copy_region.imageSubresource.layerCount = 1;
		copy_region.imageExtent = size;

		cmd.copyBufferToImage(upload_buffer.buffer, new_image.image,
		    vk::ImageLayout::eTransferDstOptimal, copy_region);

		vkutil::transition_image(cmd, new_image.image,
		    vk::ImageLayout::eTransferDstOptimal,
		    vk::ImageLayout::eShaderReadOnlyOptimal);
	});

	if (mapped_here) {
		vmaUnmapMemory(m_vk.allocator, upload_buffer.allocation);
	}
	destroy_buffer(upload_buffer);

	return new_image;
}

auto VulkanRenderer::destroy_image(AllocatedImage const &img) -> void
{
	if (img.image_view) {
		m_device.destroyImageView(img.image_view);
	}
	vmaDestroyImage(
	    m_vk.allocator, static_cast<VkImage>(img.image), img.allocation);
}

auto VulkanRenderer::create_buffer(size_t alloc_size,
    vk::BufferUsageFlags usage, VmaMemoryUsage memory_usage) -> AllocatedBuffer
{
	vk::BufferCreateInfo buffer_ci {};
	buffer_ci.size = alloc_size;
	buffer_ci.usage = usage;
	buffer_ci.sharingMode = vk::SharingMode::eExclusive;

	VmaAllocationCreateInfo alloc_ci {};
	alloc_ci.usage = memory_usage;
	alloc_ci.flags = 0;
	if (memory_usage == VMA_MEMORY_USAGE_CPU_ONLY) {
		alloc_ci.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT
		    | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	}

	AllocatedBuffer buffer;
	VK_CHECK(m_logger,
	    vmaCreateBuffer(m_vk.allocator,
	        reinterpret_cast<VkBufferCreateInfo const *>(&buffer_ci), &alloc_ci,
	        reinterpret_cast<VkBuffer *>(&buffer.buffer), &buffer.allocation,
	        &buffer.info));

	return buffer;
}

auto VulkanRenderer::destroy_buffer(AllocatedBuffer const &buffer) -> void
{
	vmaDestroyBuffer(m_vk.allocator, buffer.buffer, buffer.allocation);
}

auto VulkanRenderer::upload_mesh(
    std::span<uint32_t> indices, std::span<Vertex> vertices) -> GPUMeshBuffers
{
	auto const vertex_buffer_size { vertices.size() * sizeof(Vertex) };
	auto const index_buffer_size { indices.size() * sizeof(uint32_t) };

	GPUMeshBuffers new_surface;
	new_surface.vertex_buffer = create_buffer(vertex_buffer_size,
	    vk::BufferUsageFlagBits::eVertexBuffer
	        | vk::BufferUsageFlagBits::eTransferDst
	        | vk::BufferUsageFlagBits::eShaderDeviceAddress,
	    VMA_MEMORY_USAGE_GPU_ONLY);

	vk::BufferDeviceAddressInfo device_address_info {};
	device_address_info.buffer = new_surface.vertex_buffer.buffer;

	new_surface.vertex_buffer_address
	    = m_device.getBufferAddress(device_address_info);

	new_surface.index_buffer = create_buffer(index_buffer_size,
	    vk::BufferUsageFlagBits::eIndexBuffer
	        | vk::BufferUsageFlagBits::eTransferDst
	        | vk::BufferUsageFlagBits::eShaderDeviceAddress,
	    VMA_MEMORY_USAGE_GPU_ONLY);

	auto staging { create_buffer(vertex_buffer_size + index_buffer_size,
		vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_CPU_ONLY) };

	VmaAllocationInfo info {};
	vmaGetAllocationInfo(m_vk.allocator, staging.allocation, &info);

	void *data = info.pMappedData;
	bool mapped_here { false };
	if (!data) {
		VkResult res = vmaMapMemory(m_vk.allocator, staging.allocation, &data);
		assert(res == VK_SUCCESS);
		mapped_here = true;
	}

	memcpy(data, vertices.data(), vertex_buffer_size);
	memcpy(reinterpret_cast<void *>(
	           reinterpret_cast<size_t>(data) + vertex_buffer_size),
	    indices.data(), index_buffer_size);

	immediate_submit([&](vk::CommandBuffer cmd) {
		vk::BufferCopy vertex_copy {};
		vertex_copy.dstOffset = 0;
		vertex_copy.srcOffset = 0;
		vertex_copy.size = vertex_buffer_size;

		cmd.copyBuffer(
		    staging.buffer, new_surface.vertex_buffer.buffer, vertex_copy);

		vk::BufferCopy index_copy {};
		index_copy.dstOffset = 0;
		index_copy.srcOffset = vertex_buffer_size;
		index_copy.size = index_buffer_size;

		cmd.copyBuffer(
		    staging.buffer, new_surface.index_buffer.buffer, index_copy);
	});

	if (mapped_here) {
		vmaUnmapMemory(m_vk.allocator, staging.allocation);
	}
	destroy_buffer(staging);

	return new_surface;
}

} // namespace Lunar
