/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef XEMU_LIBRETRO_MEDIA_H
#define XEMU_LIBRETRO_MEDIA_H

typedef enum XemuLibretroMedia {
    XEMU_MEDIA_NONE,
    XEMU_MEDIA_UNREADABLE,
    XEMU_MEDIA_UNKNOWN,
    XEMU_MEDIA_XBOX_DISC,
    /* A plausible FATX container, not proof of a bootable Chihiro title. */
    XEMU_MEDIA_FATX_CANDIDATE,
    XEMU_MEDIA_CHD,
    /* CUE/GDI descriptor or MODE1 ISO/BIN; structural validation follows. */
    XEMU_MEDIA_OPTICAL_CANDIDATE,
} XemuLibretroMedia;

XemuLibretroMedia xemu_libretro_probe_media(const char *path);

#endif
