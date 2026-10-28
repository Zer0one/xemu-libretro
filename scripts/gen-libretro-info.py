#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Derive profile metadata from the donor's combined info file."""
import argparse
from pathlib import Path
import re

parser = argparse.ArgumentParser()
parser.add_argument("profile", choices=("xbox", "chihiro", "full"))
parser.add_argument("name")
parser.add_argument("extensions")
parser.add_argument("source", type=Path)
args = parser.parse_args()
info = {}
for line in args.source.read_text().splitlines():
    if "=" in line and not line.lstrip().startswith("#"):
        key, value = line.split("=", 1)
        info[key.strip()] = value.strip().strip('"')
firmware = [{field: info[f"firmware{i}_{field}"] for field in ("desc", "path", "opt")}
            for i in range(int(info["firmware_count"]))]
info = {k: v for k, v in info.items() if not re.match(r"firmware\d+_", k)}
root = {"xbox": "xemu", "chihiro": "xemu-chihiro", "full": "xemu-full"}[args.profile]
display_name = "Sega - Chihiro" if args.profile == "chihiro" else args.name
info.update(display_name=display_name, corename=args.name,
            supported_extensions=args.extensions, supports_no_game="true")
if args.profile == "xbox":
    firmware = firmware[:4]
elif args.profile == "chihiro":
    firmware = firmware[4:]
    for entry in firmware:
        for field in ("path", "desc"):
            entry[field] = entry[field].replace("xemu/Chihiro/", root + "/")
        # Static frontend metadata cannot express ZIP/folder alternatives.
        # Let the core validate required BIOS at load instead of rejecting a
        # valid archive-only setup because the listed loose path is absent.
        entry["opt"] = "true"
    info.update(manufacturer="Sega", systemname="Chihiro", systemid="chihiro", database="")
else:
    for entry in firmware:
        for field in ("path", "desc"):
            entry[field] = entry[field].replace("xemu/", root + "/")
        entry["opt"] = "true"  # Requirements depend on the selected machine.
    info.update(manufacturer="Microsoft|Sega", systemname="Xbox|Chihiro", systemid="", database="Microsoft - Xbox")
info["firmware_count"] = str(len(firmware))
for i, entry in enumerate(firmware):
    info.update({f"firmware{i}_{key}": value for key, value in entry.items()})
info["notes"] = (f"Writable data lives under system/{root}/.|"
                 "Use the matching library and info file.|Save states are not supported.")
if args.profile != "xbox":
    info["notes"] += ("|Chihiro firmware is resolved by the core: loose files, associated game sets, or chihiro/seganetw ZIPs and folders."
                      "|System profile roots, their Chihiro subdirectory and system/xemu/Chihiro/ are supported."
                      "|Listed loose firmware paths are alternatives; a Chihiro BIOS is still required.")
info["description"] = f"{display_name}: a Libretro core built from the common xemu source base."
print("# Generated from ui/xemu_libretro.info; do not edit this build output.")
for key, value in info.items():
    print(f'{key} = "{value}"')
