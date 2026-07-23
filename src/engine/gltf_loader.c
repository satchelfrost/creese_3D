#define CGLTF_IMPLEMENTATION
#include "../external/cgltf.h"

//typedef struct {
//    Rvk_Buffer vtx_buff;
//    Rvk_Buffer idx_buff;
//    glTF_Material material;
//    glTF_Indices indices;
//    glTF_Vertices vertices;
//    Attribute_Flags flags;
//} glTF_Primitive;
//
//typedef struct {
//    glTF_Primitive *items;
//    size_t count;
//    size_t capacity;
//} glTF_Primitives;
//
//typedef struct {
//    glTF_Primitives primitives;
//} glTF_Mesh;
//
//typedef struct {
//    glTF_Mesh *items;
//    size_t count;
//    size_t capacity;
//} glTF_Meshes;
//
//typedef struct {
//    Rvk_Texture *items;
//    size_t count;
//    size_t capacity;
//} glTF_Textures;
//
//typedef struct {
//    Cvr_Image *items;
//    size_t count;
//    size_t capacity;
//} glTF_Images;
//
//typedef struct {
//    /* CPU-relevant data.
//     * can be freed once uploaded to GPU */
//    String_Builder file_buffer;
//    cgltf_data *gltf_data;
//
//    /* GPU-relevant data */
//    glTF_Meshes meshes;
//    glTF_Textures textures;
//    glTF_Images images;
//} glTF_Model;

const char *cgltf_res_to_str(cgltf_result res)
{
    switch (res) {
    case cgltf_result_success:         return "success";
    case cgltf_result_data_too_short:  return "data_too_short";
    case cgltf_result_unknown_format:  return "unknown_format";
    case cgltf_result_invalid_json:    return "invalid_json";
    case cgltf_result_invalid_gltf:    return "invalid_gltf";
    case cgltf_result_invalid_options: return "invalid_options";
    case cgltf_result_file_not_found:  return "file_not_found";
    case cgltf_result_io_error:        return "io_error";
    case cgltf_result_out_of_memory:   return "out_of_memory";
    case cgltf_result_legacy_gltf:     return "legacy_gltf";
    default:
        assert(0 && "unreachable");
    }
}

const char *cgltf_attr_type_to_str(cgltf_attribute_type attr_type)
{
    switch (attr_type) {
    case cgltf_attribute_type_position: return "position";
    case cgltf_attribute_type_normal:   return "normal";
    case cgltf_attribute_type_tangent:  return "tangent";
    case cgltf_attribute_type_texcoord: return "texcoord";
    case cgltf_attribute_type_color:    return "color";
    case cgltf_attribute_type_joints:   return "joints";
    case cgltf_attribute_type_weights:  return "weights";
    case cgltf_attribute_type_custom:   return "custom";
    case cgltf_attribute_type_invalid:  return "invalid";
    default:                            return "unknown attribute";
    }
}

#define GLTF_ATTR_PTR(accessor_ptr, out_type) \
    (out_type *)(accessor_ptr)->buffer_view->buffer->data + \
    (accessor_ptr)->buffer_view->offset / sizeof(out_type) + \
    (accessor_ptr)->offset / sizeof(out_type)

