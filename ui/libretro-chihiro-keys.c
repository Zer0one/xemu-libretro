/* SPDX-License-Identifier: MIT
 * Copyright (c) 2017 Jannik Vogel (JayFoxRox)
 * Copyright (c) 2026 Réda Chérif-Touil (Tovarichtch)
 * Native adaptation: see licenses/chihiro-netboot.license.txt.
 */
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "libretro-chihiro-keys.h"
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "util/miniz/miniz.h"

bool xemu_chihiro_parse_key(const uint8_t *data, size_t size, bool pic,
                            XemuChihiroKey *key)
{
    static const size_t offsets[] = {
        0x6e0, 0x6bc, 0x6aa, 0x7de, 0x7be, 0x79e, 0x77e, 0x698, 0x686, 0x674,
    };
    uint8_t responses[80];
    XemuChihiroKey parsed = {0};
    if (!data || !key) return false;
    memset(key, 0, sizeof(*key));
    if (pic) {
        /* Match the reference's offsets for both 16 KiB and 16 KiB+16 dumps. */
        if (size != 0x4000 && size != 0x4010) return false;
        for (size_t i = 0; i < 10; i++)
            for (size_t j = 0; j < 8; j++)
                responses[i * 8 + j] = data[offsets[i] + j * 2];
        data = responses;
    } else if (size != 80) {
        return false;
    }
    memcpy(parsed.des, data + 49, 7);
    parsed.des[7] = data[41];
    unsigned nonzero = 0;
    for (size_t i = 0; i < 8; i++) nonzero |= parsed.des[i];
    if (!nonzero) return false;
    for (size_t i = 0; i < 7 && data[33 + i]; i++) {
        /* Loader names address ASCII ISO9660 entries. Reject invalid encoding
         * rather than converting it into an ambiguous replacement character. */
        if (data[33 + i] < 0x20 || data[33 + i] > 0x7e) return false;
        parsed.loader[i] = (char)data[33 + i];
    }
    if (!parsed.loader[0]) return false;
    *key = parsed;
    return true;
}

static bool extension(const char *path, const char *suffix)
{
    size_t n = strlen(path), m = strlen(suffix);
    if (n < m) return false;
    for (size_t i = 0; i < m; i++) {
        unsigned c = (unsigned char)path[n - m + i];
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if (c != (unsigned char)suffix[i]) return false;
    }
    return true;
}

static bool bios_pic(const char *path)
{
    const char *name = path;
    for (const char *p = path; *p; p++)
        if (*p == '/' || *p == '\\') name = p + 1;
    return !strcmp(name, "317-0352-exp.pic") ||
           !strcmp(name, "317-0422-jpn.pic") || !strcmp(name, "317-unknown.pic");
}

