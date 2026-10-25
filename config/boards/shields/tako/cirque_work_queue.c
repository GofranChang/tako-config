/* SPDX-License-Identifier: MIT */
#include <zephyr/init.h>
#include <zephyr/kernel.h>

K_THREAD_STACK_DEFINE(tako_cirque_work_stack, 2048);
struct k_work_q tako_cirque_work_q;

static int tako_cirque_queue_init(void) {
    static const struct k_work_queue_config config = {.name = "Tako Cirque"};
    k_work_queue_start(&tako_cirque_work_q, tako_cirque_work_stack,
                      K_THREAD_STACK_SIZEOF(tako_cirque_work_stack), 5, &config);
    return 0;
}
/* Start before Pinnacle enables its DR interrupt (POST_KERNEL, priority 90). */
SYS_INIT(tako_cirque_queue_init, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
BUILD_ASSERT(CONFIG_KERNEL_INIT_PRIORITY_DEFAULT < CONFIG_INPUT_PINNACLE_INIT_PRIORITY,
             "Cirque work queue must start before the input driver");