void populate_attributes(Mesh *mesh, cgltf_attribute attribute)
{
    float *positions = NULL;
    float *normals   = NULL;
    float *uvs       = NULL;
    float *tangets   = NULL;
    float *colors    = NULL;
    float *joints    = NULL;
    float *weights   = NULL;

    switch (attribute.type) {
    case cgltf_attribute_type_position:
        assert(attribute.data->type == cgltf_type_vec3);
        positions = GLTF_ATTR_PTR(attribute.data, float);
        for (size_t i = 0; i < attribute.data->count; i++) {
            Vector3 position = {positions[i*3+0], positions[i*3+1], positions[i*3+2]};
            da_append(&mesh->cpu.positions, position);
        }
    break;
    case cgltf_attribute_type_normal:
        assert(attribute.data->type == cgltf_type_vec3);
        normals = GLTF_ATTR_PTR(attribute.data, float);
        for (size_t i = 0; i < attribute.data->count; i++) {
            Vector3 normal = {normals[i*3+0], normals[i*3+1], normals[i*3+2]};
            da_append(&mesh->cpu.normals, normal);
        }
    break;
    case cgltf_attribute_type_tangent:
        assert(attribute.data->type == cgltf_type_vec4);
        tangets = GLTF_ATTR_PTR(attribute.data, float);
        for (size_t i = 0; i < attribute.data->count; i++) {
            Vector4 tanget = {tangets[i*4+0], tangets[i*4+1], tangets[i*4+2], tangets[i*4+3]};
            da_append(&mesh->cpu.tangets, tanget);
        }
    break;
    case cgltf_attribute_type_texcoord:
        assert(attribute.data->type == cgltf_type_vec2);
        uvs = GLTF_ATTR_PTR(attribute.data, float);
        for (size_t i = 0; i < attribute.data->count; i++) {
            Vector2 uv = {uvs[i*2+0], uvs[i*2+1]};
            da_append(&mesh->cpu.uvs, uv);
        }
    break;
    case cgltf_attribute_type_color:
        assert(attribute.data->type == cgltf_type_vec3 || attribute.data->type == cgltf_type_vec4);
        colors = GLTF_ATTR_PTR(attribute.data, float);
        for (size_t i = 0; i < attribute.data->count; i++) {
            Color c = {0};
            uint32_t color = 0;
            if (attribute.data->type == cgltf_type_vec3) {
                c.r = colors[i*3+0]*255;
                c.g = colors[i*3+1]*255;
                c.b = colors[i*3+2]*255;
                c.a = 255;
                color = color_to_uint32_t(c);
            } else if (attribute.data->type == cgltf_type_vec4) {
                c.r = colors[i*4+0]*255;
                c.g = colors[i*4+1]*255;
                c.b = colors[i*4+2]*255;
                c.a = colors[i*4+3]*255;
                color = color_to_uint32_t(c);
            }
            da_append(&mesh->cpu.colors, color);
        }
    break;
    case cgltf_attribute_type_joints:
        assert(attribute.data->type == cgltf_type_vec4);
        joints = GLTF_ATTR_PTR(attribute.data, float);
        for (size_t i = 0; i < attribute.data->count; i++) {
            Vector4 joint = {joints[i*4+0], joints[i*4+1], joints[i*4+2], joints[i*4+3]};
            da_append(&mesh->cpu.joints, joint);
        }
    break;
    case cgltf_attribute_type_weights:
        assert(attribute.data->type == cgltf_type_vec4);
        weights = GLTF_ATTR_PTR(attribute.data, float);
        for (size_t i = 0; i < attribute.data->count; i++) {
            Vector4 weight = {weights[i*4+0], weights[i*4+1], weights[i*4+2], weights[i*4+3]};
            da_append(&mesh->cpu.weights, weight);
        }
    break;
    case cgltf_attribute_type_custom:
    case cgltf_attribute_type_invalid:
    default:
        printf("attribute %s unsupported\n", cgltf_attr_type_to_str(attribute.type));
        assert(0);
    }
}

