/* SPDX-License-Identifier: MIT */
#ifndef TAKO_CIRQUE_MOTION_H
#define TAKO_CIRQUE_MOTION_H

#include <stdint.h>

/* Preserve fractional movement per axis, without filtering or buffering frames.
 * The caller resets each remainder on lift, new contact and data-mode changes.
 * divisor is nonzero, as required by the pinned driver's devicetree binding.
 */
static inline int64_t tako_cirque_scale_delta(int32_t delta, uint16_t multiplier,
                                              uint16_t divisor, int32_t *remainder) {
    int64_t numerator = (int64_t)delta * multiplier + *remainder;
    *remainder = numerator % divisor;
    return numerator / divisor;
}

#endif
