#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "py/runtime.h"
#include "py/mphal.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "btstack.h"
#include "classic/sdp_server.h"

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
#define MAX_ATTRIBUTE_VALUE_SIZE 512
#define MAX_DEVICES 10
#define INQUIRY_INTERVAL 5

static btstack_packet_callback_registration_t hci_event_cb;
static uint8_t hid_descriptor_storage[MAX_ATTRIBUTE_VALUE_SIZE];
static uint16_t hid_host_cid = 0;
static bool hid_descriptor_available = false;
static hid_protocol_mode_t hid_report_mode = HID_PROTOCOL_MODE_REPORT;
static bool bt_running = false;

// ======== 掃描結構 ========
enum DEVICE_STATE { NAME_REQUEST, NAME_INQUIRED, NAME_FETCHED };
struct device {
    bd_addr_t address;
    uint8_t pageScanRepetitionMode;
    uint16_t clockOffset;
    enum DEVICE_STATE state;
};
static struct device devices[MAX_DEVICES];
static int deviceCount = 0;
static bd_addr_t remote_addr;
static bool mac_found = false;
static bool scanning = true;

enum SCAN_STATE { SCAN_INIT, SCAN_ACTIVE } scan_state = SCAN_INIT;

// ======== DS4 HID Report 解析 ========
struct __attribute__((packed)) input_report_17 {
    uint8_t report_id;
    uint8_t pad[2];
    uint8_t lx, ly, rx, ry;
    uint8_t buttons[3];
    uint8_t l2, r2;
};

static void handle_interrupt_report(const uint8_t *packet, uint16_t len) {
    if (len < sizeof(struct input_report_17) + 1) return;
    if (packet[0] != 0xa1 || packet[1] != 0x11) return;

    struct input_report_17 *r = (struct input_report_17 *)&packet[1];

    ds4_state.lx = r->lx;
    ds4_state.ly = r->ly;
    ds4_state.rx = r->rx;
    ds4_state.ry = r->ry;
    ds4_state.l2 = r->l2;
    ds4_state.r2 = r->r2;
    ds4_state.hat = r->buttons[0] & 0x0f;
    ds4_state.buttons =
        ((r->buttons[0] & 0xf0) << 8) |
        ((r->buttons[2] & 0x03) << 8) |
        r->buttons[1];
}

// ======== 掃描邏輯 ========
static void start_scan(void) {
    printf("[DS4] Starting inquiry scan...\n");
    gap_inquiry_start(INQUIRY_INTERVAL);
}

static int get_device_index(bd_addr_t addr) {
    for (int i = 0; i < deviceCount; i++)
        if (bd_addr_cmp(addr, devices[i].address) == 0) return i;
    return -1;
}

static void do_next_name_request(void) {
    for (int i = 0; i < deviceCount; i++) {
        if (devices[i].state == NAME_REQUEST) {
            devices[i].state = NAME_INQUIRED;
            gap_remote_name_request(devices[i].address,
                devices[i].pageScanRepetitionMode,
                devices[i].clockOffset | 0x8000);
            return;
        }
    }
    start_scan();
}

static void continue_remote_names(void) {
    for (int i = 0; i < deviceCount; i++)
        if (devices[i].state == NAME_REQUEST) {
            do_next_name_request();
            return;
        }
    start_scan();
}

