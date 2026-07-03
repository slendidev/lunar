#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include <SDL3/SDL_video.h>
#include <VkBootstrap.h>
#include <smath/smath.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include "CPUTexture.h"
#include "Colors.h"
#include "DeletionQueue.h"
#include "Loader.h"
#include "Logger.h"
#include "Pipeline.h"
#include "Types.h"

namespace Lunar {

struct GPUDrawPushConstants {
	smath::Mat4 world_matrix;
	vk::DeviceAddress vertex_buffer;
};

constexpr unsigned FRAME_OVERLAP = 2;

struct VulkanRenderer {
	struct ScreenshotPixels {
		std::span<std::uint8_t const> pixels;
		vk::Extent2D extent;
	};

	struct GL {

		enum class GeometryKind {
			Triangles,
			TriangleStrip,
			TriangleFan,
			Quads
		};

		explicit GL(VulkanRenderer &renderer);

		auto begin_drawing(vk::CommandBuffer cmd, AllocatedImage &color_target,
		    AllocatedImage *depth_target = nullptr) -> void;
		auto end_drawing() -> void;

		auto begin(GeometryKind kind) -> void;
		template<size_t N>
		auto vert(smath::Vec<N, float> const &position) -> void
		{
			static_assert(N == 2 || N == 3 || N == 4,
			    "Position must be a 2D, 3D, or 4D vec");

			smath::Vec3 pos { 0.0f, 0.0f, 0.0f };
			pos[0] = position[0];
			pos[1] = position[1];
			if constexpr (N >= 3) {
				pos[2] = position[2];
			}

			push_vertex(pos);
		}

		auto color(smath::Vec3 const &rgb) -> void;
		auto color(smath::Vec4 const &rgba) -> void;
		auto uv(smath::Vec2 const &uv) -> void;
		auto normal(smath::Vec3 const &normal) -> void;

		auto set_texture(std::optional<AllocatedImage const *> texture
		    = std::nullopt) -> void;
		auto set_culling(bool enabled) -> void;
		auto draw_rectangle(smath::Vec2 pos, smath::Vec2 size,
		    smath::Vec4 color = smath::Vec4 { Colors::WHITE, 1.0f },
		    float rotation = 0.0f) -> void;
		auto draw_sphere(smath::Vec3 center, float radius, int rings = 16,
		    int segments = 32, std::optional<smath::Vec4> sphere_color = {})
		    -> void;
		auto draw_texture_cyl(AllocatedImage const *texture,
		    smath::Vec3 sphere_center, PolarCoordinate coord, float rad,
		    float scale, bool y_flip) -> void;
		auto end() -> void;
		auto flush() -> void;

		auto use_pipeline(Pipeline &pipeline) -> void;
		auto set_transform(smath::Mat4 const &transform) -> void;
		auto push_transform() -> void;
		auto pop_transform() -> void;
		auto draw_mesh(GPUMeshBuffers const &mesh, smath::Mat4 const &transform,
		    uint32_t index_count, uint32_t first_index = 0,
		    int32_t vertex_offset = 0) -> void;
		auto draw_indexed(Pipeline &pipeline, vk::DescriptorSet descriptor_set,
		    AllocatedBuffer const &vertex_buffer,
		    AllocatedBuffer const &index_buffer, uint32_t index_count,
		    std::span<std::byte const> push_constants) -> void;

	private:
		auto push_vertex(smath::Vec3 const &pos) -> void;
		auto emit_indices(size_t start, size_t count) -> void;
		auto bind_pipeline_if_needed() -> void;

		VulkanRenderer &m_renderer;
		vk::CommandBuffer m_cmd {};
		AllocatedImage *m_color_target { nullptr };
		AllocatedImage *m_depth_target { nullptr };
		GeometryKind m_current_kind { GeometryKind::Triangles };
		bool m_inside_primitive { false };
		bool m_drawing { false };
		Pipeline *m_active_pipeline { nullptr };
		bool m_culling_enabled { true };
		smath::Mat4 m_transform { smath::Mat4::identity() };
		std::vector<smath::Mat4> m_transform_stack;
		smath::Vec4 m_current_color { 1.0f, 1.0f, 1.0f, 1.0f };
		smath::Vec3 m_current_normal { 0.0f, 0.0f, 1.0f };
		smath::Vec2 m_current_uv { 0.0f, 0.0f };
		AllocatedImage const *m_bound_texture { nullptr };
		size_t m_primitive_start { 0 };
		std::vector<Vertex> m_vertices;
		std::vector<uint32_t> m_indices;
	};

