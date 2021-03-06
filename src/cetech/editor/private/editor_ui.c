#include <float.h>
#include <stdio.h>
#include <time.h>

#include <celib/hashlib.h>
#include <celib/config.h>
#include <celib/memory.h>
#include <celib/api_system.h>
#include <celib/ydb.h>
#include <celib/array.inl>
#include <celib/module.h>

#include <cetech/editor/command_system.h>
#include <celib/hash.inl>
#include <celib/cdb.h>
#include <cetech/gfx/debugui.h>
#include <celib/fmath.inl>
#include <cetech/editor/asset_browser.h>
#include <cetech/resource/builddb.h>
#include <celib/os.h>
#include <cetech/editor/property_editor.h>
#include <cetech/resource/resource.h>
#include <cetech/resource/sourcedb.h>
#include <cetech/gfx/private/iconfontheaders/icons_font_awesome.h>

#include "cetech/editor/editor_ui.h"

#define _SET_BOOL \
    CE_ID64_0("set_bool", 0xc7b18e7df0217558ULL)

#define _SET_STR \
    CE_ID64_0("set_str", 0x5096c6f990f09debULL)

#define _SET_UINT64 \
    CE_ID64_0("set_uint64", 0xdf40f6493b54f476ULL)

#define _SET_VEC3 \
    CE_ID64_0("set_vec3", 0xc9710c4624eccfb0ULL)

#define _SET_VEC4 \
    CE_ID64_0("set_vec4", 0x5d3fca6c7467c890ULL)

#define _SET_FLOAT \
    CE_ID64_0("set_float", 0x3a22b9e23704ab12ULL)

#define _NEW_VALUE \
    CE_ID64_0("new_value", 0x1d77a29c912111fULL)

#define _OLD_VALUE \
    CE_ID64_0("old_value", 0x8115d649f2a9636aULL)

#define _KEYS \
    CE_ID64_0("keys", 0xa62f9297dc969e85ULL)

#define _PROP \
    CE_ID64_0("property", 0xcbd168fb77919b23ULL)


static void _collect_keys(struct ct_resource_id rid,
                          uint64_t obj,
                          ce_cdb_obj_o *w,
                          uint32_t idx) {
    if (ce_cdb_a0->read_uint64(obj, ASSET_NAME, 0) == rid.name) {
        return;
    }

    uint64_t k = ce_cdb_a0->key(obj);
    ce_cdb_a0->set_uint64(w, idx, k);

    uint64_t parent = ce_cdb_a0->parent(obj);
    _collect_keys(rid, parent, w, idx + 1);
}

static uint64_t _find_recursive(uint64_t obj,
                                uint64_t keys) {
    uint64_t n = ce_cdb_a0->prop_count(keys);

    uint64_t it_obj = obj;
    for (int i = 0; i < n; ++i) {
        uint64_t k = ce_cdb_a0->read_uint64(keys, n - 1 - i, 0);
        it_obj = ce_cdb_a0->read_subobject(it_obj, k, 0);
    }

    return it_obj;
}


static uint64_t _create_recursive(uint64_t obj,
                                  uint64_t keys,
                                  uint64_t kidx) {
    uint64_t k = ce_cdb_a0->read_uint64(keys, kidx, 0);
    if (!k) {
        return obj;
    }

    uint64_t root = ce_cdb_a0->read_subobject(obj, k, 0);

    uint64_t new_obj = ce_cdb_a0->create_from(ce_cdb_a0->db(), root);
    ce_cdb_obj_o *w = ce_cdb_a0->write_begin(obj);
    ce_cdb_a0->set_subobject(w, k, new_obj);
    ce_cdb_a0->write_commit(w);

    return _create_recursive(new_obj, keys, kidx - 1);
}

static uint64_t _find_recursive_create(uint64_t obj,
                                       uint64_t keys) {
    uint64_t n = ce_cdb_a0->prop_count(keys);

    uint64_t it_obj = obj;
    for (int i = 0; i < n; ++i) {
        uint64_t kidx = n - 1 - i;
        uint64_t k = ce_cdb_a0->read_uint64(keys, kidx, 0);

        if (!ce_cdb_a0->prop_exist_norecursive(it_obj, k)) {
            it_obj = _create_recursive(it_obj, keys, kidx);
            break;
        } else {
            it_obj = ce_cdb_a0->read_subobject(it_obj, k, 0);
        }
    }

    return it_obj;
}


