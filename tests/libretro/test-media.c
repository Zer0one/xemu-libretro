/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdlib.h>
#include "ui/libretro-media.h"

int main(int argc, char **argv)
{
    assert(xemu_libretro_probe_media(NULL) == XEMU_MEDIA_NONE);
    assert(xemu_libretro_probe_media("") == XEMU_MEDIA_NONE);
    assert(argc == 3);
    assert(xemu_libretro_probe_media(argv[1]) == (XemuLibretroMedia)atoi(argv[2]));
    return 0;
}
