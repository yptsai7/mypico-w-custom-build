#include <stdint.h>
#include <string.h>

#include "py/runtime.h"
#include "py/mphal.h"

// 定義 DS4 狀態結構
typedef struct _ds4_state_t {
    uint8_t left_stick_x;
    uint8_t left_stick_y;
    uint8_t right_stick_x;
    uint8_t right_stick_y;
    uint16_t buttons;
    uint8_t l_trigger;
    uint8_t r_trigger;
} ds4_state_t;

static ds4_state_t ds4_state;

// --- 核心解析函數 ---
static void parse_ds4_report(const uint8_t *packet, uint16_t size) {
    if (size < 10) return;
    
    // 解析 DS4 標準 HID Report (通常從 index 1 開始)
    ds4_state.left_stick_x = packet[1];
    ds4_state.left_stick_y = packet[2];
    ds4_state.right_stick_x = packet[3];
    ds4_state.right_stick_y = packet[4];
    ds4_state.buttons = (packet[6] << 8) | packet[5];
    ds4_state.l_trigger = packet[8];
    ds4_state.r_trigger = packet[9];
}

// --- MicroPython 介面 ---

// 提供給 Python 呼叫：傳入藍牙收到的 bytes
static mp_obj_t ds4_update(mp_obj_t data_obj) {
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(data_obj, &bufinfo, MP_BUFFER_READ);
    parse_ds4_report(bufinfo.buf, bufinfo.len);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(ds4_update_obj, ds4_update);

// 讀取按鍵
static mp_obj_t ds4_read_buttons(void) {
    return mp_obj_new_int(ds4_state.buttons);
}
static MP_DEFINE_CONST_FUN_OBJ_0(ds4_read_buttons_obj, ds4_read_buttons);

// 讀取搖桿 (LX, LY, RX, RY)
static mp_obj_t ds4_read_sticks(void) {
    mp_obj_t tuple[4] = {
        mp_obj_new_int(ds4_state.left_stick_x),
        mp_obj_new_int(ds4_state.left_stick_y),
        mp_obj_new_int(ds4_state.right_stick_x),
        mp_obj_new_int(ds4_state.right_stick_y)
    };
    return mp_obj_new_tuple(4, tuple);
}
static MP_DEFINE_CONST_FUN_OBJ_0(ds4_read_sticks_obj, ds4_read_sticks);

// 註冊模組屬性
static const mp_rom_map_elem_t ds4_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_ds4) },
    { MP_ROM_QSTR(MP_QSTR_update), MP_ROM_PTR(&ds4_update_obj) }, // 新增 update
    { MP_ROM_QSTR(MP_QSTR_buttons), MP_ROM_PTR(&ds4_read_buttons_obj) },
    { MP_ROM_QSTR(MP_QSTR_sticks), MP_ROM_PTR(&ds4_read_sticks_obj) },
};
static MP_DEFINE_CONST_DICT(ds4_module_globals, ds4_module_globals_table);

// 定義模組對象
const mp_obj_module_t ds4_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&ds4_module_globals,
};

// 註冊模組
MP_REGISTER_MODULE(MP_QSTR_ds4, ds4_user_cmodule);
