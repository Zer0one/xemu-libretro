/* SPDX-License-Identifier: MIT
 * Format adapted from Tovarichtch/chihiro-netboot; see licenses/chihiro-netboot.license.txt.
 */
#ifndef XEMU_LIBRETRO_CHIHIRO_KEYS_H
#define XEMU_LIBRETRO_CHIHIRO_KEYS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct XemuChihiroKey {
    uint8_t des[8];
    char loader[8];
    bool bios_fallback;
} XemuChihiroKey;

typedef enum XemuChihiroKeyResult {
    XEMU_CHIHIRO_KEYS_OK,
    XEMU_CHIHIRO_KEYS_IO,
    XEMU_CHIHIRO_KEYS_INVALID,
    XEMU_CHIHIRO_KEYS_LIMIT,
} XemuChihiroKeyResult;

/* The visitor receives bounded raw dump bytes, valid only during the call.
 * Return false to stop enumeration. No extraction to disk is performed. */
typedef bool (*XemuChihiroKeyVisitor)(void *opaque, const char *name,
    const uint8_t *data, size_t size, bool pic, const XemuChihiroKey *key);
XemuChihiroKeyResult xemu_chihiro_visit_keys(const char *path,
    XemuChihiroKeyVisitor visitor, void *opaque);

/* No filesystem access. No key bytes are logged by this module. */
bool xemu_chihiro_parse_key(const uint8_t *data, size_t size, bool pic,
                            XemuChihiroKey *key);
/* Read .data/.pic or ZIP directly, without extracting files. On failure count
 * is zero. Caller owns the bounded candidate array; game keys precede BIOS keys. */
XemuChihiroKeyResult xemu_chihiro_read_keys(const char *path,
    XemuChihiroKey *keys, size_t capacity, size_t *count);
#endif
