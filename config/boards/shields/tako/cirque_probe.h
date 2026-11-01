/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <stdint.h>
void tako_cirque_probe_irq(void);
void tako_cirque_probe_work(void);
void tako_cirque_probe_sample(int result, uint8_t z);
void tako_cirque_probe_report(int result, uint16_t code, int32_t value, bool sync);
uint16_t tako_cirque_probe_tap_limit(uint16_t default_limit);
