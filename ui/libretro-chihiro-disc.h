/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef XEMU_LIBRETRO_CHIHIRO_DISC_H
#define XEMU_LIBRETRO_CHIHIRO_DISC_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Prepare/change/close only with the previous emulation thread stopped.
 * Reads during startup, reload and snapshots are serialized by QEMU's BQL. */
bool libretro_chihiro_disc_prepare(const char *path, const char *keys_dir,
                                   char *error, size_t capacity);
void libretro_chihiro_disc_close(void);
/* Owned by the active disc, valid until replacement/close. No secret bytes. */
const char *libretro_chihiro_disc_set(void);
const char *libretro_chihiro_disc_parent(void);
const char *libretro_chihiro_disc_key_source(void);
const char *libretro_chihiro_disc_warning(void);
/* -1 means ordinary file input, which keeps the donor's original path. */
int64_t libretro_chihiro_disc_size(const char *path);
/* FATX-confirmed DIMM for any CHD/FATX title; zero if absent or ambiguous. */
unsigned libretro_chihiro_disc_dimm_mib(const char *path);
bool libretro_chihiro_disc_read(const char *path, uint64_t offset,
                                void *buffer, size_t size);
#endif
