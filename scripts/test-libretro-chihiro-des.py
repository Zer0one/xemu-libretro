#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check the existing Sega DES implementation against macOS CommonCrypto.

Synthetic inputs only. Compile the actual device source, replacing only its
QEMU portability include. This does not change the device or install anything.
"""
import ctypes as C
import os
from pathlib import Path
import random
import shlex
import subprocess
import sys
import tempfile

if sys.platform != 'darwin':
    raise SystemExit('This independent oracle test requires macOS CommonCrypto')

root = Path(__file__).resolve().parents[1]
crypto = C.CDLL('/usr/lib/libSystem.B.dylib')
crypto.CCCrypt.restype = C.c_int
crypto.CCCrypt.argtypes = [C.c_ulong, C.c_ulong, C.c_ulong, C.c_void_p,
    C.c_size_t, C.c_void_p, C.c_void_p, C.c_size_t, C.c_void_p,
    C.c_size_t, C.POINTER(C.c_size_t)]

def oracle(data, key):
    out = C.create_string_buffer(len(data))
    moved = C.c_size_t()
    assert crypto.CCCrypt(1, 1, 2, key, 8, None, data, len(data), out,
                          len(data), C.byref(moved)) == 0
    assert moved.value == len(data)
    return out.raw

def swap_blocks(data):
    return b''.join(data[i:i+8][::-1] for i in range(0, len(data), 8))

with tempfile.TemporaryDirectory(prefix='xemu-des-test-') as tmp:
    work = Path(tmp)
    (work/'qemu').mkdir()
    (work/'qemu/osdep.h').write_text('#include <stdint.h>\n#include <stdbool.h>\n')
    libpath = work/'des.so'
    subprocess.run(shlex.split(os.environ.get('CC', 'cc')) + [
        '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
        '-I'+str(work), str(root/'hw/xbox/chihiro/chihiro-des.c'),
        '-o', str(libpath)], check=True)
    lib = C.CDLL(str(libpath))
    decrypt = lib.chihiro_des_decrypt_buffer
    decrypt.argtypes = [C.c_void_p, C.c_uint32, C.c_uint64]
    decrypt.restype = None
    # Check the independent oracle against the standard DES known-answer vector.
    assert oracle(bytes.fromhex('85e813540f0ab405'),
                  bytes.fromhex('133457799bbcdff1')) == bytes.fromhex('0123456789abcdef')
    rng = random.Random(0x444553)
    for size in [8, 16, 2048, 2352] * 64:
        data, key = rng.randbytes(size), rng.randbytes(8)
        expected = swap_blocks(oracle(swap_blocks(data), key[::-1]))
        buf = C.create_string_buffer(data, size)
        # PIC/.data bytes form the same little-endian key used by the DIMM.
        decrypt(buf, size, int.from_bytes(key, 'little'))
        assert buf.raw == expected, size
    print('DES known-answer vector and 256 Sega byte-order comparisons passed')
