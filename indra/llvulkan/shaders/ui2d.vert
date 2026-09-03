#version 450
// 2D UI quad vertex shader. Per-vertex: position (pixels, top-left origin),
// uv, color. Projection comes from a push-constant ortho matrix so the whole
// batch shares one upload. gl_VertexIndex-free: geometry comes from a real
// vertex buffer (added with batching in 3a-2).

layout(location = 0) in vec2 inPos;   // pixel coords, top-left origin
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inColor;

layout(push_constant) uniform Push
{
    mat4 ortho;   // pixels -> NDC, top-left origin (y down)
} pc;

layout(location = 0) out vec2 vUV;
layout(location = 1) out vec4 vColor;

void main()
{
    gl_Position = pc.ortho * vec4(inPos, 0.0, 1.0);
    vUV = inUV;
    vColor = inColor;
}
