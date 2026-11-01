/* SPDX-License-Identifier: MIT
 * Optional counters only. No polling timer, logging or transport changes.
 */
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
    GAP_8, GAP_16, GAP_32, GAP_64, GAP_OVER64, COUNT
};
static atomic_t counts[COUNT];
static atomic_t pending_irq, last_frame, nonzero_frame;
static int64_t reset_at;

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
    update_max(MAX_TX_US, k_cyc_to_us_floor32(k_cycle_get_32() - start));
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
    atomic_clear(&pending_irq); atomic_clear(&last_frame); atomic_clear(&nonzero_frame);
    reset_at = k_uptime_get();
    shell_print(sh, "Cirque counters reset; calibration and settings unchanged");
    return 0;
}
static int probe_show(const struct shell *sh, size_t argc, char **argv) {
    ARG_UNUSED(argc); ARG_UNUSED(argv);
    shell_print(sh, "TAKO_CIRQUE_PROBE_V1 elapsed_ms=%lld", (long long)(k_uptime_get() - reset_at));
    shell_print(sh, "irq=%u work=%u read=%u read_err=%u contact=%u low_z=%u",
                get(IRQ), get(WORK), get(READ_OK), get(READ_ERROR), get(CONTACT), get(LOW_Z));
    shell_print(sh, "generated_frames=%u nonzero_frames=%u dispatched_frames=%u queued=%u dropped=%u",
                get(GENERATED_FRAMES), get(NONZERO_FRAMES), get(DISPATCHED_FRAMES), get(QUEUED), get(DROPPED));
    shell_print(sh, "tx_events=%u tx_err=%u max_irq_wait_us=%u max_tx_us=%u",
                get(TX_EVENTS), get(TX_ERROR), get(MAX_WORK_US), get(MAX_TX_US));
    shell_print(sh, "frame_gaps_us <=8000:%u <=16000:%u <=32000:%u <=64000:%u >64000:%u",
                get(GAP_8), get(GAP_16), get(GAP_32), get(GAP_64), get(GAP_OVER64));
    bt_conn_foreach(BT_CONN_TYPE_LE, show_connection, (void *)sh);
    static const struct i2c_dt_spec bus = I2C_DT_SPEC_GET(DT_NODELABEL(tako_cirque));
    uint8_t command = 0xa4, mode = 0, rate = 0;
    int mode_rc = i2c_write_read_dt(&bus, &command, 1, &mode, 1);
    command = 0xa9;
    int rate_rc = i2c_write_read_dt(&bus, &command, 1, &rate, 1);
    shell_print(sh, "feed=0x%02x sample_rate_reg=%u read_rc=%d/%d", mode, rate, mode_rc, rate_rc);
    return 0;
}
SHELL_STATIC_SUBCMD_SET_CREATE(probe_commands,
    SHELL_CMD_ARG(show, NULL, "Read timing/queue/connection counters", probe_show, 1, 0),
    SHELL_CMD_ARG(reset, NULL, "Reset counters only", probe_reset, 1, 0),
    SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(cirque_probe, &probe_commands, "Cirque timing diagnostics", NULL);