// ======== 主封包處理 ========
static void packet_handler(uint8_t ptype, uint16_t channel,
                           uint8_t *packet, uint16_t size) {
    UNUSED(channel); UNUSED(size);
    if (ptype != HCI_EVENT_PACKET) return;

    uint8_t event = hci_event_packet_get_type(packet);
    bd_addr_t addr;
    uint8_t status;

    // ---- 掃描階段 ----
    if (scanning && !mac_found) {
        switch (scan_state) {
        case SCAN_INIT:
            if (event == BTSTACK_EVENT_STATE &&
                btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
                start_scan();
                scan_state = SCAN_ACTIVE;
            }
            break;
        case SCAN_ACTIVE:
            if (event == GAP_EVENT_INQUIRY_RESULT) {
                if (deviceCount >= MAX_DEVICES) break;
                gap_event_inquiry_result_get_bd_addr(packet, addr);
                if (get_device_index(addr) >= 0) break;

                memcpy(devices[deviceCount].address, addr, 6);
                devices[deviceCount].pageScanRepetitionMode =
                    gap_event_inquiry_result_get_page_scan_repetition_mode(packet);
                devices[deviceCount].clockOffset =
                    gap_event_inquiry_result_get_clock_offset(packet);

                printf("[DS4] Found: %s\n", bd_addr_to_str(addr));

                if (gap_event_inquiry_result_get_name_available(packet)) {
                    char name[240];
                    int nlen = gap_event_inquiry_result_get_name_len(packet);
                    memcpy(name, gap_event_inquiry_result_get_name(packet), nlen);
                    name[nlen] = 0;
                    printf("[DS4] Name: %s\n", name);
                    devices[deviceCount].state = NAME_FETCHED;
                    if (strcmp(name, "Wireless Controller") == 0) {
                        bd_addr_copy(remote_addr, addr);
                        mac_found = true;
                        scanning = false;
                    }
                } else {
                    devices[deviceCount].state = NAME_REQUEST;
                }
                deviceCount++;
            } else if (event == GAP_EVENT_INQUIRY_COMPLETE) {
                for (int i = 0; i < deviceCount; i++)
                    if (devices[i].state == NAME_INQUIRED)
                        devices[i].state = NAME_REQUEST;
                continue_remote_names();
            } else if (event == HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE) {
                reverse_bd_addr(&packet[3], addr);
                int idx = get_device_index(addr);
                if (idx >= 0 && packet[2] == 0) {
                    char *name = (char *)&packet[9];
                    printf("[DS4] Remote name: %s\n", name);
                    devices[idx].state = NAME_FETCHED;
                    if (strcmp(name, "Wireless Controller") == 0) {
                        bd_addr_copy(remote_addr, addr);
                        mac_found = true;
                        scanning = false;
                    }
                }
                continue_remote_names();
            }
            break;
        }

        if (mac_found) {
            printf("[DS4] Connecting to %s\n", bd_addr_to_str(remote_addr));
            status = hid_host_connect(remote_addr, hid_report_mode, &hid_host_cid);
            if (status != ERROR_CODE_SUCCESS)
                printf("[DS4] Connect failed: 0x%02x\n", status);
        }
        return;
    }

    // ---- 連線階段 ----
    switch (event) {
    case HCI_EVENT_PIN_CODE_REQUEST:
        hci_event_pin_code_request_get_bd_addr(packet, addr);
        gap_pin_code_response(addr, "0000");
        break;
    case HCI_EVENT_DISCONNECTION_COMPLETE:
        printf("[DS4] Disconnected\n");
        ds4_state.connected = false;
        hid_host_cid = 0;
        hid_descriptor_available = false;
        mac_found = false;
        scanning = true;
        scan_state = SCAN_INIT;
        deviceCount = 0;
        start_scan();
        scan_state = SCAN_ACTIVE;
        break;
    case HCI_EVENT_HID_META: {
        uint8_t hid_ev = hci_event_hid_meta_get_subevent_code(packet);
        switch (hid_ev) {
        case HID_SUBEVENT_INCOMING_CONNECTION:
            hid_subevent_incoming_connection_get_address(packet, addr);
            hid_host_accept_connection(
                hid_subevent_incoming_connection_get_hid_cid(packet),
                hid_report_mode);
            break;
        case HID_SUBEVENT_CONNECTION_OPENED:
            status = hid_subevent_connection_opened_get_status(packet);
            if (status != ERROR_CODE_SUCCESS) {
                printf("[DS4] Open failed: 0x%02x\n", status);
                ds4_state.connected = false;
                break;
            }
            hid_host_cid = hid_subevent_connection_opened_get_hid_cid(packet);
            printf("[DS4] Connected!\n");
            ds4_state.connected = true;
            break;
        case HID_SUBEVENT_DESCRIPTOR_AVAILABLE:
            if (hid_subevent_descriptor_available_get_status(packet)
                    == ERROR_CODE_SUCCESS) {
                hid_descriptor_available = true;
                hid_host_send_get_report(hid_host_cid,
                    HID_REPORT_TYPE_FEATURE, 0x05);
            }
            break;
        case HID_SUBEVENT_REPORT:
            if (hid_descriptor_available)
                handle_interrupt_report(
                    hid_subevent_report_get_report(packet),
                    hid_subevent_report_get_report_len(packet));
            break;
        case HID_SUBEVENT_CONNECTION_CLOSED:
            ds4_state.connected = false;
            hid_host_cid = 0;
            break;
        }
        break;
    }
    default: break;
    }
}

