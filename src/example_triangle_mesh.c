#include "creese_3D.h"

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

struct {
    Vector3 position;
    Color color;
} triangle[] = {
    { .position = {-0.5f, -0.5f, 0.0f}, .color = RED,},
    { .position = { 0.0f,  0.5f, 0.0f}, .color = GREEN,},
    { .position = { 0.5f, -0.5f, 0.0f}, .color = BLUE,},
};

int main()
{
    Model model = {0};

    // model = load_obj_model("assets/dragon.obj");

    // String_Builder sb = {0};
    // model.attr_mask = 1<<ATTRIBUTE_POSITION;
    // if (!read_entire_file("assets/dragon.bin", &sb)) return 1;
    // model.positions.items    = (Vector3 *)sb.items;
    // model.positions.count    = sb.count/(sizeof(Vector3));
    // model.positions.capacity = sb.capacity/(sizeof(Vector3));
    // model.tri_count = model.positions.count/3;

    model.attr_mask = 1<<ATTRIBUTE_POSITION | 1<<ATTRIBUTE_COLOR | 1<<ATTRIBUTE_NORMAL;
    for (size_t i = 0; i < ARRAY_LEN(cube_verts); i++) {
        da_append(&model.positions, cube_verts[i].position);
        da_append(&model.normals, cube_verts[i].normal);
        da_append(&model.colors, color_to_uint32_t(cube_verts[i].color));
        da_append(&model.indices, i);
    }
    model.tri_count = model.indices.count/3;

    // model.attr_mask = 1<<ATTRIBUTE_POSITION | 1<<ATTRIBUTE_COLOR;
    // for (size_t i = 0; i < ARRAY_LEN(triangle); i++) {
    //     da_append(&model.positions, triangle[i].position);
    //     da_append(&model.colors, color_to_uint32_t(triangle[i].color));
    //     da_append(&model.indices, i);
    // }
    // model.tri_count = model.indices.count/3;

    Camera camera = {
        .position   = {0.0f, 1.0f, 3.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .fovy       = 45.0f,
    };

    init_window(640, 480, "triangle mesh");

    init_triangle_model_ds(&model);
    update_triangle_model(model);

    while (!window_should_close()) {
        if (is_key_down(KEY_SPACE)) printf("%d\n", get_avg_fps());
        update_camera_free(&camera);

        begin_drawing();
            clear_background(BLACK);
            begin_mode_3D(camera);
                rotate_y(get_time());
                draw_model(model);
            end_mode_3D();
        end_drawing();
    }

    destroy_model(model);
    close_window();
    return 0;
}
