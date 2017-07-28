#ifdef CETECH_CAN_COMPILE

//==============================================================================
// Include
//==============================================================================

#include <celib/allocator.h>
#include <celib/array.inl>
#include <celib/handler.inl>

#include <cetech/core/hash.h>
#include <cetech/core/memory.h>

#include <cetech/machine/machine.h>
#include <cetech/core/api_system.h>

#include <cetech/engine/resource.h>
#include <cetech/core/os/errors.h>
#include <cetech/core/yaml.h>

#include <cetech/engine/entity.h>

#include "material_blob.h"
#include <cetech/core/os/path.h>
#include <cetech/core/os/vio.h>

CETECH_DECL_API(ct_memory_a0);
CETECH_DECL_API(ct_resource_a0);
CETECH_DECL_API(ct_path_a0);
CETECH_DECL_API(ct_vio_a0);
CETECH_DECL_API(ct_hash_a0);

using namespace celib;

namespace material_compiler {
    namespace {
        struct material_compile_output {
            Array<char> uniform_names;
            Array<uint8_t> data;

            uint32_t texture_count;
            uint32_t vec4f_count;
            uint32_t mat33f_count;
            uint32_t mat44f_count;
        };

        void _preprocess(const char *filename,
                         yaml_node_t root,
                         ct_compilator_api *capi) {
            auto a = ct_memory_a0.main_allocator();

            yaml_node_t parent_node = yaml_get_node(root, "parent");

            if (yaml_is_valid(parent_node)) {
                char prefab_file[256] = {};
                char prefab_str[256] = {};
                yaml_as_string(parent_node, prefab_str,
                               CETECH_ARRAY_LEN(prefab_str));
                snprintf(prefab_file, CETECH_ARRAY_LEN(prefab_file),
                         "%s.material",
                         prefab_str);

                capi->add_dependency(filename, prefab_file);

                const char *source_dir = ct_resource_a0.compiler_get_source_dir();
                char *full_path = ct_path_a0.join(a, 2, source_dir,
                                                  prefab_file);

                ct_vio *prefab_vio = ct_vio_a0.from_file(full_path,
                                                         VIO_OPEN_READ);

                CEL_FREE(a, full_path);

                char prefab_data[prefab_vio->size(prefab_vio->inst) + 1];
                memset(prefab_data, 0, prefab_vio->size(prefab_vio->inst) + 1);
                prefab_vio->read(prefab_vio->inst, prefab_data, sizeof(char),
                                 prefab_vio->size(prefab_vio->inst));
                prefab_vio->close(prefab_vio->inst);

                yaml_document_t h;
                yaml_node_t prefab_root = yaml_load_str(prefab_data, &h);

                _preprocess(filename, prefab_root, capi);
                yaml_merge(root, prefab_root);
            }
        }

        void _forach_texture_clb(yaml_node_t key,
                                 yaml_node_t value,
                                 void *_data) {

            struct material_compile_output *output = (material_compile_output *) _data;

            output->texture_count += 1;

            char tmp_buffer[512] = {};
            char uniform_name[32] = {};

            yaml_as_string(key, uniform_name,
                           CETECH_ARRAY_LEN(uniform_name) - 1);

            yaml_as_string(value, tmp_buffer, CETECH_ARRAY_LEN(tmp_buffer));
            uint64_t texture_name = ct_hash_a0.id64_from_str(tmp_buffer);

            array::push(output->uniform_names, uniform_name,
                        CETECH_ARRAY_LEN(uniform_name));
            array::push(output->data, (uint8_t *) &texture_name,
                        sizeof(uint64_t));
        }

        void _forach_vec4fs_clb(yaml_node_t key,
                                yaml_node_t value,
                                void *_data) {
            struct material_compile_output *output = (material_compile_output *) _data;

            output->vec4f_count += 1;

            char uniform_name[32] = {};
            yaml_as_string(key, uniform_name,
                           CETECH_ARRAY_LEN(uniform_name) - 1);

            float v[4];
            yaml_as_vec4(value, v);

            array::push(output->uniform_names, uniform_name,
                        CETECH_ARRAY_LEN(uniform_name));

            array::push(output->data, reinterpret_cast<const uint8_t *>(v),
                        sizeof(float) * 4);
        }