	enum class AntiAliasingKind {
		NONE,
		MSAA_2X,
		MSAA_4X,
		MSAA_8X,
	};

	struct KmsSurfaceConfig { };

	VulkanRenderer(SDL_Window *window, Logger &logger,
	    std::span<std::string const> instance_extensions = {},
	    std::span<std::string const> device_extensions = {});
	VulkanRenderer(KmsSurfaceConfig config, Logger &logger,
	    std::span<std::string const> instance_extensions = {},
	    std::span<std::string const> device_extensions = {});
	~VulkanRenderer();

	auto render(std::function<void(GL &)> const &record = {}) -> void;
	auto render_to_image(vk::Image target_image, vk::Extent2D target_extent,
	    std::function<void(GL &)> const &record = {}) -> void;
	auto resize(uint32_t width, uint32_t height) -> void;
	auto set_offscreen_extent(vk::Extent2D extent) -> void;
	auto set_antialiasing(AntiAliasingKind kind) -> void;
	auto set_antialiasing_immediate(AntiAliasingKind kind) -> void;
	auto antialiasing() const -> AntiAliasingKind
	{
		return m_vk.antialiasing_kind;
	}

	auto immediate_submit(std::function<void(vk::CommandBuffer cmd)> &&function,
	    bool flush_frame_deletion_queue = true,
	    bool clear_frame_descriptors = true) -> void;
	auto upload_mesh(std::span<uint32_t> indices, std::span<Vertex> vertices)
	    -> GPUMeshBuffers;
	auto destroy_buffer(AllocatedBuffer const &buffer) -> void;
	auto create_image(CPUTexture const &texture, vk::ImageUsageFlags flags,
	    bool mipmapped = false) -> AllocatedImage;
	auto create_cubemap(std::span<uint8_t const> pixels, uint32_t face_size,
	    vk::Format format, vk::ImageUsageFlags flags) -> AllocatedImage;
	auto destroy_image(AllocatedImage const &img) -> void;
	auto destroy_image_later(AllocatedImage img) -> void;
	auto rectangle_mesh() const -> GPUMeshBuffers const &
	{
		return m_vk.rectangle;
	}
	auto white_texture() const -> AllocatedImage const &
	{
		return m_vk.white_image;
	}
	auto gray_texture() const -> AllocatedImage const &
	{
		return m_vk.gray_image;
	}
	auto black_texture() const -> AllocatedImage const &
	{
		return m_vk.black_image;
	}
	auto error_texture() const -> AllocatedImage const &
	{
		return m_vk.error_image;
	}
	auto draw_extent() const -> vk::Extent2D { return m_vk.draw_extent; }
	auto mesh_pipeline() -> Pipeline & { return m_vk.mesh_pipeline; }
	auto wayland_pipeline() -> Pipeline & { return m_vk.wayland_pipeline; }
	auto triangle_pipeline() -> Pipeline & { return m_vk.triangle_pipeline; }
	auto device() const -> vk::Device { return m_device; }
	auto instance() const -> vk::Instance { return m_instance; }
	auto physical_device() const -> vk::PhysicalDevice
	{
		return m_physical_device;
	}
	auto graphics_queue() const -> vk::Queue { return m_vk.graphics_queue; }
	auto graphics_queue_family() const -> uint32_t
	{
		return m_vk.graphics_queue_family;
	}
	auto draw_image_format() const -> vk::Format
	{
		return m_vk.draw_image.format;
	}
	auto depth_image_format() const -> vk::Format
	{
		return m_vk.depth_image.format;
	}
	auto msaa_samples() const -> vk::SampleCountFlagBits
	{
		return m_vk.msaa_samples;
	}
	auto single_image_descriptor_layout() const -> vk::DescriptorSetLayout
	{
		return m_vk.single_image_descriptor_layout;
	}
	auto gl_api() -> GL & { return gl; }
	auto get_screenshot() const -> std::optional<AllocatedImage>
	{
		return m_latest_screenshot;
	}
	auto set_imgui_enabled(bool enabled) -> void { m_imgui_enabled = enabled; }
	auto imgui_enabled() const -> bool { return m_imgui_enabled; }
	auto request_screenshot() -> void { m_screenshot_requested = true; }
	auto get_screenshot_pixels() const
	    -> std::optional<VulkanRenderer::ScreenshotPixels>
	{
		if (m_latest_screenshot_pixels.empty()
		    || m_latest_screenshot_extent.width == 0
		    || m_latest_screenshot_extent.height == 0) {
			return {};
		}

		auto const span { std::span<std::uint8_t const> {
			m_latest_screenshot_pixels.data(),
			m_latest_screenshot_pixels.size() } };
		return ScreenshotPixels { span, m_latest_screenshot_extent };
	}

