#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Verify automatic HDD creation never replaces an existing disk."""
from pathlib import Path
import os, shlex, subprocess, tempfile
root = Path(__file__).resolve().parents[1]
s = (root / 'ui/libretro.c').read_text()
start = s.index('static bool write_bundled_hdd(')
function = s[start:s.index('\n}', start) + 2]
source = r'''
#include <glib.h>
#include <glib/gstdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#define O_BINARY 0
#define LRLOG_ERROR(...) ((void)0)
#define LRLOG_INFO(...) ((void)0)
static const uint8_t xbox_hdd_data[] = { 'Q', 'F', 'I', 0xfb, 1, 2, 3, 4 };
static const size_t xbox_hdd_size = sizeof(xbox_hdd_data);
static bool fail_write;
static ssize_t qemu_write_full(int fd, const void *data, size_t n)
{
    if (fail_write) { write(fd, data, 2); return -1; }
    return write(fd, data, n);
}
''' + function + r'''
int main(int argc, char **argv)
{
    assert(argc == 2);
    char *path = g_build_filename(argv[1], "profile", "xbox_hdd.qcow2", NULL);
    assert(write_bundled_hdd(path));
    gchar *bytes = NULL; gsize size = 0;
    assert(g_file_get_contents(path, &bytes, &size, NULL));
    assert(size == xbox_hdd_size && !memcmp(bytes, xbox_hdd_data, size));
    g_free(bytes);
    /* A race, unreadable existing disk or explicit path must not replace data. */
    assert(g_file_set_contents(path, "existing-user-hdd", -1, NULL));
    assert(!write_bundled_hdd(path));
    assert(g_file_get_contents(path, &bytes, &size, NULL));
    assert(size == 17 && !memcmp(bytes, "existing-user-hdd", size));
    g_free(bytes);
    assert(g_remove(path) == 0);
    fail_write = true;
    assert(!write_bundled_hdd(path));
    assert(!g_file_test(path, G_FILE_TEST_EXISTS));
    fail_write = false;
    assert(write_bundled_hdd(path));
    g_free(path);
    puts("HDD creation, existing-disk preservation, partial-write cleanup and retry passed.");
}
'''
with tempfile.TemporaryDirectory(prefix='xemu-hdd-') as tmp:
    work = Path(tmp); (work / 'test.c').write_text(source)
    flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'glib-2.0'], text=True))
    subprocess.run(shlex.split(os.environ.get('CC', 'cc')) + ['-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', str(work / 'test.c'), '-o', str(work / 'test')] + flags, check=True)
    subprocess.run([str(work / 'test'), str(work)], check=True)
