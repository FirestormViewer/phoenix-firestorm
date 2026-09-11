#version 450
// 2D UI quad fragment shader. Samples the bound texture and multiplies by the
// interpolated vertex color (tint). Alpha-blended by the pipeline state. Solid
// quads bind a 1x1 white texture so the output is just the vertex color.

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;

layout(set = 0, binding = 0) uniform sampler2D tex0;
layout(constant_id = 0) const bool alphaMask = false;

layout(location = 0) out vec4 outColor;

void main()
{
    vec4 sampled = texture(tex0, vUV);
    outColor = alphaMask ? vec4(vColor.rgb, sampled.a * vColor.a) : sampled * vColor;
}
