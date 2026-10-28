/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Exercise the real option registration code through frontend API callbacks. */
#include <assert.h>
#include <stdio.h>
#include "ui/libretro_core_options.h"

static unsigned api_version, language, registrations, variable_reads;
static struct retro_core_option_v2_definition original[64];
static size_t original_count;

static bool expected(const char *key)
{
    static const char *const xbox[] = {
        "xemu_memory", "xemu_network_backend", "xemu_skip_boot_anim", "xemu_avpack",
        "xemu_video_aspect",
    };
    static const char *const arcade[] = {
        "xemu_chihiro_freeplay", "xemu_chihiro_gun_aim",
        "xemu_chihiro_crosshair", "xemu_chihiro_ffb",
        "xemu_chihiro_steering_deadzone",
    };
    for (unsigned i = 0; i < sizeof(arcade) / sizeof(arcade[0]); i++)
        if (XEMU_LIBRETRO_PROFILE == 1 && !strcmp(key, arcade[i])) return false;
    for (unsigned i = 0; i < sizeof(xbox) / sizeof(xbox[0]); i++)
        if (XEMU_LIBRETRO_PROFILE == 2 && !strcmp(key, xbox[i])) return false;
    for (size_t i = 0; i < original_count; i++)
        if (!strcmp(key, original[i].key)) return true;
    return false;
}

static const struct retro_core_option_v2_definition *baseline(const char *key)
{
    for (size_t i = 0; i < original_count; i++)
        if (!strcmp(key, original[i].key)) return &original[i];
    assert(!"unknown or obsolete option exposed");
    return NULL;
}

static void check_count(size_t count)
{
    assert(count == original_count - (XEMU_LIBRETRO_PROFILE == 3 ? 0 : 5));
}

static void check_v2(const struct retro_core_options_v2 *options, bool local)
{
    size_t count = 0;
    for (; options->definitions[count].key; count++) {
        const struct retro_core_option_v2_definition *d = &options->definitions[count];
        assert(expected(d->key));
        assert(strcmp(d->key, "xemu_display_filtering"));
        const char *category = d->category_key;
        if (XEMU_LIBRETRO_PROFILE == 2) {
            assert(!category || strcmp(category, "chihiro"));
            if (!strcmp(d->key, "xemu_chihiro_freeplay"))
                assert(category && !strcmp(category, "system"));
            else if (!strncmp(d->key, "xemu_chihiro_", 13))
                assert(category && !strcmp(category, "input"));
        } else if (!strncmp(d->key, "xemu_chihiro_", 13)) {
            assert(category && !strcmp(category, "chihiro"));
        }
        if (category && options->categories) {
            bool found = false;
            for (size_t j = 0; options->categories[j].key; j++)
                found |= !strcmp(category, options->categories[j].key);
            assert(found);
        }
        if (!local) {
            const struct retro_core_option_v2_definition *b = baseline(d->key);
            assert(!strcmp(d->default_value, b->default_value));
            assert(!memcmp(d->values, b->values, sizeof(d->values)));
        }
        for (size_t j = 0; j < count; j++)
            assert(strcmp(d->key, options->definitions[j].key));
    }
    if (!local) check_count(count);
    if (options->categories) {
        for (size_t i = 0; options->categories[i].key; i++) {
            if (XEMU_LIBRETRO_PROFILE == 2)
                assert(strcmp(options->categories[i].key, "chihiro"));
            bool used = false;
            for (size_t j = 0; j < count; j++) {
                const char *key = options->definitions[j].category_key;
                used |= key && !strcmp(key, options->categories[i].key);
            }
            assert(used);
        }
    }
}

static void check_v1(const struct retro_core_option_definition *defs, bool local)
{
    size_t count = 0;
    for (; defs[count].key; count++) {
        assert(expected(defs[count].key));
        if (!local) {
            const struct retro_core_option_v2_definition *b = baseline(defs[count].key);
            assert(!strcmp(defs[count].default_value, b->default_value));
            assert(!memcmp(defs[count].values, b->values, sizeof(b->values)));
        }
    }
    if (!local) check_count(count);
}

