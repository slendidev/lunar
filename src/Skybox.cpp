#include "Skybox.h"

#include <array>
#include <cstddef>
#include <span>
#include <vector>

#include "CPUTexture.h"
#include "DescriptorWriter.h"
#include "GraphicsPipelineBuilder.h"
#include "Util.h"

namespace Lunar {

namespace {

struct SkyboxPushConstants {
	smath::Mat4 mvp;
};

struct FaceOffset {
	uint32_t x;
	uint32_t y;
};

constexpr std::array<FaceOffset, 6> CROSS_OFFSETS {
	FaceOffset { 2, 1 }, // +X
	FaceOffset { 0, 1 }, // -X
	FaceOffset { 1, 0 }, // +Y
	FaceOffset { 1, 2 }, // -Y
	FaceOffset { 1, 1 }, // +Z
	FaceOffset { 3, 1 }, // -Z
};

} // namespace

auto Skybox::rebuild_pipeline(VulkanRenderer &renderer) -> bool
{
	Pipeline::Builder pipeline_builder { renderer.device(), renderer.logger() };

	uint8_t skybox_vert_shader_data[] {
#embed "skybox_vert.spv"
	};
	auto skybox_vert_shader
	    = vkutil::load_shader_module(std::span<uint8_t>(skybox_vert_shader_data,
	                                     sizeof(skybox_vert_shader_data)),
	        renderer.device());
	if (!skybox_vert_shader) {
		renderer.logger().err("Failed to load skybox vert shader");
		return false;
	}

	uint8_t skybox_frag_shader_data[] {
#embed "skybox_frag.spv"
	};
	auto skybox_frag_shader
	    = vkutil::load_shader_module(std::span<uint8_t>(skybox_frag_shader_data,
	                                     sizeof(skybox_frag_shader_data)),
	        renderer.device());
	if (!skybox_frag_shader) {
		renderer.logger().err("Failed to load skybox frag shader");
		return false;
	}

	vk::PushConstantRange push_constant_range {};
	push_constant_range.stageFlags = vk::ShaderStageFlagBits::eVertex;
	push_constant_range.offset = 0;
	push_constant_range.size = sizeof(SkyboxPushConstants);

	std::array push_constant_ranges { push_constant_range };
	pipeline_builder.set_push_constant_ranges(push_constant_ranges);
	std::array descriptor_set_layouts {
		renderer.single_image_descriptor_layout()
	};
	pipeline_builder.set_descriptor_set_layouts(descriptor_set_layouts);

	VkVertexInputBindingDescription binding {};
	binding.binding = 0;
	binding.stride = sizeof(Vertex);
	binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription attribute {};
	attribute.location = 0;
	attribute.binding = 0;
	attribute.format = VK_FORMAT_R32G32B32_SFLOAT;
	attribute.offset = offsetof(Vertex, position);

	std::array bindings { binding };
	std::array attributes { attribute };

	m_pipeline = pipeline_builder.build_graphics(
	    [&](GraphicsPipelineBuilder &builder) -> GraphicsPipelineBuilder & {
		    builder.set_vertex_input(bindings, attributes);
		    return builder
		        .set_shaders(skybox_vert_shader.get(), skybox_frag_shader.get())
		        .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		        .set_polygon_mode(VK_POLYGON_MODE_FILL)
		        .set_cull_mode(
		            VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
		        .set_multisampling(
		            static_cast<VkSampleCountFlagBits>(renderer.msaa_samples()))
		        .disable_blending()
		        .enable_depth_testing(false, VK_COMPARE_OP_LESS_OR_EQUAL)
		        .set_color_attachment_format(
		            static_cast<VkFormat>(renderer.draw_image_format()))
		        .set_depth_format(
		            static_cast<VkFormat>(renderer.depth_image_format()));
	    });

	m_pipeline_samples = renderer.msaa_samples();
	return true;
}

auto Skybox::init(VulkanRenderer &renderer, std::filesystem::path const &path)
    -> void
{
	if (ok) {
		destroy(renderer);
		ok = false;
	}

	CPUTexture texture { path };

	if (texture.width == 0 || texture.height == 0) {
		renderer.logger().err("Skybox texture is empty: {}", path.string());
		ok = false;
		return;
	}

	if (texture.width % 4 != 0 || texture.height % 3 != 0
	    || texture.width / 4 != texture.height / 3) {
		renderer.logger().err(
		    "Skybox texture must be 4x3 faces: {}", path.string());
		ok = false;
		return;
	}

	uint32_t const face_size = texture.width / 4;
	size_t const face_bytes = static_cast<size_t>(face_size) * face_size * 4;

	std::vector<uint8_t> cubemap_pixels(face_bytes * CROSS_OFFSETS.size());

	for (size_t face = 0; face < CROSS_OFFSETS.size(); ++face) {
		auto const offset = CROSS_OFFSETS[face];
		for (uint32_t y = 0; y < face_size; ++y) {
			for (uint32_t x = 0; x < face_size; ++x) {
				uint32_t const src_x = offset.x * face_size + x;
				uint32_t const src_y = offset.y * face_size + y;
				size_t const src_index
				    = (static_cast<size_t>(src_y) * texture.width + src_x) * 4;
				size_t const dst_index = face * face_bytes
				    + (static_cast<size_t>(y) * face_size + x) * 4;
				std::copy_n(texture.pixels.data() + src_index, 4,
				    cubemap_pixels.data() + dst_index);
			}
		}
	}

	m_cubemap = renderer.create_cubemap(cubemap_pixels, face_size,
	    texture.format, vk::ImageUsageFlagBits::eSampled);
	if (!m_cubemap.image) {
		renderer.logger().err("Failed to create cubemap image");
		return;
	}

	vk::SamplerCreateInfo sampler_ci {};
	sampler_ci.magFilter = vk::Filter::eLinear;
	sampler_ci.minFilter = vk::Filter::eLinear;
	sampler_ci.mipmapMode = vk::SamplerMipmapMode::eLinear;
	sampler_ci.addressModeU = vk::SamplerAddressMode::eClampToEdge;
	sampler_ci.addressModeV = vk::SamplerAddressMode::eClampToEdge;
	sampler_ci.addressModeW = vk::SamplerAddressMode::eClampToEdge;
	m_sampler = renderer.device().createSamplerUnique(sampler_ci);

	vk::DescriptorPoolSize pool_size {};
	pool_size.type = vk::DescriptorType::eCombinedImageSampler;
	pool_size.descriptorCount = 1;

	vk::DescriptorPoolCreateInfo pool_ci {};
	pool_ci.maxSets = 1;
	pool_ci.poolSizeCount = 1;
	pool_ci.pPoolSizes = &pool_size;
	m_descriptor_pool = renderer.device().createDescriptorPoolUnique(pool_ci);

	vk::DescriptorSetAllocateInfo alloc_info {};
	alloc_info.descriptorPool = m_descriptor_pool.get();
	alloc_info.descriptorSetCount = 1;
	vk::DescriptorSetLayout layout {
		renderer.single_image_descriptor_layout()
	};
	alloc_info.pSetLayouts = &layout;
	m_descriptor_set
	    = renderer.device().allocateDescriptorSets(alloc_info).front();

	DescriptorWriter()
	    .write_image(0, m_cubemap.image_view, m_sampler.get(),
	        static_cast<VkImageLayout>(vk::ImageLayout::eShaderReadOnlyOptimal),
	        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
	    .update_set(renderer.device(), m_descriptor_set);

	std::vector<Vertex> vertices;
	vertices.reserve(8);

	auto push_vertex = [&](smath::Vec3 const &pos) {
		Vertex v {};
		v.position = pos;
		v.u = 0.0f;
		v.v = 0.0f;
		v.normal = smath::Vec3 { 0.0f, 0.0f, 1.0f };
		v.color = smath::Vec4 { 1.0f, 1.0f, 1.0f, 1.0f };
		vertices.emplace_back(v);
	};

	push_vertex(smath::Vec3 { -1.0f, -1.0f, -1.0f });
	push_vertex(smath::Vec3 { 1.0f, -1.0f, -1.0f });
	push_vertex(smath::Vec3 { 1.0f, 1.0f, -1.0f });
	push_vertex(smath::Vec3 { -1.0f, 1.0f, -1.0f });
	push_vertex(smath::Vec3 { -1.0f, -1.0f, 1.0f });
	push_vertex(smath::Vec3 { 1.0f, -1.0f, 1.0f });
	push_vertex(smath::Vec3 { 1.0f, 1.0f, 1.0f });
	push_vertex(smath::Vec3 { -1.0f, 1.0f, 1.0f });

	std::vector<uint32_t> indices {
		4,
		5,
		6,
		4,
		6,
		7, // +Z
		1,
		0,
		3,
		1,
		3,
		2, // -Z
		5,
		1,
		2,
		5,
		2,
		6, // +X
		0,
		4,
		7,
		0,
		7,
		3, // -X
		7,
		6,
		2,
		7,
		2,
		3, // +Y
		0,
		1,
		5,
		0,
		5,
		4, // -Y
	};

	m_index_count = static_cast<uint32_t>(indices.size());
	m_cube_mesh = renderer.upload_mesh(indices, vertices);

	if (!rebuild_pipeline(renderer)) {
		ok = false;
		return;
	}

	ok = true;
}

auto Skybox::destroy(VulkanRenderer &renderer) -> void
{
	if (m_cube_mesh.index_buffer.buffer) {
		renderer.destroy_buffer(m_cube_mesh.index_buffer);
	}
	if (m_cube_mesh.vertex_buffer.buffer) {
		renderer.destroy_buffer(m_cube_mesh.vertex_buffer);
	}
	if (m_cubemap.image) {
		renderer.destroy_image(m_cubemap);
	}
	m_sampler.reset();
	m_descriptor_pool.reset();
	m_pipeline.reset();
	m_pipeline_samples = vk::SampleCountFlagBits::e1;
	m_descriptor_set = vk::DescriptorSet {};
	m_cube_mesh = {};
	m_cubemap = {};
	m_index_count = 0;
	ok = false;
}

auto Skybox::draw(VulkanRenderer::GL &gl, VulkanRenderer &renderer,
    smath::Mat4 const &mvp) -> void
{
	if (!ok) {
		return;
	}

	if (m_pipeline_samples != renderer.msaa_samples()) {
		if (!rebuild_pipeline(renderer)) {
			return;
		}
	}

	SkyboxPushConstants push_constants { mvp };
	auto bytes { std::as_bytes(std::span { &push_constants, 1 }) };
	gl.draw_indexed(m_pipeline, m_descriptor_set, m_cube_mesh.vertex_buffer,
	    m_cube_mesh.index_buffer, m_index_count, bytes);
}

} // namespace Lunar
