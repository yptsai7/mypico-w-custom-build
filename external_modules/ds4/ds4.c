#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "py/runtime.h"
#include "py/obj.h"
#include "py/mphal.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "btstack.h"

// ======== DS4 狀態結構 ========
typedef struct {
    uint8_t lx, ly, rx, ry;
    uint16_t buttons;
    uint8_t l2, r2;
    uint8_t hat;
    bool connected;
} ds4_state_t;

static ds4_state_t ds4_state = {
    .lx = 0x80, .ly = 0x80,
    .rx = 0x80, .ry = 0x80,
    .buttons = 0, .l2 = 0, .r2 = 0,
    .hat = 0x8, .connected = false
};

// ... 此處請保留您原本的 BTstack 回調 (packet_handler 等) 與邏輯 ...

// ======== MicroPython 接口 ========

static mp_obj_t ds4_sticks(void) {
    mp_obj_t t[4] = {
        mp_obj_new_int(ds4_state.lx),
        mp_obj_new_int(ds4_state.ly),
        mp_obj_new_int(ds4_state.rx),
        mp_obj_new_int(ds4_state.ry),
    };
    return mp_obj_new_tuple(4, t);
}
// 修正：移除巨集前的 static，避免 QSTR 掃描失敗
MP_DEFINE_CONST_FUN_OBJ_0(ds4_read_sticks_obj, ds4_sticks);

static mp_obj_t ds4_triggers(void) {
    mp_obj_t t[2] = {
        mp_obj_new_int(ds4_state.l2),
        mp_obj_new_int(ds4_state.r2),
    };
    return mp_obj_new_tuple(2, t);
}
MP_DEFINE_CONST_FUN_OBJ_0(ds4_triggers_obj, ds4_triggers);

static mp_obj_t ds4_hat(void) {
    return mp_obj_new_int(ds4_state.hat);
}
MP_DEFINE_CONST_FUN_OBJ_0(ds4_hat_obj, ds4_hat);

static const mp_rom_map_elem_t ds4_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),  MP_ROM_QSTR(MP_QSTR_ds4) },
    { MP_ROM_QSTR(MP_QSTR_read_sticks),   MP_ROM_PTR(&ds4_read_sticks_obj) },
    { MP_ROM_QSTR(MP_QSTR_read_triggers), MP_ROM_PTR(&ds4_triggers_obj) },
    { MP_ROM_QSTR(MP_QSTR_read_hat),      MP_ROM_PTR(&ds4_hat_obj) },
};

static MP_DEFINE_CONST_DICT(ds4_module_globals, ds4_module_globals_table);

const mp_obj_module_t ds4_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&ds4_module_globals,
};

// 註冊模組到 MicroPython (不加分號)
MP_REGISTER_MODULE(MP_QSTR_ds4, ds4_user_cmodule);
