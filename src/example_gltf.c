#include "creese_3D.h"

int main()
{
    // Model model = load_model_from_gltf_into_memory("assets/psx/scene.gltf");
    Model model = load_model_from_gltf_into_memory("assets/robot.glb");

    init_window(500, 500, "loading gltf");

    while (!window_should_close()) {
        begin_drawing();
            clear_background(BLUE);
        end_drawing();
    }

    close_window();

    return 0;
}