static void _prop_label(const char *label,
                        uint64_t _obj,
                        uint64_t prop_key_hash) {
    if (ce_cdb_a0->prefab(_obj)
        && ce_cdb_a0->prop_exist_norecursive(_obj, prop_key_hash)) {

        char lbl[256] = {};
        snprintf(lbl, CE_ARRAY_LEN(lbl), "%s##revert_%llu_%llu",
                 ICON_FA_RECYCLE, _obj, prop_key_hash);

        bool remove_change = ct_debugui_a0->Button(lbl,
                                                   (float[2]) {});
        if (remove_change) {
            ce_cdb_obj_o *w = ce_cdb_a0->write_begin(_obj);
            ce_cdb_a0->delete_property(w, prop_key_hash);
            ce_cdb_a0->write_commit(w);
        }
        ct_debugui_a0->SameLine(0, 4);
    }

    ct_debugui_a0->Text("%s", label);
    ct_debugui_a0->NextColumn();
}

static void ui_float(struct ct_resource_id rid,
                     uint64_t obj,
                     uint64_t prop_key_hash,
                     const char *label,
                     float min_f,
                     float max_f) {
    float value = 0;
    float value_new = 0;

    uint64_t keys = ce_cdb_a0->create_object(ce_cdb_a0->db(), 0);
    ce_cdb_obj_o *w = ce_cdb_a0->write_begin(keys);
    _collect_keys(rid, obj, w, 0);
    ce_cdb_a0->write_commit(w);

    obj = _find_recursive(ct_sourcedb_a0->get(rid), keys);

    if (!obj) {
        return;
    }

    value_new = ce_cdb_a0->read_float(obj, prop_key_hash, value_new);
    value = value_new;

    const float min = !max_f ? -FLT_MAX : min_f;
    const float max = !max_f ? FLT_MAX : max_f;

    _prop_label(label, obj, prop_key_hash);

    char labelid[128] = {'\0'};
    sprintf(labelid, "##%sprop_float_%d", label, 0);

    if (ct_debugui_a0->DragFloat(labelid,
                                 &value_new, 1.0f,
                                 min, max,
                                 "%.3f", 1.0f)) {

        uint64_t cmd_obj = ce_cdb_a0->create_object(ce_cdb_a0->db(), 0);
        ce_cdb_obj_o *w = ce_cdb_a0->write_begin(cmd_obj);
        ce_cdb_a0->set_uint64(w, ASSET_NAME, rid.name);
        ce_cdb_a0->set_uint64(w, ASSET_TYPE, rid.type);
        ce_cdb_a0->set_uint64(w, _PROP, prop_key_hash);
        ce_cdb_a0->set_subobject(w, _KEYS, keys);
        ce_cdb_a0->set_float(w, _NEW_VALUE, value_new);
        ce_cdb_a0->set_float(w, _OLD_VALUE, value);
        ce_cdb_a0->write_commit(w);

        struct ct_cdb_cmd_s cmd = {
                .header = {
                        .size = sizeof(struct ct_cdb_cmd_s),
                        .type = _SET_FLOAT,
                },
                .cmd=cmd_obj,
        };

        ct_cmd_system_a0->execute(&cmd.header);
    }

    ct_debugui_a0->NextColumn();
}

static void ui_bool(struct ct_resource_id rid,
                    uint64_t obj,
                    uint64_t prop_key_hash,
                    const char *label) {
    bool value = false;
    bool value_new = false;

    uint64_t keys = ce_cdb_a0->create_object(ce_cdb_a0->db(), 0);
    ce_cdb_obj_o *w = ce_cdb_a0->write_begin(keys);
    _collect_keys(rid, obj, w, 0);
    ce_cdb_a0->write_commit(w);

    obj = _find_recursive(ct_sourcedb_a0->get(rid), keys);

    value_new = ce_cdb_a0->read_bool(obj, prop_key_hash, value_new);
    value = value_new;

    _prop_label(label, obj, prop_key_hash);

    char labelid[128] = {'\0'};
    sprintf(labelid, "##%sprop_float_%d", label, 0);

    if (ct_debugui_a0->Checkbox(labelid, &value_new)) {

        uint64_t cmd_obj = ce_cdb_a0->create_object(ce_cdb_a0->db(), 0);
        ce_cdb_obj_o *w = ce_cdb_a0->write_begin(cmd_obj);
        ce_cdb_a0->set_uint64(w, ASSET_NAME, rid.name);
        ce_cdb_a0->set_uint64(w, ASSET_TYPE, rid.type);
        ce_cdb_a0->set_uint64(w, _PROP, prop_key_hash);
        ce_cdb_a0->set_subobject(w, _KEYS, keys);
        ce_cdb_a0->set_bool(w, _NEW_VALUE, value_new);
        ce_cdb_a0->set_bool(w, _OLD_VALUE, value);
        ce_cdb_a0->write_commit(w);

        struct ct_cdb_cmd_s cmd = {
                .header = {
                        .size = sizeof(struct ct_cdb_cmd_s),
                        .type = _SET_BOOL,
                },
                .cmd= cmd_obj,
        };


        ct_cmd_system_a0->execute(&cmd.header);
    }

    ct_debugui_a0->NextColumn();
}

