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
    default:
        assert(0 && "unreachable");
    }
}

Model load_model_from_gltf_into_memory(const char *file_name)
{
    Model model = {0};
    String_Builder sb = {0};
    if (!read_entire_file(file_name, &sb)) return model;

    cgltf_data *data = NULL;
    cgltf_options options = {0};
    cgltf_result res = cgltf_parse(&options, sb.items, sb.count, &data);
    if (res != cgltf_result_success) {
        printf("gltf error %s, for file %s\n", cgltf_res_to_str(res), file_name);
        return model;
    }

    res = cgltf_load_buffers(&options, data, file_name);
    if (res != cgltf_result_success) {
        printf("gltf error %s, while loading buffers\n", cgltf_res_to_str(res));
        return model;
    }

    printf("mesh count %zu\n", data->meshes_count);
    
    sb_free(sb);

//
//    /* we need to populate the image type (SRGB for base color, UNORM for other).
//     * loading the image doesn't give us the type, but instead the primitive gives us this info,
//     * so we reserve some memory ahead of time */
//    da_reserve(&model->images, model->gltf_data->images_count);
//
//    if (print_progress) printf("loading meshes...\n");
//    for (size_t m = 0; m < model->gltf_data->meshes_count; m++) {
//
//        glTF_Mesh mesh = {0};
//
//        for (size_t p = 0; p < model->gltf_data->meshes[m].primitives_count; p++) {
//            cgltf_primitive primitive = model->gltf_data->meshes[m].primitives[p];
//            assert(primitive.type == cgltf_primitive_type_triangles);
//
//            /* interleave the attributes for this primitive */
//            glTF_Primitive prim = {0};
//            for (size_t a = 0; a < primitive.attributes_count; a++)
//                populate_gltf_vertices(&prim, primitive.attributes[a]);
//
//            /* grab material indices */
//            cgltf_texture *texture = NULL;
//            texture = primitive.material->pbr_metallic_roughness.base_color_texture.texture;
//            if (texture) {
//                prim.material.base_image_index = cgltf_image_index(model->gltf_data, texture->image);
//                prim.material.flags |= MATERIAL_BASE;
//                // memcpy(prim.material.base_color_factor, primitive.material->pbr_metallic_roughness.base_color_factor, 4*sizeof(float));
//                prim.material.base_color_factor[0] = primitive.material->pbr_metallic_roughness.base_color_factor[0];
//                prim.material.base_color_factor[1] = primitive.material->pbr_metallic_roughness.base_color_factor[1];
//                prim.material.base_color_factor[2] = primitive.material->pbr_metallic_roughness.base_color_factor[2];
//                prim.material.base_color_factor[3] = primitive.material->pbr_metallic_roughness.base_color_factor[3];
//            }
//            texture = primitive.material->normal_texture.texture;
//            if (texture) {
//                prim.material.normal_image_index = cgltf_image_index(model->gltf_data, texture->image);
//                prim.material.flags |= MATERIAL_NORMAL;
//            }
//            texture = primitive.material->pbr_metallic_roughness.metallic_roughness_texture.texture;
//            if (texture) {
//                prim.material.metallic_roughness_image_index = cgltf_image_index(model->gltf_data, texture->image);
//                prim.material.flags |= MATERIAL_METALLIC_ROUGHNESS;
//            }
//
//            /* store the image type */
//            if (prim.material.flags & MATERIAL_BASE)
//                model->images.items[prim.material.base_image_index].type = IMAGE_TYPE_SRGB;
//            if (prim.material.flags & MATERIAL_NORMAL)
//                model->images.items[prim.material.normal_image_index].type = IMAGE_TYPE_UNORM;
//            if (prim.material.flags & MATERIAL_METALLIC_ROUGHNESS)
//                model->images.items[prim.material.metallic_roughness_image_index].type = IMAGE_TYPE_UNORM;
//
//            /* grab indices */
//            assert(primitive.indices->component_type == cgltf_component_type_r_16u);
//
//            // uint16_t *indices = (uint16_t *)cgltf_buffer_view_data(primitive.indices->buffer_view) + primitive.indices->offset/2;
//            uint16_t *indices = GLTF_ATTR_PTR(primitive.indices, uint16_t);
//            for (size_t i = 0; i < primitive.indices->count; i++)
//                da_append(&prim.indices, indices[i]);
//
//            da_append(&mesh.primitives, prim);
//        }
//        da_append(&model->meshes, mesh);
//    }
//
//    /* load images */
//    if (print_progress) printf("loading images...\n");
//    for (size_t i = 0; i < model->gltf_data->images_count; i++) {
//        const char *image_path = temp_sprintf("assets/sponza/%s", model->gltf_data->images[i].uri); // TODO: easy fix, but bug
//
//        if (print_progress) {
//            float percentage = i / (float)model->gltf_data->images_count;
//            size_t loaded = percentage*PROGRESS_BAR;
//            printf("\r[");
//            for (size_t j = 0; j < PROGRESS_BAR; j++) {
//                printf("%s", (j <= loaded) ? "=" : ".");
//            }
//            printf("] %.f%%", (i == model->gltf_data->images_count - 1) ?  100 : percentage*100);
//            fflush(stdout);
//        }
//
//        Cvr_Image image = load_image(image_path);
//        image.type = model->images.items[i].type;
//        da_append(&model->images, image);
//    }
//    if (print_progress) printf("\n");

    return model;
}

//void destroy_gltf_model(glTF_Model model)
//{
//    for (size_t i = 0; i < model.meshes.count; i++) {
//        glTF_Mesh mesh = model.meshes.items[i];
//        for (size_t j = 0; j < mesh.primitives.count; j++) {
//            glTF_Primitive p = mesh.primitives.items[j];
//            da_free(p.vertices);
//            da_free(p.indices);
//            rvk_destroy_buffer(p.vtx_buff);
//            rvk_destroy_buffer(p.idx_buff);
//        }
//    }
//
//    for (size_t i = 0; i < model.textures.count; i++)
//        rvk_destroy_texture(model.textures.items[i]);
//}

