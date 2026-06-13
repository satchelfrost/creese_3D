#version 450

#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_EXT_shader_atomic_int64 : enable

struct Point {
    float x, y, z;
    uint color;
};

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

layout(std430, binding = 1) buffer point_data {
   Point points[ ];
};

layout(std430, binding = 2) buffer frame_data {
   uint64_t frame_buff[ ];
};

layout(local_size_x = 1024, local_size_y = 1, local_size_z = 1) in;

layout(push_constant) uniform constants
{
    uint batch_offset;
    uint base_offset;
    uint point_count;
} push_const;

// TODO: should this be more configurable?
#define NORMALIZED_DEPTH

void main()
{
    uint index = gl_GlobalInvocationID.x + push_const.base_offset + push_const.batch_offset;

    bool out_of_buffer_bounds = index >= points.length();
    bool out_of_point_range = index >= push_const.base_offset + push_const.point_count;
    if (out_of_buffer_bounds || out_of_point_range) return;

    Point point = points[index];
    vec4 clip = ubo.proj*ubo.view*ubo.model*vec4(point.x, point.y, point.z, 1.0);
    bool in_frustum = -clip.w < clip.x && clip.x < clip.w &&
                      -clip.w < clip.y && clip.y < clip.w &&
                          0.0 < clip.z && clip.z < clip.w;
    if (!in_frustum) return;
    vec3 ndc = clip.xyz / clip.w;
    if (ndc.x < -1.0 || ndc.x > 1.0 || 
        ndc.y < -1.0 || ndc.y > 1.0 ||
        ndc.z <  0.0 || ndc.z > 1.0 &&
        clip.w <= 0) return;

    /* find pixel id within the frame buffer */
    ivec2 img_size = ivec2(ubo.frame_width, ubo.frame_height);
    vec2 img_pos = (ndc.xy*0.5 + 0.5)*img_size;
    ivec2 pixel_coords = ivec2(img_pos);
    int pixel_id = pixel_coords.x + pixel_coords.y*ubo.frame_width;

    /* Normalized depth looses some information because of the divide by w,
     * so if we don't need read the normalized depth, then it is actually more accurate to store w directly */
#ifdef NORMALIZED_DEPTH
    uint depth = floatBitsToUint(ndc.z);
#else
    uint depth = floatBitsToUint(clip.w);
#endif

    /* update pixel color with point color if we found a close point */
    uint64_t old_depth = frame_buff[pixel_id] >> 32;

    if (depth <= old_depth) {
        uint64_t packed = uint64_t(depth) << 32 | uint64_t(point.color);
        atomicMin(frame_buff[pixel_id], packed);
    }
}
