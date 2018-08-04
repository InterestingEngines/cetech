#ifndef CETECH_RESOURCE_INTERNAL_H
#define CETECH_RESOURCE_INTERNAL_H


#include "celib/hashlib.h"
#include <cetech/resource/resource.h>


void resource_compiler_register(const char *type,
                                ct_resource_compilator_t compilator,
                                bool yaml_based);

void resource_compiler_compile_all();

int resource_compiler_get_filename(char *filename,
                                   size_t max_ken,
                                   struct ct_resource_id resource_id);

const char *resource_compiler_get_source_dir();


char *resource_compiler_get_tmp_dir(struct ce_alloc *alocator,
                                    const char *platform);

char *resource_compiler_external_join(struct ce_alloc *alocator,
                                      const char *name);

void resource_compiler_create_build_dir(struct ce_config_a0 config);

const char *resource_compiler_get_core_dir();

void compile_and_reload(const char *filename);

void type_name_from_filename(const char *fullname,
                             struct ct_resource_id *resource_id,
                             char *short_name);

char *resource_compiler_get_build_dir(struct ce_alloc *a,
                                      const char *platform);

int package_init(struct ce_api_a0 *api);

void package_shutdown();

struct ce_task_counter_t *package_load(uint64_t name);

void package_unload(uint64_t name);

int package_is_loaded(uint64_t name);

void package_flush(struct ce_task_counter_t *counter);


#endif //CETECH_RESOURCE_INTERNAL_H
