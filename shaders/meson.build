fs = import('fs')

glslc = find_program('glslc', required : false)
glslang = find_program('glslangValidator', required : false)

if glslc.found()
	shader_compiler = glslc
	shader_compile_cmd = [shader_compiler, '-o', '@OUTPUT@', '@INPUT@']
elif glslang.found()
	shader_compiler = glslang
	shader_compile_cmd = [shader_compiler, '-V', '@INPUT@', '-o', '@OUTPUT@']
else
	error('Either glslc or glslangValidator is required to build shaders')
endif

shader_sources = files(
	'gradient.comp',
)

spirv_shaders = []
foreach shader : shader_sources
	shader_name = fs.stem(shader)
	spirv_shaders += custom_target(
		shader_name + '_spv',
		input : shader,
		output : shader_name + '.spv',
		command : shader_compile_cmd,
		build_by_default : true,
	)
endforeach
