#version 450

layout(location = 0) out vec4 out_color;

layout(push_constant) uniform PushConstants {
	vec2 resolution;
} pc;

void main()
{
	vec2 uv = (gl_FragCoord.xy + vec2(0.5)) / pc.resolution;

	float v = sin(uv.x * 10.0) + cos(uv.y * 10.0);

	float r = 0.5 + 0.5 * cos(6.2831 * (uv.x + v));
	float g = 0.5 + 0.5 * cos(6.2831 * (uv.y + v + 0.33));
	float b = 0.5 + 0.5 * cos(6.2831 * (uv.x - uv.y + 0.66));

	out_color = vec4(r, g, b, 1.0);
}

