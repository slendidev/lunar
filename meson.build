project('vr-compositor', 'cpp',
	version: '0.1',
	default_options: [
		'cpp_std=c++26',
		'warning_level=everything',
		'werror=true',
	],
	subproject_dir: 'subprojects',
)

cmake = import('cmake')

fastgltf_opts = cmake.subproject_options()
fastgltf_opts.set_override_option('cpp_std', 'c++23')
fastgltf_opts.set_override_option('warning_level', '0')
fastgltf_opts.set_override_option('werror', 'false')
fastgltf = cmake.subproject('fastgltf', options: fastgltf_opts)

cc = meson.get_compiler('cpp')

wayland_dep = dependency('wayland-server')
vulkan_dep = dependency('vulkan')
openxr_dep = dependency('openxr')
zlib_dep = dependency('zlib')
sdl3_dep = dependency('sdl3')
imgui_src = files(
	'thirdparty/imgui/imgui.cpp',
	'thirdparty/imgui/imgui_draw.cpp',
	'thirdparty/imgui/imgui_tables.cpp',
	'thirdparty/imgui/imgui_widgets.cpp',
	'thirdparty/imgui/backends/imgui_impl_vulkan.cpp',
	'thirdparty/imgui/backends/imgui_impl_sdl3.cpp',
)
fastgltf_dep = fastgltf.dependency('fastgltf')

vkbootstrap_dev = get_option('vkbootstrap_dev')
vkbootstrap_lib = get_option('vkbootstrap_lib')

vkbootstrap_inc = include_directories(
	join_paths(vkbootstrap_dev, 'include')
)

vkbootstrap_dep = cc.find_library(
	'vk-bootstrap',
	dirs: join_paths(vkbootstrap_lib, 'lib'),
	required: true,
)

add_project_arguments('-Wpedantic', language : ['c', 'cpp'])
add_project_arguments(
	[
		'-Wno-c++98-compat',
		'-Wno-c++98-compat-pedantic',
		'-Wno-covered-switch-default',
		'-Wno-undef',
		'-Wno-padded',
		'-Wno-unsafe-buffer-usage',
		'-Wno-c23-extensions',
		'-Wno-old-style-cast',
		'-Wno-implicit-int-float-conversion',
		'-Wno-implicit-float-conversion',
		'-Wno-c++98-compat',
	],
	language : 'cpp'
)

subdir('shaders')

imgui_src = files(
	'thirdparty/imgui/imgui.cpp',
	'thirdparty/imgui/imgui_draw.cpp',
	'thirdparty/imgui/imgui_tables.cpp',
	'thirdparty/imgui/imgui_widgets.cpp',
	'thirdparty/imgui/imgui_demo.cpp',
	'thirdparty/imgui/backends/imgui_impl_vulkan.cpp',
	'thirdparty/imgui/backends/imgui_impl_sdl3.cpp',
)

imgui_inc = include_directories('thirdparty/imgui', 'thirdparty/imgui/backends')

imgui_lib = static_library('imgui',
	imgui_src,
	include_directories: imgui_inc,
	dependencies: [
		vulkan_dep,
		sdl3_dep,
	],
	cpp_args: [
		'-w',
	],
)

exe = executable('vr-compositor',
	[
		'src/main.cpp',
		'src/Impls.cpp',
		'src/Util.cpp',
		'src/Logger.cpp',
		'src/DescriptorLayoutBuilder.cpp',
		'src/DescriptorAllocator.cpp',
		'src/GraphicsPipelineBuilder.cpp',
		'src/VulkanRenderer.cpp',
		'src/Application.cpp',
	],
	include_directories: [
		vkbootstrap_inc,
		imgui_inc,
		'thirdparty/smath/include'
	],
	link_with: imgui_lib,
	dependencies: [
		wayland_dep,
		vulkan_dep,
		openxr_dep,
		vkbootstrap_dep,
		zlib_dep,
		sdl3_dep,
		fastgltf_dep,
	],
	cpp_args: [
		'--embed-dir=' + join_paths(meson.project_build_root(), 'shaders')
	],
)