static void ui_str(struct ct_resource_id rid,
                   uint64_t obj,
                   uint64_t prop_key_hash,
                   const char *label,
                   uint32_t i) {
    char labelid[128] = {'\0'};

    const char *value = 0;

    uint64_t keys = ce_cdb_a0->create_object(ce_cdb_a0->db(), 0);
    ce_cdb_obj_o *w = ce_cdb_a0->write_begin(keys);
    _collect_keys(rid, obj, w, 0);
    ce_cdb_a0->write_commit(w);

    obj = _find_recursive(ct_sourcedb_a0->get(rid), keys);

    value = ce_cdb_a0->read_str(obj, prop_key_hash, "");

    char buffer[128] = {'\0'};
    strcpy(buffer, value);

    sprintf(labelid, "##%sprop_str_%d", label, i);

    _prop_label(label, obj, prop_key_hash);

    bool change = false;

    ct_debugui_a0->PushItemWidth(-1);
    change |= ct_debugui_a0->InputText(labelid,
                                       buffer,
                                       CE_ARRAY_LEN(buffer),
                                       0,
                                       0, NULL);
    ct_debugui_a0->PopItemWidth();

    if (change) {
        uint64_t cmd_obj = ce_cdb_a0->create_object(ce_cdb_a0->db(), 0);
        ce_cdb_obj_o *w = ce_cdb_a0->write_begin(cmd_obj);
        ce_cdb_a0->set_uint64(w, ASSET_NAME, rid.name);
        ce_cdb_a0->set_uint64(w, ASSET_TYPE, rid.type);
        ce_cdb_a0->set_uint64(w, _PROP, prop_key_hash);
        ce_cdb_a0->set_subobject(w, _KEYS, keys);
        ce_cdb_a0->set_str(w, _NEW_VALUE, buffer);
        ce_cdb_a0->set_str(w, _OLD_VALUE, value);
        ce_cdb_a0->write_commit(w);

        struct ct_cdb_cmd_s cmd = {
                .header = {
                        .size = sizeof(struct ct_cdb_cmd_s),
                        .type = _SET_STR,
                },
                .cmd= cmd_obj,
        };

        ct_cmd_system_a0->execute(&cmd.header);
    }

    ct_debugui_a0->NextColumn();
}

static void ui_str_combo(struct ct_resource_id rid,
                         uint64_t obj,
                         uint64_t prop_key_hash,
                         const char *label,
                         void (*combo_items)(uint64_t obj,
                                             char **items,
                                             uint32_t *items_count),
                         uint32_t i) {

    const char *value = 0;

    uint64_t keys = ce_cdb_a0->create_object(ce_cdb_a0->db(), 0);
    ce_cdb_obj_o *w = ce_cdb_a0->write_begin(keys);
    _collect_keys(rid, obj, w, 0);
    ce_cdb_a0->write_commit(w);

    obj = _find_recursive(ct_sourcedb_a0->get(rid), keys);

    if (!obj) {
        return;
    }

    value = ce_cdb_a0->read_str(obj, prop_key_hash, NULL);

    char *items = NULL;
    uint32_t items_count = 0;

    combo_items(obj, &items, &items_count);

    int current_item = -1;

    const char *items2[items_count];
    memset(items2, 0, sizeof(const char *) * items_count);

    for (int j = 0; j < items_count; ++j) {
        items2[j] = &items[j * 128];
        if (value) {
            if (ce_id_a0->id64(items2[j]) == ce_id_a0->id64(value)) {
                current_item = j;
            }
        }
    }

    _prop_label(label, obj, prop_key_hash);

    char labelid[128] = {'\0'};

    char buffer[128] = {'\0'};

    if (value) {
        strcpy(buffer, value);
    }

    sprintf(labelid, "##%scombo_%d", label, i);
    ct_debugui_a0->PushItemWidth(-1);
    bool change = ct_debugui_a0->Combo(labelid,
                                       &current_item, items2,
                                       items_count, -1);
    ct_debugui_a0->PopItemWidth();

    if (change) {
        strcpy(buffer, items2[current_item]);
    }

    if (change) {
        uint64_t cmd_obj = ce_cdb_a0->create_object(ce_cdb_a0->db(), 0);
        ce_cdb_obj_o *w = ce_cdb_a0->write_begin(cmd_obj);
        ce_cdb_a0->set_uint64(w, ASSET_NAME, rid.name);
        ce_cdb_a0->set_uint64(w, ASSET_TYPE, rid.type);
        ce_cdb_a0->set_uint64(w, _PROP, prop_key_hash);
        ce_cdb_a0->set_subobject(w, _KEYS, keys);
        ce_cdb_a0->set_str(w, _NEW_VALUE, buffer);

        if (value) {
            ce_cdb_a0->set_str(w, _OLD_VALUE, value);
        }

        ce_cdb_a0->write_commit(w);

        struct ct_cdb_cmd_s cmd = {
                .header = {
                        .size = sizeof(struct ct_cdb_cmd_s),
                        .type = _SET_STR,
                },
                .cmd= cmd_obj,
        };

        ct_cmd_system_a0->execute(&cmd.header);
    }
    ct_debugui_a0->NextColumn();
}