	auto logger() const -> Logger & { return m_logger; }

	GL gl;

	std::optional<AllocatedImage> m_latest_screenshot {};
	std::vector<std::uint8_t> m_latest_screenshot_pixels {};
	vk::Extent2D m_latest_screenshot_extent {};
	vk::ImageLayout m_latest_screenshot_layout { vk::ImageLayout::eUndefined };
	bool m_screenshot_requested { false };

private:
	struct RenderCommand {
		struct SetAntiAliasing {
			AntiAliasingKind kind;
		};

		std::variant<SetAntiAliasing> payload;
	};

	auto vk_init() -> void;
	auto swapchain_init() -> void;
	auto setup_kms_surface() -> void;
	auto commands_init() -> void;
	auto sync_init() -> void;
	auto descriptors_init() -> void;
	auto pipelines_init() -> void;
	auto triangle_pipeline_init() -> void;
	auto mesh_pipeline_init() -> void;
	auto imgui_init() -> void;
	auto default_data_init() -> void;

	auto draw_imgui(vk::CommandBuffer cmd, vk::ImageView target_image_view)
	    -> void;
	auto log_memory_stats() -> void;

	auto create_swapchain(uint32_t width, uint32_t height) -> void;
	auto create_draw_image(uint32_t width, uint32_t height) -> void;
	auto create_msaa_color_image(uint32_t width, uint32_t height) -> void;
	auto destroy_draw_image() -> void;
	auto create_depth_image(uint32_t width, uint32_t height) -> void;
	auto destroy_depth_image() -> void;
	auto destroy_msaa_color_image() -> void;
	auto recreate_swapchain(uint32_t width, uint32_t height) -> void;
	auto destroy_swapchain() -> void;
	auto ensure_screenshot_buffers(vk::Extent2D extent) -> void;
	auto destroy_screenshot_buffers() -> void;
	auto emit_frame_screenshot(FrameData &frame) -> void;
#if defined(TRACY_ENABLE)
	auto ensure_tracy_frame_buffers(vk::Extent2D extent) -> void;
	auto destroy_tracy_frame_buffers() -> void;
	auto emit_tracy_frame_image(FrameData &frame) -> void;
#endif
	auto create_image(vk::Extent3D size, vk::Format format,
	    vk::ImageUsageFlags flags,
	    vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1,
	    bool mipmapped = false) -> AllocatedImage;
	auto create_image_no_view(vk::Extent3D size, vk::Format format,
	    vk::ImageUsageFlags flags,
	    vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1,
	    bool mipmapped = false) -> AllocatedImage;
	auto create_image(void const *data, vk::Extent3D size, vk::Format format,
	    vk::ImageUsageFlags flags, bool mipmapped = false) -> AllocatedImage;

	auto create_buffer(size_t alloc_size, vk::BufferUsageFlags usage,
	    VmaMemoryUsage memory_usage) -> AllocatedBuffer;
	auto enqueue_render_command(RenderCommand &&command) -> void;
	auto process_render_commands() -> void;
	auto apply_antialiasing(AntiAliasingKind kind) -> void;

