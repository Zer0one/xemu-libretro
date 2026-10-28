/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "libretro-chihiro-key-search.h"
#include "libretro-chihiro-media-table.h"

static const char *basename_of(const char *path)
{
    const char *base = path;
    for (; *path; path++) if (*path == '/' || *path == '\\') base = path + 1;
    return base;
}

static void hash_hex(const uint8_t sha1[20], char hex[41])
{
    for (unsigned i = 0; i < 20; i++) snprintf(hex + i * 2, 3, "%02x", sha1[i]);
}

static bool stem(const char *name, char out[129])
{
    const char *dot = strrchr(name, '.');
    size_t n = dot ? (size_t)(dot - name) : strlen(name);
    if (!n || n > 128) return false;
    for (size_t i = 0; i < n; i++) {
        if (!g_ascii_isalnum(name[i]) && name[i] != '-' && name[i] != '_') return false;
        out[i] = g_ascii_tolower(name[i]);
    }
    out[n] = 0;
    return true;
}

const XemuChihiroMedia *xemu_chihiro_media_lookup(const char *path,
                                                const uint8_t sha1[20])
{
    if (!path) return NULL;
    char hex[41], name[129];
    if (sha1) {
        hash_hex(sha1, hex);
        for (size_t i = 0; i < G_N_ELEMENTS(media_table); i++)
            if (!strcmp(hex, media_table[i].disc_sha1)) return &media_table[i];
    }
    if (stem(basename_of(path), name))
        for (size_t i = 0; i < G_N_ELEMENTS(media_table); i++)
            if (!strcmp(name, media_table[i].disc)) return &media_table[i];
    return NULL;
}

/* Normalize separators only; retain symlink traversal semantics and relative
 * paths. In particular C:/ must remain a drive root on Windows. */
static void parent_dir(char path[4096])
{
    for (char *p = path; *p; p++) if (*p == '\\') *p = '/';
    char *slash = strrchr(path, '/');
    if (!slash) {
        g_strlcpy(path, !strcmp(path, ".") ? ".." : ".", 4096);
    } else if (slash == path || (slash == path + 2 && path[1] == ':')) {
        slash[1] = 0;
    } else {
        *slash = 0;
    }
}

typedef struct Search {
    const XemuChihiroMedia *media;
    XemuChihiroKeysFound *found;
    const char *source;
    char aliases[6][129];
    size_t names;
    bool hash_only, pic, any_name, accepted;
    XemuChihiroKeyCheck check;
    void *opaque;
} Search;

static void add_name(Search *s, const char *name)
{
    char clean[129];
    if (!name || !stem(name, clean)) return;
    for (size_t i = 0; i < s->names; i++) if (!strcmp(clean, s->aliases[i])) return;
    if (s->names < G_N_ELEMENTS(s->aliases))
        g_strlcpy(s->aliases[s->names++], clean, sizeof(clean));
}

static bool accept_key(void *opaque, const char *name, const uint8_t *data,
                       size_t size, bool pic, const XemuChihiroKey *key)
{
    Search *s = opaque;
    if (pic != s->pic) return true;
    char *digest = g_compute_checksum_for_data(G_CHECKSUM_SHA1, data, size);
    bool verified = s->media && !strcmp(digest, s->media->key_sha1);
    bool named = false;
    char clean[129];
    if (stem(basename_of(name), clean))
        for (size_t i = 0; i < s->names; i++)
            if (!strcmp(clean, s->aliases[i])) named = true;
    if ((s->hash_only && !verified) || (!s->hash_only && !named && !s->any_name)) {
        g_free(digest);
        return true;
    }
    if (s->check) {
        char source[4352];
        if (strcmp(s->source, name))
            snprintf(source, sizeof(source), "%s:%s", s->source, name);
        else
            g_strlcpy(source, name, sizeof(source));
        s->accepted = s->check(s->opaque, key, source, digest, verified);
        g_free(digest);
        return !s->accepted;
    }
    for (size_t i = 0; i < s->found->count; i++)
        if (!memcmp(s->found->keys[i].des, key->des, sizeof(key->des)) &&
            !strcmp(s->found->keys[i].loader, key->loader)) {
            g_free(digest);
            return true;
        }
    size_t n = s->found->count;
    if (n == G_N_ELEMENTS(s->found->keys)) { g_free(digest); return false; }
    s->found->keys[n] = *key;
    s->found->verified[n] = verified;
    g_strlcpy(s->found->sha1[n], digest, sizeof(s->found->sha1[n]));
    if (strcmp(s->source, name))
        snprintf(s->found->source[n], sizeof(s->found->source[n]), "%s:%s", s->source, name);
    else
        g_strlcpy(s->found->source[n], name, sizeof(s->found->source[n]));
    s->found->count++;
    g_free(digest);
    return s->found->count < G_N_ELEMENTS(s->found->keys);
}

static bool scan_file(Search *s, const char *path)
{
    s->source = path;
    size_t before = s->found ? s->found->count : 0;
    XemuChihiroKeyResult result = xemu_chihiro_visit_keys(path, accept_key, s);
    /* Array searches roll back earlier members on a later CRC/format failure.
     * Streaming validation stops at the first valid, CRC-checked member. */
    if (s->check) return result == XEMU_CHIHIRO_KEYS_OK && s->accepted;
    if (result != XEMU_CHIHIRO_KEYS_OK) s->found->count = before;
    return s->found->count > before;
}

