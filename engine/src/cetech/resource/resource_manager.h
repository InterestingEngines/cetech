#pragma once

#include <cinttypes>

#include "cetech/filesystem/filesystem.h"
#include "cetech/task_manager/task_manager.h"

#include "celib/string/stringid_types.h"
#include "celib/memory/memory_types.h"

#include "rapidjson/document.h"

namespace cetech {
    class ResourceManager {
        public:
            typedef char* (* resource_loader_clb_t)(FSFile*, Allocator&);
            typedef void (* resource_unloader_clb_t)(Allocator&, void*);
            typedef void (* resource_online_clb_t)(void*);
            typedef void (* resource_offline_clb_t)(void*);

            virtual ~ResourceManager() {}

            virtual void register_loader(StringId64_t type, resource_loader_clb_t clb) = 0;
            virtual void register_unloader(StringId64_t type, resource_unloader_clb_t clb) = 0;
            virtual void register_online(StringId64_t type, resource_online_clb_t clb) = 0;
            virtual void register_offline(StringId64_t type, resource_offline_clb_t clb) = 0;

            virtual void load(char** loaded_data, StringId64_t type, const StringId64_t* names,
                              const uint32_t count) = 0;
            virtual void add_loaded(char** loaded_data,
                                    StringId64_t type,
                                    const StringId64_t* names,
                                    const uint32_t count) = 0;

            virtual void unload(StringId64_t type, const StringId64_t* names, const uint32_t count) = 0;

            virtual bool can_get(StringId64_t type, StringId64_t* names, const uint32_t count) = 0;
            virtual const char* get(StringId64_t type, StringId64_t name) = 0;

            static ResourceManager* make(Allocator& allocator, FileSystem* fs);
            static void destroy(Allocator& allocator, ResourceManager* rm);
    };
}