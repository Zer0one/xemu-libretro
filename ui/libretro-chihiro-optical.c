/* SPDX-License-Identifier: GPL-2.0-or-later
 * GD density markers and CHD pregap conventions follow MAME cdrom.cpp.
 * See docs/chihiro-direct-media.md for format references and limits.
 */
#define _FILE_OFFSET_BITS 64
#include <glib.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "libretro-chihiro-optical.h"
#include "libretro-chihiro-chd.h"
#define MAX_IMAGE (UINT64_C(16) << 30)
#define MAX_DESCRIPTOR (512 * 1024)

typedef struct Track {
    char *path;
    uint64_t offset, frames, index0, index1, file_size;
    unsigned number, unit, data_offset, base, file_id;
    bool data, hd, indexed, has_index0;
} Track;
struct XemuChihiroOptical {
    XemuChihiroChd *chd;
    FILE *file;
    uint64_t offset, frames;
    uint32_t unit, data_offset, base;
    bool declared_sha1;
    uint8_t sha1[20];
};
static bool seek_read(FILE *f, uint64_t offset, void *data, size_t size)
{
    if (offset > INT64_MAX) return false;
#ifdef _WIN32
    if (_fseeki64(f, (int64_t)offset, SEEK_SET)) return false;
#else
    if (fseeko(f, (off_t)offset, SEEK_SET)) return false;
#endif
    return fread(data, 1, size, f) == size;
}
static bool eq(const char *a, const char *b) { return a && !g_ascii_strcasecmp(a, b); }
static char *token(char **p)
{
    while (g_ascii_isspace(**p)) (*p)++;
    if (!**p) return NULL;
    char *out = *p;
    if (**p == '"') {
        out = ++*p;
        while (**p && **p != '"') (*p)++;
        if (!**p) return NULL;
        *(*p)++ = 0;
    } else {
        while (**p && !g_ascii_isspace(**p)) (*p)++;
        if (**p) *(*p)++ = 0;
    }
    return out;
}
static bool number(const char *s, uint64_t max, uint64_t *value)
{
    if (!s || !g_ascii_isdigit(*s)) return false;
    char *end;
    guint64 n = g_ascii_strtoull(s, &end, 10);
    if (*end || n > max) return false;
    *value = n; return true;
}
static bool msf(const char *s, uint64_t *frames)
{
    unsigned m, sec, f; int end = 0;
    if (!s || sscanf(s, "%u:%u:%u%n", &m, &sec, &f, &end) != 3 ||
        s[end] || m > 2000 || sec >= 60 || f >= 75) return false;
    *frames = ((uint64_t)m * 60 + sec) * 75 + f; return true;
}
static char *track_path(const char *descriptor, const char *name)
{
    if (!name || !*name || strlen(name) >= 4096) return NULL;
    char *local = g_strdup(name);
    for (char *p = local; *p; p++) if (*p == '\\') *p = '/';
    char *dir = g_path_get_dirname(descriptor);
    char *path = g_path_is_absolute(local) ? g_strdup(local) : g_build_filename(dir, local, NULL);
    g_free(local); g_free(dir);
    if (strlen(path) >= 4096) { g_free(path); return NULL; }
    return path;
}
static bool stat_track(Track *t)
{
    GStatBuf st;
    if (!t->path || g_stat(t->path, &st) || !S_ISREG(st.st_mode) ||
        st.st_size <= 0 || (uint64_t)st.st_size > MAX_IMAGE ||
        t->offset >= (uint64_t)st.st_size) return false;
    t->file_size = st.st_size;
    return true;
}
static bool parse_gdi(char **lines, const char *path, Track *tracks, unsigned *count)
{
    unsigned line = 0;
    while (lines[line] && !*g_strstrip(lines[line])) line++;
    uint64_t n;
    if (!lines[line] || !number(g_strstrip(lines[line++]), 99, &n) || !n) return false;
    *count = (unsigned)n;
    for (unsigned i = 0; i < *count; i++) {
        while (lines[line] && !*g_strstrip(lines[line])) line++;
        if (!lines[line]) return false;
        char *p = lines[line++], *v[6];
        for (unsigned j = 0; j < 6; j++) if (!(v[j] = token(&p))) return false;
        uint64_t num, lba, kind, unit, offset;
        if (token(&p) || !number(v[0], 99, &num) || num != i + 1 ||
            !number(v[1], MAX_IMAGE / 2048, &lba) ||
            !number(v[2], 4, &kind) || (kind != 0 && kind != 4) ||
            !number(v[3], 2352, &unit) || (unit != 2048 && unit != 2352) ||
            !number(v[5], MAX_IMAGE, &offset)) return false;
        Track *t = &tracks[i];
        *t = (Track){ .number = num, .base = lba, .unit = unit,
                     .offset = offset, .data = kind == 4, .hd = lba >= 45000,
                     .data_offset = unit == 2352 ? 16 : 0 };
        t->path = track_path(path, v[4]);
        if (!stat_track(t) || (t->file_size - offset) % unit) return false;
        t->frames = (t->file_size - offset) / unit;
    }
    while (lines[line]) if (*g_strstrip(lines[line++])) return false;
    return true;
}
static bool parse_cue(char **lines, const char *path, Track *tracks, unsigned *count)
{
    char *file = NULL; unsigned file_id = 0; bool hd = false, ok = false;
    Track *current = NULL;
    for (unsigned line = 0; lines[line]; line++) {
        if (strlen(lines[line]) >= 4096) goto done;
        char *p = lines[line], *cmd = token(&p);
        if (!cmd) continue;
        if (eq(cmd, "REM")) {
            p = g_strstrip(p);
            if (eq(p, "HIGH-DENSITY AREA")) hd = true;
            else if (eq(p, "SINGLE-DENSITY AREA")) hd = false;
        } else if (eq(cmd, "FILE")) {
            char *name = token(&p), *type = token(&p);
            if (!name || !eq(type, "BINARY") || token(&p)) goto done;
            g_free(file); file = track_path(path, name);
            if (!file) goto done;
            file_id++; current = NULL;
        } else if (eq(cmd, "TRACK")) {
            uint64_t num; char *v = token(&p), *mode = token(&p);
            if (!file || *count == 99 || !number(v, 99, &num) || !num || !mode || token(&p)) goto done;
            bool data = eq(mode, "MODE1/2352") || eq(mode, "MODE1/2048");
            if (!data && !eq(mode, "AUDIO")) goto done;
            current = &tracks[(*count)++];
            *current = (Track){ .number = num, .data = data, .hd = hd,
                .unit = eq(mode, "MODE1/2048") ? 2048 : 2352,
                .data_offset = eq(mode, "MODE1/2352") ? 16 : 0,
                .file_id = file_id, .base = hd || num == 3 ? 45000 : 0,
                .path = g_strdup(file) };
        } else if (eq(cmd, "INDEX")) {
            char *v = token(&p), *stamp = token(&p); uint64_t idx, frame;
            if (!current || !number(v, 99, &idx) || !msf(stamp, &frame) || token(&p)) goto done;
            if (idx == 0) { current->index0 = frame; current->has_index0 = true; }
            if (idx == 1) { if (current->indexed) goto done; current->index1 = frame; current->indexed = true; }
        } else if (eq(cmd, "PREGAP") || eq(cmd, "POSTGAP")) {
            uint64_t gap;
            if (!current || !msf(token(&p), &gap) || token(&p)) goto done;
            /* Synthetic gaps consume no bytes in a BIN file. */
        } else if (!eq(cmd, "TITLE") && !eq(cmd, "PERFORMER") &&
                   !eq(cmd, "SONGWRITER") && !eq(cmd, "CATALOG") &&
                   !eq(cmd, "ISRC") && !eq(cmd, "FLAGS")) goto done;
    }
    if (!*count) goto done;
    for (unsigned i = 0; i < *count; i++) {
        Track *t = &tracks[i];
        if (!t->indexed || !stat_track(t) || t->file_size % t->unit ||
            (t->has_index0 && t->index0 > t->index1)) goto done;
        uint64_t end = t->file_size / t->unit;
        if (i + 1 < *count && tracks[i+1].file_id == t->file_id) {
            Track *next = &tracks[i+1];
            if (next->unit != t->unit) goto done;
            end = next->has_index0 ? next->index0 : next->index1;
        }
        if (t->index1 >= end || end > t->file_size / t->unit) goto done;
        t->offset = t->index1 * t->unit;
        t->frames = end - t->index1;
    }
    ok = true;
done:
    g_free(file); return ok;
}
static bool descriptor(XemuChihiroOptical *d, const char *path, bool gdi)
{
    GStatBuf st;
    if (g_stat(path, &st) || st.st_size <= 0 || st.st_size > MAX_DESCRIPTOR) return false;
    FILE *f = g_fopen(path, "rb"); if (!f) return false;
    char *text = g_malloc(MAX_DESCRIPTOR + 1);
    size_t n = fread(text, 1, MAX_DESCRIPTOR + 1, f);
    bool read_ok = !ferror(f) && n == (size_t)st.st_size && !memchr(text, 0, n);
    fclose(f);
    if (!read_ok) { g_free(text); return false; }
    text[n] = 0;
    /* UTF-8 BOM is optional in a descriptor. */
    char *begin = n >= 3 && !memcmp(text, "\xef\xbb\xbf", 3) ? text + 3 : text;
    char **lines = g_strsplit(begin, "\n", -1);
    Track *tracks = g_new0(Track, 99); unsigned count = 0;
    bool ok = gdi ? parse_gdi(lines, path, tracks, &count) : parse_cue(lines, path, tracks, &count);
    int selected = -1;
    if (ok) {
        for (unsigned i = 0; i < count; i++) if (tracks[i].data && tracks[i].hd) { selected = i; break; }
        if (selected < 0)
            for (unsigned i = 0; i < count; i++) if (tracks[i].data && tracks[i].number == 3) { selected = i; break; }
        if (selected < 0) {
            for (unsigned i = 0; i < count; i++) if (tracks[i].data) {
                if (selected >= 0) { ok = false; break; }
                selected = i;
            }
        }
        if (selected < 0) ok = false;
        if (ok) {
            Track *t = &tracks[selected];
            d->file = g_fopen(t->path, "rb");
            d->offset = t->offset; d->frames = t->frames;
            d->unit = t->unit; d->base = t->base; d->data_offset = t->data_offset;
            ok = d->file != NULL;
        }
    }
    for (unsigned i = 0; i < count; i++) g_free(tracks[i].path);
    g_free(tracks); g_strfreev(lines); g_free(text);
    return ok;
}
static bool select_chd(XemuChihiroOptical *d)
{
    static const uint32_t tags[] = {0x43484744, 0x43485432, 0x43485452}; /* CHGD, CHT2, CHTR */
    Track tracks[99] = {0}; unsigned count = 0; uint32_t tag = 0;
    char meta[512];
    for (unsigned i = 0; i < G_N_ELEMENTS(tags); i++)
        if (xemu_chihiro_chd_metadata(d->chd, tags[i], 0, meta, sizeof(meta))) { tag = tags[i]; break; }
    if (!tag) return false;
    uint64_t start = 0;
    for (; count < 99; count++) {
        if (!xemu_chihiro_chd_metadata(d->chd, tag, count, meta, sizeof(meta))) break;
        unsigned num, frames, pad = 0, pre = 0, post = 0;
        char type[32], sub[32], pgtype[32] = "", pgsub[32]; int parsed;
        if (tag == tags[0])
            parsed = sscanf(meta, "TRACK:%u TYPE:%31s SUBTYPE:%31s FRAMES:%u PAD:%u PREGAP:%u PGTYPE:%31s PGSUB:%31s POSTGAP:%u", &num,type,sub,&frames,&pad,&pre,pgtype,pgsub,&post) == 9;
        else if (tag == tags[1])
            parsed = sscanf(meta, "TRACK:%u TYPE:%31s SUBTYPE:%31s FRAMES:%u PREGAP:%u PGTYPE:%31s PGSUB:%31s POSTGAP:%u", &num,type,sub,&frames,&pre,pgtype,pgsub,&post) == 8;
        else
            parsed = sscanf(meta, "TRACK:%u TYPE:%31s SUBTYPE:%31s FRAMES:%u", &num,type,sub,&frames) == 4;
        if (!parsed || !frames || num != count + 1 || pre >= frames || pad > frames ||
            start + frames > xemu_chihiro_chd_size(d->chd) / d->unit) return false;
        Track *t = &tracks[count];
        t->data = !strcmp(type, "MODE1_RAW") || !strcmp(type, "MODE1");
        t->number = num; t->data_offset = !strcmp(type, "MODE1_RAW") ? 16 : 0;
        /* MAME marks a stored pregap with V; synthetic gaps occupy no data. */
        uint64_t skip = pre && pgtype[0] == 'V' ? pre : 0;
        t->offset = (start + skip) * d->unit;
        if (skip + pad >= frames || post > MAX_IMAGE / 2048) return false;
        t->frames = frames - skip - pad;
        start += ((uint64_t)frames + 3) & ~UINT64_C(3);
    }
    if (!count) return false;
    int selected = -1;
    for (unsigned i = 0; i < count; i++) if (tracks[i].data && tracks[i].number == 3) { selected = i; break; }
    if (selected < 0)
        for (unsigned i = 0; i < count; i++) if (tracks[i].data) {
            if (selected >= 0) return false;
            selected = i;
        }
    if (selected < 0) return false;
    Track *t = &tracks[selected];
    d->offset = t->offset; d->frames = t->frames;
    d->data_offset = t->data_offset; d->base = t->number == 3 ? 45000 : 0;
    return d->frames && d->data_offset + 2048 <= d->unit;
}
XemuChihiroOptical *xemu_chihiro_optical_open(const char *path, char *error, size_t capacity)
{
    const char *why = "Cannot open Chihiro optical image";
    XemuChihiroOptical *d = g_new0(XemuChihiroOptical, 1);
    if (!path || strlen(path) >= 4096) goto fail;
    FILE *f = g_fopen(path, "rb"); if (!f) goto fail;
    uint8_t magic[8]; size_t n = fread(magic, 1, sizeof(magic), f); fclose(f);
    const char *ext = strrchr(path, '.');
    why = "Unsupported or malformed optical track layout (CHD/CUE/GDI/ISO)";
    if (n == 8 && !memcmp(magic, "MComprHD", 8)) {
        d->chd = xemu_chihiro_chd_open(path); if (!d->chd) goto fail;
        d->unit = xemu_chihiro_chd_unit_size(d->chd);
        if (!select_chd(d)) goto fail;
        d->declared_sha1 = xemu_chihiro_chd_sha1(d->chd, d->sha1);
    } else if (eq(ext, ".cue") || eq(ext, ".gdi")) {
        if (!descriptor(d, path, eq(ext, ".gdi"))) goto fail;
    } else {
        GStatBuf st;
        if (g_stat(path, &st) || !S_ISREG(st.st_mode) || st.st_size <= 0 ||
            (uint64_t)st.st_size > MAX_IMAGE) goto fail;
        d->file = g_fopen(path, "rb"); if (!d->file) goto fail;
        uint8_t sig[7];
        if (!(st.st_size % 2048) && seek_read(d->file, 16 * 2048, sig, 7) && !memcmp(sig, "\1CD001\1", 7)) {
            d->unit = 2048;
        } else if (!(st.st_size % 2352) && seek_read(d->file, 16 * 2352 + 16, sig, 7) && !memcmp(sig, "\1CD001\1", 7)) {
            d->unit = 2352; d->data_offset = 16;
        } else goto fail;
        d->frames = (uint64_t)st.st_size / d->unit;
    }
    uint8_t pvd[2048];
    why = "Optical image has no supported MODE1 ISO9660 volume";
    if (!xemu_chihiro_optical_read(d, 16 * 2048, pvd, sizeof(pvd)) || memcmp(pvd, "\1CD001\1", 7)) goto fail;
    uint32_t root = (uint32_t)pvd[158] | (uint32_t)pvd[159]<<8 | (uint32_t)pvd[160]<<16 | (uint32_t)pvd[161]<<24;
    /* Standalone high-density track ISO/BINs retain absolute GD extents. */
    if (!d->base && root >= 45000 && root - 45000 < d->frames) d->base = 45000;
    if (root < d->base || root - d->base >= d->frames) goto fail;
    return d;
fail:
    xemu_chihiro_optical_close(d);
    if (error && capacity) snprintf(error, capacity, "%s", why);
    return NULL;
}
void xemu_chihiro_optical_close(XemuChihiroOptical *d)
{
    if (!d) return;
    xemu_chihiro_chd_close(d->chd);
    if (d->file) fclose(d->file);
    g_free(d);
}
uint64_t xemu_chihiro_optical_size(const XemuChihiroOptical *d) { return d ? d->frames * 2048 : 0; }
uint32_t xemu_chihiro_optical_base(const XemuChihiroOptical *d) { return d ? d->base : 0; }
bool xemu_chihiro_optical_sha1(const XemuChihiroOptical *d, uint8_t sha1[20])
{
    if (!d || !d->declared_sha1 || !sha1) return false;
    memcpy(sha1, d->sha1, 20); return true;
}
bool xemu_chihiro_optical_read(XemuChihiroOptical *d, uint64_t offset, void *buffer, size_t size)
{
    if (!d || (!buffer && size) || offset > d->frames * 2048 || size > d->frames * 2048 - offset) return false;
    uint8_t sector[2048], *out = buffer;
    while (size) {
        uint64_t frame = offset / 2048; size_t skip = offset % 2048;
        size_t n = MIN(size, 2048 - skip);
        uint64_t physical = d->offset + frame * d->unit + d->data_offset;
        bool ok = d->chd ? xemu_chihiro_chd_read(d->chd, physical, sector, 2048) : seek_read(d->file, physical, sector, 2048);
        if (!ok) return false;
        memcpy(out, sector + skip, n); out += n; offset += n; size -= n;
    }
    return true;
}
