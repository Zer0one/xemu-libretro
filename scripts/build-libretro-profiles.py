#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Build fixed profiles in separate directories with the usual configure args.

Example: python3 scripts/build-libretro-profiles.py -- --target-list=i386-softmmu
Dependencies and platform compiler flags are the same as for a normal build.
"""
import argparse
import json
from pathlib import Path
import shlex
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--profiles", nargs="+", choices=("xbox", "chihiro", "full"),
                    default=["xbox", "chihiro"])
parser.add_argument("--build-root", type=Path, default=Path("build/profiles"))
parser.add_argument("--jobs", type=int, default=6)
parser.add_argument("--dry-run", action="store_true")
parser.add_argument("configure_args", nargs=argparse.REMAINDER)
args = parser.parse_args()
if args.jobs < 1:
    parser.error("--jobs must be positive")
if len(args.profiles) != len(set(args.profiles)):
    parser.error("profiles must not be repeated")
source = Path(__file__).resolve().parents[1]
configure_args = args.configure_args
if configure_args[:1] == ["--"]:
    configure_args = configure_args[1:]
for profile in args.profiles:
    build = (args.build_root / profile).resolve()
    command = [str(source / "configure"), *configure_args,
               "-Dlibretro=true", f"-Dlibretro_system={profile}"]
    print(f"{profile}: cwd={build}\n{shlex.join(command)}", flush=True)
    if args.dry_run:
        continue
    build.mkdir(parents=True, exist_ok=True)
    subprocess.run(command, cwd=build, check=True)
    stem = "xemu_libretro" if profile == "xbox" else f"xemu_{profile}_libretro"
    targets = json.loads((build / "meson-info/intro-targets.json").read_text())
    target = next(t for t in targets if t["name"] == stem)
    library = Path(target["filename"][0])
    subprocess.run(["ninja", "-C", str(build), f"-j{args.jobs}", library.name], check=True)
    print(f"Built: {library}\nMetadata: {build / (stem + '.info')}", flush=True)
