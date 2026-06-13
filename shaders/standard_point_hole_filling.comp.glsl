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

layout(std430, binding = 1) buffer frame_data_0 {
   uint64_t read_frame_buff[ ];
};

layout(std430, binding = 2) buffer frame_data_1 {
   uint64_t write_frame_buff[ ];
};

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

#define KERNEL_SIZE 3

// n = (-1)^s * (1 + m*2^-23) * 2^(x - 127)
// s=0   x=127           m=0
//  0  01111111 00000000000000000000000
#define FLOAT_ONE 0x3f800000UL

/* not sure what this value should be since I couln't get weighted average to work right */
#define DEPTH_MODULATOR 500.0f
// #define WEIGHTED_AVG

#define READ_ONLY_MASK (1<<24)

// leave alpha channel blank as I'm using that for the read only mask
//             b g r
#define CYAN 0xffff00UL

vec4 srgb_to_linear(vec4 srgb)
{
	vec3 b_less = step(vec3(0.04045),srgb.xyz);
	vec3 lin_out = mix(srgb.xyz/vec3(12.92), pow((srgb.xyz+vec3(0.055))/vec3(1.055),vec3(2.4)), b_less);
	return vec4(lin_out,srgb.w);
}

vec4 linear_to_srgb(vec4 linear)
{
    bvec3 cutoff = lessThan(linear.xyz, vec3(0.0031308));
    vec3 higher = 1.055*pow(linear.xyz, vec3(1.0/2.4)) - 0.055;
    vec3 lower = linear.xyz*12.92;
    return vec4(mix(higher, lower, cutoff), linear.w);
}

void main()
{
    uvec2 id = gl_GlobalInvocationID.xy;
    if (id.x >= ubo.frame_width) return;
    if (id.y >= ubo.frame_height) return;

    ivec2 center_pixel_coords = ivec2(id);
    int center_pixel_id = center_pixel_coords.x + center_pixel_coords.y*ubo.frame_width;
    uint64_t center_pixel = read_frame_buff[center_pixel_id];
    uint64_t center_pixel_color = center_pixel&0xffffffffUL;
    uint64_t center_pixel_depth = center_pixel>>32;
    uint64_t valid_neighbors[KERNEL_SIZE*KERNEL_SIZE-1] = {0,0,0,0,0,0,0,0};
    int valid_neighbor_count = 0;

    /* if pixel is marked read only, we still need to copy over to the secondary buffer */
    if ((center_pixel_color&READ_ONLY_MASK) > 0) {
        write_frame_buff[center_pixel_id] = center_pixel;
        return;
    }

    /* search kernel for valid neighbors */
    int x0 = int(id.x - KERNEL_SIZE/2);
    int y0 = int(id.y - KERNEL_SIZE/2);
    for (int i = 0; i < KERNEL_SIZE; i++) {
        int yp = y0 + i;
        if (!(0 <= yp && yp < ubo.frame_height)) continue;
        for (int j = 0; j < KERNEL_SIZE; j++) {
            int xp = x0 + j;
            if (!(0 <= xp && xp < ubo.frame_width)) continue;

            /* ignore center pixel */
            if (i == KERNEL_SIZE/2 && j == KERNEL_SIZE/2) continue;

            int neighbor_pixel_id = xp + yp*ubo.frame_width;
            uint64_t neighbor_pixel = read_frame_buff[neighbor_pixel_id];
            uint64_t neighbor_pixel_depth = neighbor_pixel>>32;
            uint64_t neighbor_pixel_color = neighbor_pixel&0xffffffff;
            bool valid_neighbor = (neighbor_pixel_color&READ_ONLY_MASK) > 0;
            if (valid_neighbor) valid_neighbors[valid_neighbor_count++] = neighbor_pixel;
        }
    }

    if (valid_neighbor_count == 0) {
        center_pixel &= 0x000000UL;
        /* seeing cyan will indicate a bug or too few iterations */
        write_frame_buff[center_pixel_id] = center_pixel | CYAN;
        return;
    }

    /* if the center is valid then we apply the weighted sum of color and depth */
    if (center_pixel_depth != FLOAT_ONE) {
        float z0 = uintBitsToFloat(uint(center_pixel>>32));
        float weighted_depth = 0.0f;
        vec4 weighted_color = vec4(0.0);
        for (int i = 0; i < valid_neighbor_count; i++) {
            float zi = uintBitsToFloat(uint(valid_neighbors[i]>>32));
#ifdef WEIGHTED_AVG
            float wi = 0.5*(1.0f - min(1, abs(zi - z0)/DEPTH_MODULATOR));
#else
            float wi = 1.0f;
#endif
            vec4 color = vec4(unpackUnorm4x8(uint(valid_neighbors[i]&0xffffffffUL)));
            color = srgb_to_linear(color);

            weighted_color += color*wi;
            weighted_depth += zi*wi;
        }

        weighted_depth     /= valid_neighbor_count;
        weighted_color.xyz /= valid_neighbor_count;
        weighted_color = linear_to_srgb(weighted_color);
        uint64_t final_color = uint64_t(packUnorm4x8(weighted_color));
        uint64_t final_depth = uint64_t(floatBitsToUint(weighted_depth));
        final_color |= READ_ONLY_MASK;
        write_frame_buff[center_pixel_id] = final_depth<<32 | final_color; 
    } else {
        /* If we made it here then two things are true: the depth == 1.0 and this is not a background pixel (otherwise it
         * it would have been marked read-only in the hidden surface removal shader).
         * This pixel needs a depth, so we initialize it with the median of its valid neighbors */

        /* first sort valid depths */
        for (int i = 0; i < valid_neighbor_count; i++) {
            for (int j = i + 1; j < valid_neighbor_count; j++) {
                if (valid_neighbors[j] < valid_neighbors[i]) {
                    uint64_t tmp = valid_neighbors[j];
                    valid_neighbors[j] = valid_neighbors[i];
                    valid_neighbors[i] = tmp;
                }
            }
        }

        /* write the median depth */
        uint64_t median_depth = 0 ;
        if (valid_neighbor_count%2 == 0) // does averaging on the integer representation of floating point work???
            median_depth = valid_neighbors[valid_neighbor_count/2]/2 + valid_neighbors[valid_neighbor_count/2+1]/2;
        else
            median_depth = valid_neighbors[valid_neighbor_count/2];
        write_frame_buff[center_pixel_id] = median_depth<<32 | center_pixel_color;
    }
}
