#include "creese_3D.h"

#define FONT_SIZE 42

/*
 *      1---------3
 *    / |        /|
 *   0----------2 |
 *   |  |       | |
 *   |  |       | |
 *   |  5-------|-7
 *   |/         |/
 *   4----------6
 *
 */

#define V0 {-0.5f,  0.5f,  0.5f}
#define V1 {-0.5f,  0.5f, -0.5f}
#define V2 { 0.5f,  0.5f,  0.5f}
#define V3 { 0.5f,  0.5f, -0.5f}
#define V4 {-0.5f, -0.5f,  0.5f}
#define V5 {-0.5f, -0.5f, -0.5f}
#define V6 { 0.5f, -0.5f,  0.5f}
#define V7 { 0.5f, -0.5f, -0.5f}
#define N_UP       { 0.0f,  1.0f,  0.0f}
#define N_DOWN     { 0.0f, -1.0f,  0.0f}
#define N_LEFT     {-1.0f,  0.0f,  0.0f}
#define N_RIGHT    { 1.0f,  0.0f,  0.0f}
#define N_FORWARD  { 0.0f,  0.0f, -1.0f}
#define N_BACKWARD { 0.0f,  0.0f,  1.0f}

struct {
    Vector3 position;
    Vector3 normal;
    Color color;
} cube_verts[] = {
    // TOP
    { .position = V0, .color = PINK, .normal = N_UP,},
    { .position = V1, .color = PINK, .normal = N_UP,},
    { .position = V2, .color = PINK, .normal = N_UP,},
    { .position = V2, .color = PINK, .normal = N_UP,},
    { .position = V1, .color = PINK, .normal = N_UP,},
    { .position = V3, .color = PINK, .normal = N_UP,},
    // FRONT
    { .position = V0, .color = BLUE, .normal = N_BACKWARD,},
    { .position = V2, .color = BLUE, .normal = N_BACKWARD,},
    { .position = V4, .color = BLUE, .normal = N_BACKWARD,},
    { .position = V4, .color = BLUE, .normal = N_BACKWARD,},
    { .position = V2, .color = BLUE, .normal = N_BACKWARD,},
    { .position = V6, .color = BLUE, .normal = N_BACKWARD,},
    // RIGHT
    { .position = V6, .color = VIOLET, .normal = N_RIGHT,},
    { .position = V2, .color = VIOLET, .normal = N_RIGHT,},
    { .position = V3, .color = VIOLET, .normal = N_RIGHT,},
    { .position = V6, .color = VIOLET, .normal = N_RIGHT,},
    { .position = V3, .color = VIOLET, .normal = N_RIGHT,},
    { .position = V7, .color = VIOLET, .normal = N_RIGHT,},
    // BACK
    { .position = V7, .color = DARKGREEN, .normal = N_FORWARD},
    { .position = V3, .color = DARKGREEN, .normal = N_FORWARD},
    { .position = V5, .color = DARKGREEN, .normal = N_FORWARD},
    { .position = V5, .color = DARKGREEN, .normal = N_FORWARD},
    { .position = V3, .color = DARKGREEN, .normal = N_FORWARD},
    { .position = V1, .color = DARKGREEN, .normal = N_FORWARD},
    // LEFT
    { .position = V5, .color = GOLD, .normal = N_LEFT,},
    { .position = V1, .color = GOLD, .normal = N_LEFT,},
    { .position = V0, .color = GOLD, .normal = N_LEFT,},
    { .position = V5, .color = GOLD, .normal = N_LEFT,},
    { .position = V0, .color = GOLD, .normal = N_LEFT,},
    { .position = V4, .color = GOLD, .normal = N_LEFT,},
    // BOTTOM
    { .position = V5, .color = MAGENTA, .normal = N_DOWN,},
    { .position = V4, .color = MAGENTA, .normal = N_DOWN,},
    { .position = V6, .color = MAGENTA, .normal = N_DOWN,},
    { .position = V5, .color = MAGENTA, .normal = N_DOWN,},
    { .position = V6, .color = MAGENTA, .normal = N_DOWN,},
    { .position = V7, .color = MAGENTA, .normal = N_DOWN,},
};

int main()
{
    Model bunny = load_obj_model("assets/bunny.obj");
    bunny.color = color_to_uint32_t(RED);

    Model cube = {0};
    for (size_t i = 0; i < ARRAY_LEN(cube_verts); i++) {
        da_append(&cube.positions, cube_verts[i].position);
        da_append(&cube.colors, color_to_uint32_t(cube_verts[i].color));
        da_append(&cube.indices, i);
    }
    cube.tri_count = cube.indices.count/3;
    cube.clockwise = true;

    Camera camera = {
        .position   = {0.0f, 2.0f, 5.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .fovy       = 45.0f,
    };

    init_window(640, 480, "triangle mesh");

    init_triangle_model_ds(&bunny);
    update_triangle_model(bunny);
    init_triangle_model_ds(&cube);
    update_triangle_model(cube);

    bool draw_bunny = true;
    Font font = load_font("assets/RobotoMono-Medium.ttf", FONT_SIZE);
    String_Builder sb = {0};

    while (!window_should_close()) {
        if (is_key_down(KEY_F)) printf("%d\n", get_avg_fps());
        if (is_key_pressed(KEY_SPACE)) draw_bunny = !draw_bunny;
        update_camera_free(&camera);

        begin_drawing();
            clear_background(BLACK);
            begin_mode_3D(camera);
                rotate_y(get_time());
                draw_model(draw_bunny ? bunny : cube);
            end_mode_3D();

            sb.count = 0;
            sb_appendf(&sb, "FPS:%d", get_avg_fps());
            draw_text_at_base(font, sb.items, sb.count, 20, FONT_SIZE, WHITE);
        end_drawing();
    }

    destroy_model(bunny);
    destroy_model(cube);
    close_window();
    return 0;
}
