#version 450
layout(set = 0, binding = 0) uniform sampler2D glyphImage;
layout(location = 0) in vec2 glyphTexcoord;
layout(location = 1) in vec4 glyphColor;
layout(location = 0) out vec4 fragmentColor;
void main()
{
    fragmentColor = glyphColor * texture(glyphImage, glyphTexcoord);
}