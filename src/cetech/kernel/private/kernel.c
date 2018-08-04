#include "corelib/allocator.h"

#include "corelib/api_system.h"
#include <corelib/os.h>
#include "cetech/kernel/kernel.h"
#include "corelib/memory.h"
#include "corelib/config.h"
#include "corelib/module.h"
#include "corelib/hashlib.h"
#include "corelib/private/api_private.h"

#include <corelib/ebus.h>

#include <corelib/core.h>
#include <corelib/buffer.inl>
#include <corelib/task.h>
#include <cetech/resource/resource.h>
#include <cetech/resource/package.h>
#include <cetech/machine/machine.h>
#include <cetech/gfx/debugui.h>
#include <cetech/gfx/renderer.h>


#include <cetech/static_module.h>
#include <corelib/log.h>
#include <cetech/game_system/game_system.h>
#include <corelib/fs.h>


static struct KernelGlobals {
    uint64_t config_object;
    int is_running;
    struct ct_alloc *allocator;
} _G;

void register_api(struct ct_api_a0 *api);

const char *_platform() {
#if CT_PLATFORM_LINUX
    return "linux";
#elif defined(CETECH_WINDOWS)
    return "windows";
#elif CT_PLATFORM_OSX
    return "darwin";
#endif
    return NULL;
}

#define LOG_WHERE "kernel"

int init_config(int argc,
                const char **argv,
                uint64_t object) {

    ct_cdb_obj_o *writer = ct_cdb_a0->write_begin(object);
    ct_cdb_a0->set_str(writer, CONFIG_PLATFORM, _platform());
    ct_cdb_a0->set_str(writer, CONFIG_NATIVE_PLATFORM, _platform());
    ct_cdb_a0->set_str(writer, CONFIG_BUILD, "build");
    ct_cdb_a0->write_commit(writer);

    if (!ct_config_a0->parse_args(argc, argv)) {
        return 0;
    }

    const char *build_dir_str = ct_cdb_a0->read_str(object, CONFIG_BUILD, "");
    char *build_dir = NULL;
    ct_os_a0->path->join(&build_dir, _G.allocator, 2,
                         build_dir_str,
                         ct_cdb_a0->read_str(object, CONFIG_NATIVE_PLATFORM,
                                             ""));

    char *build_config = NULL;
    ct_os_a0->path->join(&build_config, _G.allocator, 2, build_dir,
                         "global.config");

    const char *source_dir_str = ct_cdb_a0->read_str(object, CONFIG_SRC, "");
    char *source_config = NULL;
    ct_os_a0->path->join(&source_config, _G.allocator, 2, source_dir_str,
                         "global.config");

    if (ct_cdb_a0->read_uint64(object, CONFIG_COMPILE, 0)) {
        ct_os_a0->path->make_path(build_dir);
        ct_os_a0->path->copy_file(_G.allocator, source_config, build_config);
    }

    ct_config_a0->load_from_yaml_file(build_config, _G.allocator);

    ct_buffer_free(source_config, _G.allocator);
    ct_buffer_free(build_config, _G.allocator);
    ct_buffer_free(build_dir, _G.allocator);

    if (!ct_config_a0->parse_args(argc, argv)) {
        return 0;
    }

    return 1;
}

int cetech_kernel_init(int argc,
                       const char **argv) {
    ct_log_a0->register_handler(ct_log_a0->stdout_yaml_handler, NULL);

    ct_corelib_init();

    struct ct_api_a0 *api = ct_api_a0;

    _G = (struct KernelGlobals) {
            .allocator = ct_memory_a0->system,
            .config_object  = ct_config_a0->obj(),
    };

    init_config(argc, argv, ct_config_a0->obj());

    CETECH_ADD_STATIC_MODULE(builddb);
    CETECH_ADD_STATIC_MODULE(resourcesystem);
    CETECH_ADD_STATIC_MODULE(resourcecompiler);

    CT_INIT_API(api, ct_ebus_a0);
    ct_ebus_a0->create_ebus("kernel", KERNEL_EBUS);

    init_static_modules();

    uint64_t root = ct_hashlib_a0->id64("modules");
    const char *module_path = ct_cdb_a0->read_str(ct_config_a0->obj(),
                                                  CONFIG_MODULE_DIR,
                                                  "bin/darwin64");

    ct_fs_a0->map_root_dir(root, module_path, true);

    ct_module_a0->load_dirs(module_path);
    ct_config_a0->log_all();


    CT_INIT_API(api, ct_resource_a0);
    CT_INIT_API(api, ct_os_a0);
    CT_INIT_API(api, ct_package_a0);
    CT_INIT_API(api, ct_machine_a0);
    CT_INIT_API(api, ct_debugui_a0);
    CT_INIT_API(api, ct_renderer_a0);

#if defined(CETECH_DEVELOP)
    ct_resource_a0->set_autoload(true);
#else
    ct_resource_a0->set_autoload(0);
#endif

    return 1;
}


int cetech_kernel_shutdown() {
    ct_log_a0->debug(LOG_WHERE, "Shutdown");
    ct_module_a0->unload_all();
    ct_corelib_shutdown();

    return 1;
}

