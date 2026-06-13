#version 450

#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_EXT_shader_atomic_int64 : enable

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

layout(std430, binding = 1) buffer frame_data_0 {
   uint64_t read_frame_buff[ ];
};

layout(std430, binding = 2) buffer frame_data_1 {
   uint64_t write_frame_buff[ ];
};

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

vec3 get_world_position_from_depth(uint depth)
{
    vec2 pixel_coords = vec2(gl_GlobalInvocationID.xy);
    float xc = pixel_coords.x/ubo.frame_width*2.0 - 1.0;
    float yc = pixel_coords.y/ubo.frame_height*2.0 - 1.0;
    float zc = uintBitsToFloat(depth);
    vec4 clip = vec4(xc, yc, zc, 1.0);
    vec4 view_pos = inverse(ubo.proj) * clip;
    view_pos /= view_pos.w;
    vec4 world = inverse(ubo.view) * view_pos;

    return world.xyz;
}

// n = (-1)^s * (1 + m*2^-23) * 2^(x - 127)
// s=0   x=127           m=0
//  0  01111111 00000000000000000000000
#define FLOAT_ONE 0x3f800000UL

#define KERNEL_SIZE 7
#define SECTOR_COUNT 8
// TODO: make threshold uniform or push constant
#define THRESHOLD (SECTOR_COUNT * 0.98)
#define READ_ONLY_MASK (1<<24)

void main()
{
    uvec2 id = gl_GlobalInvocationID.xy;
    if (id.x >= ubo.frame_width) return;
    if (id.y >= ubo.frame_height) return;

    /*
    *          KERNEL_SIZE 7
    * 
    * Imagine a circle circumscribing the 7x7 square with eight sectors (s0 - s7).
    * Certain sectors will share pixels.
    * For example the, line dividing sectors s0 and s7 causes them to share pixels along that line
    * (i.e. (row, col) = (0, 3), (1, 3), and (2, 3)). To make it easier we will have a two look up tables that tells us
    * which sectors to apply the calculation. Both lookup tables are needed if the pixel in question is overlapped
    * by more than one sector. Only one lookup table is needed if the pixel is in only one sector.
    *
    *      0     1     2     3     4     5     6
    *   +-----+-----+-----+-----+-----+-----+-----+
    * 0 |s6,s7|  s7 | s7  |s7,s0| s0  | s0  |s0,s1|
    *   +-----+-----+-----+-----+-----+-----+-----+
    * 1 | s6  |s6,s7| s7  |s7,s0| s0  |s0,s1| s1  |
    *   +-----+-----+-----+-----+-----+-----+-----+
    * 2 | s6  | s6  |s6,s7|s7,s0|s0,s1| s1  | s1  |
    *   +-----+-----+-----+-----+-----+-----+-----+
    * 3 |s6,s5|s6,s5|s6,s5|  x  |s1,s2|s1,s2|s1,s2|
    *   +-----+-----+-----+-----+-----+-----+-----+
    * 4 | s5  | s5  |s5,s4|s4,s3|s3,s2| s2  | s2  |
    *   +-----+-----+-----+-----+-----+-----+-----+
    * 5 | s5  |s5,s4| s4  |s4,s3| s3  |s3,s2| s2  |
    *   +-----+-----+-----+-----+-----+-----+-----+
    * 6 |s5,s4| s4  | s4  |s4,s3| s3  | s3  |s3,s2|
    *   +-----+-----+-----+-----+-----+-----+-----+
    */

    float sector_max_dot[SECTOR_COUNT] = {0, 0, 0, 0, 0, 0, 0, 0};

    /* index of first overlapping sector (-1 invalid) */
    int sector_lookup_0[KERNEL_SIZE*KERNEL_SIZE] = {
        7, 7, 7, 7, 0, 0, 0,  
        6, 6, 7, 7, 0, 0, 1,  
        6, 6, 6, 7, 0, 1, 1,  
        6, 6, 6,-1, 1, 1, 1,  
        5, 5, 5, 4, 2, 2, 2,  
        5, 5, 4, 4, 3, 3, 2,  
        5, 4, 4, 4, 3, 3, 3,  
    };
    /* index of second overlapping sector if any (-1 invalid) */
    int sector_lookup_1[KERNEL_SIZE*KERNEL_SIZE] = {
         6,-1,-1, 0,-1,-1, 1,  
        -1, 7,-1, 0,-1, 1,-1,  
        -1,-1, 7, 0, 1,-1,-1,  
         5, 5, 5,-1, 2, 2, 2,  
        -1,-1, 4, 3, 3,-1,-1,  
        -1, 4,-1, 3,-1, 2,-1,  
         4,-1,-1, 3,-1,-1, 2,  
    };

    int x0 = int(id.x - KERNEL_SIZE/2);
    int y0 = int(id.y - KERNEL_SIZE/2);
    ivec2 center_pixel_coords = ivec2(id);
    int center_pixel_id = center_pixel_coords.x + center_pixel_coords.y*ubo.frame_width;
    uint64_t center_pixel = read_frame_buff[center_pixel_id];
    uint64_t center_pixel_depth = center_pixel>>32;
    uint64_t center_pixel_color = center_pixel&0xffffffff;
    vec3 p0 = get_world_position_from_depth(uint(center_pixel_depth));

    /* loop over the visibility kernel */
    for (int i = 0; i < KERNEL_SIZE; i++) {
        int yp = y0 + i;
        if (!(0 <= yp && yp < ubo.frame_height)) continue;
        for (int j = 0; j < KERNEL_SIZE; j++) {
            int xp = x0 + j;
            if (!(0 <= xp && xp < ubo.frame_width)) continue;

            /* ignore center pixel */
            if (i == KERNEL_SIZE/2 && j == KERNEL_SIZE/2) continue;

            int neighbor_pixel_id = xp + yp*ubo.frame_width;
            int sector_idx0 = sector_lookup_0[j + i*KERNEL_SIZE];
            int sector_idx1 = sector_lookup_1[j + i*KERNEL_SIZE];
            vec3 pi = get_world_position_from_depth(uint(read_frame_buff[neighbor_pixel_id]>>32));
            vec3 v  = normalize(ubo.camera_pos.xyz - p0);
            vec3 c  = normalize(pi - p0);

            /* continues if we are center pixel (which shouldn't happen if we ignore center pixel, here just in case) */
            if (sector_idx0 < 0) continue;
            sector_max_dot[sector_idx0] = max(sector_max_dot[sector_idx0], dot(v, c));

            /* continues if we are center pixel or pixel is only in one sector */
            if (sector_idx1 < 0) continue;
            sector_max_dot[sector_idx1] = max(sector_max_dot[sector_idx1], dot(v, c));
        }
    }

    float sum = 0.0f;
    for (int i = 0; i < SECTOR_COUNT; i++) sum += sector_max_dot[i];

    if (sum >= THRESHOLD) {
        write_frame_buff[center_pixel_id] = (FLOAT_ONE << 32) | (ubo.bg_color&0xffffffUL);
    } else {
        center_pixel_color |= READ_ONLY_MASK;
        write_frame_buff[center_pixel_id] = center_pixel_depth<<32 | center_pixel_color;
    }
}
