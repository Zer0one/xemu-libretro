/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stdio.h>
#include <string.h>
#include "libretro-profile.h"
#include "libretro-chihiro-firmware.h"
#if XEMU_LIBRETRO_PROFILE != XEMU_PROFILE_XBOX
#include <glib.h>
#include <sys/stat.h>
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "util/miniz/miniz.h"

typedef struct FirmwareSpec {
    const char *names[4], *set;
    size_t size;
} FirmwareSpec;
static const FirmwareSpec specs[XEMU_FW_COUNT] = {
    {{"chihiro_xbox_bios.bin"}, "chihiro", 524288},
    {{"fpr21042_m29w160et.bin", "fpr-23887_29lv160te.ic4", "fpr-23887.bin"}, "chihiro", 2097152},
    {{"ic10_g24lc64.bin"}, "chihiro", 8192},
    {{"ic11_24lc024.bin"}, "chihiro", 128},
    {{"pc20_g24lc64.bin"}, "chihiro", 8192},
    {{"ver1305.bin"}, "seganetw", 2097152},
};
static const char *base_name(const char *p)
{
    const char *base = p;
    for (; *p; p++) if (*p == '/' || *p == '\\') base = p + 1;
    return base;
}
static void dirname_inplace(char *path)
{
    for (char *p = path; *p; p++) if (*p == '\\') *p = '/';
    char *s = strrchr(path, '/');
    if (!s) g_strlcpy(path, !strcmp(path, ".") ? ".." : ".", 4096);
    else if (s == path || (s == path + 2 && path[1] == ':')) s[1] = 0;
    else *s = 0;
}
static bool valid_size(unsigned id, uint64_t size)
{
    /* Preserve the donor's <=4 MiB media-board flash acceptance. */
    return id == XEMU_FW_FLASH ? size && size <= 4 * 1024 * 1024 : size == specs[id].size;
}
static bool loose(const char *dir, unsigned id, XemuChihiroFirmware *fw)
{
    for (unsigned i = 0; specs[id].names[i]; i++) {
        char path[4096]; struct stat st;
        int n = snprintf(path, sizeof(path), "%s/%s", dir, specs[id].names[i]);
        if (n < 0 || n >= (int)sizeof(path) || stat(path, &st) ||
            !S_ISREG(st.st_mode) || !valid_size(id, st.st_size)) continue;
        FILE *f = fopen(path, "rb");
        if (!f) continue;
        fclose(f);
        g_strlcpy(fw->path[id], path, sizeof(fw->path[id]));
        g_strlcpy(fw->source[id], path, sizeof(fw->source[id]));
        return true;
    }
    return false;
}
static bool zip_firmware(const char *archive, const char *cache, unsigned id,
                          XemuChihiroFirmware *fw)
{
    struct stat st;
    if (stat(archive, &st) || !S_ISREG(st.st_mode) || st.st_size > 64 * 1024 * 1024) return false;
    mz_zip_archive zip = {0};
    if (!mz_zip_reader_init_file(&zip, archive, 0)) return false;
    bool ok = false;
    unsigned count = mz_zip_reader_get_num_files(&zip);
    if (count > 4096) goto done;
    for (unsigned name = 0; specs[id].names[name] && !ok; name++)
    for (unsigned i = 0; i < count && !ok; i++) {
        mz_zip_archive_file_stat entry;
        if (!mz_zip_reader_file_stat(&zip, i, &entry) || entry.m_is_directory ||
            g_ascii_strcasecmp(base_name(entry.m_filename), specs[id].names[name]) ||
            !valid_size(id, entry.m_uncomp_size)) continue;
        size_t size = (size_t)entry.m_uncomp_size;
        uint8_t *data = g_malloc(size);
        if (!mz_zip_reader_extract_to_mem(&zip, i, data, size, 0)) { g_free(data); continue; }
        char *sha = g_compute_checksum_for_data(G_CHECKSUM_SHA256, data, size);
        char dir[4096], dest[4096];
        int n = snprintf(dir, sizeof(dir), "%s/%s", cache, sha);
        int m = snprintf(dest, sizeof(dest), "%s/%s", dir, specs[id].names[name]);
        g_free(sha);
        if (n < 0 || n >= (int)sizeof(dir) || m < 0 || m >= (int)sizeof(dest)) { g_free(data); continue; }
        /* Reuse only an intact cache entry. Never overwrite source ROMs or
         * mutable machine EEPROM/save files. GLib writes cache files atomically. */
        gchar *previous = NULL; gsize length = 0;
        bool intact = !stat(dest, &st) && st.st_size == (int64_t)size &&
            g_file_get_contents(dest, &previous, &length, NULL) &&
            length == size && !memcmp(previous, data, size);
        g_free(previous);
        ok = intact || (g_mkdir_with_parents(dir, 0755) == 0 &&
                        g_file_set_contents(dest, (const char *)data, size, NULL));
        g_free(data);
        if (ok) {
            g_strlcpy(fw->path[id], dest, sizeof(fw->path[id]));
            snprintf(fw->source[id], sizeof(fw->source[id]), "%s:%s", archive, entry.m_filename);
        }
    }
done:
    mz_zip_reader_end(&zip);
    return ok;
}
static bool set_files(const char *root, const char *set, const char *cache,
                       unsigned id, XemuChihiroFirmware *fw)
{
    if (!set || !*set) return false;
    char path[4096];
    int n = snprintf(path, sizeof(path), "%s/%s.zip", root, set);
    if (n >= 0 && n < (int)sizeof(path) && zip_firmware(path, cache, id, fw)) return true;
    n = snprintf(path, sizeof(path), "%s/%s", root, set);
    return n >= 0 && n < (int)sizeof(path) && loose(path, id, fw);
}
static void add_alias(char names[4][256], unsigned *count, const char *name)
{
    if (!name || !*name || *count == 4) return;
    const char *dot = strrchr(name, '.');
    size_t n = dot ? (size_t)(dot-name) : strlen(name);
    if (!n || n > 255) return;
    for (size_t i = 0; i < n; i++)
        if (name[i] == '/' || name[i] == '\\') return;
    for (unsigned i = 0; i < *count; i++)
        if (strlen(names[i]) == n && !g_ascii_strncasecmp(names[i], name, n)) return;
    memcpy(names[*count], name, n); names[*count][n] = 0; (*count)++;
}

