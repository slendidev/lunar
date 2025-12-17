#pragma once

#include <array>
#include <functional>
#include <mutex>
#include <optional>
#include <variant>
#include <vector>

#include <SDL3/SDL_video.h>
#include <VkBootstrap.h>
#include <smath.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

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
	enum class AntiAliasingKind {
		NONE,
		MSAA_2X,
		MSAA_4X,
		MSAA_8X,
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
		auto draw_rectangle(smath::Vec2 pos, smath::Vec2 size,
		    smath::Vec4 color = smath::Vec4 { Colors::WHITE, 1.0f },
		    float rotation = 0.0f) -> void;
		auto end() -> void;
		auto flush() -> void;

		auto use_pipeline(Pipeline &pipeline) -> void;
		auto set_transform(smath::Mat4 const &transform) -> void;
		auto draw_mesh(GPUMeshBuffers const &mesh, smath::Mat4 const &transform,
		    uint32_t index_count, uint32_t first_index = 0,
		    int32_t vertex_offset = 0) -> void;

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
		smath::Mat4 m_transform { smath::Mat4::identity() };
		smath::Vec4 m_current_color { 1.0f, 1.0f, 1.0f, 1.0f };
		smath::Vec3 m_current_normal { 0.0f, 0.0f, 1.0f };
		smath::Vec2 m_current_uv { 0.0f, 0.0f };
		AllocatedImage const *m_bound_texture { nullptr };
		size_t m_primitive_start { 0 };
		std::vector<Vertex> m_vertices;
		std::vector<uint32_t> m_indices;
	};

	VulkanRenderer(SDL_Window *window, Logger &logger);
	~VulkanRenderer();

	auto render(std::function<void(GL &)> const &record = {}) -> void;
	auto resize(uint32_t width, uint32_t height) -> void;
	auto set_antialiasing(AntiAliasingKind kind) -> void;
	auto antialiasing() const -> AntiAliasingKind
	{
		return m_vk.antialiasing_kind;
	}

	auto immediate_submit(std::function<void(vk::CommandBuffer cmd)> &&function,
	    bool flush_frame_deletion_queue = true,
	    bool clear_frame_descriptors = true) -> void;
	auto upload_mesh(std::span<uint32_t> indices, std::span<Vertex> vertices)
	    -> GPUMeshBuffers;
	auto rectangle_mesh() const -> GPUMeshBuffers const &
	{
		return m_vk.rectangle;
	}
	auto test_meshes() const -> std::vector<std::shared_ptr<Mesh>> const &
	{
		return m_vk.test_meshes;
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
	auto triangle_pipeline() -> Pipeline & { return m_vk.triangle_pipeline; }
	auto gl_api() -> GL & { return gl; }

	auto logger() const -> Logger & { return m_logger; }

	GL gl;

private:
	struct RenderCommand {
		struct SetAntiAliasing {
			AntiAliasingKind kind;
		};

		std::variant<SetAntiAliasing> payload;
	};

	auto vk_init() -> void;
	auto swapchain_init() -> void;
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

	auto create_swapchain(uint32_t width, uint32_t height) -> void;
	auto create_draw_image(uint32_t width, uint32_t height) -> void;
	auto create_msaa_color_image(uint32_t width, uint32_t height) -> void;
	auto destroy_draw_image() -> void;
	auto create_depth_image(uint32_t width, uint32_t height) -> void;
	auto destroy_depth_image() -> void;
	auto destroy_msaa_color_image() -> void;
	auto recreate_swapchain(uint32_t width, uint32_t height) -> void;
	auto destroy_swapchain() -> void;
	auto create_image(vk::Extent3D size, vk::Format format,
	    vk::ImageUsageFlags flags,
	    vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1,
	    bool mipmapped = false) -> AllocatedImage;
	auto create_image(void const *data, vk::Extent3D size, vk::Format format,
	    vk::ImageUsageFlags flags, bool mipmapped = false) -> AllocatedImage;
	auto destroy_image(AllocatedImage const &img) -> void;

	auto create_buffer(size_t alloc_size, vk::BufferUsageFlags usage,
	    VmaMemoryUsage memory_usage) -> AllocatedBuffer;
	auto destroy_buffer(AllocatedBuffer const &buffer) -> void;
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
		vk::Extent2D draw_extent {};
		AntiAliasingKind antialiasing_kind { AntiAliasingKind::NONE };
		vk::SampleCountFlagBits msaa_samples { vk::SampleCountFlagBits::e1 };
		vk::SampleCountFlags supported_framebuffer_samples {};

		VmaAllocator allocator;

		GPUSceneData scene_data {};
		vk::DescriptorSetLayout gpu_scene_data_descriptor_layout {};

		vk::DescriptorSetLayout single_image_descriptor_layout {};

		Pipeline triangle_pipeline;
		Pipeline mesh_pipeline;

		GPUMeshBuffers rectangle;

		vk::UniqueDescriptorPool imgui_descriptor_pool;

		DeletionQueue deletion_queue;

		vk::UniqueFence imm_fence;
		vk::UniqueCommandBuffer imm_command_buffer;
		vk::UniqueCommandPool imm_command_pool;

		uint64_t frame_number { 0 };

		std::vector<std::shared_ptr<Mesh>> test_meshes;

		AllocatedImage white_image {};
		AllocatedImage black_image {};
		AllocatedImage gray_image {};
		AllocatedImage error_image {};

		vk::UniqueSampler default_sampler_linear;
		vk::UniqueSampler default_sampler_nearest;
	} m_vk;

	SDL_Window *m_window { nullptr };
	Logger &m_logger;
	std::mutex m_command_mutex;
	std::vector<RenderCommand> m_pending_render_commands;
};

} // namespace Lunar
