#version 450

#extension GL_ARB_gpu_shader_int64 : enable

layout (binding = 0) uniform UBO {
    mat4 proj;
    mat4 view;
    mat4 model;
    int frame_width;
    int frame_height;
    uint bg_color;
    int padding_1;
    vec4 camera_pos;
} ubo;

layout(std430, binding = 1) buffer frame_data {
   uint64_t frame_buff[ ];
};

layout (rgba8, binding = 2) uniform image2D out_image;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

vec4 srgb_to_linear(vec4 srgb)
{
	vec3 b_less = step(vec3(0.04045),srgb.xyz);
	vec3 lin_out = mix(srgb.xyz/vec3(12.92), pow((srgb.xyz+vec3(0.055))/vec3(1.055),vec3(2.4)), b_less);
	return vec4(lin_out,srgb.w);;
}

void main()
{
    uvec2 id = gl_GlobalInvocationID.xy;
    if (id.x >= ubo.frame_width) return;
    if (id.y >= ubo.frame_height) return;

    ivec2 pixel_coords = ivec2(id);
    int pixel_id = pixel_coords.x + pixel_coords.y*ubo.frame_width;
    vec4 color = vec4(unpackUnorm4x8(uint(frame_buff[pixel_id] & 0xffffffffUL)));
    color = srgb_to_linear(color);
    imageStore(out_image, pixel_coords, color);
}