static int name_compare(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static bool scan_directory(Search *s, const char *path)
{
    GDir *dir = g_dir_open(path, 0, NULL);
    if (!dir) return false;
    GPtrArray *names = g_ptr_array_new_with_free_func(g_free);
    const char *name;
    unsigned entries = 0;
    while ((name = g_dir_read_name(dir))) {
        const char *ext = strrchr(name, '.');
        if (ext && !g_ascii_strcasecmp(ext, s->pic ? ".pic" : ".data")) {
            if (++entries > 4096) break;
            g_ptr_array_add(names, g_strdup(name));
        }
    }
    g_dir_close(dir);
    bool found = false;
    /* Bound candidate dumps, not unrelated ROM sets in a large MAME root.
     * Never accept a nondeterministic truncated candidate list. */
    if (entries <= 4096) {
        g_ptr_array_sort(names, name_compare);
        for (unsigned i = 0; i < names->len; i++) {
            char file[4096];
            int n = snprintf(file, sizeof(file), "%s/%s", path, (char *)names->pdata[i]);
            if (n < 0 || n >= (int)sizeof(file)) continue;
            if (scan_file(s, file)) found = true;
            if (s->accepted || (s->found && s->found->count == G_N_ELEMENTS(s->found->keys))) break;
        }
    }
    g_ptr_array_free(names, true);
    return found;
}

bool xemu_chihiro_find_keys(const char *path, const char *keys_dir,
    const XemuChihiroMedia *media, XemuChihiroKeysFound *found)
{
    if (!found) return false;
    memset(found, 0, sizeof(*found));
    if (!path || !keys_dir || !media || strlen(path) >= 4096 || strlen(keys_dir) >= 4096)
        return false;
    Search s = { .media = media, .found = found };
    char roots[3][4096];
    g_strlcpy(roots[0], path, sizeof(roots[0]));
    parent_dir(roots[0]);
    g_strlcpy(roots[1], roots[0], sizeof(roots[1]));
    parent_dir(roots[1]);
    g_strlcpy(roots[2], keys_dir, sizeof(roots[2]));
    /* Container aliases exclude the raw PIC filename. */
    add_name(&s, media->set); add_name(&s, media->parent);
    add_name(&s, basename_of(path));
    size_t containers = s.names;
    add_name(&s, media->key); add_name(&s, media->disc);
    for (unsigned pass = 0; pass < 2; pass++) {
        s.hash_only = pass == 0;
        for (unsigned root = 0; root < 3; root++) {
            bool duplicate = false;
            for (unsigned j = 0; j < root; j++) if (!strcmp(roots[j], roots[root])) duplicate = true;
            if (duplicate) continue;
            /* ZIP(s), associated directory(s), loose files at this root. */
            for (unsigned place = 0; place < 3; place++) {
                size_t locations = place == 2 ? 1 : containers;
                for (size_t i = 0; i < locations; i++) {
                    char location[4096];
                    int n = place == 2 ? snprintf(location, sizeof(location), "%s", roots[root]) :
                        snprintf(location, sizeof(location), "%s/%s%s", roots[root], s.aliases[i], place == 0 ? ".zip" : "");
                    if (n < 0 || n >= (int)sizeof(location)) continue;
                    for (unsigned format = 0; format < 2; format++) {
                        s.pic = format == 1;
                        bool ok = place == 0 ? scan_file(&s, location) : scan_directory(&s, location);
                        if (ok && s.hash_only) return true;
                        if (found->count == G_N_ELEMENTS(found->keys)) return true;
                    }
                }
            }
        }
    }
    return found->count > 0;
}

/* The fallback uses the actual image basename (spaces, dots and UTF-8 are
 * valid), never the MAME set or containing-directory name. */
bool xemu_chihiro_try_image_keys(const char *path, const char *keys_dir,
    const XemuChihiroMedia *media, XemuChihiroKeyCheck check, void *opaque)
{
    if (!path || !keys_dir || !check || strlen(path) >= 4096 || strlen(keys_dir) >= 4096)
        return false;
    const char *base = basename_of(path), *dot = strrchr(base, '.');
    size_t length = dot ? (size_t)(dot - base) : strlen(base);
    if (!length || length > 255 || (length == 1 && *base == '.') ||
        (length == 2 && !memcmp(base, "..", 2))) return false;
    char name[256], roots[3][4096];
    memcpy(name, base, length); name[length] = 0;
    g_strlcpy(roots[0], path, sizeof(roots[0])); parent_dir(roots[0]);
    g_strlcpy(roots[1], keys_dir, sizeof(roots[1]));
    g_strlcpy(roots[2], keys_dir, sizeof(roots[2])); parent_dir(roots[2]);
    if (strlen(roots[2]) + 8 >= sizeof(roots[2])) return false;
    g_strlcat(roots[2], "/redump", sizeof(roots[2]));
    Search s = { .media = media, .check = check, .opaque = opaque, .any_name = true };
    for (unsigned root = 0; root < 3; root++) {
        /* Preserve ZIP / associated folder / loose-root precedence. */
        for (unsigned place = 0; place < 3; place++) {
            char location[4096];
            int n = snprintf(location, sizeof(location), "%s/%s%s", roots[root], name,
                             place == 0 ? ".zip" : "");
            if (n < 0 || n >= (int)sizeof(location)) continue;
            for (unsigned format = 0; format < 2; format++) {
                s.pic = format == 1;
                if (place == 0) {
                    if (scan_file(&s, location)) return true;
                } else if (place == 1) {
                    if (scan_directory(&s, location)) return true;
                } else {
                    char file[4096];
                    n = snprintf(file, sizeof(file), "%s.%s", location, s.pic ? "pic" : "data");
                    if (n >= 0 && n < (int)sizeof(file) && scan_file(&s, file)) return true;
                }
            }
        }
    }
    return false;
}
