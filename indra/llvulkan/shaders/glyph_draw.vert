#version 450
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texcoord;
layout(location = 2) in vec4 color;
layout(push_constant) uniform View
{
    mat4 projection;
    mat4 textureTransform;
} view;
layout(location = 0) out vec2 glyphTexcoord;
layout(location = 1) out vec4 glyphColor;
void main()
{
    gl_Position = view.projection * vec4(position, 1.0);
    glyphTexcoord = (view.textureTransform * vec4(texcoord, 0.0, 1.0)).xy;
    glyphColor = color;
}