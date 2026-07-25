#define TINYOBJ_LOADER_C_IMPLEMENTATION
#include "../external/tinyobj_loader_c.h"

typedef struct {
    String_Builder obj;
    String_Builder mtl;
} Obj_File_Data;

void obj_file_reader(void *ctx, const char *file_path, int is_mtl, const char *obj_file_path, char **items, size_t *count)
{
    UNUSED(obj_file_path);

    Obj_File_Data *file_data = (Obj_File_Data *)ctx;
    String_Builder *sb = (is_mtl) ? &file_data->mtl : &file_data->obj;
    if (!read_entire_file(file_path, sb)) {
        *count = 0;
        return;
    }

    *items = sb->items;
    *count = sb->count;
}

Model load_model_from_obj_into_memory(const char *file_path)
{
    Model model = {0};
    Mesh mesh = {0};
    unsigned int flags = TINYOBJ_FLAG_TRIANGULATE;
    tinyobj_attrib_t attr = {0};
    tinyobj_shape_t *shapes = NULL;
    size_t num_shapes = 0;
    tinyobj_material_t *materials = NULL;
    size_t num_materials = 0;
    Obj_File_Data file_data = {0};

    int res = tinyobj_parse_obj(&attr, &shapes, &num_shapes, &materials,
                                &num_materials, file_path, obj_file_reader, &file_data, flags);
    if (res != TINYOBJ_SUCCESS) {
        fprintf(stderr, "failed to parse obj file %s, error: %d\n", file_path, res);
        return model;
    }

    if (!attr.num_vertices)  {
        fprintf(stderr, "model contained no vertices\n");
        return model;
    }

    for (size_t i = 0; i < attr.num_faces; i++) {
        int v_idx  = attr.faces[i].v_idx;
        int vt_idx = attr.faces[i].vt_idx;
        int vn_idx = attr.faces[i].vn_idx;
        if (attr.num_vertices) {
            Vector3 v = {attr.vertices[v_idx*3 + 0], attr.vertices[v_idx*3 + 1], attr.vertices[v_idx*3 + 2]};
            da_append(&mesh.cpu.positions, v);
        }
        if (attr.num_normals) {
            Vector3 n = {attr.normals[vn_idx*3 + 0], attr.normals[vn_idx*3 + 1], attr.normals[vn_idx*3 + 2]};
            da_append(&mesh.cpu.normals, n);
        }
        if (attr.num_texcoords) {
            Vector2 t = {attr.texcoords[vt_idx*2 + 0], attr.texcoords[vt_idx*2 + 1]};
            da_append(&mesh.cpu.uvs, t);
        }

        da_append(&mesh.cpu.indices, i);
    }

    mesh.tri_count = mesh.cpu.indices.count/3;

    // is this a good default?
    mesh.ccw = true;

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
        da_append(&mesh.cpu.colors, final_color);
        da_append(&mesh.cpu.colors, final_color);
        da_append(&mesh.cpu.colors, final_color);
    }

    tinyobj_attrib_free(&attr);
    tinyobj_shapes_free(shapes, num_shapes);
    tinyobj_materials_free(materials, num_materials);
    sb_free(file_data.mtl);
    sb_free(file_data.obj);

    // obj is limited to one fused mesh for now
    da_append(&model.meshes, mesh);

    return model;
}

Model load_model_from_obj(const char *file_path)
{
    Model model = load_model_from_obj_into_memory(file_path);
    load_model_gpu(&model);
    return model;
}