void application_quit() {
    _G.is_running = 0;
}


void _init_config() {
    _G.config_object = ct_config_a0->obj();

    ct_cdb_obj_o *writer = ct_cdb_a0->write_begin(_G.config_object);

    if (!ct_cdb_a0->prop_exist(_G.config_object, CONFIG_BOOT_PKG)) {
        ct_cdb_a0->set_str(writer, CONFIG_BOOT_PKG, "boot");
    }

    if (!ct_cdb_a0->prop_exist(_G.config_object, CONFIG_GAME)) {
        ct_cdb_a0->set_str(writer, CONFIG_GAME, "editor");
    }

    if (!ct_cdb_a0->prop_exist(_G.config_object, CONFIG_DAEMON)) {
        ct_cdb_a0->set_uint64(writer, CONFIG_DAEMON, 0);
    }

    if (!ct_cdb_a0->prop_exist(_G.config_object, CONFIG_COMPILE)) {
        ct_cdb_a0->set_uint64(writer, CONFIG_COMPILE, 0);
    }

    if (!ct_cdb_a0->prop_exist(_G.config_object, CONFIG_CONTINUE)) {
        ct_cdb_a0->set_uint64(writer, CONFIG_CONTINUE, 0);
    }

    if (!ct_cdb_a0->prop_exist(_G.config_object, CONFIG_WAIT)) {
        ct_cdb_a0->set_uint64(writer, CONFIG_WAIT, 0);
    }

    ct_cdb_a0->write_commit(writer);
}

static void _boot_stage() {
    const char *boot_pkg_str = ct_cdb_a0->read_str(_G.config_object,
                                                   CONFIG_BOOT_PKG, "");
    uint64_t boot_pkg = ct_hashlib_a0->id64(boot_pkg_str);


    uint64_t core_pkg = ct_hashlib_a0->id64("core/core");
    uint64_t resources[] = {core_pkg, boot_pkg};

    ct_resource_a0->load_now(PACKAGE_TYPE, resources, 2);

    struct ct_task_counter_t *boot_pkg_cnt = ct_package_a0->load(boot_pkg);
    ct_package_a0->flush(boot_pkg_cnt);

    struct ct_task_counter_t *core_pkg_cnt = ct_package_a0->load(core_pkg);
    ct_package_a0->flush(core_pkg_cnt);
}

static void _boot_unload() {
    const char *boot_pkg_str = ct_cdb_a0->read_str(_G.config_object,
                                                   CONFIG_BOOT_PKG, "");
    uint64_t boot_pkg = ct_hashlib_a0->id64(boot_pkg_str);

    uint64_t core_pkg = ct_hashlib_a0->id64("core/core");

    uint64_t resources[] = {core_pkg, boot_pkg};

    ct_package_a0->unload(boot_pkg);
    ct_package_a0->unload(core_pkg);

    ct_resource_a0->unload(PACKAGE_TYPE, resources, 2);
}


static void on_quit(uint64_t event) {
    CT_UNUSED(event)

    application_quit();
}

static void cetech_kernel_start() {

    _init_config();

    if (ct_cdb_a0->read_uint64(_G.config_object, CONFIG_COMPILE, 0)) {
        ct_resource_a0->compiler_compile_all();

        if (!ct_cdb_a0->read_uint64(_G.config_object, CONFIG_CONTINUE, 0)) {
            return;
        }
    }

    _boot_stage();

    ct_ebus_a0->connect(KERNEL_EBUS, KERNEL_QUIT_EVENT, on_quit, 0);

    uint64_t event = ct_cdb_a0->create_object(ct_cdb_a0->db(),
                                              KERNEL_INIT_EVENT);

    ct_ebus_a0->broadcast(KERNEL_EBUS, event);

    _G.is_running = 1;

    const uint64_t fq = ct_os_a0->time->perf_freq();
    uint64_t last_tick = ct_os_a0->time->perf_counter();

    while (_G.is_running) {
        uint64_t now_ticks = ct_os_a0->time->perf_counter();
        float dt = ((float) (now_ticks - last_tick)) / fq;
        last_tick = now_ticks;

        ct_ebus_a0->begin_frame();

        uint64_t event;
        event = ct_cdb_a0->create_object(ct_cdb_a0->db(), KERNEL_UPDATE_EVENT);
        ct_cdb_obj_o *w = ct_cdb_a0->write_begin(event);
        ct_cdb_a0->set_float(w, KERNEL_EVENT_DT, dt);
        ct_cdb_a0->write_commit(w);

        ct_ebus_a0->broadcast(KERNEL_EBUS, event);

        ct_cdb_a0->gc();
    }

    event = ct_cdb_a0->create_object(ct_cdb_a0->db(), KERNEL_SHUTDOWN_EVENT);
    ct_ebus_a0->broadcast(KERNEL_EBUS, event);

    ct_ebus_a0->disconnect(KERNEL_EBUS, KERNEL_QUIT_EVENT, on_quit);

    _boot_unload();
}

int main(int argc,
         const char **argv) {

    if (cetech_kernel_init(argc, argv)) {
        cetech_kernel_start();
    }

    return cetech_kernel_shutdown();
}
