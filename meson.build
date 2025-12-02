project('vr-compositor', 'cpp',
	version: '0.1',
	default_options: ['cpp_std=c++23']
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

add_project_arguments(
	'-Wall',
	'-Wextra',
	'-Wpedantic',
	'-Werror',
	language : ['c', 'cpp']
)

exe = executable('vr-compositor',
	[
		'src/main.cpp',
		'src/Logger.cpp',
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
	]
)
