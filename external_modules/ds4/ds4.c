#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "py/runtime.h"
#include "py/mphal.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/async_context.h"
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
    bd_addr_
