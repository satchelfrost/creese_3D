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

layout(std430, binding = 1) buffer vert_data {
   float positions[ ];
};

layout(std430, binding = 2) buffer normal_data {
   float normals[ ];
};

layout(std430, binding = 3) buffer tex_coord_data {
   float tex_coords[ ];
};

layout(std430, binding = 4) buffer color_data {
   uint colors[ ];
};

layout(std430, binding = 5) buffer tanget_data {
   float tangets[ ];
};

layout(std430, binding = 6) buffer index_data {
   uint indices[ ];
};

layout(std430, binding = 7) buffer frame_data {
   uint64_t frame_buff[ ];
};

layout(local_size_x = 1024, local_size_y = 1, local_size_z = 1) in;

layout(push_constant) uniform constants {
    uint attr_mask;
    uint tri_count;
    uint color;
} pc;

#define ATTRIBUTE_POSITION   0 
#define ATTRIBUTE_TEX_COORDS 1
#define ATTRIBUTE_NORMAL     2

int edge_cross(ivec2 v0, ivec2 v1, ivec2 p)
{
    // basically we can use the sign of the z component of a cross product to determine if a point is inside a triangle
    // however, let's review the cross product.

    // a X b
    //
    // one way to remember the cross product write both 3D vectors size by side, duplicating them on the right:
    // ax ay az ax ay az
    // bx by bz bx by az
    //
    // One by one cross out a column moving right, and take the determinant (ad - bc) of each 2x2,
    //
    // e.g. cross out first column and take ay*bz - by*az
    //
    // .  ay az .  .  .
    // .  by bz .  .  .
    // x-component = (ay*bz - by*az)
    //
    // .  .  az ax .  .
    // .  .  bz bx .  .
    // y-component = (az*bx - bz*ax)
    //
    // .  .  .  ax ay .
    // .  .  .  bx by .
    // z-component = (ax*by - bx*ay)
    //
    // ^We only care about this last one because that tells us if the vector points into the screen
    //  or out of the screen
    //
    ivec2 a = v1 - v0;
    ivec2 b = p  - v0;
    return a.x*b.y - b.x*a.y;
}

bool is_top_left(ivec2 v0, ivec2 v1)
{
    /* top left rule: pixel lies inside triangle if it lies on a flat top edge,
     * or a left edge of a triangle (again this assumes clockwise) */
    ivec2 edge = v1 - v0;
    bool is_top_edge = edge.y == 0 && edge.x > 0;
    bool is_left_edge = edge.y < 0;
    return is_top_edge || is_left_edge;
}

bool in_frustum(vec4 clip)
{
    return -clip.w < clip.x && clip.x < clip.w &&
           -clip.w < clip.y && clip.y < clip.w &&
               0.0 < clip.z && clip.z < clip.w;
}

void main()
{
    uint tri_id = gl_GlobalInvocationID.x;
    if (tri_id >= pc.tri_count) return;
    
    // TODO: actuall handle all attributes
    uint idx0 = indices[tri_id*3+0];
    uint idx1 = indices[tri_id*3+1];
    uint idx2 = indices[tri_id*3+2];
    vec3 pos0 = vec3(positions[idx0*3+0], positions[idx0*3+1], positions[idx0*3+2]);
    vec3 pos1 = vec3(positions[idx1*3+0], positions[idx1*3+1], positions[idx1*3+2]);
    vec3 pos2 = vec3(positions[idx2*3+0], positions[idx2*3+1], positions[idx2*3+2]);
    vec4 color0 = unpackUnorm4x8(colors[idx0]);
    vec4 color1 = unpackUnorm4x8(colors[idx1]);
    vec4 color2 = unpackUnorm4x8(colors[idx2]);

    mat4 mvp = ubo.proj*ubo.view*ubo.model;
    vec4 clip0 = mvp*vec4(pos0.xyz, 1.0);
    vec4 clip1 = mvp*vec4(pos1.xyz, 1.0);
    vec4 clip2 = mvp*vec4(pos2.xyz, 1.0);
    if (!in_frustum(clip0)) return; // TODO: not ideal
    if (!in_frustum(clip1)) return; // TODO: not ideal
    if (!in_frustum(clip2)) return; // TODO: not ideal
    vec3 ndc0 = clip0.xyz/clip0.w;
    vec3 ndc1 = clip1.xyz/clip1.w;
    vec3 ndc2 = clip2.xyz/clip2.w;
    ivec2 img_size = ivec2(ubo.frame_width, ubo.frame_height);
    ivec2 v0 = ivec2((ndc0.xy*0.5 + 0.5)*img_size);
    ivec2 v1 = ivec2((ndc1.xy*0.5 + 0.5)*img_size);
    ivec2 v2 = ivec2((ndc2.xy*0.5 + 0.5)*img_size);

    /* get the 2D bounding box of the projected positions */
    int x_min = min(min(v0.x, v1.x), v2.x);
    int x_max = max(max(v0.x, v1.x), v2.x);
    int y_min = min(min(v0.y, v1.y), v2.y);
    int y_max = max(max(v0.y, v1.y), v2.y);
    int bias0 = is_top_left(v0, v1) ? 0 : -1;
    int bias1 = is_top_left(v1, v2) ? 0 : -1;
    int bias2 = is_top_left(v2, v0) ? 0 : -1;

    float area = float(edge_cross(v0, v1, v2));

    for (int y = y_min; y <= y_max; y++) {
        for (int x = x_min; x <= x_max; x++) {
            ivec2 p = ivec2(x, y);
            int w0 = edge_cross(v0, v1, p) + bias0;
            int w1 = edge_cross(v1, v2, p) + bias1;
            int w2 = edge_cross(v2, v0, p) + bias2;
            /* assumes triangle is clockwise */
            bool inside = w0 >= 0 && w1 >= 0 && w2 >= 0;
            // bool inside = w0 <= 0 && w1 <= 0 && w2 <= 0;
            if (!inside) continue;

            float alpha = w0/area;
            float beta  = w1/area;
            float gamma = w2/area;

            uint depth = floatBitsToUint(alpha*ndc0.z + beta*ndc1.z + gamma*ndc2.z);
            uint color = packUnorm4x8(alpha*color0 + beta*color1 + gamma*color2);
            // uint color = 0xffffffff;
            int pixel_id = p.x + p.y*ubo.frame_width;
            uint64_t old_depth = frame_buff[pixel_id] >> 32;

            /* early-z test (the non-atomic depth test first) */
            if (depth <= old_depth) {
                uint64_t packed = uint64_t(depth) << 32 | uint64_t(color);
                /* atomic depth test */
                atomicMin(frame_buff[pixel_id], packed);
            }
        }
    }
}
