/* SPDX-License-Identifier: MIT */
#ifndef TAKO_CIRQUE_MOTION_PACKET_H
#define TAKO_CIRQUE_MOTION_PACKET_H

#include <stdint.h>
#include <string.h>

/* Private INPUT_EV_MSC code on Tako split input channel 0. Both endpoints
 * must support it. Preserve the full signed 16-bit range of each driver axis.
 */
#define TAKO_CIRQUE_MOTION_CODE 0x544b

static inline int32_t tako_cirque_pack_xy(int16_t x, int16_t y) {
    uint32_t bits = (uint32_t)(uint16_t)x | ((uint32_t)(uint16_t)y << 16);
    int32_t value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}
static inline int32_t tako_cirque_unpack_x(int32_t value) {
    uint32_t bits = (uint32_t)value & 0xffffU;
    return (int32_t)bits - ((bits & 0x8000U) ? 65536 : 0);
}
static inline int32_t tako_cirque_unpack_y(int32_t value) {
    uint32_t bits = (uint32_t)value >> 16;
    return (int32_t)bits - ((bits & 0x8000U) ? 65536 : 0);
}
#endif
