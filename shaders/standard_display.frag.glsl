#version 450

layout (binding = 0) uniform sampler2D display_texture;

layout (location = 0) in vec2 in_uv;
layout (location = 0) out vec4 out_color;



void main()
{
    out_color = texture(display_texture, in_uv);
}
