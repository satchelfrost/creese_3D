#include "creese_3D.h"

#define GLTF_FILE "assets/psx/scene.gltf"
// #define GLTF_FILE "assets/robot.glb"

int main()
{
    Model model = load_model_from_gltf_into_memory(GLTF_FILE);

    init_window(500, 500, "loading gltf");

    while (!window_should_close()) {
        begin_drawing();
            clear_background(BLUE);
        end_drawing();
    }

    close_window();

    return 0;
}
