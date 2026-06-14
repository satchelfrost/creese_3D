#include "creese_3D.h"

#define TINYOBJ_LOADER_C_IMPLEMENTATION
#include "./external/tinyobj_loader_c.h"

typedef enum {
    ATTRIBUTE_POSITION,
    ATTRIBUTE_TEX_COORDS,
    ATTRIBUTE_NORMAL,
    ATTRIBUTE_COUNT,
} Attribute;

typedef struct {
    Vector3 *items;
    size_t count;
    size_t capacity;
} Positions;

typedef struct {
    Vector3 *items;
    size_t count;
    size_t capacity;
} Normals;

typedef struct {
    Vector2 *items;
    size_t count;
    size_t capacity;
} Tex_Coords;

typedef struct {
    uint32_t *items;
    size_t count;
    size_t capacity;
} Indices;

typedef struct {
    uint32_t attr_mask;
    uint32_t attr_count; // TODO: still need this?
    Positions positions;
    Tex_Coords tex_coords;
    Normals normals;
    Indices indices;
} Obj;

typedef struct {
    String_Builder obj;
    String_Builder mtl;
} Obj_File_Data;

void file_reader(void *ctx, const char *file_name, int is_mtl, const char *obj_file_name, char **items, size_t *count)
{
    UNUSED(obj_file_name);

    Obj_File_Data *file_data = (Obj_File_Data *)ctx;
    String_Builder *sb = (is_mtl) ? &file_data->mtl : &file_data->obj;
    if (!read_entire_file(file_name, sb)) {
        *count = 0;
        return;
    }

    *items = sb->items;
    *count = sb->count;
}

void free_obj(Obj obj)
{
    da_free(obj.positions);
    da_free(obj.tex_coords);
    da_free(obj.normals);
    da_free(obj.indices);
}

Obj load_obj(const char *file_name)
{
    Obj obj = {0};
    unsigned int flags = TINYOBJ_FLAG_TRIANGULATE;
    tinyobj_attrib_t attr = {0};
    tinyobj_shape_t *shapes = NULL;
    size_t num_shapes = 0;
    tinyobj_material_t *materials = NULL;
    size_t num_materials = 0;
    Obj_File_Data file_data = {0};

    int res = tinyobj_parse_obj(&attr, &shapes, &num_shapes, &materials,
                                &num_materials, file_name, file_reader, &file_data, flags);
    if (res != TINYOBJ_SUCCESS) {
        fprintf(stderr, "failed to parse obj file %s, error: %d\n", file_name, res);
        return obj;
    }

    if (materials) printf("WARNING: not handling materials\n");

    if (attr.num_vertices)  obj.attr_mask |= (1<<ATTRIBUTE_POSITION);
    if (attr.num_normals)   obj.attr_mask |= (1<<ATTRIBUTE_NORMAL);
    if (attr.num_texcoords) obj.attr_mask |= (1<<ATTRIBUTE_TEX_COORDS);

    for (size_t i = 0; i < attr.num_faces; i++) {
        int v_idx  = attr.faces[i].v_idx;
        int vt_idx = attr.faces[i].vt_idx;
        int vn_idx = attr.faces[i].vn_idx;
        if (attr.num_vertices) {
            Vector3 v = {attr.vertices[v_idx*3 + 0], attr.vertices[v_idx*3 + 1], attr.vertices[v_idx*3 + 2]};
            da_append(&obj.positions, v);
        }
        if (attr.num_normals) {
            Vector3 n = {attr.normals[vn_idx*3 + 0], attr.normals[vn_idx*3 + 1], attr.normals[vn_idx*3 + 2]};
            da_append(&obj.normals, n);
        }
        if (attr.num_texcoords) {
            Vector2 t = {attr.texcoords[vt_idx*2 + 0], attr.texcoords[vt_idx*2 + 1]};
            da_append(&obj.tex_coords, t);
        }

        da_append(&obj.indices, v_idx); // can't this just be i? shouldn't it?
    }

    tinyobj_attrib_free(&attr);
    tinyobj_shapes_free(shapes, num_shapes);
    tinyobj_materials_free(materials, num_materials);

    return obj;
}

int main()
{
    Obj obj = load_obj("assets/dragon.obj");
    if (!obj.attr_mask) return 1;

    init_window(500, 500, "triangle mesh");

    while (!window_should_close()) {
        begin_drawing();
            clear_background(BLUE);
        end_drawing();
    }

    close_window();
    return 0;
}
