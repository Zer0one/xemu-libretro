#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Synthetic admission tests; no firmware, game or writable user media."""
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="xemu-media-tests-") as tmp:
    work = Path(tmp)
    exe = work / "probe"
    subprocess.run(shlex.split(os.environ.get("CC", "cc")) + [
        "-std=c11", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
        "-I" + str(root), str(root / "tests/libretro/test-media.c"),
        str(root / "ui/libretro-media.c"), "-o", str(exe)], check=True)
    cases = [(work / "missing", 1), (work, 1)]
    for name, data in (("empty", b""), ("unknown", b"unrecognized format"),
                       ("short-fatx", b"FATX")):
        path = work / name
        path.write_bytes(data)
        cases.append((path, 2))
    for spc, directory, size, expected in (
            (0, 1, 65536, 2), (3, 1, 65536, 2), (256, 1, 65536, 2),
            (8, 0, 65536, 2), (8, 1, 16, 2), (8, 1, 65536, 4)):
        path = work / f"fatx-{spc}-{directory}-{size}"
        with path.open("wb") as out:
            out.write(struct.pack("<4sIII", b"FATX", 1, spc, directory))
            out.truncate(size)
        cases.append((path, expected))
    for offset in (0x10000, 0x18310000):
        path = work / f"xbox-{offset}"
        with path.open("wb") as out:
            out.seek(offset)  # Sparse fixture, not a copied game image.
            out.write(b"MICROSOFT*XBOX*MEDIA")
        cases.append((path, 3))
    path = work / "chd-header"
    path.write_bytes(b"MComprHD" + b"\0" * 8)
    cases.append((path, 5))
    for name in ['disc.cue','disc.GDI']:
        path=work/name;path.write_text('descriptor admission; detailed validation is deferred')
        cases.append((path,6))
    for unit,skip in [(2048,0),(2352,16)]:
        path=work/f'optical-{unit}.bin'
        with path.open('wb') as f:
            f.seek(16*unit+skip);f.write(b'\1CD001\1');f.truncate(32*unit)
        cases.append((path,6))
    for path, expected in cases:
        subprocess.run([str(exe), str(path), str(expected)], check=True)
    print(f"{len(cases)} media fixtures passed; null/empty paths checked on every run")