static void ui_resource(struct ct_resource_id rid,
                        uint64_t obj,
                        uint64_t prop_key_hash,
                        const char *label,
                        uint64_t resource_type,
                        uint32_t i) {

    uint64_t keys = ce_cdb_a0->create_object(ce_cdb_a0->db(), 0);
    ce_cdb_obj_o *w = ce_cdb_a0->write_begin(keys);
    _collect_keys(rid, obj, w, 0);
    ce_cdb_a0->write_commit(w);

    obj = _find_recursive(ct_sourcedb_a0->get(rid), keys);

    if (!obj) {
        return;
    }

    const char *value = ce_cdb_a0->read_str(obj, prop_key_hash, 0);
    uint64_t value_id = ce_id_a0->id64(value);

    uint64_t resource_obj = ct_sourcedb_a0->get((struct ct_resource_id) {
            .type = resource_type,
            .name = value_id,
    });

    char buffer[128] = {'\0'};
    sprintf(buffer, "%s", value);

    char labelid[128] = {'\0'};
    sprintf(labelid, "##%sprop_str_%d", label, i);


    bool change = false;

    ct_debugui_a0->Separator();
    bool resource_open = ct_debugui_a0->TreeNodeEx(label,
                                                   0);

    ct_debugui_a0->NextColumn();
    ct_debugui_a0->PushItemWidth(-1);

    change |= ct_debugui_a0->InputText(labelid,
                                       buffer,
                                       strlen(buffer),
                                       DebugInputTextFlags_ReadOnly,
                                       0, NULL);
    ct_debugui_a0->PopItemWidth();
    ct_debugui_a0->NextColumn();

    uint64_t new_value = 0;
    if (ct_debugui_a0->BeginDragDropTarget()) {
        const struct DebugUIPayload *payload;
        payload = ct_debugui_a0->AcceptDragDropPayload("asset", 0);

        if (payload) {
            uint64_t drag_obj = *((uint64_t *) payload->Data);

            if (drag_obj) {
                uint64_t asset_type = ce_cdb_a0->read_uint64(drag_obj,
                                                             ASSET_TYPE,
                                                             0);

                uint64_t asset_name = ce_cdb_a0->read_uint64(drag_obj,
                                                             ASSET_NAME,
                                                             0);

                if (resource_type == asset_type) {
                    new_value = asset_name;
                    change = true;
                }
            }
        }

        ct_debugui_a0->EndDragDropTarget();
    }

    if (resource_open) {
        ct_property_editor_a0->draw((struct ct_resource_id) {
                .type = resource_type,
                .name = value_id,
        }, resource_obj);
        ct_debugui_a0->TreePop();
    }

    if (change) {
        const char *new_value_str = ce_id_a0->str_from_id64(new_value);

        uint64_t cmd_obj = ce_cdb_a0->create_object(ce_cdb_a0->db(), 0);
        ce_cdb_obj_o *w = ce_cdb_a0->write_begin(cmd_obj);
        ce_cdb_a0->set_uint64(w, ASSET_NAME, rid.name);
        ce_cdb_a0->set_uint64(w, ASSET_TYPE, rid.type);
        ce_cdb_a0->set_uint64(w, _PROP, prop_key_hash);
        ce_cdb_a0->set_subobject(w, _KEYS, keys);
        ce_cdb_a0->set_str(w, _NEW_VALUE, new_value_str);

        if (value) {
            ce_cdb_a0->set_str(w, _OLD_VALUE, value);
        }
        ce_cdb_a0->write_commit(w);

        struct ct_cdb_cmd_s cmd = {
                .header = {
                        .size = sizeof(struct ct_cdb_cmd_s),
                        .type = _SET_STR,
                },
                .cmd= cmd_obj,
        };

        ct_cmd_system_a0->execute(&cmd.header);
    }
}