Model load_model_from_gltf_into_memory(const char *file_path)
{

    Model model = {0};
    String_Builder sb = {0};
    if (!read_entire_file(file_path, &sb)) return model;

    cgltf_data *data = NULL;
    cgltf_options options = {0};
    cgltf_result res = cgltf_parse(&options, sb.items, sb.count, &data);
    if (res != cgltf_result_success) {
        printf("gltf error %s, for file %s\n", cgltf_res_to_str(res), file_path);
        return model;
    }

    res = cgltf_load_buffers(&options, data, file_path);
    if (res != cgltf_result_success) {
        printf("gltf error %s, while loading buffers\n", cgltf_res_to_str(res));
        return model;
    }

    /* we need to populate the image type (SRGB for base color, UNORM for other).
     * loading the image doesn't give us the type, but instead the primitive gives us this info,
     * so we reserve some memory ahead of time */
    da_reserve(&model.images, data->images_count);

    for (size_t m = 0; m < data->meshes_count; m++) {
        for (size_t p = 0; p < data->meshes[m].primitives_count; p++) {
            Mesh mesh = {0};
            cgltf_primitive primitive = data->meshes[m].primitives[p];
            assert(primitive.type == cgltf_primitive_type_triangles);
            for (size_t a = 0; a < primitive.attributes_count; a++) {
                cgltf_attribute attribute = primitive.attributes[a];
                populate_attributes(&mesh, attribute);
            }

            /* base color */
            Color color = {
                .r = primitive.material->pbr_metallic_roughness.base_color_factor[0]*255,
                .g = primitive.material->pbr_metallic_roughness.base_color_factor[1]*255,
                .b = primitive.material->pbr_metallic_roughness.base_color_factor[2]*255,
                .a = primitive.material->pbr_metallic_roughness.base_color_factor[3]*255,
            };
            mesh.material.base_color = color_to_uint32_t(color);

            /* various texture indices */
            cgltf_texture *texture = NULL;

            /* albedo texture */
            texture = primitive.material->pbr_metallic_roughness.base_color_texture.texture;
            if (texture) {
                size_t index = cgltf_image_index(data, texture->image);
                mesh.material.albedo_image_index = index;
                model.images.items[index].type = IMAGE_TYPE_SRGB;
                mesh.material.mask |= 1<<MATERIAL_ALBEDO;
            }

            /* normal texture */
            texture = primitive.material->normal_texture.texture;
            if (texture) {
                size_t index = cgltf_image_index(data, texture->image);
                mesh.material.normal_image_index = index;
                model.images.items[index].type = IMAGE_TYPE_UNORM;
                mesh.material.mask |= 1<<MATERIAL_NORMAL;
            }

            texture = primitive.material->pbr_metallic_roughness.metallic_roughness_texture.texture;
            if (texture) {
                size_t index = cgltf_image_index(data, texture->image);
                mesh.material.metallic_roughness_image_index = index;
                model.images.items[index].type = IMAGE_TYPE_UNORM;
                mesh.material.mask |= 1<<MATERIAL_METALLIC_ROUGHNESS;
            }

            if (mesh.material.mask == 0) mesh.material.mask = 1<<MATERIAL_NO_TEXTURES;

            /* grab indices */
            if (primitive.indices->component_type == cgltf_component_type_r_16u) {
                uint16_t *indices = GLTF_ATTR_PTR(primitive.indices, uint16_t);
                for (size_t i = 0; i < primitive.indices->count; i++)
                    da_append(&mesh.cpu.indices, indices[i]);
            } else if (primitive.indices->component_type == cgltf_component_type_r_32u) {
                uint32_t *indices = GLTF_ATTR_PTR(primitive.indices, uint32_t);
                for (size_t i = 0; i < primitive.indices->count; i++)
                    da_append(&mesh.cpu.indices, indices[i]);
            } else {
                printf("index component type %d unsupported\n", primitive.indices->component_type);
                assert(0);
            }

            da_append(&model.meshes, mesh);
        }
    }

    /* get the path prefix to the gltf (i.e. string up to and including the last '/') */
    String_View sv = sv_from_cstr(file_path);
    size_t char_path_count = 0;
    do {
        String_View lhs = sv_chop_by_delim(&sv, '/');
        if (sv.count) {
            char_path_count += lhs.count;
            char_path_count += 1; // include count for '/' charcter
        }
    } while (sv.count);
    String_View path_prefix = sv_from_parts(file_path, char_path_count);

    /* load images */
    size_t checkpoint = temp_save();
    for (size_t i = 0; i < data->images_count; i++) {
        const char *image_path = temp_sprintf(SV_Fmt"%s", SV_Arg(path_prefix), data->images[i].uri);
        Creese_Image image = load_image(image_path);
        image.type = model.images.items[i].type;
        da_append(&model.images, image);
    }
    temp_rewind(checkpoint);

    sb_free(sb);

    return model;
}
