/* SPDX-License-Identifier: MIT
 * Optional on-screen probes. All counters are atomic; wrappers forward each
 * call exactly once and preserve its return value. No logging/USB interface.
 */
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/input/input.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/sys/atomic.h>
#include <lvgl.h>
#include <zmk/display.h>
#include <zmk/endpoints.h>
#include <zmk/usb.h>
#include <zmk/pointing/input_split.h>
#include <zmk/split/transport/central.h>

static atomic_t keys, motion, processed, queue_errors;
static atomic_t usb_done, usb_errors, usb_pending, usb_last_error;
static atomic_t connections, disconnects, disconnect_reason, heartbeat;

int __real_zmk_split_transport_central_peripheral_event_handler(
    const struct zmk_split_transport_central *transport, uint8_t source,
    struct zmk_split_transport_peripheral_event ev);
int __wrap_zmk_split_transport_central_peripheral_event_handler(
    const struct zmk_split_transport_central *transport, uint8_t source,
    struct zmk_split_transport_peripheral_event ev) {
    if (ev.type == ZMK_SPLIT_TRANSPORT_PERIPHERAL_EVENT_TYPE_KEY_POSITION_EVENT) {
        atomic_inc(&keys);
    } else if (ev.type == ZMK_SPLIT_TRANSPORT_PERIPHERAL_EVENT_TYPE_INPUT_EVENT) {
        atomic_inc(&motion);
    }
    return __real_zmk_split_transport_central_peripheral_event_handler(transport, source, ev);
}

int __real_zmk_input_split_report_peripheral_event(uint8_t reg, uint8_t type, uint16_t code,
                                                  int32_t value, bool sync);
int __wrap_zmk_input_split_report_peripheral_event(uint8_t reg, uint8_t type, uint16_t code,
                                                  int32_t value, bool sync) {
    int ret = __real_zmk_input_split_report_peripheral_event(reg, type, code, value, sync);
    if (ret < 0) {
        atomic_inc(&queue_errors);
    }
    return ret;
}

static void input_probe(struct input_event *evt) { atomic_inc(&processed); }
INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_NODELABEL(tako_cirque_split)), input_probe);

#define USB_PROBE(kind)                                                                            \
    int __real_zmk_usb_hid_send_##kind##_report(void);                                               \
    int __wrap_zmk_usb_hid_send_##kind##_report(void) {                                              \
        atomic_inc(&usb_pending);                                                                  \
        int ret = __real_zmk_usb_hid_send_##kind##_report();                                         \
        atomic_dec(&usb_pending);                                                                  \
        if (ret < 0) {                                                                             \
            atomic_inc(&usb_errors);                                                               \
            atomic_set(&usb_last_error, ret);                                                      \
        } else {                                                                                   \
            atomic_inc(&usb_done);                                                                 \
        }                                                                                          \
        return ret;                                                                                \
    }
USB_PROBE(keyboard)
USB_PROBE(consumer)
USB_PROBE(mouse)

static void connected(struct bt_conn *conn, uint8_t err) {
    if (!err) { atomic_inc(&connections); }
}
static void disconnected(struct bt_conn *conn, uint8_t reason) {
    atomic_dec(&connections);
    atomic_inc(&disconnects);
    atomic_set(&disconnect_reason, reason);
}
BT_CONN_CB_DEFINE(diagnostic_connections) = {
    .connected = connected,
    .disconnected = disconnected,
};

static void heartbeat_cb(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(heartbeat_work, heartbeat_cb);
static void heartbeat_cb(struct k_work *work) {
    atomic_inc(&heartbeat);
    k_work_reschedule(&heartbeat_work, K_SECONDS(1));
}

static lv_obj_t *label;
static unsigned count(atomic_t *value) { return (unsigned)atomic_get(value) % 100000; }
static void draw_diagnostics(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(diagnostic_draw_work, draw_diagnostics);
static void draw_diagnostics(struct k_work *work) {
    if (!label) {
        /* A top-layer label covers the usual widgets during diagnosis only. */
        label = lv_label_create(lv_layer_top());
        lv_obj_set_size(label, 128, 64);
        lv_obj_set_pos(label, 0, 0);
        lv_obj_set_style_text_font(label, &lv_font_unscii_8, 0);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_set_style_bg_color(label, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(label, LV_OPA_COVER, 0);
        lv_obj_set_style_text_line_space(label, 1, 0);
    }
    lv_label_set_text_fmt(label,
        "B%u D%u R%02X %s\nK%05u M%05u\nI%05u Q%05u\nU%05u E%05u\nP%u ERR%d\nW%05u T%05u\nUSB:%d DIAG1",
        count(&connections), count(&disconnects), (unsigned)atomic_get(&disconnect_reason),
        zmk_endpoints_selected().transport == ZMK_TRANSPORT_USB ? "USB" : "BLE",
        count(&keys), count(&motion), count(&processed), count(&queue_errors),
        count(&usb_done), count(&usb_errors), count(&usb_pending),
        (int)atomic_get(&usb_last_error), count(&heartbeat),
        (unsigned)(k_uptime_get() / 1000) % 100000, (int)zmk_usb_get_status());
    k_work_reschedule_for_queue(zmk_display_work_q(), &diagnostic_draw_work, K_MSEC(500));
}

static void start_draw(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(start_draw_work, start_draw);
static void start_draw(struct k_work *work) {
    if (!zmk_display_is_initialized()) {
        k_work_reschedule(&start_draw_work, K_MSEC(100));
        return;
    }
    k_work_reschedule_for_queue(zmk_display_work_q(), &diagnostic_draw_work, K_NO_WAIT);
}
static int diagnostics_init(void) {
    k_work_reschedule(&heartbeat_work, K_SECONDS(1));
    k_work_reschedule(&start_draw_work, K_MSEC(100));
    return 0;
}
SYS_INIT(diagnostics_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
