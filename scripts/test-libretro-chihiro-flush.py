#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Compile the actual persistence entrypoints with instrumented device stubs.

This checks call ordering and separation from teardown, not real save files.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'hw/xbox/chihiro/chihiro.c').read_text()

def function(signature):
    start = source.index(signature)
    end = source.index('\n}', start) + 2
    return source[start:end]

prefix = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
typedef struct Notifier { int unused; } Notifier;
static bool chihiro_active, resolve_ok, usb_ok;
static char chihiro_save_path[64], events[16];
static unsigned calls, resolves;
static bool chihiro_resolve_save_path(void) {
    resolves++;
    if (resolve_ok) strcpy(chihiro_save_path, "synthetic.sav");
    return resolve_ok;
}
static bool chihiro_usb_save_flush(const char *path) {
    assert(path[0]); events[calls++] = 'S'; return usb_ok;
}
static bool chihiro_dimm_sys_flush(void) { events[calls++] = 'D'; return true; }
static void chihiro_netboard_exit(void) { events[calls++] = 'X'; }
'''
suffix = r'''
int main(void) {
    chihiro_flush_persistent_data(); assert(calls == 0);
    chihiro_active = true;
    chihiro_flush_persistent_data(); assert(calls == 0 && resolves == 1);
    resolve_ok = usb_ok = true;
    chihiro_flush_persistent_data(); assert(calls == 2 && !memcmp(events, "SD", 2));
    calls = 0;
    chihiro_flush_persistent_data(); assert(calls == 2 && resolves == 2);
    calls = 0; usb_ok = false;
    chihiro_flush_persistent_data(); assert(calls == 2 && !memcmp(events, "SD", 2));
    calls = 0;
    chihiro_flush_save_now(); assert(calls == 3 && !memcmp(events, "XSD", 3));
    puts("6 persistence cases passed; content flush never tears down the netboard");
}
'''
code = prefix + '\n'.join(function(s) for s in (
    'void chihiro_flush_persistent_data(void)',
    'static void chihiro_exit_notify(Notifier *notifier, void *data)',
    'void chihiro_flush_save_now(void)')) + suffix
with tempfile.TemporaryDirectory(prefix='xemu-flush-test-') as tmp:
    work = Path(tmp); (work/'test.c').write_text(code)
    subprocess.run(shlex.split(os.environ.get('CC', 'cc')) + [
        '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
        str(work/'test.c'), '-o', str(work/'test')], check=True)
    subprocess.run([str(work/'test')], check=True)