bool libretro_chihiro_firmware_resolve(const char *system, const char *profile,
    const char *content, const char *set, const char *parent,
    XemuChihiroFirmware *fw, char *error, size_t capacity)
{
    if (!system || !profile || !content || !fw || strlen(content) >= 4096) return false;
    memset(fw, 0, sizeof(*fw));
    char sys[4][4096], cache[4096], roots[2][4096];
    int n = snprintf(sys[0], sizeof(sys[0]), "%s/%s", system, profile);
    if (n < 0 || n >= (int)sizeof(sys[0]) - 80) return false;
    snprintf(sys[1], sizeof(sys[1]), "%s/Chihiro", sys[0]);
    snprintf(sys[2], sizeof(sys[2]), "%s/xemu/Chihiro", system);
    g_strlcpy(sys[3], system, sizeof(sys[3]));
    snprintf(cache, sizeof(cache), "%s/firmware-cache", sys[0]);
    g_strlcpy(roots[0], content, sizeof(roots[0])); dirname_inplace(roots[0]);
    g_strlcpy(roots[1], roots[0], sizeof(roots[1])); dirname_inplace(roots[1]);
    char aliases[4][256] = {{0}}; unsigned names = 0;
    add_alias(aliases, &names, set); add_alias(aliases, &names, parent);
    add_alias(aliases, &names, base_name(content));
    add_alias(aliases, &names, base_name(roots[0]));
    for (unsigned id = 0; id < XEMU_FW_COUNT; id++) {
        bool found = false;
        /* System sources retain precedence, independently for each firmware. */
        for (unsigned i = 0; i < 4 && !found; i++)
            found = loose(sys[i], id, fw) || set_files(sys[i], specs[id].set, cache, id, fw);
        /* Associated game sets, then shared BIOS sets beside media. */
        for (unsigned i = 0; i < 2 && !found && *content; i++)
            for (unsigned j = 0; j < names && !found; j++)
                found = set_files(roots[i], aliases[j], cache, id, fw);
        for (unsigned i = 0; i < 2 && !found && *content; i++)
            found = set_files(roots[i], specs[id].set, cache, id, fw) || loose(roots[i], id, fw);
    }
    if (!*fw->path[XEMU_FW_BIOS]) {
        if (error && capacity) snprintf(error, capacity,
            "Chihiro BIOS not found in System, associated game sets or chihiro.zip/chihiro beside content");
        return false;
    }
    return true;
}
#else
bool libretro_chihiro_firmware_resolve(const char *system, const char *profile,
    const char *content, const char *set, const char *parent,
    XemuChihiroFirmware *fw, char *error, size_t capacity)
{
    (void)system; (void)profile; (void)content; (void)set; (void)parent; (void)fw;
    if (error && capacity) snprintf(error, capacity, "Chihiro firmware requires the Chihiro core");
    return false;
}
#endif