static void ui_vec3(struct ct_resource_id rid,
                    uint64_t _obj,
                    uint64_t prop_key_hash,
                    const char *label,
                    float min_f,
                    float max_f) {
    float value[3] = {};
    float value_new[3] = {};

    uint64_t keys = ce_cdb_a0->create_object(ce_cdb_a0->db(), 0);
    ce_cdb_obj_o *w = ce_cdb_a0->write_begin(keys);
    _collect_keys(rid, _obj, w, 0);
    ce_cdb_a0->write_commit(w);

    uint64_t obj = _find_recursive(ct_sourcedb_a0->get(rid), keys);

    if (!obj) {
        return;
    }

    ce_cdb_a0->read_vec3(obj, prop_key_hash, value_new);
    ce_vec3_move(value, value_new);

    const float min = !min_f ? -FLT_MAX : min_f;
    const float max = !max_f ? FLT_MAX : max_f;

    _prop_label(label, obj, prop_key_hash);

    char labelid[128] = {'\0'};
    sprintf(labelid, "##%sprop_vec3_%d", label, 0);

    ct_debugui_a0->PushItemWidth(-1);
    if (ct_debugui_a0->DragFloat3(labelid,
                                  value_new, 1.0f,
                                  min, max,
                                  "%.3f", 1.0f)) {

        uint64_t cmd_obj = ce_cdb_a0->create_object(ce_cdb_a0->db(), 0);
        ce_cdb_obj_o *w = ce_cdb_a0->write_begin(cmd_obj);
        ce_cdb_a0->set_uint64(w, ASSET_NAME, rid.name);
        ce_cdb_a0->set_uint64(w, ASSET_TYPE, rid.type);
        ce_cdb_a0->set_uint64(w, _PROP, prop_key_hash);
        ce_cdb_a0->set_subobject(w, _KEYS, keys);
        ce_cdb_a0->set_vec3(w, _NEW_VALUE, value_new);
        ce_cdb_a0->set_vec3(w, _OLD_VALUE, value);
        ce_cdb_a0->write_commit(w);

        struct ct_cdb_cmd_s cmd = {
                .header = {
                        .size = sizeof(struct ct_cdb_cmd_s),
                        .type = _SET_VEC3,
                },
                .cmd= cmd_obj,
        };

        ct_cmd_system_a0->execute(&cmd.header);
    }

    ct_debugui_a0->PopItemWidth();


    ct_debugui_a0->NextColumn();

}

static void ui_vec4(struct ct_resource_id rid,
                    uint64_t obj,
                    uint64_t prop_key_hash,
                    const char *label,
                    float min_f,
                    float max_f) {
    float value[4] = {};
    float value_new[4] = {};

    uint64_t keys = ce_cdb_a0->create_object(ce_cdb_a0->db(), 0);
    ce_cdb_obj_o *w = ce_cdb_a0->write_begin(keys);
    _collect_keys(rid, obj, w, 0);
    ce_cdb_a0->write_commit(w);

    obj = _find_recursive(ct_sourcedb_a0->get(rid), keys);

    ce_cdb_a0->read_vec4(obj, prop_key_hash, value_new);
    ce_vec4_move(value, value_new);

    const float min = !min_f ? -FLT_MAX : min_f;
    const float max = !max_f ? FLT_MAX : max_f;

    _prop_label(label, obj, prop_key_hash);

    char labelid[128] = {'\0'};
    sprintf(labelid, "##%sprop_vec3_%d", label, 0);

    ct_debugui_a0->PushItemWidth(-1);
    if (ct_debugui_a0->DragFloat4(labelid,
                                  value_new, 1.0f,
                                  min, max,
                                  "%.3f", 1.0f)) {

        uint64_t cmd_obj = ce_cdb_a0->create_object(ce_cdb_a0->db(), 0);
        ce_cdb_obj_o *w = ce_cdb_a0->write_begin(cmd_obj);
        ce_cdb_a0->set_uint64(w, ASSET_NAME, rid.name);
        ce_cdb_a0->set_uint64(w, ASSET_TYPE, rid.type);
        ce_cdb_a0->set_uint64(w, _PROP, prop_key_hash);
        ce_cdb_a0->set_subobject(w, _KEYS, keys);
        ce_cdb_a0->set_vec4(w, _NEW_VALUE, value_new);
        ce_cdb_a0->set_vec4(w, _OLD_VALUE, value);
        ce_cdb_a0->write_commit(w);

        struct ct_cdb_cmd_s cmd = {
                .header = {
                        .size = sizeof(struct ct_cdb_cmd_s),
                        .type = _SET_VEC4,
                },
                .cmd= cmd_obj,
        };

        ct_cmd_system_a0->execute(&cmd.header);
    }
    ct_debugui_a0->PopItemWidth();

    ct_debugui_a0->NextColumn();
}

