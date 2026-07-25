#version 450

#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_EXT_shader_atomic_int64 : enable

layout (binding = 0) uniform UBO {
    mat4 proj;
    mat4 view;
    mat4 model; // TODO: get rid of this
    int frame_width;
    int frame_height;
    uint bg_color; // TODO: also get rid of this
    int padding_1;
    vec4 camera_pos;
} ubo;

layout(std430, binding = 1) buffer vert_data {
   float positions[ ];
};

layout(std430, binding = 2) buffer normal_data {
   float normals[ ];
};

layout(std430, binding = 3) buffer uv_data {
   float uvs[ ];
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

layout(std430, binding = 8) buffer joint_data {
   float joints[ ];
};

layout(std430, binding = 9) buffer weight_data {
   float weights[ ];
};

layout (binding = 10) uniform sampler2D sampler_color;
layout (binding = 11) uniform sampler2D sampler_normal;
layout (binding = 12) uniform sampler2D sampler_metallic_roughness;

layout(local_size_x = 1024, local_size_y = 1, local_size_z = 1) in;

layout(push_constant) uniform constants {
    mat4 model;
    uint attr_mask;
    uint tri_count;
    uint color;
    uint ccw;
    uint material_mask;
} pc;

#define MATERIAL_NO_TEXTURES        (1<<0)
#define MATERIAL_ALBEDO             (1<<1)
#define MATERIAL_METALLIC_ROUGHNESS (1<<2)
#define MATERIAL_NORMAL             (1<<3)

#define ATTRIBUTE_NORMAL       (1<<0)
#define ATTRIBUTE_UV           (1<<1)
#define ATTRIBUTE_TANGET       (1<<2)
#define ATTRIBUTE_COLOR        (1<<3)
#define ATTRIBUTE_JOINT_WEIGHT (1<<4)

// #define BIAS (0.00001f)
#define BIAS (1.0000f)

int edge_cross(ivec2 v0, ivec2 v1, ivec2 p)
{
    // a X b
    //
    // ax ay az ax ay az
    // bx by bz bx by az
    //
    // e.g. cross out first column and take ay*bz - by*az
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
    // ^We only care about the z component one because that tells us if the vector points into the screen
    //  or out of the screen
    ivec2 a = v1 - v0;
    ivec2 b = p  - v0;
    return a.x*b.y - b.x*a.y;
}

bool is_top_left_cw(ivec2 v0, ivec2 v1)
{
    /* top left rule: pixel lies inside triangle if it lies on a flat top edge,
     * or a left edge of a triangle (again this assumes clockwise) */
    ivec2 edge = v1 - v0;
    bool is_top_edge = edge.y == 0 && edge.x > 0;
    bool is_left_edge = edge.y < 0;
    return is_top_edge || is_left_edge;
}

bool is_top_left_ccw(ivec2 v0, ivec2 v1)
{
    /* top left rule: pixel lies inside triangle if it lies on a flat top edge,
     * or a left edge of a triangle (again this assumes counter clockwise) */
    ivec2 edge = v1 - v0;
    bool is_top_edge = edge.y == 0 && edge.x < 0;
    bool is_left_edge = edge.y > 0;
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
    if (indices.length() < 3) return;
    
    /* grab vertices and indices */
    uint idx0 = indices[tri_id*3+0];
    uint idx1 = indices[tri_id*3+1];
    uint idx2 = indices[tri_id*3+2];
    vec4 pos0 = vec4(positions[idx0*3+0], positions[idx0*3+1], positions[idx0*3+2], 1.0);
    vec4 pos1 = vec4(positions[idx1*3+0], positions[idx1*3+1], positions[idx1*3+2], 1.0);
    vec4 pos2 = vec4(positions[idx2*3+0], positions[idx2*3+1], positions[idx2*3+2], 1.0);
    mat4 mvp = ubo.proj*ubo.view*pc.model;
    vec4 clip0 = mvp*vec4(pos0.xyz, 1.0);
    vec4 clip1 = mvp*vec4(pos1.xyz, 1.0);
    vec4 clip2 = mvp*vec4(pos2.xyz, 1.0);
    if (!in_frustum(clip0)) return;
    if (!in_frustum(clip1)) return;
    if (!in_frustum(clip2)) return;
    vec3 ndc0 = clip0.xyz/clip0.w;
    vec3 ndc1 = clip1.xyz/clip1.w;
    vec3 ndc2 = clip2.xyz/clip2.w;
    float inv0 = 1.0/clip0.w;
    float inv1 = 1.0/clip1.w;
    float inv2 = 1.0/clip2.w;
    ivec2 img_size = ivec2(ubo.frame_width, ubo.frame_height);
    ivec2 v0 = ivec2((ndc0.xy*0.5 + 0.5)*img_size);
    ivec2 v1 = ivec2((ndc1.xy*0.5 + 0.5)*img_size);
    ivec2 v2 = ivec2((ndc2.xy*0.5 + 0.5)*img_size);

    /* color */
    vec4 color0; vec4 color1; vec4 color2;
    vec2 uv0; vec2 uv1; vec2 uv2;
    bool have_albedo = (pc.material_mask&MATERIAL_ALBEDO) > 0;
    if (have_albedo) {
        uv0 = vec2(uvs[idx0*2+0], uvs[idx0*2+1]);
        uv1 = vec2(uvs[idx1*2+0], uvs[idx1*2+1]);
        uv2 = vec2(uvs[idx2*2+0], uvs[idx2*2+1]);
        uv0 *= inv0;
        uv1 *= inv1;
        uv2 *= inv2;
    } else if ((pc.attr_mask&ATTRIBUTE_COLOR) > 0) {
        color0 = unpackUnorm4x8(colors[idx0]);
        color1 = unpackUnorm4x8(colors[idx1]);
        color2 = unpackUnorm4x8(colors[idx2]);
    } else {
        color0 = unpackUnorm4x8(pc.color);
        color1 = unpackUnorm4x8(pc.color);
        color2 = unpackUnorm4x8(pc.color);
    }

    /* normals */
    vec3 n0; vec3 n1; vec3 n2;
    if ((pc.attr_mask&ATTRIBUTE_NORMAL) > 0) {
        n0 = vec3(normals[idx0*3+0], normals[idx0*3+1], normals[idx0*3+2]);
        n1 = vec3(normals[idx1*3+0], normals[idx1*3+1], normals[idx1*3+2]);
        n2 = vec3(normals[idx2*3+0], normals[idx2*3+1], normals[idx2*3+2]);
    }

    /* get the 2D bounding box of the projected positions */
    int x_min = min(min(v0.x, v1.x), v2.x);
    int x_max = max(max(v0.x, v1.x), v2.x);
    int y_min = min(min(v0.y, v1.y), v2.y);
    int y_max = max(max(v0.y, v1.y), v2.y);
    int bias0 = 0;
    int bias1 = 0;
    int bias2 = 0;

    /* I may come back to this when it either becomes relevant (e.g. pinholes), or I switch to fixed point math */
    // if (pc.clockwise > 0) {
    //     bias0 = is_top_left_cw(v0, v1) ? 0 : -1;
    //     bias1 = is_top_left_cw(v1, v2) ? 0 : -1;
    //     bias2 = is_top_left_cw(v2, v0) ? 0 : -1;
    // } else {
    //     bias0 = is_top_left_ccw(v0, v1) ? 0 : 1;
    //     bias1 = is_top_left_ccw(v1, v2) ? 0 : 1;
    //     bias2 = is_top_left_ccw(v2, v0) ? 0 : 1;
    // }

    float area = float(edge_cross(v0, v1, v2));
    vec3 light_pos = vec3(2.0, 2.0, 5.0);

    for (int y = y_min; y <= y_max; y++) {
        for (int x = x_min; x <= x_max; x++) {
            ivec2 p = ivec2(x, y);
            int w0 = edge_cross(v1, v2, p) + bias0;
            int w1 = edge_cross(v2, v0, p) + bias1;
            int w2 = edge_cross(v0, v1, p) + bias2;

            bool inside = false;
            if (pc.ccw > 0) inside = w0 <= 0 && w1 <= 0 && w2 <= 0;
            else            inside = w0 >= 0 && w1 >= 0 && w2 >= 0;
            if (!inside) continue;

            float alpha = w0/area;
            float beta  = w1/area;
            float gamma = w2/area;

            uint depth = floatBitsToUint(alpha*ndc0.z + beta*ndc1.z + gamma*ndc2.z);
            uint color;
            vec4 interp_color = alpha*color0 + beta*color1 + gamma*color2;

            if (have_albedo) {
                vec2 uv = alpha*uv0 + beta*uv1 + gamma*uv2;
                float inv = 1.0f/(alpha*inv0 + beta*inv1 + gamma*inv2);
                interp_color = texture(sampler_color, uv*inv);
            }

            if ((pc.attr_mask&ATTRIBUTE_NORMAL) > 0) {
                vec4 world_pos = ubo.model*(alpha*pos0 + beta*pos1 + gamma*pos2);
                vec3 n = alpha*n0 + beta*n1 + gamma*n2;
                n = normalize(transpose(inverse(mat3(ubo.model)))*n);
                vec3 to_light = normalize(light_pos - world_pos.xyz);
                float diffuse = max(dot(n, to_light), 0.1);
                color = packUnorm4x8(diffuse*interp_color);
            } else {
                color = packUnorm4x8(interp_color);
            }

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
