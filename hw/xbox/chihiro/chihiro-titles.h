/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef CHIHIRO_TITLES_H
#define CHIHIRO_TITLES_H
#include <string.h>

/* One catalogue for every title identified by the donor's boot.id map.
 * DIMM MiB is an evidence-backed reference configuration, not a size estimate.
 * Zero means that the hardware value still needs evidence. Never turn an
 * unknown catalogue value into a guessed per-title default.
 * Source keys/coverage: docs/chihiro-dimm-sources.md.
 * Runtime validation: docs/chihiro-direct-media.md. */
typedef struct ChihiroTitle {
    char id[5];
    const char *name;
    unsigned dimm_mib;
} ChihiroTitle;

static const ChihiroTitle chihiro_titles[] = {
    { "SBFN", "hotd3",       0 },
    { "SBFY", "crtaxihr",    512 }, /* SEGA UK manual, System Information; [CTHR-UK]. */
    { "SBFZ", "vcop3",       512 }, /* SEGA Deluxe manual, printed p.38; [VC3-DLX]. */
    { "SGBZ", "outr2",     512 }, /* Verified FATX layout and core boot. */
    { "SBHC", "mj2",         0 },
    { "SBHF", "ollie",       512 }, /* SEGA 999-2026, printed p.53; [OLLIE-DLX]. */
    { "SBHQ", "wangmid",     0 },
    { "SBHU", "ghostsqu",    512 }, /* SEGA 4201-6831-01, printed p.68; [GHOST-DLX]. */
    { "SBJK", "gundamos", 1024 }, /* Donor config: 603 MB image needs 1 GiB. */
    { "SBJE", "outr2st",   512 }, /* SEGA manual and four verified layouts. */
    { "SBKD", "wangmid2",    512 }, /* MAME hardware notes: Japan Rev A JP5; [MAME-WMMT2]. */
    { "SBKK", "mj3",         0 },
    { "SBLF", "scg06nt",     0 },
    { "SBME", "mj3evo",      0 },
};

static inline const ChihiroTitle *chihiro_title_find(const void *id)
{
    for (unsigned i = 0; i < sizeof(chihiro_titles) / sizeof(chihiro_titles[0]); i++) {
        if (!memcmp(chihiro_titles[i].id, id, 4)) return &chihiro_titles[i];
    }
    return NULL;
}
#endif
