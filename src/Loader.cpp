#include "Loader.h"

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/util.hpp>

#include "VulkanRenderer.h"

namespace fastgltf {
template<>
struct ElementTraits<smath::Vec4>
    : ElementTraitsBase<smath::Vec4, AccessorType::Vec4, float> { };
template<>
struct ElementTraits<smath::Vec3>
    : ElementTraitsBase<smath::Vec3, AccessorType::Vec3, float> { };
template<>
struct ElementTraits<smath::Vec2>
    : ElementTraitsBase<smath::Vec2, AccessorType::Vec2, float> { };
}

#ifndef __cpp_lib_format_path

#	include <filesystem>
#	include <format>
#	include <string>
#	include <type_traits>

template<class CharT>
struct std::formatter<std::filesystem::path, CharT>
    : std::formatter<std::basic_string<CharT>, CharT> {
	template<class FormatContext>
	auto format(std::filesystem::path const &p, FormatContext &ctx) const
	{
		std::basic_string<CharT> s;

		if constexpr (std::is_same_v<CharT, char>) {
			s = p.generic_string();
		} else if constexpr (std::is_same_v<CharT, wchar_t>) {
			s = p.generic_wstring();
		}
#	if defined(__cpp_lib_char8_t)
		else if constexpr (std::is_same_v<CharT, char8_t>) {
			s = p.generic_u8string();
		}
#	endif
		else if constexpr (std::is_same_v<CharT, char16_t>) {
			s = p.generic_u16string();
		} else if constexpr (std::is_same_v<CharT, char32_t>) {
			s = p.generic_u32string();
		}

		return std::formatter<std::basic_string<CharT>, CharT>::format(s, ctx);
	}
};

namespace Lunar {

auto Mesh::load_gltf_meshes(
    VulkanRenderer &renderer, std::filesystem::path const path)
    -> std::optional<std::vector<std::shared_ptr<Mesh>>>
{
	renderer.logger().debug("Loading GLTF from file: {}", path);

	auto data = fastgltf::GltfDataBuffer::FromPath(path);
	if (data.error() != fastgltf::Error::None) {
		renderer.logger().err("Failed to open glTF file: {} (error {})", path,
		    fastgltf::to_underlying(data.error()));
		return {};
	}

	constexpr auto gltfOptions { fastgltf::Options::LoadExternalBuffers };

	fastgltf::Parser parser;

	auto load { parser.loadGltf(data.get(), path.parent_path(), gltfOptions) };
	if (load.error() != fastgltf::Error::None) {
		renderer.logger().err(
		    "Failed to load glTF: {}", fastgltf::to_underlying(load.error()));
		return {};
	}
	fastgltf::Asset gltf { std::move(load.get()) };

	std::vector<std::shared_ptr<Mesh>> meshes;

	std::vector<uint32_t> indices;
	std::vector<Vertex> vertices;
	for (auto &mesh : gltf.meshes) {
		Mesh new_mesh;

		new_mesh.name = mesh.name;

		indices.clear();
		vertices.clear();

		for (auto &&p : mesh.primitives) {
			Surface new_surface;
			new_surface.start_index = static_cast<uint32_t>(indices.size());
			new_surface.count = static_cast<uint32_t>(
			    gltf.accessors[p.indicesAccessor.value()].count);

			size_t initial_vertex = vertices.size();

			{ // Indices
				auto &accessor = gltf.accessors[p.indicesAccessor.value()];
				indices.reserve(indices.size() + accessor.count);
				fastgltf::iterateAccessor<std::uint32_t>(
				    gltf, accessor, [&](std::uint32_t idx) {
					    indices.emplace_back(idx + initial_vertex);
				    });
			}

			{ // Vertex positions
				auto &accessor = gltf.accessors[p.findAttribute("POSITION")
				        ->accessorIndex];

				for (auto pos :
				    fastgltf::iterateAccessor<smath::Vec3>(gltf, accessor)) {
					Vertex v {
						.position = pos,
						.u = 0,
						.normal = { 0, 0, 0 },
						.v = 0,
						.color = { 1.0f, 1.0f, 1.0f, 1.0f },
					};
					vertices.emplace_back(v);
				}
			}

			if (auto attr = p.findAttribute("NORMAL")) { // Normals
				auto &accessor = gltf.accessors[attr->accessorIndex];
				size_t local_index = 0;
				for (auto normal :
				    fastgltf::iterateAccessor<smath::Vec3>(gltf, accessor)) {
					vertices[initial_vertex + local_index].normal = normal;
					local_index++;
				}
			}

			if (auto attr = p.findAttribute("TEXCOORD_0")) { // UVs
				auto &accessor = gltf.accessors[attr->accessorIndex];
				size_t local_index = 0;
				for (auto uv :
				    fastgltf::iterateAccessor<smath::Vec2>(gltf, accessor)) {
					uv.unpack(vertices[initial_vertex + local_index].u,
					    vertices[initial_vertex + local_index].v);
					local_index++;
				}
			}

			if (auto attr = p.findAttribute("COLOR_0")) { // Colors
				auto &accessor = gltf.accessors[attr->accessorIndex];
				size_t local_index = 0;

				switch (accessor.type) {
				case fastgltf::AccessorType::Vec3: {
					for (auto c3 : fastgltf::iterateAccessor<smath::Vec3>(
					         gltf, accessor)) {
						auto &dst
						    = vertices[initial_vertex + local_index].color;
						dst = { c3.x(), c3.y(), c3.z(), 1.0f };
						++local_index;
					}
					break;
				}
				case fastgltf::AccessorType::Vec4: {
					for (auto c4 : fastgltf::iterateAccessor<smath::Vec4>(
					         gltf, accessor)) {
						vertices[initial_vertex + local_index].color = c4;
						++local_index;
					}
					break;
				}
				default:
					renderer.logger().warn(
					    "Unsupported COLOR_0 accessor type ({}) on mesh '{}'",
					    static_cast<int>(accessor.type), new_mesh.name);
					break;
				}
			}

			constexpr bool OVERRIDE_COLORS = true;
			if (OVERRIDE_COLORS) {
				for (auto &vtx : vertices) {
					vtx.color = smath::Vec4(vtx.normal, 1.f);
				}
			}

			new_mesh.surfaces.emplace_back(new_surface);
		}

		new_mesh.mesh_buffers = renderer.upload_mesh(indices, vertices);

		meshes.emplace_back(std::make_shared<Mesh>(std::move(new_mesh)));
	}

	return meshes;
}

#endif // __cpp_lib_format_path

} // namespace Lunar