XemuChihiroKeyResult xemu_chihiro_visit_keys(const char *path,
    XemuChihiroKeyVisitor visitor, void *opaque)
{
    struct stat st;
    uint8_t data[0x4010];
    if (!path || !visitor) return XEMU_CHIHIRO_KEYS_INVALID;
    if (stat(path, &st) || !S_ISREG(st.st_mode)) return XEMU_CHIHIRO_KEYS_IO;
    if (!extension(path, ".zip")) {
        bool pic = extension(path, ".pic");
        if ((!pic && !extension(path, ".data")) ||
            (pic ? st.st_size != 0x4000 && st.st_size != 0x4010 : st.st_size != 80))
            return XEMU_CHIHIRO_KEYS_INVALID;
        FILE *f = fopen(path, "rb");
        if (!f) return XEMU_CHIHIRO_KEYS_IO;
        size_t size = fread(data, 1, sizeof(data), f);
        bool failed = ferror(f);
        fclose(f);
        if (failed || size != (size_t)st.st_size) return XEMU_CHIHIRO_KEYS_IO;
        XemuChihiroKey key;
        if (!xemu_chihiro_parse_key(data, size, pic, &key)) return XEMU_CHIHIRO_KEYS_INVALID;
        key.bios_fallback = pic && bios_pic(path);
        visitor(opaque, path, data, size, pic, &key);
        return XEMU_CHIHIRO_KEYS_OK;
    }
    /* MAME game ZIPs can include drive-board firmware (OutRun 2 exceeds
     * 8 MiB). Bound the container, but extract only tiny security members. */
    if (st.st_size > 64 * 1024 * 1024) return XEMU_CHIHIRO_KEYS_LIMIT;
    mz_zip_archive zip = {0};
    if (!mz_zip_reader_init_file(&zip, path, 0)) return XEMU_CHIHIRO_KEYS_INVALID;
    XemuChihiroKeyResult result = XEMU_CHIHIRO_KEYS_OK;
    size_t found = 0;
    mz_uint entries = mz_zip_reader_get_num_files(&zip);
    if (entries > 4096) { result = XEMU_CHIHIRO_KEYS_LIMIT; goto done; }
    /* Format priority does not depend on ZIP central-directory order. */
    for (unsigned format = 0; format < 2; format++)
    for (mz_uint i = 0; i < entries; i++) {
        mz_zip_archive_file_stat entry;
        if (!mz_zip_reader_file_stat(&zip, i, &entry)) {
            result = XEMU_CHIHIRO_KEYS_INVALID; goto done;
        }
        if (entry.m_is_directory) continue;
        bool pic = extension(entry.m_filename, ".pic");
        if (!pic && !extension(entry.m_filename, ".data")) continue;
        if (pic != (format == 1)) continue;
        if (pic ? entry.m_uncomp_size != 0x4000 && entry.m_uncomp_size != 0x4010 :
                  entry.m_uncomp_size != 80) continue;
        if (!mz_zip_reader_extract_to_mem(&zip, i, data, (size_t)entry.m_uncomp_size, 0)) {
            result = XEMU_CHIHIRO_KEYS_INVALID; goto done;
        }
        XemuChihiroKey key;
        if (!xemu_chihiro_parse_key(data, (size_t)entry.m_uncomp_size, pic, &key)) continue;
        key.bios_fallback = pic && bios_pic(entry.m_filename);
        found++;
        if (!visitor(opaque, entry.m_filename, data,
                     (size_t)entry.m_uncomp_size, pic, &key)) goto done;
    }
    if (!found) result = XEMU_CHIHIRO_KEYS_INVALID;
done:
    mz_zip_reader_end(&zip);
    return result;
}

typedef struct KeyArray {
    XemuChihiroKey *keys;
    size_t count, capacity;
    bool overflow;
} KeyArray;

static bool append_key(void *opaque, const char *name, const uint8_t *data,
                       size_t size, bool pic, const XemuChihiroKey *key)
{
    KeyArray *a = opaque;
    (void)name; (void)data; (void)size; (void)pic;
    if (a->count == a->capacity) { a->overflow = true; return false; }
    size_t pos = a->count++;
    if (!key->bios_fallback)
        while (pos && a->keys[pos - 1].bios_fallback) {
            a->keys[pos] = a->keys[pos - 1];
            pos--;
        }
    a->keys[pos] = *key;
    return true;
}

XemuChihiroKeyResult xemu_chihiro_read_keys(const char *path,
    XemuChihiroKey *keys, size_t capacity, size_t *count)
{
    if (!count) return XEMU_CHIHIRO_KEYS_INVALID;
    *count = 0;
    if (!keys || !capacity) return XEMU_CHIHIRO_KEYS_INVALID;
    KeyArray a = { .keys = keys, .capacity = capacity };
    XemuChihiroKeyResult result = xemu_chihiro_visit_keys(path, append_key, &a);
    if (a.overflow) result = XEMU_CHIHIRO_KEYS_LIMIT;
    if (result == XEMU_CHIHIRO_KEYS_OK) *count = a.count;
    return result;
}
