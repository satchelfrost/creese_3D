#include "creese_3D.h"

typedef struct {
    Point *items;
    size_t count;
    size_t capacity;
} Points;

Points gen_points(size_t num_points)
{
    Points points = {0};

    for (size_t i = 0; i < num_points; i++) {
        float theta = PI*rand()/RAND_MAX;
        float phi   = 2.0f*PI*rand()/RAND_MAX;
        float r     = 10.0f*rand()/RAND_MAX;
        uint32_t color = color_to_uint32_t(color_from_hsv(r*360.0f, 1.0f, 1.0f));
        Point point = {
            .x     = r*sin(theta)*cos(phi),
            .y     = r*sin(theta)*sin(phi),
            .z     = r*cos(theta),
            .color = color,
        };
        da_append(&points, point);
    }

    return points;
}

int main()
{
    init_window(1600, 900, "Basic GPU point rasterization");

    Camera camera = {
        .position = {0.0f, 0.0f, 10.0f},
        .target   = {0.0f, 0.0f, 0.0f},
        .up       = {0.0f, 1.0f, 0.0f},
        .fovy     = 45.0f,
    };

    String_Builder sb = {0};
    Font font = load_font("assets/RobotoMono-Medium.ttf", 40);
    Points points = gen_points(1000000);

    Rvk_Buffer point_buff = create_compute_buffer(sizeof(Point)*points.count, points.items);
    VkDescriptorSet render_ds = VK_NULL_HANDLE;
    alloc_point_render_ds(&render_ds);
    update_point_render_ds(point_buff, render_ds);

    while (!window_should_close()) {
        /* input */
        update_camera_free(&camera);
        if (is_key_down(KEY_F)) log_fps();

        /* drawing */
        begin_drawing();
            begin_mode_3D(camera);
                clear_background(DARKGRAY);
                draw_points(render_ds, points.count, 0);

                sb.count = 0;
                sb_appendf(&sb, "FPS:%d", get_avg_fps());
                draw_text_at_base(font, sb.items, sb.count, 20, 40, BLACK);
            end_mode_3D();
        end_drawing();
    }

    destroy_buffer(point_buff);
    da_free(points);
    unload_font(font);

    close_window();

    return 0;
}
