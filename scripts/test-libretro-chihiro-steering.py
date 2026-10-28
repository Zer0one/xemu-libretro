#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise the donor radial filter together with the real Chihiro JVS mapper."""
from pathlib import Path
import os, shlex, subprocess, tempfile
root = Path(__file__).resolve().parents[1]
mapper = (root / 'ui/libretro-chihiro.c').read_text()
stubs = (root / 'ui/libretro-stubs.c').read_text()
def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "ui/libretro.h"
static int16_t raw_x, raw_y;
static bool left, right;
int libretro_deadzone_percent[4] = {10, 10, 0, 0};
static int16_t input_state_cb(unsigned p, unsigned d, unsigned i, unsigned a)
{ (void)p; (void)d; (void)i; return a == RETRO_DEVICE_ID_ANALOG_X ? raw_x : raw_y; }
static bool pad(unsigned p, unsigned id)
{ (void)p; return id == RETRO_DEVICE_ID_JOYPAD_LEFT ? left : right; }
''' + function(stubs, 'void libretro_input_stick(unsigned port, unsigned index, int16_t *x, int16_t *y)\n{') + '\n' + function(mapper, 'static int16_t stick(') + '\n' + function(mapper, 'static uint16_t axis_smooth(') + '\n' + function(mapper, 'static uint16_t steering(') + r'''
int main(void)
{
    const int zones[] = {0, 5, 10, 15, 20, 25, 30};
    unsigned checked = 0;
    for (unsigned d = 0; d < sizeof(zones) / sizeof(zones[0]); d++) {
        libretro_deadzone_percent[0] = zones[d];
        for (int value = INT16_MIN; value <= INT16_MAX; value++) {
            raw_x = value;
            for (unsigned diagonal = 0; diagonal < 2; diagonal++) {
                raw_y = diagonal ? value : 0;
                double length = hypot((double)raw_x, (double)raw_y);
                double dead = 32767.0 * zones[d] / 100.0;
                double expected = zones[d] == 0 ? raw_x : length <= dead ? 0 :
                    raw_x * (length - dead) / (32767.0 - dead) * 32767.0 / length;
                if (expected > 32767) expected = 32767;
                if (expected < -32768) expected = -32768;
                int16_t x, y;
                libretro_input_stick(0, 0, &x, &y);
                assert(abs(x - (int)expected) <= 1);
                if (diagonal) assert(x == y);
                for (unsigned invert = 0; invert < 2; invert++) {
                    int travel = invert ? x == INT16_MIN ? INT16_MAX : -x : x;
                    uint16_t ramp = 0;
                    assert(steering(0, &ramp, invert) == (uint16_t)(0x8000 + travel));
                    assert(ramp == 0x8000);
                    checked++;
                }
            }
        }
    }
    libretro_deadzone_percent[0] = 10;
    raw_x = 2000; raw_y = 0;
    uint16_t ramp = 0x8000;
    assert(steering(0, &ramp, false) == 0x8000); /* Original centering repro. */
    for (unsigned invert = 0; invert < 2; invert++) {
        left = true; right = false; ramp = 0x8000;
        for (unsigned i = 0; i < 8; i++) steering(0, &ramp, invert);
        assert(ramp == (invert ? 0xffff : 0));
        left = false; right = true; ramp = 0x8000;
        for (unsigned i = 0; i < 8; i++) steering(0, &ramp, invert);
        assert(ramp == (invert ? 0 : 0xffff));
        left = right = true; ramp = 0x8000;
        assert(steering(0, &ramp, invert) == 0x8000);
        left = right = false;
    }
    puts("Donor radial filtering, JVS centering, signed endpoints and digital steering passed.");
    printf("%u mappings: all signed axis values, axial/diagonal, seven settings, both orientations.\n", checked);
}
'''
with tempfile.TemporaryDirectory(prefix='chihiro-steering-') as tmp:
    p = Path(tmp); (p / 'test.c').write_text(code)
    subprocess.run(shlex.split(os.environ.get('CC', 'cc')) + ['-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', '-I' + str(root), str(p / 'test.c'), '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True)
