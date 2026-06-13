#version 450

#extension GL_ARB_gpu_shader_int64 : enable

// n = (-1)^s * (1 + m*2^-23) * 2^(x - 127)
// s=0   x=127           m=0
//  0  01111111 00000000000000000000000
#define FLOAT_ONE 0x3f800000UL

layout(std430, binding = 0) buffer frame_data {
   uint64_t frame_buff[ ];
};

layout(push_constant) uniform constants {
    uint frame_width;
    uint frame_height;
    uint clear_color;
} pc;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

void main()
{
    uvec2 id = gl_GlobalInvocationID.xy;
    if (id.x >= pc.frame_width) return;
    if (id.y >= pc.frame_height) return;
    ivec2 pixel_coords = ivec2(id);
    uint pixel_id = pixel_coords.x + pixel_coords.y*pc.frame_width;
    frame_buff[pixel_id] = (FLOAT_ONE << 32) | pc.clear_color;
}