static bool environment(unsigned command, void *data)
{
    switch (command) {
    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
        *(unsigned *)data = api_version;
        return api_version != 0; /* Also exercise the unsupported-query fallback. */
    case RETRO_ENVIRONMENT_GET_LANGUAGE:
        *(unsigned *)data = language;
        return true;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL: {
        const struct retro_core_options_v2_intl *v = data;
        check_v2(v->us, false);
        if (v->local) check_v2(v->local, true);
        registrations++;
        return true;
    }
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
        check_v2(data, false);
        registrations++;
        return true;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL: {
        const struct retro_core_options_intl *v = data;
        check_v1(v->us, false);
        if (v->local) check_v1(v->local, true);
        registrations++;
        return true;
    }
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
        check_v1(data, false);
        registrations++;
        return true;
    case RETRO_ENVIRONMENT_SET_VARIABLES: {
        const struct retro_variable *vars = data;
        size_t count = 0;
        for (; vars[count].key; count++) {
            assert(expected(vars[count].key));
            const struct retro_core_option_v2_definition *b = baseline(vars[count].key);
            const char *values = strstr(vars[count].value, "; ");
            assert(values);
            values += 2;
            assert(!strncmp(values, b->default_value, strlen(b->default_value)));
            assert(values[strlen(b->default_value)] == '|' ||
                   values[strlen(b->default_value)] == '\0');
        }
        check_count(count);
        registrations++;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE:
        variable_reads++;
        ((struct retro_variable *)data)->value = "stale frontend value";
        return true;
    default:
        assert(!"unexpected environment command");
        return false;
    }
}

int main(void)
{
    while (option_defs_us[original_count].key) original_count++;
#ifdef __APPLE__
    assert(original_count == 22);
#else
    assert(original_count == 21);
#endif
    memcpy(original, option_defs_us, original_count * sizeof(original[0]));
    assert(xemu_profile_fixed_machine() ==
           (XEMU_LIBRETRO_PROFILE == 3 ? -1 : XEMU_LIBRETRO_PROFILE == 2));
    for (api_version = 0; api_version <= 2; api_version++) {
        for (language = 0; language <= RETRO_LANGUAGE_LAST; language++) {
            for (unsigned repeat = 0; repeat < 2; repeat++) {
                bool categories = false;
                unsigned before = registrations;
                libretro_set_core_options(environment, &categories);
                assert(registrations == before + 1);
                assert(categories == (api_version == 2));
            }
        }
    }
    for (size_t i = 0; i < original_count; i++) {
        struct retro_variable var = { original[i].key, "old value" };
        unsigned before = variable_reads;
        bool allowed = expected(var.key);
        assert(libretro_get_core_variable(environment, &var) == allowed);
        assert(variable_reads == before + allowed);
        assert((var.value != NULL) == allowed);
    }
    const char *paths[] = { "xemu_bootrom_path", "xemu_bios_path",
                            "xemu_hdd_path", "xemu_eeprom_path" };
    for (size_t i = 0; i < 4; i++) {
        struct retro_variable var = { paths[i], NULL };
        assert(libretro_get_core_variable(environment, &var) ==
               (XEMU_LIBRETRO_PROFILE != 2));
    }
    struct retro_variable retired = { "xemu_display_filtering", "old value" };
    unsigned reads_before = variable_reads;
    assert(!libretro_get_core_variable(environment, &retired) && !retired.value);
    assert(variable_reads == reads_before);
    struct retro_variable var = { "xemu_renderer", "old value" };
    assert(!libretro_get_core_variable(NULL, &var) && !var.value);
    printf("%s: %u registrations, APIs 0/1/2, all languages, defaults and stale reads OK\n",
           XEMU_LIBRETRO_PROFILE_NAME, registrations);
    return 0;
}
