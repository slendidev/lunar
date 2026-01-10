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

wayland_dep = dependency('wayland-server', include_type: 'system')
vulkan_dep = dependency('vulkan', include_type: 'system')
openxr_dep = dependency('openxr', include_type: 'system')
zlib_dep = dependency('zlib', include_type: 'system')
sdl3_dep = dependency('sdl3', include_type: 'system')
libinput_dep = dependency('libinput', include_type: 'system')
libudev_dep = dependency('libudev', include_type: 'system')
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
tracy_enable = get_option('tracy_enable')

if tracy_enable and get_option('buildtype') != 'debugoptimized'
	warning('Profiling builds should set --buildtype=debugoptimized')
endif

tracy = disabler()
if tracy_enable
	tracy_proj = subproject('tracy', default_options: [
		'default_library=static',
		'warning_level=0',
		'werror=false',
	])
	tracy = tracy_proj.get_variable('tracy_dep')
endif

vkbootstrap_inc = include_directories(
	join_paths(vkbootstrap_dev, 'include'),
	is_system: true,
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
		# Thanks fastgltf
		'-Wno-ctad-maybe-unsupported',
		'-Wno-switch-default',
		'-Wno-nrvo',
		'-Wno-shadow-field-in-constructor',
		'-Wno-deprecated-copy-with-dtor',
		'-Wno-double-promotion',
		'-Wno-weak-vtables',
		'-Wno-switch-enum',
		'-Wno-sign-conversion',
		'-Wno-documentation',
		'-Wno-float-equal',
		'-Wno-cast-function-type-strict',
		'-Wno-exit-time-destructors',
		'-Wno-zero-as-null-pointer-constant',
		'-Wno-unused-macros',
		'-Wno-reserved-macro-identifier',
		'-Wno-suggest-override',
		'-Wno-macro-redefined',
		'-DVULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE',
		'-DVULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1',
		# Thanks forityf source
		'-Wno-#warnings'
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

exe_deps = [
	wayland_dep,
	vulkan_dep,
	openxr_dep,
	vkbootstrap_dep,
	zlib_dep,
	sdl3_dep,
	fastgltf_dep,
	libinput_dep,
	libudev_dep,
]
if tracy_enable
	exe_deps += tracy
endif

exe = executable('vr-compositor',
	[
		'src/main.cpp',
		'src/Impls.cpp',
		'src/Util.cpp',
		'src/Logger.cpp',
		'src/DescriptorLayoutBuilder.cpp',
		'src/DescriptorAllocator.cpp',
		'src/GraphicsPipelineBuilder.cpp',
		'src/DescriptorAllocatorGrowable.cpp',
		'src/Pipeline.cpp',
		'src/Loader.cpp',
		'src/DescriptorWriter.cpp',
		'src/VulkanRenderer.cpp',
		'src/Application.cpp',
	],
	include_directories: [
		vkbootstrap_inc,
		imgui_inc,
		'thirdparty/smath/include'
	],
	link_with: imgui_lib,
	dependencies: exe_deps,
	cpp_args: [
		'--embed-dir=' + join_paths(meson.project_build_root(), 'shaders'),
	],
)
