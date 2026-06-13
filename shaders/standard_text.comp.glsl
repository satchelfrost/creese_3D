#version 450

#extension GL_ARB_gpu_shader_int64 : enable

layout (binding = 0) uniform sampler2D bitmap;

layout(std430, binding = 1) buffer frame_data {
   uint64_t frame_buff[ ];
};

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(push_constant) uniform constants
{
    int x;
    int y;
    int x0;
    int y0;
    int x1;
    int y1;
    uint color;
    int frame_width;
    int frame_height;
    int bitmap_width;
    int bitmap_height;
} pc;

uint alpha_blend(uint top, uint bottom)
{
    uint top_a = (top>>24 & 0xff);
    uint top_r = (top>> 0 & 0xff) * top_a;
    uint top_g = (top>> 8 & 0xff) * top_a;
    uint top_b = (top>>16 & 0xff) * top_a;
    uint bottom_r = (bottom>> 0 & 0xff) * (255 - top_a);
    uint bottom_g = (bottom>> 8 & 0xff) * (255 - top_a);
    uint bottom_b = (bottom>>16 & 0xff) * (255 - top_a);
    uint bottom_a = (bottom>>24 & 0xff) * (255 - top_a);
    uint res_r = (top_r + bottom_r)/255;
    uint res_g = (top_g + bottom_g)/255;
    uint res_b = (top_b + bottom_b)/255;
    uint res_a = (top_a + bottom_a)/255;

    return res_a<<24 | res_b<<16 | res_g<<8 | res_r;
}

void main()
{
    uvec2 id = gl_GlobalInvocationID.xy;
    int glyph_width  = pc.x1 - pc.x0 + 1;
    int glyph_height = pc.y1 - pc.y0 + 1;
    ivec2 pen = ivec2(pc.x + id.x, pc.y + id.y);
    if (pen.x >= pc.frame_width || pen.y >= pc.frame_height) return; // frame bounds check
    if (id.x  >= glyph_width    || id.y  >= glyph_height)    return; // glyph bounds check
    int pixel_id = pen.x + pen.y*pc.frame_width;

    // +--------------------+ <--- screen
    // |  (x,y)             |
    // |    +----+ <-------------- text rectangle
    // |    |    |          |
    // |    |  . <---------------- pen position (i.e. <x,y> + <id.x, id.y>)
    // |    +----+          |
    // |                    |
    // |                    |
    // +--------------------+

    ivec2 texel_coords = pen - ivec2(pc.x, pc.y) + ivec2(pc.x0, pc.y0);
    bool inside_bitmap = 0 <= texel_coords.x && texel_coords.x < pc.bitmap_width &&
                         0 <= texel_coords.y && texel_coords.y < pc.bitmap_height;
    if (!inside_bitmap) return;
    float alpha = texelFetch(bitmap, texel_coords, 0).r;

    uint top = pc.color & 0x00ffffff;
    uint alpha_int = uint(alpha*255);
    top = top | alpha_int<<24;

    uint bottom = uint(frame_buff[pixel_id]);
    uint result = alpha_blend(top, bottom);

    /* note theses are not thread safe, however for text it generally shouldn't matter because there's only
     * one invocation corresponding to one pixel, and there should also be a barrier on the frame buffer anyway */
    frame_buff[pixel_id] &= 0xffffffffff000000UL; // clear rgb
    frame_buff[pixel_id] |= result;
}
