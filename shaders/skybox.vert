#version 450

layout (location = 0) in vec3 in_position;
layout (location = 0) out vec3 out_dir;

layout(push_constant) uniform constants {
	mat4 mvp;
} PushConstants;

void main() {
	out_dir = in_position;
	vec4 pos = PushConstants.mvp * vec4(in_position, 1.0f);
	gl_Position = vec4(pos.xy, pos.w, pos.w);
}
