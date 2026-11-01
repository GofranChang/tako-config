/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <limits.h>
#include "cirque_motion_packet.h"

static void check(int16_t x, int16_t y) {
    int32_t packed = tako_cirque_pack_xy(x, y);
    assert(tako_cirque_unpack_x(packed) == x);
    assert(tako_cirque_unpack_y(packed) == y);
}
int main(void) {
    const int16_t edges[] = {INT16_MIN, -32767, -256, -1, 0, 1, 255, 32766, INT16_MAX};
    for (unsigned i = 0; i < sizeof(edges) / sizeof(edges[0]); i++) {
        for (unsigned j = 0; j < sizeof(edges) / sizeof(edges[0]); j++) {
            check(edges[i], edges[j]);
        }
    }
    /* Exercise every possible axis value, including independent signs. */
    for (int32_t i = INT16_MIN; i <= INT16_MAX; i++) {
        check((int16_t)i, (int16_t)(-1 - i));
        check((int16_t)i, 0);
        check(0, (int16_t)i);
    }
    return 0;
}