	vk::Instance m_instance {};
	vk::PhysicalDevice m_physical_device {};
	vk::Device m_device {};

	struct {
		vkb::Instance instance;
		vkb::PhysicalDevice phys_dev;
		vkb::Device dev;
		vkb::Swapchain swapchain;
	} m_vkb;

	struct {
		auto get_current_frame() -> FrameData &
		{
			return frames.at(frame_number % frames.size());
		}

		vk::SwapchainKHR swapchain {};
		vk::SurfaceKHR surface {};
		vk::Format swapchain_image_format {};

		uint32_t graphics_queue_family { 0 };
		vk::Queue graphics_queue {};

		std::vector<vk::Image> swapchain_images;
		std::vector<vk::UniqueImageView> swapchain_image_views;
		std::vector<vk::UniqueSemaphore> present_semaphores;
		vk::Extent2D swapchain_extent;

		std::array<FrameData, FRAME_OVERLAP> frames;
		AllocatedImage draw_image {};
		vk::ImageLayout draw_image_layout { vk::ImageLayout::eUndefined };
		AllocatedImage msaa_color_image {};
		vk::ImageLayout msaa_color_image_layout { vk::ImageLayout::eUndefined };
		AllocatedImage depth_image {};
		vk::ImageLayout depth_image_layout { vk::ImageLayout::eUndefined };
#if defined(TRACY_ENABLE)
		AllocatedImage tracy_capture_image {};
		vk::ImageLayout tracy_capture_image_layout {
			vk::ImageLayout::eUndefined
		};
		vk::Extent2D tracy_capture_extent {};
#endif
		vk::Extent2D draw_extent {};
		AntiAliasingKind antialiasing_kind { AntiAliasingKind::NONE };
		vk::SampleCountFlagBits msaa_samples { vk::SampleCountFlagBits::e1 };
		vk::SampleCountFlags supported_framebuffer_samples {};

		VmaAllocator allocator;

		GPUSceneData scene_data {};
		vk::DescriptorSetLayout gpu_scene_data_descriptor_layout {};

		vk::DescriptorSetLayout single_image_descriptor_layout {};

		Pipeline triangle_pipeline;
		Pipeline triangle_pipeline_culled;
		Pipeline mesh_pipeline;
		Pipeline mesh_pipeline_culled;
		Pipeline wayland_pipeline;

		GPUMeshBuffers rectangle;

		vk::UniqueDescriptorPool imgui_descriptor_pool;

		DeletionQueue deletion_queue;

		vk::UniqueFence imm_fence;
		vk::UniqueCommandBuffer imm_command_buffer;
		vk::UniqueCommandPool imm_command_pool;

		uint64_t frame_number { 0 };

		AllocatedImage white_image {};
		AllocatedImage black_image {};
		AllocatedImage gray_image {};
		AllocatedImage error_image {};

		vk::UniqueSampler default_sampler_linear;
		vk::UniqueSampler default_sampler_nearest;
	} m_vk;

	struct KmsState {
		vk::DisplayKHR display {};
		vk::DisplayModeKHR mode {};
		vk::Extent2D extent {};
		uint32_t plane_index { 0 };
		uint32_t plane_stack_index { 0 };
		std::string display_name {};
	};

	SDL_Window *m_window { nullptr };
	Logger &m_logger;
	std::mutex m_command_mutex;
	std::vector<RenderCommand> m_pending_render_commands;
	bool m_use_kms { false };
	bool m_imgui_enabled { true };
	bool m_mem_stats_enabled { false };
	std::chrono::steady_clock::time_point m_last_mem_stats {};
	std::optional<KmsState> m_kms_state {};
	vk::PhysicalDevice m_kms_physical_device {};
	vk::Extent2D m_kms_extent {};
	bool m_kms_physical_device_set { false };
	std::vector<std::string> m_extra_instance_extensions {};
	std::vector<std::string> m_extra_device_extensions {};
};

} // namespace Lunar