static void ui_color(struct ct_resource_id rid,
                     uint64_t obj,
                     uint64_t prop_key_hash,
                     const char *label,
                     float min_f,
                     float max_f) {
    float value[4] = {};
    float value_new[4] = {};

    uint64_t keys = ce_cdb_a0->create_object(ce_cdb_a0->db(), 0);
    ce_cdb_obj_o *w = ce_cdb_a0->write_begin(keys);
    _collect_keys(rid, obj, w, 0);
    ce_cdb_a0->write_commit(w);

    obj = _find_recursive(ct_sourcedb_a0->get(rid), keys);

    ce_cdb_a0->read_vec4(obj, prop_key_hash, value_new);
    ce_vec4_move(value, value_new);

    _prop_label(label, obj, prop_key_hash);

    char labelid[128] = {'\0'};
    sprintf(labelid, "##%sprop_vec3_%d", label, 0);


    ct_debugui_a0->PushItemWidth(-1);
    if (ct_debugui_a0->ColorEdit4(labelid,
                                  value_new, 1)) {

        uint64_t cmd_obj = ce_cdb_a0->create_object(ce_cdb_a0->db(), 0);
        ce_cdb_obj_o *w = ce_cdb_a0->write_begin(cmd_obj);
        ce_cdb_a0->set_uint64(w, ASSET_NAME, rid.name);
        ce_cdb_a0->set_uint64(w, ASSET_TYPE, rid.type);
        ce_cdb_a0->set_uint64(w, _PROP, prop_key_hash);
        ce_cdb_a0->set_subobject(w, _KEYS, keys);
        ce_cdb_a0->set_vec4(w, _NEW_VALUE, value_new);
        ce_cdb_a0->set_vec4(w, _OLD_VALUE, value);
        ce_cdb_a0->write_commit(w);

        struct ct_cdb_cmd_s cmd = {
                .header = {
                        .size = sizeof(struct ct_cdb_cmd_s),
                        .type = _SET_VEC4,
                },
                .cmd= cmd_obj,
        };

        ct_cmd_system_a0->execute(&cmd.header);
    }
    ct_debugui_a0->PopItemWidth();

    ct_debugui_a0->NextColumn();
}

static struct ct_editor_ui_a0 editor_ui_a0 = {
        .ui_float = ui_float,
        .ui_str = ui_str,
        .ui_str_combo = ui_str_combo,
        .ui_resource = ui_resource,
        .ui_vec3 = ui_vec3,
        .ui_vec4 = ui_vec4,
        .ui_color= ui_color,
        .ui_bool = ui_bool,
};

struct ct_editor_ui_a0 *ct_editor_ui_a0 = &editor_ui_a0;


static void set_vec3_cmd(const struct ct_cmd *cmd,
                         bool inverse) {
    const struct ct_cdb_cmd_s *pos_cmd = (const struct ct_cdb_cmd_s *) cmd;

    struct ct_resource_id rid = {
            .name=ce_cdb_a0->read_uint64(pos_cmd->cmd, ASSET_NAME, 0),
            .type=ce_cdb_a0->read_uint64(pos_cmd->cmd, ASSET_TYPE, 0),
    };


    uint64_t asset_obj = ct_sourcedb_a0->get(rid);
    uint64_t keys = ce_cdb_a0->read_subobject(pos_cmd->cmd, _KEYS, 0);
    uint64_t prop = ce_cdb_a0->read_uint64(pos_cmd->cmd, _PROP, 0);

    uint64_t obj = _find_recursive_create(asset_obj, keys);

    float v[3] = {};
    ce_cdb_a0->read_vec3(pos_cmd->cmd, inverse ? _OLD_VALUE : _NEW_VALUE, v);

    ce_cdb_obj_o *w = ce_cdb_a0->write_begin(obj);
    ce_cdb_a0->set_vec3(w, prop, v);
    ce_cdb_a0->write_commit(w);
}

