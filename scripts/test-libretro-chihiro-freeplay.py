#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Check the actual EEPROM override with a retained machine and changed settings."""
from pathlib import Path
import os, shlex, subprocess, tempfile
root = Path(__file__).resolve().parents[1]
source = (root / 'hw/xbox/chihiro/chihiro-usb.c').read_text()
start = source.index('static void chihiro_apply_freeplay(')
function = source[start:source.index('\n}', start) + 2]
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct { uint8_t ic11[512]; } ChihiroUSBState;
static struct { struct { struct { bool freeplay; } settings; } chihiro; } g_config;
''' + ('static bool chihiro_freeplay_setting = true;\n' if 'chihiro_freeplay_setting' in function else '') + function + r'''
static void check(const ChihiroUSBState *s, bool enabled)
{
    for (int base = 0; base <= 0x40; base += 0x40) {
        assert(s->ic11[base + 0x22] == (enabled ? 10 : 1));
        assert(s->ic11[base + 0x23] == enabled);
        unsigned sum = 0;
        for (int i = 0; i < 52; i++) sum += s->ic11[base + 0x0c + i];
        assert(s->ic11[base + 0x0a] == (sum & 255));
        assert(s->ic11[base + 0x0b] == ((sum >> 8) & 255));
    }
}
int main(void)
{
    ChihiroUSBState s = {0};
    /* Preserve non-ACBU firmware and unrelated EEPROM data. */
    ChihiroUSBState before = s;
    g_config.chihiro.settings.freeplay = true;
    chihiro_apply_freeplay(&s);
    assert(!memcmp(&s, &before, sizeof(s)));
    for (int base = 0; base <= 0x40; base += 0x40) {
        memcpy(s.ic11 + base, "ACBU0001", 8);
        s.ic11[base + 8] = 13;
        memcpy(s.ic11 + base + 0x14, "SBJE", 4);
        s.ic11[base + 0x1c] = 0x63;
        s.ic11[base + 0x22] = 10;
        s.ic11[base + 0x23] = 1; /* Existing user save is Free Play. */
    }
    memset(s.ic11 + 128, 0x5a, 384);
    /* First load uses enabled; subsequent loads change config without
     * constructing a new machine. Original startup latch stays true. */
    chihiro_apply_freeplay(&s); check(&s, true);
    before = s;
    g_config.chihiro.settings.freeplay = false;
    chihiro_apply_freeplay(&s); check(&s, false);
    for (size_t i = 0; i < sizeof(s.ic11); i++) {
        bool changed = i == 0x22 || i == 0x23 || i == 0x0a || i == 0x0b ||
                       i == 0x62 || i == 0x63 || i == 0x4a || i == 0x4b;
        if (!changed) assert(s.ic11[i] == before.ic11[i]);
    }
    g_config.chihiro.settings.freeplay = true;
    chihiro_apply_freeplay(&s); check(&s, true);
    g_config.chihiro.settings.freeplay = false;
    chihiro_apply_freeplay(&s); check(&s, false);
    before = s; chihiro_apply_freeplay(&s);
    assert(!memcmp(&s, &before, sizeof(s)));
    puts("Retained-machine enabled/disabled/enabled/disabled, both slots, checksums and unrelated data passed.");
}
'''
with tempfile.TemporaryDirectory(prefix='xemu-freeplay-') as tmp:
    p = Path(tmp); (p / 'test.c').write_text(code)
    subprocess.run(shlex.split(os.environ.get('CC', 'cc')) + ['-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', str(p / 'test.c'), '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True)
