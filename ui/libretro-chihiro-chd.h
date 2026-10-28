/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef XEMU_LIBRETRO_CHIHIRO_CHD_H
#define XEMU_LIBRETRO_CHIHIRO_CHD_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct XemuChihiroChd XemuChihiroChd;
/* Read-only logical CHD stream, before GD-ROM track/sector interpretation.
 * Each instance owns one hunk cache; use from one loading thread only. */
XemuChihiroChd *xemu_chihiro_chd_open(const char *path);
void xemu_chihiro_chd_close(XemuChihiroChd *disc);
uint64_t xemu_chihiro_chd_size(const XemuChihiroChd *disc);
uint32_t xemu_chihiro_chd_unit_size(const XemuChihiroChd *disc);
/* Declared combined SHA-1 from the CHD header, not a full content audit. */
bool xemu_chihiro_chd_sha1(const XemuChihiroChd *disc, uint8_t sha1[20]);
/* Text track metadata only; reject missing, truncated or unterminated entries. */
bool xemu_chihiro_chd_metadata(XemuChihiroChd *disc, uint32_t tag,
                              uint32_t index, char *text, uint32_t capacity);
bool xemu_chihiro_chd_read(XemuChihiroChd *disc, uint64_t offset,
                          void *buffer, size_t size);
#endif
