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

static uint16_t hid_host_cid = 0;

// ======== 解析 DS4 HID 數據包 ========
static void handle_ds4_report(const uint8_t *packet, uint16_t size) {
    if (size < 10) return;
    // DS4 標準 Report ID 0x01 位移量
    ds4_state.lx = packet[1];
    ds4_state.ly = packet[2];
    ds4_state.rx = packet[3];
    ds4_state.ry = packet[4];
    ds4_state.hat = packet[5] & 0x0F;
    ds4_state.buttons = packet[5] | (packet[6] << 8);
    ds4_state.l2 = packet[8];
    ds4_state.r2 = packet[9];
}

// ======== BTstack 事件處理器 ========
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    uint8_t event = hci_event_packet_get_type(packet);

    if (packet_type == HCI_EVENT_PACKET) {
        if (event == BTSTACK_EVENT_STATE && btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
            printf("DS4: 藍牙已就緒，請將手把設為配對模式 (PS+Share)\n");
        } else if (event == HCI_EVENT_HID_META) {
            uint8_t subevent = hci_event_hid_meta_get_subevent_code(packet);
            if (subevent == HID_SUBEVENT_CONNECTION_OPENED) {
                if (hid_subevent_connection_opened_get_status(packet) == 0) {
                    hid_host_cid = hid_subevent_connection_opened_get_hid_cid(packet);
                    ds4_state.connected = true;
                    printf("DS4: 連線成功!\n");
                }
            } else if (subevent == HID_SUBEVENT_CONNECTION_CLOSED) {
                hid_host_cid = 0;
                ds4_state.connected = false;
                printf("DS4: 連線中斷\n");
            }
        }
    } else if (packet_type == HID_DATA_PACKET) {
        handle_ds4_report(packet, size);
    }
}

// ======== MicroPython 接口 ========

static mp_obj_t ds4_read_sticks(void) {
    mp_obj_t t[4] = {
        mp_obj_new_int(ds4_state.lx),
        mp_obj_new_int(ds4_state.ly),
        mp_obj_new_int(ds4_state.rx),
        mp_obj_new_int(ds4_state.ry),
    };
    return mp_obj_new_tuple(4, t);
}
MP_DEFINE_CONST_FUN_OBJ_0(ds4_read_sticks_obj, ds4_read_sticks);

static mp_obj_t ds4_is_connected(void) {
    return mp_obj_new_bool(ds4_state.connected);
}
MP_DEFINE_CONST_FUN_OBJ_0(ds4_is_connected_obj, ds4_is_connected);

static const mp_rom_map_elem_t ds4_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),  MP_ROM_QSTR(MP_QSTR_ds4) },
    { MP_ROM_QSTR(MP_QSTR_sticks),    MP_ROM_PTR(&ds4_read_sticks_obj) },
    { MP_ROM_QSTR(MP_QSTR_connected), MP_ROM_PTR(&ds4_is_connected_obj) },
};
static MP_DEFINE_CONST_DICT(ds4_module_globals, ds4_module_globals_table);

const mp_obj_module_t ds4_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&ds4_module_globals,
};

// 註冊模組名為 ds4，末尾不要加分號
MP_REGISTER_MODULE(MP_QSTR_ds4, ds4_user_cmodule);
