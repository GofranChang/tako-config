/* SPDX-License-Identifier: MIT
 * Force-included ONLY for the pinned upstream input_pinnacle.c. Include the
 * kernel first so its own inline helpers keep their original definitions.
 */
#pragma once
#include <zephyr/kernel.h>

extern struct k_work_q tako_cirque_work_q;

/* Pinnacle's GPIO callback must not queue its blocking input producer on the
 * system queue: Bluetooth TX completion needs that queue to drain consumers.
 */
#define k_work_submit(work) k_work_submit_to_queue(&tako_cirque_work_q, (work))
