/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef XEMU_LIBRETRO_CHIHIRO_OPTICAL_H
#define XEMU_LIBRETRO_CHIHIRO_OPTICAL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct XemuChihiroOptical XemuChihiroOptical;
/* Selected MODE1 volume, independent of CHD/CUE/GDI/raw storage. Owned by
 * the loading thread, then used under the same BQL as the disc adapter. */
XemuChihiroOptical *xemu_chihiro_optical_open(const char *path, char *error, size_t capacity);
void xemu_chihiro_optical_close(XemuChihiroOptical *disc);
uint64_t xemu_chihiro_optical_size(const XemuChihiroOptical *disc);
uint32_t xemu_chihiro_optical_base(const XemuChihiroOptical *disc);
bool xemu_chihiro_optical_sha1(const XemuChihiroOptical *disc, uint8_t sha1[20]);
bool xemu_chihiro_optical_read(XemuChihiroOptical *disc, uint64_t offset, void *buffer, size_t size);
#endif
