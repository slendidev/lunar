project('vr-compositor', 'cpp',
	version: '0.1',
	default_options: [
		'cpp_std=c++26',
		'warning_level=everything',
		'werror=true',
	]
)

cc = meson.get_compiler('cpp')

wayland_dep = dependency('wayland-server')
vulkan_dep = dependency('vulkan')
openxr_dep = dependency('openxr')
zlib_dep = dependency('zlib')
sdl3_dep = dependency('sdl3')

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
	],
	language : 'cpp'
)

subdir('shaders')

exe = executable('vr-compositor',
	[
		'src/main.cpp',
		'src/Impls.cpp',
		'src/Util.cpp',
		'src/Logger.cpp',
		'src/DescriptorLayoutBuilder.cpp',
		'src/DescriptorAllocator.cpp',
		'src/Application.cpp',
	],
	include_directories: vkbootstrap_inc,
	dependencies: [
		wayland_dep,
		vulkan_dep,
		openxr_dep,
		vkbootstrap_dep,
		zlib_dep,
		sdl3_dep,
	],
	cpp_args: [
		'--embed-dir=' + join_paths(meson.project_build_root(), 'shaders')
	],
)
