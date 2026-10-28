/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Frontend admission only: leave guest filesystem/boot parsing to the donor. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "libretro-media.h"

static uint32_t read_le32(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

XemuLibretroMedia xemu_libretro_probe_media(const char *path)
{
    static const char magic[] = "MICROSOFT*XBOX*MEDIA";
    static const long partitions[] = { 0, 0x18300000 };
    struct stat st;
    if (!path || !path[0]) {
        return XEMU_MEDIA_NONE;
    }
    if (stat(path, &st) || !S_ISREG(st.st_mode)) {
        return XEMU_MEDIA_UNREADABLE;
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        return XEMU_MEDIA_UNREADABLE;
    }
    const char *ext = strrchr(path, '.');
    char suffix[5] = {0};
    if (ext && strlen(ext) == 4)
        for (unsigned i = 0; i < 4; i++)
            suffix[i] = ext[i] >= 'A' && ext[i] <= 'Z' ? ext[i] + ('a'-'A') : ext[i];
    if (!strcmp(suffix, ".cue") || !strcmp(suffix, ".gdi")) {
        fclose(f); return XEMU_MEDIA_OPTICAL_CANDIDATE;
    }
    XemuLibretroMedia result = XEMU_MEDIA_UNKNOWN;
    for (unsigned i = 0; i < sizeof(partitions) / sizeof(partitions[0]); i++) {
        char buf[sizeof(magic) - 1];
        if (fseek(f, partitions[i] + 0x10000, SEEK_SET) == 0 &&
            fread(buf, 1, sizeof(buf), f) == sizeof(buf) &&
            !memcmp(buf, magic, sizeof(buf))) {
            result = XEMU_MEDIA_XBOX_DISC;
            goto out;
        }
    }
    unsigned char hdr[16];
    if (fseek(f, 0, SEEK_SET) == 0 &&
        fread(hdr, 1, sizeof(hdr), f) == sizeof(hdr)) {
      if (!memcmp(hdr, "MComprHD", 8)) {
        result = XEMU_MEDIA_CHD;
      } else if (!memcmp(hdr, "FATX", 4)) {
        uint32_t spc = read_le32(hdr + 8), root = read_le32(hdr + 12);
        if (spc && spc <= 128 && !(spc & (spc - 1)) && root &&
            st.st_size >= 4096 + (int64_t)spc * 512) {
            result = XEMU_MEDIA_FATX_CANDIDATE;
        }
      }
    }
    if (result == XEMU_MEDIA_UNKNOWN) {
        char pvd[7];
        if ((fseek(f, 16L*2048, SEEK_SET) == 0 && fread(pvd,1,7,f) == 7 &&
             !memcmp(pvd, "\1CD001\1", 7)) ||
            (fseek(f, 16L*2352+16, SEEK_SET) == 0 && fread(pvd,1,7,f) == 7 &&
             !memcmp(pvd, "\1CD001\1", 7)))
            result = XEMU_MEDIA_OPTICAL_CANDIDATE;
    }

out:
    if (ferror(f)) {
        result = XEMU_MEDIA_UNREADABLE;
    }
    fclose(f);
    return result;
}
