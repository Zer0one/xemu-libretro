/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef XEMU_LIBRETRO_CHIHIRO_KEY_SEARCH_H
#define XEMU_LIBRETRO_CHIHIRO_KEY_SEARCH_H
#include "libretro-chihiro-keys.h"

typedef struct XemuChihiroMedia {
    const char *disc, *disc_sha1, *set, *parent, *key, *key_sha1;
} XemuChihiroMedia;

typedef struct XemuChihiroKeysFound {
    XemuChihiroKey keys[16];
    bool verified[16];
    char source[16][4352];
    char sha1[16][41];
    size_t count;
} XemuChihiroKeysFound;

/* CHD declared SHA-1 first, then basename. Unknown media returns NULL. */
const XemuChihiroMedia *xemu_chihiro_media_lookup(const char *path,
                                                const uint8_t sha1[20]);
/* MAME-only lookup; a missing catalogue association returns false.
 * Search beside content, beside its directory, then in the profile keys root.
 * A full hash pass precedes the name fallback. No recursion or host config. */
bool xemu_chihiro_find_keys(const char *path, const char *keys_dir,
    const XemuChihiroMedia *media, XemuChihiroKeysFound *found);
/* A candidate is accepted only after the caller's content validation. This
 * streams every associated dump, avoiding a fixed candidate-array cutoff. */
typedef bool (*XemuChihiroKeyCheck)(void *opaque, const XemuChihiroKey *key,
    const char *source, const char *sha1, bool verified);
bool xemu_chihiro_try_image_keys(const char *path, const char *keys_dir,
    const XemuChihiroMedia *media, XemuChihiroKeyCheck check, void *opaque);
#endif
