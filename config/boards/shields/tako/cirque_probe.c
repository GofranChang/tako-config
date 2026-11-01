/* SPDX-License-Identifier: MIT
 * Default path: counters only, without polling timers or logging.
 * Explicit shell commands can change temporary tap/BLE parameters for A/B tests.
 */
#include <string.h>
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/input/input.h>
#include <zmk/split/peripheral.h>
#include "cirque_probe.h"

enum {
    IRQ, WORK, READ_OK, READ_ERROR, CONTACT, LOW_Z,
    QUEUED, DROPPED, GENERATED_FRAMES, NONZERO_FRAMES, DISPATCHED_FRAMES,
    TX_EVENTS, TX_ERROR, MAX_WORK_US, MAX_TX_US,
    GAP_8, GAP_16, GAP_32, GAP_64, GAP_OVER64,
    DISPATCH_GAP_8, DISPATCH_GAP_16, DISPATCH_GAP_32, DISPATCH_GAP_64, DISPATCH_GAP_OVER64,
    TX_WAIT_8, TX_WAIT_16, TX_WAIT_32, TX_WAIT_64, TX_WAIT_OVER64, COUNT
};
static atomic_t counts[COUNT];
static atomic_t pending_irq, last_frame, last_dispatch, nonzero_frame;
static int64_t reset_at;
static atomic_t tap_limit_override;
uint16_t tako_cirque_probe_tap_limit(uint16_t default_limit) {
    unsigned value = (unsigned)atomic_get(&tap_limit_override);
    return value ? value : default_limit;
}

static void update_max(unsigned idx, uint32_t value) {
    atomic_val_t old = atomic_get(&counts[idx]);
    while (value > (uint32_t)old && !atomic_cas(&counts[idx], old, value)) {
        old = atomic_get(&counts[idx]);
    }
}

void tako_cirque_probe_irq(void) {
    atomic_inc(&counts[IRQ]);
    atomic_cas(&pending_irq, 0, k_cycle_get_32());
}
void tako_cirque_probe_work(void) {
    atomic_inc(&counts[WORK]);
    uint32_t start = (uint32_t)atomic_set(&pending_irq, 0);
    if (start) {
        update_max(MAX_WORK_US, k_cyc_to_us_floor32(k_cycle_get_32() - start));
    }
}
void tako_cirque_probe_sample(int result, uint8_t z) {
    atomic_inc(&counts[result < 0 ? READ_ERROR : READ_OK]);
    if (result == 0 && z) {
        atomic_inc(&counts[CONTACT]);
        if (z < DT_PROP(DT_NODELABEL(tako_cirque), absolute_touch_min_z)) {
            atomic_inc(&counts[LOW_Z]);
        }
    }
}
void tako_cirque_probe_report(int result, uint16_t code, int32_t value, bool sync) {
    atomic_inc(&counts[result < 0 ? DROPPED : QUEUED]);
    if (code != INPUT_REL_X && code != INPUT_REL_Y) {
        return;
    }
    if (value) { atomic_set(&nonzero_frame, 1); }
    if (!sync) { return; }
    atomic_inc(&counts[GENERATED_FRAMES]);
    if (atomic_set(&nonzero_frame, 0)) { atomic_inc(&counts[NONZERO_FRAMES]); }
    uint32_t now = k_cycle_get_32();
    uint32_t previous = (uint32_t)atomic_set(&last_frame, now);
    if (previous) {
        uint32_t us = k_cyc_to_us_floor32(now - previous);
        unsigned bucket = us <= 8000 ? GAP_8 : us <= 16000 ? GAP_16 :
                          us <= 32000 ? GAP_32 : us <= 64000 ? GAP_64 : GAP_OVER64;
        atomic_inc(&counts[bucket]);
    }
}

