#include "creese_3D.h"

#define TINYOBJ_LOADER_C_IMPLEMENTATION
#include "./external/tinyobj_loader_c.h"

// typedef struct {
//     String_Builder obj;
//     String_Builder mtl;
// } Obj_File_Data;
//
// void file_reader(void *ctx, const char *file_name, int is_mtl, const char *obj_file_name, char **items, size_t *count)
// {
//     UNUSED(obj_file_name);
//
//     Obj_File_Data *file_data = (Obj_File_Data *)ctx;
//     String_Builder *sb = (is_mtl) ? &file_data->mtl : &file_data->obj;
//     if (!read_entire_file(file_name, sb)) {
//         *count = 0;
//         return;
//     }
//
//     *items = sb->items;
//     *count = sb->count;
// }
//
// void free_obj(Obj obj)
// {
//     da_free(obj.positions);
//     da_free(obj.tex_coords);
//     da_free(obj.normals);
//     da_free(obj.indices);
// }
//
// Obj load_obj(const char *file_name)
// {
//     Obj obj = {0};
//     unsigned int flags = TINYOBJ_FLAG_TRIANGULATE;
//     tinyobj_attrib_t attr = {0};
//     tinyobj_shape_t *shapes = NULL;
//     size_t num_shapes = 0;
//     tinyobj_material_t *materials = NULL;
//     size_t num_materials = 0;
//     Obj_File_Data file_data = {0};
//
//     int res = tinyobj_parse_obj(&attr, &shapes, &num_shapes, &materials,
//                                 &num_materials, file_name, file_reader, &file_data, flags);
//     if (res != TINYOBJ_SUCCESS) {
//         fprintf(stderr, "failed to parse obj file %s, error: %d\n", file_name, res);
//         return obj;
//     }
//
//     if (materials) printf("WARNING: not handling materials\n");
//
//     if (attr.num_vertices)  obj.attr_mask |= (1<<ATTRIBUTE_POSITION);
//     if (attr.num_normals)   obj.attr_mask |= (1<<ATTRIBUTE_NORMAL);
//     if (attr.num_texcoords) obj.attr_mask |= (1<<ATTRIBUTE_TEX_COORDS);
//
//     for (size_t i = 0; i < attr.num_faces; i++) {
//         int v_idx  = attr.faces[i].v_idx;
//         int vt_idx = attr.faces[i].vt_idx;
//         int vn_idx = attr.faces[i].vn_idx;
//         if (attr.num_vertices) {
//             Vector3 v = {attr.vertices[v_idx*3 + 0], attr.vertices[v_idx*3 + 1], attr.vertices[v_idx*3 + 2]};
//             da_append(&obj.positions, v);
//         }
//         if (attr.num_normals) {
//             Vector3 n = {attr.normals[vn_idx*3 + 0], attr.normals[vn_idx*3 + 1], attr.normals[vn_idx*3 + 2]};
//             da_append(&obj.normals, n);
//         }
//         if (attr.num_texcoords) {
//             Vector2 t = {attr.texcoords[vt_idx*2 + 0], attr.texcoords[vt_idx*2 + 1]};
//             da_append(&obj.tex_coords, t);
//         }
//
//         da_append(&obj.indices, v_idx); // can't this just be i? shouldn't it?
//     }
//
//     tinyobj_attrib_free(&attr);
//     tinyobj_shapes_free(shapes, num_shapes);
//     tinyobj_materials_free(materials, num_materials);
//
//     return obj;
// }

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
    // Obj obj = load_obj("assets/dragon.obj");

    Model model = {0};
    model.attr_mask = 1<<ATTRIBUTE_POSITION | 1<<ATTRIBUTE_COLOR | 1<<ATTRIBUTE_NORMAL;
    for (size_t i = 0; i < ARRAY_LEN(cube_verts); i++) {
        da_append(&model.positions, cube_verts[i].position);
        da_append(&model.normals, cube_verts[i].normal);
        da_append(&model.colors, color_to_uint32_t(cube_verts[i].color));
        da_append(&model.indices, i);
    }

    Camera camera = {
        .position   = {0.0f, 1.0f, 2.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .fovy       = 45.0f,
    };

    init_window(500, 500, "triangle mesh");

    init_triangle_model_ds(&model);
    update_triangle_model(model);

    while (!window_should_close()) {
        begin_drawing();
            clear_background(BLUE);
            begin_mode_3D(camera);
                rotate_y(get_time());
                // draw_model();
            end_mode_3D();
        end_drawing();
    }

    close_window();
    return 0;
}
