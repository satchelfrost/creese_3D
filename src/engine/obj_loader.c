#define TINYOBJ_LOADER_C_IMPLEMENTATION
#include "../external/tinyobj_loader_c.h"

typedef struct {
    String_Builder obj;
    String_Builder mtl;
} Obj_File_Data;

void obj_file_reader(void *ctx, const char *file_name, int is_mtl, const char *obj_file_name, char **items, size_t *count)
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

Model load_model_from_obj_to_host_mem(const char *file_name)
{
    Model model = {0};
    unsigned int flags = TINYOBJ_FLAG_TRIANGULATE;
    tinyobj_attrib_t attr = {0};
    tinyobj_shape_t *shapes = NULL;
    size_t num_shapes = 0;
    tinyobj_material_t *materials = NULL;
    size_t num_materials = 0;
    Obj_File_Data file_data = {0};

    int res = tinyobj_parse_obj(&attr, &shapes, &num_shapes, &materials,
                                &num_materials, file_name, obj_file_reader, &file_data, flags);
    if (res != TINYOBJ_SUCCESS) {
        fprintf(stderr, "failed to parse obj file %s, error: %d\n", file_name, res);
        return model;
    }

    if (!attr.num_vertices)  {
        fprintf(stderr, "model contained no vertices\n");
        return model;
    }

    if (attr.num_normals)   model.attribute_mask |= (1<<ATTRIBUTE_NORMAL);
    if (attr.num_texcoords) model.attribute_mask |= (1<<ATTRIBUTE_TEX_COORD);

    for (size_t i = 0; i < attr.num_faces; i++) {
        int v_idx  = attr.faces[i].v_idx;
        int vt_idx = attr.faces[i].vt_idx;
        int vn_idx = attr.faces[i].vn_idx;
        if (attr.num_vertices) {
            Vector3 v = {attr.vertices[v_idx*3 + 0], attr.vertices[v_idx*3 + 1], attr.vertices[v_idx*3 + 2]};
            da_append(&model.cpu.positions, v);
        }
        if (attr.num_normals) {
            Vector3 n = {attr.normals[vn_idx*3 + 0], attr.normals[vn_idx*3 + 1], attr.normals[vn_idx*3 + 2]};
            da_append(&model.cpu.normals, n);
        }
        if (attr.num_texcoords) {
            Vector2 t = {attr.texcoords[vt_idx*2 + 0], attr.texcoords[vt_idx*2 + 1]};
            da_append(&model.cpu.tex_coords, t);
        }

        da_append(&model.cpu.indices, i);
    }

    model.tri_count = model.cpu.indices.count/3;

    // is this a good default?
    model.ccw = true;

    for (size_t i = 0; num_materials && i < attr.num_face_num_verts; i++) {
        int mat_id = attr.material_ids[i];
        Vector3 color = {
            .x = materials[mat_id].diffuse[0],
            .y = materials[mat_id].diffuse[1],
            .z = materials[mat_id].diffuse[2],
        };
        uint32_t r = color.x*255.0f;
        uint32_t g = color.y*255.0f;
        uint32_t b = color.z*255.0f;
        uint32_t a = 255;
        uint32_t final_color = a<<24 | b<<16 | g<<8 | r;
        da_append(&model.cpu.colors, final_color);
        da_append(&model.cpu.colors, final_color);
        da_append(&model.cpu.colors, final_color);
    }

    tinyobj_attrib_free(&attr);
    tinyobj_shapes_free(shapes, num_shapes);
    tinyobj_materials_free(materials, num_materials);
    sb_free(file_data.mtl);
    sb_free(file_data.obj);

    return model;
}

Model load_model_from_obj(const char *file_name)
{
    Model model = load_model_from_obj_to_host_mem(file_name);
    load_model_gpu(&model);
    return model;
}