static void input_probe(struct input_event *event) {
    if (event->type == INPUT_EV_REL && event->sync &&
        (event->code == INPUT_REL_X || event->code == INPUT_REL_Y)) {
        atomic_inc(&counts[DISPATCHED_FRAMES]);
        uint32_t now = k_cycle_get_32();
        uint32_t previous = (uint32_t)atomic_set(&last_dispatch, now);
        if (previous) {
            uint32_t us = k_cyc_to_us_floor32(now - previous);
            unsigned bucket = us <= 8000 ? DISPATCH_GAP_8 : us <= 16000 ? DISPATCH_GAP_16 :
                              us <= 32000 ? DISPATCH_GAP_32 : us <= 64000 ? DISPATCH_GAP_64 : DISPATCH_GAP_OVER64;
            atomic_inc(&counts[bucket]);
        }
    }
}
INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_NODELABEL(tako_cirque)), input_probe);

int __real_zmk_split_peripheral_report_event(const struct zmk_split_transport_peripheral_event *ev);
int __wrap_zmk_split_peripheral_report_event(const struct zmk_split_transport_peripheral_event *ev) {
    if (ev->type != ZMK_SPLIT_TRANSPORT_PERIPHERAL_EVENT_TYPE_INPUT_EVENT) {
        return __real_zmk_split_peripheral_report_event(ev);
    }
    uint32_t start = k_cycle_get_32();
    int result = __real_zmk_split_peripheral_report_event(ev);
    atomic_inc(&counts[TX_EVENTS]);
    if (result < 0) { atomic_inc(&counts[TX_ERROR]); }
    uint32_t us = k_cyc_to_us_floor32(k_cycle_get_32() - start);
    update_max(MAX_TX_US, us);
    unsigned bucket = us <= 8000 ? TX_WAIT_8 : us <= 16000 ? TX_WAIT_16 :
                      us <= 32000 ? TX_WAIT_32 : us <= 64000 ? TX_WAIT_64 : TX_WAIT_OVER64;
    atomic_inc(&counts[bucket]);
    return result;
}