// ======== BTstack 主執行緒（Core 1）========
static void bt_main(void) {
    gap_set_security_level(LEVEL_2);

    l2cap_init();
    sdp_init();
    hid_host_init(hid_descriptor_storage, sizeof(hid_descriptor_storage));
    hid_host_register_packet_handler(packet_handler);

    gap_set_default_link_policy_settings(
        LM_LINK_POLICY_ENABLE_SNIFF_MODE |
        LM_LINK_POLICY_ENABLE_ROLE_SWITCH);
    hci_set_master_slave_policy(HCI_ROLE_MASTER);

    hci_event_cb.callback = &packet_handler;
    hci_add_event_handler(&hci_event_cb);

    hci_power_control(HCI_POWER_ON);
    bt_running = true;
    printf("[DS4] BTstack running on Core 1\n");
    btstack_run_loop_execute();
}

// ======== MicroPython API ========

static mp_obj_t ds4_start(void) {
    if (!bt_running) {
        multicore_launch_core1(bt_main);
        printf("[DS4] Core 1 launched\n");
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(ds4_start_obj, ds4_start);

static mp_obj_t ds4_connected(void) {
    return mp_obj_new_bool(ds4_state.connected);
}
static MP_DEFINE_CONST_FUN_OBJ_0(ds4_connected_obj, ds4_connected);

static mp_obj_t ds4_buttons(void) {
    return mp_obj_new_int(ds4_state.buttons);
}
static MP_DEFINE_CONST_FUN_OBJ_0(ds4_read_buttons_obj, ds4_buttons);

static mp_obj_t ds4_sticks(void) {
    mp_obj_t t[4] = {
        mp_obj_new_int(ds4_state.lx),
        mp_obj_new_int(ds4_state.ly),
        mp_obj_new_int(ds4_state.rx),
        mp_obj_new_int(ds4_state.ry),
    };
    return mp_obj_new_tuple(4, t);
}
static MP_DEFINE_CONST_FUN_OBJ_0(ds4_read_sticks_obj, ds4_sticks);

static mp_obj_t ds4_triggers(void) {
    mp_obj_t t[2] = {
        mp_obj_new_int(ds4_state.l2),
        mp_obj_new_int(ds4_state.r2),
    };
    return mp_obj_new_tuple(2, t);
}
static MP_DEFINE_CONST_FUN_OBJ_0(ds4_triggers_obj, ds4_triggers);

static mp_obj_t ds4_hat(void) {
    return mp_obj_new_int(ds4_state.hat);
}
static MP_DEFINE_CONST_FUN_OBJ_0(ds4_hat_obj, ds4_hat);

static const mp_rom_map_elem_t ds4_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),  MP_ROM_QSTR(MP_QSTR_ds4) },
    { MP_ROM_QSTR(MP_QSTR_start),     MP_ROM_PTR(&ds4_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_connected), MP_ROM_PTR(&ds4_connected_obj) },
    { MP_ROM_QSTR(MP_QSTR_buttons),   MP_ROM_PTR(&ds4_read_buttons_obj) },
    { MP_ROM_QSTR(MP_QSTR_sticks),    MP_ROM_PTR(&ds4_read_sticks_obj) },
    { MP_ROM_QSTR(MP_QSTR_triggers),  MP_ROM_PTR(&ds4_triggers_obj) },
    { MP_ROM_QSTR(MP_QSTR_hat),       MP_ROM_PTR(&ds4_hat_obj) },
};
static MP_DEFINE_CONST_DICT(ds4_module_globals, ds4_module_globals_table);

const mp_obj_module_t ds4_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&ds4_module_globals,
};
MP_REGISTER_MODULE(MP_QSTR_ds4, ds4_user_cmodule);
