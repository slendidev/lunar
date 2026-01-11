#version 450

layout (location = 0) in vec3 in_dir;

layout (location = 0) out vec4 out_frag_color;

layout (set = 0, binding = 0) uniform samplerCube environment_map;

void main() {
	vec3 dir = normalize(in_dir);
	out_frag_color = vec4(texture(environment_map, dir).rgb, 1.0f);
}
