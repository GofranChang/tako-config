/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdint.h>
#include "cirque_motion.h"

static void check_axis(int32_t delta, unsigned count, int64_t expected) {
    int32_t remainder = 0;
    int64_t sum = 0;
    for (unsigned i = 0; i < count; i++) {
        sum += tako_cirque_scale_delta(delta, 3, 10, &remainder);
    }
    assert(sum == expected);
    assert(remainder == 0);
}

int main(void) {
    /* Previously every +1/-1 sample was discarded at the 3/10 scale. */
    check_axis(1, 100, 30);
    check_axis(-1, 100, -30);
    check_axis(3, 100, 90);
    check_axis(-3, 100, -90);

    int32_t rx = 0, ry = 0;
    int64_t raw_x = 0, raw_y = 0, out_x = 0, out_y = 0;
    uint32_t seed = 1;
    /* Independent axes, direction changes and bounded fractional error. */
    for (unsigned i = 0; i < 10000; i++) {
        seed = seed * 1664525U + 1013904223U;
        int32_t dx = (int32_t)(seed % 101) - 50;
        seed = seed * 1664525U + 1013904223U;
        int32_t dy = (int32_t)(seed % 101) - 50;
        raw_x += dx;
        raw_y += dy;
        out_x += tako_cirque_scale_delta(dx, 3, 10, &rx);
        out_y += tako_cirque_scale_delta(dy, 3, 10, &ry);
        assert(raw_x * 3 == out_x * 10 + rx);
        assert(raw_y * 3 == out_y * 10 + ry);
        assert(rx > -10 && rx < 10 && ry > -10 && ry < 10);
    }
    /* A stationary finger never emits buffered movement. */
    for (unsigned i = 0; i < 100; i++) {
        assert(tako_cirque_scale_delta(0, 3, 10, &rx) == 0);
        assert(tako_cirque_scale_delta(0, 3, 10, &ry) == 0);
    }
    /* New contact / mode reset must not inherit a previous fraction. */
    rx = 0;
    assert(tako_cirque_scale_delta(3, 3, 10, &rx) == 0);
    assert(rx == 9);
    rx = 0;
    assert(tako_cirque_scale_delta(1, 3, 10, &rx) == 0);
    assert(rx == 3);
    return 0;
}
