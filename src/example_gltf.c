#include "creese_3D.h"

#define FONT_SIZE 42

#define GLTF_MODEL "assets/psx/scene.gltf"
// #define GLTF_MODEL "assets/robot.glb"
// #define GLTF_MODEL "assets/sponza/Sponza.gltf"

int main()
{
    printf("loading model %s...\n", GLTF_MODEL);
    Model model = load_model_from_gltf_into_memory(GLTF_MODEL);
    printf("loaded model %s\n", GLTF_MODEL);
    Font font = load_font("assets/RobotoMono-Medium.ttf", FONT_SIZE);
    String_Builder sb = {0};

    init_window(500, 500, "gltf");

    load_model_gpu(&model);
    Camera camera = {
        .position = {0.0f, 2.0f, 5.0f},
        .target   = {0.0f, 0.0f, 0.0f},
        .up       = {0.0f, 1.0f, 0.0f},
        .fovy     = 45.0f,
    };

    /* this particular asset has separate meshes, which are actually models */
    int mesh_idx = 0;

    while (!window_should_close()) {
        update_camera_free(&camera);

        if (is_key_pressed(KEY_SPACE)) mesh_idx = (mesh_idx + 1)%model.meshes.count;

        begin_drawing();
            clear_background(DARKGRAY);
            begin_mode_3D(camera);
                rotate_y(get_time());
                scale(0.01, 0.01, 0.01);
                draw_mesh(model.meshes.items[mesh_idx]);
                // draw_model(model);
            end_mode_3D();

            // draw FPS counter
            sb.count = 0;
            sb_appendf(&sb, "FPS:%d", get_avg_fps());
            draw_text_at_base(font, sb.items, sb.count, 20, FONT_SIZE, WHITE);
        end_drawing();
    }

    destroy_model(model);

    close_window();

    return 0;
}