        void _forach_mat44f_clb(yaml_node_t key,
                                yaml_node_t value,
                                void *_data) {
            struct material_compile_output *output = (material_compile_output *) _data;

            output->mat44f_count += 1;

            char uniform_name[32] = {};
            yaml_as_string(key, uniform_name,
                           CETECH_ARRAY_LEN(uniform_name) - 1);

            float m[16];
            yaml_as_mat44(value, m);

            array::push(output->uniform_names, uniform_name,
                        CETECH_ARRAY_LEN(uniform_name));

            array::push(output->data, (uint8_t *) m, sizeof(float) * 16);
        }

        void _forach_mat33f_clb(yaml_node_t key,
                                yaml_node_t value,
                                void *_data) {
            CEL_UNUSED(key, value, _data);

//            struct material_compile_output *output = (material_compile_output *) _data;
//
//            output->mat33f_count += 1;
//
//            char uniform_name[32] = {};
//            yaml_as_string(key, uniform_name,
//                           CETECH_ARRAY_LEN(uniform_name) - 1);
//
//            mat33f_t m = yaml_as_mat33f_t(value);
//
//            array::push(output->uniform_names, uniform_name,
//                        CETECH_ARRAY_LEN(uniform_name));
//            array::push(output->data, (uint8_t *) &m, sizeof(mat33f_t));
        }
    }

    int compiler(const char *filename,
                 ct_vio *source_vio,
                 ct_vio *build_vio,
                 ct_compilator_api *compilator_api) {

        char *source_data =
                CEL_ALLOCATE(ct_memory_a0.main_allocator(), char,
                                source_vio->size(source_vio->inst) + 1);
        memset(source_data, 0, source_vio->size(source_vio->inst) + 1);

        source_vio->read(source_vio->inst, source_data, sizeof(char),
                         source_vio->size(source_vio->inst));

        yaml_document_t h;
        yaml_node_t root = yaml_load_str(source_data, &h);

        _preprocess(filename, root, compilator_api);

        yaml_node_t shader_node = yaml_get_node(root, "shader");
        CETECH_ASSERT("material", yaml_is_valid(shader_node));

        char tmp_buffer[256] = {};
        yaml_as_string(shader_node, tmp_buffer, CETECH_ARRAY_LEN(tmp_buffer));

        struct material_compile_output output = {};
        output.uniform_names.init(ct_memory_a0.main_allocator());
        output.data.init(ct_memory_a0.main_allocator());

        yaml_node_t textures = yaml_get_node(root, "textures");
        if (yaml_is_valid(textures)) {
            yaml_node_foreach_dict(textures, _forach_texture_clb, &output);
        }

        yaml_node_t vec4 = yaml_get_node(root, "vec4f");
        if (yaml_is_valid(vec4)) {
            yaml_node_foreach_dict(vec4, _forach_vec4fs_clb, &output);
        }

        yaml_node_t mat44 = yaml_get_node(root, "mat44f");
        if (yaml_is_valid(mat44)) {
            yaml_node_foreach_dict(mat44, _forach_mat44f_clb, &output);
        }

        yaml_node_t mat33 = yaml_get_node(root, "mat33f");
        if (yaml_is_valid(mat33)) {
            yaml_node_foreach_dict(mat33, _forach_mat33f_clb, &output);
        }

        material_blob::blob_t resource = {
                .shader_name = ct_hash_a0.id64_from_str(tmp_buffer),
                .texture_count =output.texture_count,
                .vec4f_count = output.vec4f_count,
                .mat33f_count = output.mat33f_count,
                .mat44f_count = output.mat44f_count,
                .uniforms_count = (array::size(output.uniform_names) / 32),
        };

        build_vio->write(build_vio->inst, &resource, sizeof(resource), 1);

        build_vio->write(build_vio->inst, output.uniform_names._data,
                         sizeof(char),array::size(output.uniform_names));

        build_vio->write(build_vio->inst, output.data._data, sizeof(uint8_t),
                         array::size(output.data));

        output.uniform_names.destroy();
        output.data.destroy();

        CEL_FREE(ct_memory_a0.main_allocator(), source_data);
        return 1;
    }

    int init(ct_api_a0 *api) {
        CETECH_GET_API(api, ct_memory_a0);
        CETECH_GET_API(api, ct_resource_a0);
        CETECH_GET_API(api, ct_path_a0);
        CETECH_GET_API(api, ct_vio_a0);
        CETECH_GET_API(api, ct_hash_a0);

        ct_resource_a0.compiler_register(ct_hash_a0.id64_from_str("material"),
                                         compiler);

        return 1;
    }
}

#endif