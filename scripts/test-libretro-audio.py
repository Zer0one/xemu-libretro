#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise the real audio callback's readiness, pause and per-load flush state."""
from pathlib import Path
import os, shlex, subprocess, tempfile
root = Path(__file__).resolve().parents[1]
s = (root / 'ui/libretro.c').read_text()
def function(signature):
    start = s.index(signature)
    return s[start:s.index('\n}', start) + 2]
code = r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#define qatomic_read(p) (*(p))
#define qatomic_load_acquire(p) (*(p))
#define qatomic_set(p, v) (*(p) = (v))
static bool exchange(bool *p, bool v) { bool old = *p; *p = v; return old; }
#define qatomic_xchg(p, v) exchange(p, v)
static bool audio_async_ready, audio_async_enabled, audio_async_needs_flush;
static unsigned sleeps, pulls, flushes, batches;
static int available = 512;
static void g_usleep(unsigned n) { assert(n == 1000 || n == 2000); sleeps++; }
void libretro_audio_flush(void) { flushes++; }
int libretro_audio_pull(int16_t *buf, int max_frames)
{
    pulls++; assert(max_frames == 1024);
    memset(buf, 0, (size_t)available * 2 * sizeof(*buf));
    return available;
}
static size_t batch(const int16_t *buf, size_t n)
{ assert(buf && n == 512); batches++; return n; }
static size_t (*audio_batch_cb)(const int16_t *, size_t) = batch;
''' + function('static void audio_thread_callback(') + '\n' + function('static void audio_thread_set_state(') + r'''
int main(void)
{
    audio_async_needs_flush = true;
    audio_thread_set_state(true);
    audio_thread_callback();
    assert(sleeps == 1 && !pulls && !flushes && !batches);
    audio_async_ready = true;
    audio_thread_callback(); audio_thread_callback();
    assert(flushes == 1 && pulls == 2 && batches == 2);
    audio_thread_set_state(false);
    audio_thread_callback();
    assert(flushes == 1 && pulls == 2 && batches == 2);
    audio_thread_set_state(true);
    audio_async_needs_flush = true; /* Next load must flush its own boot audio. */
    audio_thread_callback();
    assert(flushes == 2 && pulls == 3 && batches == 3);
    available = 0;
    audio_thread_callback();
    assert(pulls == 4 && batches == 3);
    audio_async_ready = false;
    audio_thread_callback();
    assert(pulls == 4 && batches == 3);
    audio_async_ready = true; audio_batch_cb = NULL;
    audio_thread_callback();
    assert(pulls == 4 && batches == 3);
    puts("Audio readiness, pause/resume, per-load flush, empty ring and close guards passed.");
}
'''
with tempfile.TemporaryDirectory(prefix='xemu-audio-') as tmp:
    p = Path(tmp); (p / 'test.c').write_text(code)
    subprocess.run(shlex.split(os.environ.get('CC', 'cc')) + ['-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', str(p / 'test.c'), '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True)
