#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise the native CHD stream adapter with synthetic uncompressed CHD v5.

Requires the pinned libchdr static build with system zlib/zstd. No game media,
chdman, runtime downloads or intermediate converted images are used.
"""
import argparse
import ctypes as C
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument('--source', required=True, type=Path)
p.add_argument('--build', required=True, type=Path)
p.add_argument('--zstd-libs', default='-lzstd')
args = p.parse_args()
root = Path(__file__).resolve().parents[1]

with tempfile.TemporaryDirectory(prefix='xemu-chd-test-') as tmp:
    work = Path(tmp)
    library = work/'reader.so'
    lzma = list(args.build.glob('deps/lzma-*/libchdr-lzma.a'))
    assert len(lzma) == 1
    subprocess.run(shlex.split(os.environ.get('CC', 'cc')) + [
        '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
        '-I'+str(args.source/'include'), str(root/'ui/libretro-chihiro-chd.c'),
        str(args.build/'libchdr-static.a'), str(lzma[0]), '-lz',
        *shlex.split(args.zstd_libs), '-o', str(library)], check=True)
    lib = C.CDLL(str(library))
    lib.xemu_chihiro_chd_open.argtypes = [C.c_char_p]
    lib.xemu_chihiro_chd_open.restype = C.c_void_p
    lib.xemu_chihiro_chd_close.argtypes = [C.c_void_p]
    lib.xemu_chihiro_chd_size.argtypes = [C.c_void_p]
    lib.xemu_chihiro_chd_size.restype = C.c_uint64
    lib.xemu_chihiro_chd_unit_size.argtypes = [C.c_void_p]
    lib.xemu_chihiro_chd_unit_size.restype = C.c_uint32
    lib.xemu_chihiro_chd_read.argtypes = [C.c_void_p, C.c_uint64, C.c_void_p, C.c_size_t]
    lib.xemu_chihiro_chd_read.restype = C.c_bool
    lib.xemu_chihiro_chd_metadata.argtypes = [C.c_void_p, C.c_uint32, C.c_uint32,
                                            C.c_void_p, C.c_uint32]
    lib.xemu_chihiro_chd_metadata.restype = C.c_bool
    count = 0
    for unit in [2048, 2352, 2448]:
        hunk = unit * 2
        data = bytes(i % 251 for i in range(hunk*3))
        header = bytearray(124)
        header[:8] = b'MComprHD'
        struct.pack_into('>II', header, 8, 124, 5)
        struct.pack_into('>QQQII', header, 32, len(data), 124, 136, hunk, unit)
        tag = int.from_bytes(b'CHGD', 'big')
        metadata = b'TRACK:1 TYPE:MODE1_RAW SUBTYPE:NONE FRAMES:6\0'
        filedata = bytes(header)+struct.pack('>III', 1, 2, 3)
        filedata += struct.pack('>IIQ', tag, len(metadata), 0)+metadata
        filedata = filedata.ljust(hunk, b'\0')+data
        path = work/f'{unit}.chd'
        path.write_bytes(filedata)
        disc = lib.xemu_chihiro_chd_open(os.fsencode(path))
        assert disc
        try:
            assert lib.xemu_chihiro_chd_size(disc) == len(data)
            assert lib.xemu_chihiro_chd_unit_size(disc) == unit
            for capacity, index, expected in [(512, 0, True), (5, 0, False),
                                               (512, 1, False), (0, 0, False)]:
                out = C.create_string_buffer(512)
                assert lib.xemu_chihiro_chd_metadata(disc, tag, index, out,
                                                     capacity) == expected
                if expected:
                    assert out.value == metadata[:-1]
                count += 1
            for offset, size in [(0, 1), (hunk-7, 27), (0, len(data)),
                                 (len(data)-1, 1), (len(data), 0), (unit, unit)]:
                out = C.create_string_buffer(size)
                assert lib.xemu_chihiro_chd_read(disc, offset, out, size)
                assert out.raw == data[offset:offset+size]
                count += 1
            for offset, size in [(len(data), 1), (2**64-1, 8), (0, 2**63)]:
                out = C.create_string_buffer(8)
                assert not lib.xemu_chihiro_chd_read(disc, offset, out, size)
                count += 1
        finally:
            lib.xemu_chihiro_chd_close(disc)
        for _ in range(32):
            disc = lib.xemu_chihiro_chd_open(os.fsencode(path))
            assert disc, 'Repeated header/open failed'
            lib.xemu_chihiro_chd_close(disc)
            count += 1
        # Missing physical hunk must fail instead of returning a cached block.
        path.write_bytes(filedata[:-hunk])
        disc = lib.xemu_chihiro_chd_open(os.fsencode(path))
        assert disc
        try:
            out = C.create_string_buffer(hunk)
            assert not lib.xemu_chihiro_chd_read(disc, hunk*2, out, hunk)
            count += 1
        finally:
            lib.xemu_chihiro_chd_close(disc)
        struct.pack_into('>Q', header, 32, 17 << 30)
        path.write_bytes(header)
        assert not lib.xemu_chihiro_chd_open(os.fsencode(path))
        count += 1
    for value in [b'', b'not a CHD', bytes(123)]:
        path.write_bytes(value)
        assert not lib.xemu_chihiro_chd_open(os.fsencode(path))
        count += 1
    lib.xemu_chihiro_chd_close(None)
    print(f'{count} synthetic CHD stream/metadata/reopen cases passed')
