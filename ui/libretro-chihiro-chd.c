/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stdlib.h>
#include <string.h>
#include <libchdr/chd.h>
#include "libretro-chihiro-chd.h"

struct XemuChihiroChd {
    chd_file *file;
    FILE *stream;
    uint8_t *cache;
    uint64_t size;
    uint32_t hunk_size, unit_size, cached_hunk;
    bool cached;
};

XemuChihiroChd *xemu_chihiro_chd_open(const char *path)
{
    chd_header header;
    if (!path) return NULL;
    FILE *stream = fopen(path, "rb");
    if (!stream) return NULL;
    /* Reject implausible geometry before libchdr allocates the hunk map.
     * 16 GiB is deliberately above Chihiro GD-ROM capacity. Parent/delta
     * images are not resolved implicitly; libchdr rejects missing parents. */
    if (chd_read_header_file(stream, &header) != CHDERR_NONE ||
        !header.logicalbytes || header.logicalbytes > (UINT64_C(16) << 30) ||
        header.hunkbytes < 2048 || header.hunkbytes > 512 * 1024 ||
        (header.unitbytes != 2048 && header.unitbytes != 2352 &&
         header.unitbytes != 2448) || header.hunkbytes % header.unitbytes ||
        header.totalhunks != (header.logicalbytes + header.hunkbytes - 1) /
                             header.hunkbytes) {
        fclose(stream);
        return NULL;
    }
    XemuChihiroChd *disc = calloc(1, sizeof(*disc));
    if (!disc) {
        fclose(stream);
        return NULL;
    }
    disc->stream = stream;
    if (chd_open_file(stream, CHD_OPEN_READ, NULL, &disc->file) != CHDERR_NONE) {
        xemu_chihiro_chd_close(disc);
        return NULL;
    }
    /* Header and map are read through the same owned file handle. */
    const chd_header *opened = chd_get_header(disc->file);
    if (opened->logicalbytes != header.logicalbytes ||
        opened->hunkbytes != header.hunkbytes ||
        opened->unitbytes != header.unitbytes) {
        xemu_chihiro_chd_close(disc);
        return NULL;
    }
    disc->size = header.logicalbytes;
    disc->hunk_size = header.hunkbytes;
    disc->unit_size = header.unitbytes;
    disc->cache = malloc(disc->hunk_size);
    if (!disc->cache) {
        xemu_chihiro_chd_close(disc);
        return NULL;
    }
    return disc;
}

void xemu_chihiro_chd_close(XemuChihiroChd *disc)
{
    if (!disc) return;
    if (disc->file) chd_close(disc->file);
    if (disc->stream) fclose(disc->stream);
    free(disc->cache);
    free(disc);
}

uint64_t xemu_chihiro_chd_size(const XemuChihiroChd *disc)
{
    return disc ? disc->size : 0;
}

uint32_t xemu_chihiro_chd_unit_size(const XemuChihiroChd *disc)
{
    return disc ? disc->unit_size : 0;
}

bool xemu_chihiro_chd_sha1(const XemuChihiroChd *disc, uint8_t sha1[20])
{
    if (!disc || !sha1) return false;
    memcpy(sha1, chd_get_header(disc->file)->sha1, 20);
    return true;
}

bool xemu_chihiro_chd_metadata(XemuChihiroChd *disc, uint32_t tag,
                              uint32_t index, char *text, uint32_t capacity)
{
    uint32_t length = 0;
    if (!disc || !text || !capacity) return false;
    text[0] = 0;
    chd_error result = chd_get_metadata(disc->file, tag, index, text,
                                       capacity, &length, NULL, NULL);
    if (result != CHDERR_NONE || !length || length > capacity ||
        text[length - 1] != 0) {
        text[0] = 0;
        return false;
    }
    return true;
}

bool xemu_chihiro_chd_read(XemuChihiroChd *disc, uint64_t offset,
                          void *buffer, size_t size)
{
    if (!disc || (!buffer && size) || offset > disc->size ||
        size > disc->size - offset) return false;
    uint8_t *out = buffer;
    while (size) {
        uint32_t hunk = offset / disc->hunk_size;
        size_t within = offset % disc->hunk_size;
        size_t amount = disc->hunk_size - within;
        if (amount > size) amount = size;
        if (!disc->cached || disc->cached_hunk != hunk) {
            disc->cached = false;
            if (chd_read(disc->file, hunk, disc->cache) != CHDERR_NONE)
                return false;
            disc->cached_hunk = hunk;
            disc->cached = true;
        }
        memcpy(out, disc->cache + within, amount);
        out += amount;
        offset += amount;
        size -= amount;
    }
    return true;
}
