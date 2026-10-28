/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef XEMU_LIBRETRO_PROFILE_H
#define XEMU_LIBRETRO_PROFILE_H

#include <stdbool.h>
#include <string.h>
#include "libretro-profile-config.h"

#define XEMU_PROFILE_XBOX 1
#define XEMU_PROFILE_CHIHIRO 2
#define XEMU_PROFILE_FULL 3

#if XEMU_LIBRETRO_PROFILE < XEMU_PROFILE_XBOX || \
    XEMU_LIBRETRO_PROFILE > XEMU_PROFILE_FULL
#error Invalid Libretro machine profile
#endif

static inline const char *xemu_profile_data_subdir(void)
{
    return XEMU_LIBRETRO_PROFILE == XEMU_PROFILE_XBOX ? "xemu" :
           XEMU_LIBRETRO_PROFILE == XEMU_PROFILE_CHIHIRO ? "xemu-chihiro" :
           "xemu-full";
}

/* -1 lets the combined profile use the donor's content-based selection.
 * Dedicated profiles cannot be changed by settings or machine arguments. */
static inline int xemu_profile_fixed_machine(void)
{
    return XEMU_LIBRETRO_PROFILE == XEMU_PROFILE_FULL ? -1 :
           XEMU_LIBRETRO_PROFILE == XEMU_PROFILE_CHIHIRO;
}

static inline bool xemu_profile_option_allowed(const char *key)
{
    /* Retired: this standalone UI setting has no reader in the core's
     * rendering/presentation paths. Ignore saved frontend values as well. */
    if (!key || strcmp(key, "xemu_display_filtering") == 0 ||
        strcmp(key, "xemu_cache_shaders") == 0 ||
        strcmp(key, "xemu_chihiro_steering_deadzone") == 0) {
        return false;
    }
    if (XEMU_LIBRETRO_PROFILE == XEMU_PROFILE_XBOX &&
        strncmp(key, "xemu_chihiro_", 13) == 0) {
        return false;
    }
    if (XEMU_LIBRETRO_PROFILE == XEMU_PROFILE_CHIHIRO) {
        static const char *const console_options[] = {
            "xemu_memory", "xemu_network_backend", "xemu_skip_boot_anim",
            "xemu_avpack", "xemu_video_aspect", "xemu_bootrom_path", "xemu_bios_path",
            "xemu_hdd_path", "xemu_eeprom_path", "xemu_hdd_trim",
        };
        for (unsigned i = 0; i < sizeof(console_options) /
                                      sizeof(console_options[0]); i++) {
            if (strcmp(key, console_options[i]) == 0) {
                return false;
            }
        }
    }
    return true;
}

/* Dedicated arcade builds organize controls by purpose; Full retains the
 * donor's separate machine category. Raw option keys remain unchanged. */
static inline const char *xemu_profile_option_category(const char *key,
                                                       const char *category)
{
    if (XEMU_LIBRETRO_PROFILE == XEMU_PROFILE_CHIHIRO &&
        strncmp(key, "xemu_chihiro_", 13) == 0) {
        return strcmp(key, "xemu_chihiro_freeplay") == 0 ? "system" : "controls";
    }
    return category;
}

#endif