static void show_connection(struct bt_conn *conn, void *context) {
    const struct shell *sh = context;
    struct bt_conn_info info;
    if (bt_conn_get_info(conn, &info) || info.state != BT_CONN_STATE_CONNECTED) { return; }
    shell_print(sh, "BLE interval_us=%u latency=%u timeout_ms=%u role=%u",
                (unsigned)info.le.interval * 1250, info.le.latency,
                (unsigned)info.le.timeout * 10, info.role);
}
static unsigned get(unsigned idx) { return (unsigned)atomic_get(&counts[idx]); }
static int probe_reset(const struct shell *sh, size_t argc, char **argv) {
    ARG_UNUSED(argc); ARG_UNUSED(argv);
    for (unsigned i = 0; i < COUNT; i++) { atomic_clear(&counts[i]); }
    atomic_clear(&pending_irq); atomic_clear(&last_frame); atomic_clear(&last_dispatch); atomic_clear(&nonzero_frame);
    reset_at = k_uptime_get();
    shell_print(sh, "Cirque counters reset; calibration and settings unchanged");
    return 0;
}
static int probe_show(const struct shell *sh, size_t argc, char **argv) {
    ARG_UNUSED(argc); ARG_UNUSED(argv);
    shell_print(sh, "TAKO_CIRQUE_PROBE_V2 elapsed_ms=%lld", (long long)(k_uptime_get() - reset_at));
    shell_print(sh, "irq=%u work=%u read=%u read_err=%u contact=%u low_z=%u",
                get(IRQ), get(WORK), get(READ_OK), get(READ_ERROR), get(CONTACT), get(LOW_Z));
    shell_print(sh, "generated_frames=%u nonzero_frames=%u dispatched_frames=%u queued=%u dropped=%u",
                get(GENERATED_FRAMES), get(NONZERO_FRAMES), get(DISPATCHED_FRAMES), get(QUEUED), get(DROPPED));
    shell_print(sh, "tx_events=%u tx_err=%u max_irq_wait_us=%u max_tx_us=%u",
                get(TX_EVENTS), get(TX_ERROR), get(MAX_WORK_US), get(MAX_TX_US));
    shell_print(sh, "frame_gaps_us <=8000:%u <=16000:%u <=32000:%u <=64000:%u >64000:%u",
                get(GAP_8), get(GAP_16), get(GAP_32), get(GAP_64), get(GAP_OVER64));
    shell_print(sh, "dispatch_gaps_us <=8000:%u <=16000:%u <=32000:%u <=64000:%u >64000:%u",
                get(DISPATCH_GAP_8), get(DISPATCH_GAP_16), get(DISPATCH_GAP_32), get(DISPATCH_GAP_64), get(DISPATCH_GAP_OVER64));
    shell_print(sh, "tx_wait_us <=8000:%u <=16000:%u <=32000:%u <=64000:%u >64000:%u",
                get(TX_WAIT_8), get(TX_WAIT_16), get(TX_WAIT_32), get(TX_WAIT_64), get(TX_WAIT_OVER64));
    shell_print(sh, "tap_movement_limit=%u (runtime, resets on reboot)",
                tako_cirque_probe_tap_limit(DT_PROP(DT_NODELABEL(tako_cirque), absolute_tap_max_movement)));
    bt_conn_foreach(BT_CONN_TYPE_LE, show_connection, (void *)sh);
    static const struct i2c_dt_spec bus = I2C_DT_SPEC_GET(DT_NODELABEL(tako_cirque));
    uint8_t command = 0xa4, mode = 0, rate = 0;
    int mode_rc = i2c_write_read_dt(&bus, &command, 1, &mode, 1);
    command = 0xa9;
    int rate_rc = i2c_write_read_dt(&bus, &command, 1, &rate, 1);
    shell_print(sh, "feed=0x%02x sample_rate_reg=%u read_rc=%d/%d", mode, rate, mode_rc, rate_rc);
    return 0;
}
static int set_tap_limit(const struct shell *sh, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    bool low = strcmp(argv[0], "tap20") == 0;
    atomic_set(&tap_limit_override, low ? 20 : 0);
    shell_print(sh, "Runtime tap movement limit=%u; use with finger lifted; settings unchanged",
                tako_cirque_probe_tap_limit(DT_PROP(DT_NODELABEL(tako_cirque), absolute_tap_max_movement)));
    return 0;
}
struct latency_request { const struct shell *sh; uint16_t latency; unsigned matches; int result; };
static void request_latency(struct bt_conn *conn, void *context) {
    struct latency_request *request = context;
    struct bt_conn_info info;
    if (bt_conn_get_info(conn, &info) || info.state != BT_CONN_STATE_CONNECTED ||
        info.role != BT_CONN_ROLE_PERIPHERAL) { return; }
    struct bt_le_conn_param parameters = {
        .interval_min = info.le.interval, .interval_max = info.le.interval,
        .latency = request->latency, .timeout = info.le.timeout,
    };
    request->matches++;
    int result = bt_conn_le_param_update(conn, &parameters);
    if (result < 0) { request->result = result; }
    shell_print(request->sh, "BLE latency=%u request result=%d; run show to confirm negotiation",
                request->latency, result);
}
static int set_latency(const struct shell *sh, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    struct latency_request request = {.sh=sh, .latency=strcmp(argv[0], "latency0") == 0 ? 0 : 30};
    bt_conn_foreach(BT_CONN_TYPE_LE, request_latency, &request);
    if (!request.matches) { shell_error(sh, "No connected peripheral BLE link"); return -ENOTCONN; }
    return request.result;
}
SHELL_STATIC_SUBCMD_SET_CREATE(probe_commands,
    SHELL_CMD_ARG(show, NULL, "Read timing/queue/connection counters", probe_show, 1, 0),
    SHELL_CMD_ARG(reset, NULL, "Reset counters only", probe_reset, 1, 0),
    SHELL_CMD_ARG(tap20, NULL, "Temporary tap movement limit 20", set_tap_limit, 1, 0),
    SHELL_CMD_ARG(tap80, NULL, "Restore configured tap movement limit", set_tap_limit, 1, 0),
    SHELL_CMD_ARG(latency0, NULL, "Request temporary BLE latency 0", set_latency, 1, 0),
    SHELL_CMD_ARG(latency30, NULL, "Restore BLE latency 30", set_latency, 1, 0),
    SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(cirque_probe, &probe_commands, "Cirque timing diagnostics", NULL);
