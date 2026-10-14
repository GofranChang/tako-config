/*
 * SPDX-License-Identifier: MIT
 * Tako display policy for ZMK v0.3. Blanking runs on the display queue.
 * The upstream idle listener is disabled to give this policy sole ownership.
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/led.h>
#include <zmk/activity.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/usb.h>

BUILD_ASSERT(!IS_ENABLED(CONFIG_ZMK_DISPLAY_BLANK_ON_IDLE),
             "Tako USB display policy replaces the upstream idle listener");

/* Global timer defined by ZMK v0.3 app/src/display/main.c. */
extern struct k_timer display_timer;
static const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

#if DT_HAS_CHOSEN(zmk_display_led)
static const struct device *display_led = DEVICE_DT_GET(DT_PARENT(DT_CHOSEN(zmk_display_led)));
static const uint8_t display_led_idx = DT_NODE_CHILD_IDX(DT_CHOSEN(zmk_display_led));
#endif

static void apply_display_policy(struct k_work *work) {
    ARG_UNUSED(work);
    if (!zmk_display_is_initialized() || !device_is_ready(display)) {
        return;
    }
    enum zmk_activity_state activity = zmk_activity_get_state();
    bool awake = zmk_usb_is_powered() || activity == ZMK_ACTIVITY_ACTIVE;
#if IS_ENABLED(CONFIG_NICE_VIEW_WIDGET_STATUS) && IS_ENABLED(CONFIG_ZMK_SPLIT) && \
    !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    /* Do not pause the 20-second slideshow after the 5-second idle timeout. */
    awake = awake || activity != ZMK_ACTIVITY_SLEEP;
#endif
    if (awake) {
#if DT_HAS_CHOSEN(zmk_display_led)
        led_on(display_led, display_led_idx);
#endif
        display_blanking_off(display);
        k_timer_start(&display_timer, K_MSEC(CONFIG_ZMK_DISPLAY_TICK_PERIOD_MS),
                      K_MSEC(CONFIG_ZMK_DISPLAY_TICK_PERIOD_MS));
    } else {
        k_timer_stop(&display_timer);
        display_blanking_on(display);
#if DT_HAS_CHOSEN(zmk_display_led)
        led_off(display_led, display_led_idx);
#endif
    }
}
K_WORK_DEFINE(display_policy_work, apply_display_policy);

/* USB events may arrive before the dedicated display queue has started. */
static void request_display_policy(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(display_policy_request, request_display_policy);
static void request_display_policy(struct k_work *work) {
    ARG_UNUSED(work);
    if (!zmk_display_is_initialized()) {
        k_work_reschedule(&display_policy_request, K_MSEC(100));
        return;
    }
    k_work_submit_to_queue(zmk_display_work_q(), &display_policy_work);
}

static int display_policy_listener(const zmk_event_t *eh) {
    ARG_UNUSED(eh);
    k_work_reschedule(&display_policy_request, K_NO_WAIT);
    return ZMK_EV_EVENT_BUBBLE;
}
ZMK_LISTENER(tako_usb_display, display_policy_listener);
ZMK_SUBSCRIPTION(tako_usb_display, zmk_activity_state_changed);
ZMK_SUBSCRIPTION(tako_usb_display, zmk_usb_conn_state_changed);
