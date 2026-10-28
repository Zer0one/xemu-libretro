/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef CHIHIRO_IMAGE_LAYOUT_H
#define CHIHIRO_IMAGE_LAYOUT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "chihiro-titles.h"

typedef bool (*ChihiroImageRead)(void *opaque, uint64_t offset, void *data, size_t size);
typedef struct ChihiroImageLayout {
    unsigned dimm_mib;
    char game_id[5];
    const ChihiroTitle *title;
} ChihiroImageLayout;

/* Match the donor's FATX mount geometry and boot.id location for each board
 * capacity. Accept exactly one valid layout; never infer capacity from length.
 * Works identically for decrypted CHD data and an ordinary FATX image. */
bool chihiro_image_layout(ChihiroImageRead read, void *opaque, uint64_t size,
                          ChihiroImageLayout *layout);
#endif