static void set_vec4_cmd(const struct ct_cmd *cmd,
                         bool inverse) {
    const struct ct_cdb_cmd_s *pos_cmd = (const struct ct_cdb_cmd_s *) cmd;

    struct ct_resource_id rid = {
            .name=ce_cdb_a0->read_uint64(pos_cmd->cmd, ASSET_NAME, 0),
            .type=ce_cdb_a0->read_uint64(pos_cmd->cmd, ASSET_TYPE, 0),
    };

    uint64_t asset_obj = ct_sourcedb_a0->get(rid);
    uint64_t keys = ce_cdb_a0->read_subobject(pos_cmd->cmd, _KEYS, 0);
    uint64_t prop = ce_cdb_a0->read_uint64(pos_cmd->cmd, _PROP, 0);

    uint64_t obj = _find_recursive_create(asset_obj, keys);

    float v[4] = {};
    ce_cdb_a0->read_vec4(pos_cmd->cmd, inverse ? _OLD_VALUE : _NEW_VALUE, v);

    ce_cdb_obj_o *w = ce_cdb_a0->write_begin(obj);
    ce_cdb_a0->set_vec4(w, prop, v);
    ce_cdb_a0->write_commit(w);
}

static void set_float_cmd(const struct ct_cmd *cmd,
                          bool inverse) {
    const struct ct_cdb_cmd_s *pos_cmd = (const struct ct_cdb_cmd_s *) cmd;

    struct ct_resource_id rid = {
            .name=ce_cdb_a0->read_uint64(pos_cmd->cmd, ASSET_NAME, 0),
            .type=ce_cdb_a0->read_uint64(pos_cmd->cmd, ASSET_TYPE, 0),
    };


    uint64_t asset_obj = ct_sourcedb_a0->get(rid);
    uint64_t keys = ce_cdb_a0->read_subobject(pos_cmd->cmd, _KEYS, 0);
    uint64_t prop = ce_cdb_a0->read_uint64(pos_cmd->cmd, _PROP, 0);

    uint64_t obj = _find_recursive_create(asset_obj, keys);

    float f = ce_cdb_a0->read_float(pos_cmd->cmd,
                                    inverse ? _OLD_VALUE : _NEW_VALUE, 0);

    ce_cdb_obj_o *w = ce_cdb_a0->write_begin(obj);
    ce_cdb_a0->set_float(w, prop, f);
    ce_cdb_a0->write_commit(w);
}

static void set_uint64_cmd(const struct ct_cmd *cmd,
                           bool inverse) {
    const struct ct_cdb_cmd_s *pos_cmd = (const struct ct_cdb_cmd_s *) cmd;

    struct ct_resource_id rid = {
            .name=ce_cdb_a0->read_uint64(pos_cmd->cmd, ASSET_NAME, 0),
            .type=ce_cdb_a0->read_uint64(pos_cmd->cmd, ASSET_TYPE, 0),
    };

    uint64_t asset_obj = ct_sourcedb_a0->get(rid);
    uint64_t keys = ce_cdb_a0->read_subobject(pos_cmd->cmd, _KEYS, 0);
    uint64_t prop = ce_cdb_a0->read_uint64(pos_cmd->cmd, _PROP, 0);

    uint64_t obj = _find_recursive_create(asset_obj, keys);

    uint64_t f = ce_cdb_a0->read_uint64(pos_cmd->cmd,
                                        inverse ? _OLD_VALUE : _NEW_VALUE, 0);

    ce_cdb_obj_o *w = ce_cdb_a0->write_begin(obj);
    ce_cdb_a0->set_uint64(w, prop, f);
    ce_cdb_a0->write_commit(w);
}

static void set_bool_cmd(const struct ct_cmd *cmd,
                         bool inverse) {
    const struct ct_cdb_cmd_s *pos_cmd = (const struct ct_cdb_cmd_s *) cmd;

    struct ct_resource_id rid = {
            .name=ce_cdb_a0->read_uint64(pos_cmd->cmd, ASSET_NAME, 0),
            .type=ce_cdb_a0->read_uint64(pos_cmd->cmd, ASSET_TYPE, 0),
    };

    uint64_t asset_obj = ct_sourcedb_a0->get(rid);
    uint64_t keys = ce_cdb_a0->read_subobject(pos_cmd->cmd, _KEYS, 0);
    uint64_t prop = ce_cdb_a0->read_uint64(pos_cmd->cmd, _PROP, 0);

    uint64_t obj = _find_recursive_create(asset_obj, keys);

    bool f = ce_cdb_a0->read_bool(pos_cmd->cmd,
                                  inverse ? _OLD_VALUE : _NEW_VALUE, 0);

    ce_cdb_obj_o *w = ce_cdb_a0->write_begin(obj);
    ce_cdb_a0->set_bool(w, prop, f);
    ce_cdb_a0->write_commit(w);
}

