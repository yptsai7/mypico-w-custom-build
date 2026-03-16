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

// ======== BTstack 變數 ========
static btstack_packet_callback_registration_t hci_event_cb;
static bd_addr_t remote_addr;
static uint16_t hid_host_cid = 0;

// 解析 DS4 HID 報表 (通常是 Report ID 0x01)
static void handle_ds4_report(const uint8_t *packet, uint16_t size) {
    if (size < 10) return; // 簡單長度檢查
    
    // DS4 典型偏移量 (根據標準 HID 報表描述符)
    ds4_state.lx = packet[1];
    ds4_state.ly = packet[2];
    ds4_state.rx = packet[3];
    ds4_state.ry = packet[4];
    ds4_state.buttons = packet[5] | (packet[6] << 8);
    ds4_state.l2 = packet[8];
    ds4_state.r2 = packet[9];
    ds4_state.hat = packet[5] & 0x0F;
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    uint8_t event = hci_event_packet_get_type(packet);

    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (event) {
                case BTSTACK_EVENT_STATE:
                    if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
                        printf("BTstack: 啟動中，開始搜尋 DS4...\n");
                        gap_local_bd_addr(remote_addr); // 僅初始化
                    }
                    break;
                case GAP_EVENT_ADVERTISING_REPORT:
                    // 此處可加入自動配對邏輯
                    break;
                case HCI_EVENT_HID_META:
                    switch (hci_event_hid_meta_get_subevent_code(packet)) {
                        case HID_SUBEVENT_CONNECTION_OPENED:
                            if (hid_subevent_connection_opened_get_status(packet) == 0) {
                                hid_host_cid = hid_subevent_connection_opened_get_hid_cid(packet);
                                ds4_state.connected = true;
                                printf("DS4: 連線成功!\n");
                            }
                            break;
                        case HID_SUBEVENT_CONNECTION_CLOSED:
                            hid_host_cid = 0;
                            ds4_state.connected = false;
                            printf("DS4: 連線已中斷\n");
                            break;
                    }
                    break;
            }
            break;
        case HID_DATA_PACKET:
            handle_ds4_report(packet, size);
            break;
    }
}

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
MP_DEFINE_CONST_FUN_OBJ_0(ds4_read_sticks_obj, ds4_sticks);

static mp_obj_t ds4_triggers(void) {
    mp_obj_t t[2] = {
        mp_obj_new_int(ds4_state.l2),
        mp_obj_new_int(ds4_state.r2),
    };
    return mp_obj_new_tuple(2, t);
}
MP_DEFINE_CONST_FUN_OBJ_0(ds4_triggers_obj, ds4_triggers);

static mp_obj_t ds4_is_connected(void) {
    return mp_obj_new_bool(ds4_state.connected);
}
MP_DEFINE_CONST_FUN_OBJ_0(ds4_is_connected_obj, ds4_is_connected);

static const mp_rom_map_elem_t ds4_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),  MP_ROM_QSTR(MP_QSTR_ds4) },
    { MP_ROM_QSTR(MP_QSTR_read_sticks),   MP_ROM_PTR(&ds4_read_sticks_obj) },
    { MP_ROM_QSTR(MP_QSTR_read_triggers), MP_ROM_PTR(&ds4_triggers_obj) },
    { MP_ROM_QSTR(MP_QSTR_is_connected),  MP_ROM_PTR(&ds4_is_connected_obj) },
};
static MP_DEFINE_CONST_DICT(ds4_module_globals, ds4_module_globals_table);

const mp_obj_module_t ds4_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&ds4_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_ds4, ds4_user_cmodule);
