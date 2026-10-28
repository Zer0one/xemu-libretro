/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef XEMU_LIBRETRO_CHIHIRO_FIRMWARE_H
#define XEMU_LIBRETRO_CHIHIRO_FIRMWARE_H
#include <stdbool.h>
#include <stddef.h>
enum {
    XEMU_FW_BIOS, XEMU_FW_FLASH, XEMU_FW_IC10, XEMU_FW_IC11,
    XEMU_FW_PC20, XEMU_FW_NET, XEMU_FW_COUNT
};
typedef struct XemuChihiroFirmware {
    char path[XEMU_FW_COUNT][4096];
    char source[XEMU_FW_COUNT][4352];
} XemuChihiroFirmware;
/* Read-only source lookup. ZIP members are materialized as immutable, hashed
 * files under system/<profile>/firmware-cache for the unchanged machine code.
 * Missing BIOS is an error; ancillary firmware retains donor optionality. */
bool libretro_chihiro_firmware_resolve(const char *system, const char *profile,
    const char *content, const char *set, const char *parent,
    XemuChihiroFirmware *fw, char *error, size_t capacity);
#endif