static void set_str_cmd(const struct ct_cmd *_cmd,
                        bool inverse) {
    const struct ct_cdb_cmd_s *pos_cmd = (const struct ct_cdb_cmd_s *) _cmd;

    struct ct_resource_id rid = {
            .name=ce_cdb_a0->read_uint64(pos_cmd->cmd, ASSET_NAME, 0),
            .type=ce_cdb_a0->read_uint64(pos_cmd->cmd, ASSET_TYPE, 0),
    };

    uint64_t asset_obj = ct_sourcedb_a0->get(rid);
    uint64_t keys = ce_cdb_a0->read_subobject(pos_cmd->cmd, _KEYS, 0);
    uint64_t prop = ce_cdb_a0->read_uint64(pos_cmd->cmd, _PROP, 0);

    uint64_t obj = _find_recursive_create(asset_obj, keys);

    const char *f = ce_cdb_a0->read_str(pos_cmd->cmd,
                                        inverse ? _OLD_VALUE : _NEW_VALUE, 0);

    ce_cdb_obj_o *w = ce_cdb_a0->write_begin(obj);
    ce_cdb_a0->set_str(w, prop, f);
    ce_cdb_a0->write_commit(w);
}

static void cmd_description(char *buffer,
                            uint32_t buffer_size,
                            const struct ct_cmd *cmd,
                            bool inverse) {
//    const struct ct_cdb_cmd_s *pos_cmd = (const struct ct_cdb_cmd_s *) cmd;

    switch (cmd->type) {
//        case _SET_VEC3:
////            snprintf(buffer, buffer_size,
////                     "Set vec3 [%f, %f, %f]",
////                     pos_cmd->vec3.new_value[0],
////                     pos_cmd->vec3.new_value[1],
////                     pos_cmd->vec3.new_value[2]);
//            break;
//
//        case _SET_FLOAT:
////            snprintf(buffer, buffer_size,
////                     "Set float %f",
////                     pos_cmd->f.new_value);
//            break;
//
//        case _SET_STR:
////            snprintf(buffer, buffer_size,
////                     "Set str %s",
////                     pos_cmd->str.new_value);
//            break;

        default:
            snprintf(buffer, buffer_size, "(no description)");
            break;
    }
}

static void _init(struct ce_api_a0 *api) {
    ct_cmd_system_a0->register_cmd_execute(_SET_VEC3,
                                           (struct ct_cmd_fce) {
                                                   .execute = set_vec3_cmd,
                                                   .description = cmd_description});

    ct_cmd_system_a0->register_cmd_execute(_SET_STR,
                                           (struct ct_cmd_fce) {
                                                   .execute = set_str_cmd,
                                                   .description = cmd_description});

    ct_cmd_system_a0->register_cmd_execute(_SET_BOOL,
                                           (struct ct_cmd_fce) {
                                                   .execute = set_bool_cmd,
                                                   .description = cmd_description});

    ct_cmd_system_a0->register_cmd_execute(_SET_FLOAT,
                                           (struct ct_cmd_fce) {
                                                   .execute = set_float_cmd,
                                                   .description = cmd_description});

    ct_cmd_system_a0->register_cmd_execute(_SET_UINT64,
                                           (struct ct_cmd_fce) {
                                                   .execute = set_uint64_cmd,
                                                   .description = cmd_description});

    ct_cmd_system_a0->register_cmd_execute(_SET_VEC4,
                                           (struct ct_cmd_fce) {
                                                   .execute = set_vec4_cmd,
                                                   .description = cmd_description});

    api->register_api("ct_editor_ui_a0", ct_editor_ui_a0);
}

static void _shutdown() {
}

CE_MODULE_DEF(
        editor_ui,
        {
            CE_INIT_API(api, ce_memory_a0);
            CE_INIT_API(api, ce_id_a0);
            CE_INIT_API(api, ce_cdb_a0);
            CE_INIT_API(api, ct_cmd_system_a0);
        },
        {
            CE_UNUSED(reload);
            _init(api);
        },
        {
            CE_UNUSED(reload);
            CE_UNUSED(api);
            _shutdown();
        }
)
