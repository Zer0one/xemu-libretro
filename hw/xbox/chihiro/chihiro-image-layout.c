/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <string.h>
#include "chihiro-image-layout.h"

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static bool boot_name(const uint8_t *p)
{
    const char *expected = "boot.id";
    for (unsigned i = 0; i < 7; i++) {
        unsigned c = p[i];
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if (c != (unsigned)expected[i]) return false;
    }
    return true;
}

bool chihiro_image_layout(ChihiroImageRead read, void *opaque, uint64_t size,
                          ChihiroImageLayout *layout)
{
    static const unsigned capacities[] = {128, 256, 512, 1024};
    uint8_t header[16], directory[65536], bid[192];
    ChihiroImageLayout found = {0};
    if (!layout) return false;
    memset(layout, 0, sizeof(*layout));
    if (!read || size < 16 || !read(opaque, 0, header, sizeof(header)) ||
        memcmp(header, "FATX", 4)) return false;
    uint32_t spc = le32(header + 8), root = le32(header + 12);
    if (!spc || spc > 128 || (spc & (spc - 1)) || !root) return false;
    uint32_t cluster = spc * 512;
    for (unsigned p = 0; p < sizeof(capacities) / sizeof(capacities[0]); p++) {
        uint64_t dimm = (uint64_t)capacities[p] << 20;
        uint64_t fs = dimm - (16 << 20), entries = fs / cluster + 1;
        uint64_t data = 4096 + ((entries * (entries < 0xfff0 ? 2 : 4) + 4095) & ~UINT64_C(4095));
        uint64_t root_offset = data + (uint64_t)(root - 1) * cluster;
        if (size > dimm || root >= entries || root_offset > size ||
            cluster > size - root_offset ||
            !read(opaque, root_offset, directory, cluster)) continue;
        for (uint32_t i = 0; i + 64 <= cluster; i += 64) {
            const uint8_t *e = directory + i;
            if (e[0] == 0 || e[0] == 255) break;
            if (e[0] != 7 || (e[1] & 0x10) || !boot_name(e + 2)) continue;
            uint32_t first = le32(e + 44);
            uint64_t offset = data + (uint64_t)(first - 1) * cluster;
            if (!first || first >= entries || le32(e + 48) < sizeof(bid) ||
                offset > size || sizeof(bid) > size - offset ||
                !read(opaque, offset, bid, sizeof(bid)) ||
                memcmp(bid, "BTID", 4) || memcmp(bid + 32, "XBAM", 4)) continue;
            if (found.dimm_mib && found.dimm_mib != capacities[p]) return false;
            found.dimm_mib = capacities[p];
            memcpy(found.game_id, bid + 48, 4);
            found.title = chihiro_title_find(bid + 48);
        }
    }
    if (!found.dimm_mib) return false;
    *layout = found;
    return true;
}
