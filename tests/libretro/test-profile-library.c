/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Native metadata/coexistence and failed-load probe: no guest or user media. */
#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include "ui/libretro.h"

static size_t expected_count;
static bool options_seen;
static const char *fixture_dir;
static unsigned graphics_requests, refusals;
static bool environment(unsigned command, void *data)
{
    if (command == RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY) {
        *(const char **)data = fixture_dir;
        return true;
    }
    if (command == RETRO_ENVIRONMENT_SET_HW_RENDER) graphics_requests++;
    if (command == RETRO_ENVIRONMENT_SET_MESSAGE) refusals++;
    if (command == RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION) {
        *(unsigned *)data = 2;
        return true;
    }
    if (command == RETRO_ENVIRONMENT_GET_LANGUAGE) {
        *(unsigned *)data = RETRO_LANGUAGE_ITALIAN;
        return true;
    }
    if (command == RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL) {
        const struct retro_core_options_v2_intl *v = data;
        size_t count = 0;
        while (v->us->definitions[count].key) count++;
        assert(count == expected_count);
        options_seen = true;
        return true;
    }
    return false;
}

int main(int argc, char **argv)
{
    const char *names[] = {"Microsoft - Xbox (OG)", "Xemu-Chihiro", "Xemu-Full"};
    const char *extensions[] = {"iso|xiso", "bin|chd|cue|gdi|iso", "iso|xiso|bin|chd|cue|gdi"};
    void *cores[3];
    assert(argc == 4 || argc == 5);
    const unsigned core_count = argc - 2;
    fixture_dir = argv[argc - 1];
    for (unsigned i = 0; i < core_count; i++) {
        cores[i] = dlopen(argv[i + 1], RTLD_NOW | RTLD_LOCAL);
        if (!cores[i]) fprintf(stderr, "%s\n", dlerror());
        assert(cores[i]);
    }
    /* Revisit Xbox after querying the other pinned libraries. */
    for (unsigned pass = 0; pass <= core_count; pass++) {
        unsigned i = pass % core_count;
        void (*info_fn)(struct retro_system_info *) = dlsym(cores[i], "retro_get_system_info");
        void (*env_fn)(retro_environment_t) = dlsym(cores[i], "retro_set_environment");
        assert(info_fn && env_fn);
        struct retro_system_info info;
        info_fn(&info);
        assert(!strcmp(info.library_name, names[i]));
        assert(!strcmp(info.valid_extensions, extensions[i]));
        assert(info.need_fullpath && info.block_extract);
        expected_count = i == 2 ? 28 : i == 0 ? 24 : 22;
#ifdef __APPLE__
        expected_count++; /* MoltenVK submission selector. */
#endif
#ifndef __x86_64__
        expected_count--; /* Host x87 path unavailable on this architecture. */
#endif
        options_seen = false;
        env_fn(environment);
        assert(options_seen);
        assert(!dlsym(RTLD_DEFAULT, "retro_init"));
        void (*init_fn)(void) = dlsym(cores[i], "retro_init");
        void (*deinit_fn)(void) = dlsym(cores[i], "retro_deinit");
        assert(init_fn && deinit_fn);
        init_fn();
        bool (*load_fn)(const struct retro_game_info *) = dlsym(cores[i], "retro_load_game");
        assert(load_fn);
        const char *rejected[] = {"missing.bin", i == 0 ? "candidate.bin" :
                                                i == 1 ? "xbox.xiso" : "invalid.bin"};
        for (unsigned n = 0; n < 2; n++) {
            char path[4096];
            snprintf(path, sizeof(path), "%s/%s", fixture_dir, rejected[n]);
            struct retro_game_info game = { .path = path };
            unsigned before = refusals;
            assert(!load_fn(&game));
            assert(refusals == before + 1 && graphics_requests == 0);
        }
        deinit_fn();
        printf("%s: %zu options; %s; version %s\n", info.library_name,
               expected_count, info.valid_extensions, info.library_version);
    }
    for (unsigned i = 0; i < core_count; i++) assert(!dlclose(cores[i]));
    printf("Metadata coexistence and %u pre-graphics load refusals passed; no guest started.\n",
           2 * (core_count + 1));
    return 0;
}
