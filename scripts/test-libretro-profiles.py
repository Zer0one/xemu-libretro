#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Run profile option contract tests without building or starting QEMU."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
compiler = shlex.split(os.environ.get("CC", "cc"))
with tempfile.TemporaryDirectory(prefix="xemu-profile-tests-") as tmp:
    work = Path(tmp)
    for number, profile in enumerate(("xbox", "chihiro", "full"), 1):
        (work / "libretro-profile-config.h").write_text(
            f'#define XEMU_LIBRETRO_PROFILE {number}\n'
            f'#define XEMU_LIBRETRO_PROFILE_NAME "{profile}"\n'
        )
        for translated in (True, False):
            exe = work / "test-profiles"
            command = compiler + ["-std=c11", "-Wall", "-Wextra", "-Werror",
                "-fsanitize=address,undefined", "-g", "-I" + str(work),
                "-I" + str(root), str(root / "tests/libretro/test-profiles.c"),
                "-o", str(exe)]
            if not translated:
                command.append("-DHAVE_NO_LANGEXTRA")
            subprocess.run(command, check=True)
            print(f"Translations: {translated}", flush=True)
            subprocess.run([str(exe)], check=True)
