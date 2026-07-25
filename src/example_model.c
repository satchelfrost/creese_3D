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

/*
 *         1
 *       /  \
 *      /    \
 *     /      \
 *    /        \
 *   0----------2
 */
static struct {
    Vector3 position;
    Color color;
} triangle_verts[] = {
    {.position = {-0.5, -0.5, 0.0}, .color = RED},
    {.position = { 0.0,  0.5, 0.0}, .color = GREEN},
    {.position = { 0.5, -0.5, 0.0}, .color = BLUE},
};

/*
 *   1----------3
 *   | .        |
 *   |   .      |
 *   |     .    |
 *   |       .  |
 *   0----------2
 */
static struct {
    Vector3 position;
    Color color;
} quad_verts[] = {
    {.position = {-0.5, -0.5, 0.0}, .color = RED},
    {.position = {-0.5,  0.5, 0.0}, .color = BLUE},
    {.position = { 0.5, -0.5, 0.0}, .color = GREEN},
    {.position = { 0.5,  0.5, 0.0}, .color = GOLD},
};

static uint32_t quad_indices[] = {0, 1, 2, 2, 1, 3};

/*
 *         3
 *       / .\
 *      /  . \
 *     /  .1. \
 *    / .    . \
 *   2----------0
 */

static struct {
    Vector3 position;
    Color color;
} tetrahedron_verts[] = {
    {.position = { 0.000f, -0.333f,  0.943f}, .color = GOLD},
    {.position = { 0.816f, -0.333f, -0.471f}, .color = BLUE},
    {.position = {-0.816f, -0.333f, -0.471f}, .color = DARKGREEN},
    {.position = { 0.000f,  1.000f,  0.000f}, .color = VIOLET},
};

static uint32_t tetrahedron_indices[] = {
    0, 3, 1,
    0, 2, 3,
    0, 1, 2,
    3, 2, 1,
};

enum {
    MODEL_CUBE,
    MODEL_TRIANGLE,
    MODEL_QUAD,
    MODEL_TETRAHEDRON,
    MODEL_BUNNY,
    MODEL_COUNT,
} model = 0;

int main()
{
    init_window(640, 480, "triangle mesh");

    Model models[MODEL_COUNT] = {0};

    /* cube */
    Mesh mesh = {0};
    for (size_t i = 0; i < ARRAY_LEN(cube_verts); i++) {
        da_append(&mesh.cpu.positions, cube_verts[i].position);
        da_append(&mesh.cpu.colors, color_to_uint32_t(cube_verts[i].color));
        da_append(&mesh.cpu.normals, cube_verts[i].normal);
        da_append(&mesh.cpu.indices, i);
    }
    mesh.tri_count = mesh.cpu.indices.count/3;
    da_append(&models[MODEL_CUBE].meshes, mesh);

    /* tetrahedron */
    mesh = (Mesh){0};
    for (size_t i = 0; i < ARRAY_LEN(tetrahedron_verts); i++) {
        da_append(&mesh.cpu.positions, tetrahedron_verts[i].position);
        da_append(&mesh.cpu.colors, color_to_uint32_t(tetrahedron_verts[i].color));
    }
    for (size_t i = 0; i < ARRAY_LEN(tetrahedron_indices); i++)
        da_append(&mesh.cpu.indices, tetrahedron_indices[i]);
    mesh.tri_count = 4;
    da_append(&models[MODEL_TETRAHEDRON].meshes, mesh);

    /* quad */
    mesh = (Mesh){0};
    for (size_t i = 0; i < ARRAY_LEN(quad_verts); i++) {
        da_append(&mesh.cpu.positions, quad_verts[i].position);
        da_append(&mesh.cpu.colors, color_to_uint32_t(quad_verts[i].color));
    }
    for (size_t i = 0; i < ARRAY_LEN(quad_indices); i++)
        da_append(&mesh.cpu.indices, quad_indices[i]);
    mesh.tri_count = 2;
    da_append(&models[MODEL_QUAD].meshes, mesh);

    /* triangle */
    mesh = (Mesh){0};
    for (size_t i = 0; i < ARRAY_LEN(triangle_verts); i++) {
        da_append(&mesh.cpu.positions, triangle_verts[i].position);
        da_append(&mesh.cpu.colors, color_to_uint32_t(triangle_verts[i].color));
        da_append(&mesh.cpu.indices, i);
    }
    mesh.tri_count = 1;
    da_append(&models[MODEL_TRIANGLE].meshes, mesh);

    /* note we're not using `load_model_from_obj` because we want to manually call load_model_gpu */
    models[MODEL_BUNNY] = load_model_from_obj_into_memory("assets/bunny.obj");
    models[MODEL_BUNNY].meshes.items[0].material.color = color_to_uint32_t(RED);

    /* load models to the gpu */
    for (size_t i = 0; i < MODEL_COUNT; i++) load_model_gpu(&models[i]);

    Font font = load_font("assets/RobotoMono-Medium.ttf", FONT_SIZE);
    String_Builder sb = {0};
    Camera camera = {
        .position   = {0.0f, 2.0f, 5.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .fovy       = 45.0f,
    };

    while (!window_should_close()) {
        if (is_key_pressed(KEY_SPACE)) model = (model + 1)%MODEL_COUNT;
        update_camera_free(&camera);

        begin_drawing();
            clear_background(BLACK);
            begin_mode_3D(camera);
                rotate_y(get_time());
                draw_model(models[model]);
            end_mode_3D();

            sb.count = 0;
            sb_appendf(&sb, "FPS:%d", get_avg_fps());
            draw_text_at_base(font, sb.items, sb.count, 20, FONT_SIZE, WHITE);
        end_drawing();
    }

    for (size_t i = 0; i < MODEL_COUNT; i++) destroy_model(models[i]);

    close_window();

    return 0;
}
