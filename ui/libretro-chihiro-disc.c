/* SPDX-License-Identifier: MIT
 * ISO loader/security layout adapted from Tovarichtch/chihiro-netboot.
 * See licenses/chihiro-netboot.license.txt. DES remains the shared device code.
 */
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "libretro-profile.h"
#include "libretro-chihiro-disc.h"
#if XEMU_LIBRETRO_PROFILE != XEMU_PROFILE_XBOX
#include "libretro-chihiro-optical.h"
#include "libretro-chihiro-key-search.h"
#include "hw/xbox/chihiro/chihiro-des.h"
#include "hw/xbox/chihiro/chihiro-image-layout.h"

typedef struct Disc {
    XemuChihiroOptical *optical;
    char *path;
    uint64_t payload, size, key;
    uint32_t base;
    unsigned key_attempts;
    const XemuChihiroMedia *media;
    ChihiroImageLayout layout;
    char source[4352], warning[512];
} Disc;
static Disc *active;
static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static uint32_t be32(const uint8_t *p)
{
    return (uint32_t)p[3] | (uint32_t)p[2]<<8 | (uint32_t)p[1]<<16 | (uint32_t)p[0]<<24;
}
static bool same_name(const char *a, const char *b)
{
    while (*a && *b) {
        unsigned x=(unsigned char)*a++, y=(unsigned char)*b++;
        if (x>='a' && x<='z') x-='a'-'A';
        if (y>='a' && y<='z') y-='a'-'A';
        if (x!=y) return false;
    }
    return !*a && !*b;
}
static void release(Disc *d)
{
    if (d) { xemu_chihiro_optical_close(d->optical); free(d->path); free(d); }
}
void libretro_chihiro_disc_close(void) { release(active); active=NULL; }
int64_t libretro_chihiro_disc_size(const char *path)
{
    return active && path && !strcmp(active->path,path) ? (int64_t)active->size : -1;
}
static bool file_read(void *opaque, uint64_t offset, void *data, size_t size)
{
    return offset <= LONG_MAX && fseek(opaque, (long)offset, SEEK_SET) == 0 &&
           fread(data, 1, size, opaque) == size;
}
unsigned libretro_chihiro_disc_dimm_mib(const char *path)
{
    if (libretro_chihiro_disc_size(path) >= 0) return active->layout.dimm_mib;
    struct stat st;
    if (!path || stat(path, &st) || !S_ISREG(st.st_mode) || st.st_size <= 0) return 0;
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    ChihiroImageLayout layout;
    bool ok = chihiro_image_layout(file_read, file, (uint64_t)st.st_size, &layout);
    fclose(file);
    return ok ? layout.dimm_mib : 0;
}
/* A common logical MODE1 view supplies the same sectors for all containers. */
static bool iso_read(Disc *d, uint64_t offset, void *buffer, size_t size)
{
    return xemu_chihiro_optical_read(d->optical, offset, buffer, size);
}
static bool plain_read(Disc *d, uint64_t offset, void *buffer, size_t size)
{
    uint8_t block[65552], *out=buffer;
    if (offset>d->size || size>d->size-offset || (!buffer && size)) return false;
    while (size) {
        size_t skip=offset%8, n=size>65536 ? 65536 : size;
        size_t bytes=(skip+n+7)&~(size_t)7;
        if (!iso_read(d,d->payload+offset-skip,block,bytes)) return false;
        chihiro_des_decrypt_buffer(block,(uint32_t)bytes,d->key);
        memcpy(out,block+skip,n); out+=n; offset+=n; size-=n;
    }
    return true;
}
bool libretro_chihiro_disc_read(const char *path, uint64_t offset, void *buffer, size_t size)
{
    return libretro_chihiro_disc_size(path)>=0 && plain_read(active,offset,buffer,size);
}
typedef struct Entry { uint32_t lba,size; unsigned depth; } Entry;
static bool extent(Disc *d, uint32_t lba, uint32_t size, uint64_t *offset)
{
    if (lba<d->base) return false;
    *offset=(uint64_t)(lba-d->base)*2048;
    uint64_t total = xemu_chihiro_optical_size(d->optical);
    return *offset <= total && size <= total - *offset;
}
static bool find_file(Disc *d, const char *name, uint64_t *offset, uint32_t *size)
{
    uint8_t pvd[2048]; Entry queue[64]; unsigned count=1, records=0;
    if (!iso_read(d,16*2048,pvd,sizeof(pvd)) || memcmp(pvd,"\1CD001\1",7)) return false;
    queue[0]=(Entry){le32(pvd+158),le32(pvd+166),0};
    for (unsigned dir=0;dir<count;dir++) {
        Entry e=queue[dir]; uint64_t start;
        if(e.depth>8 || e.size>4*1024*1024 || !extent(d,e.lba,e.size,&start)) return false;
        for(uint32_t pos=0;pos<e.size;) {
            uint8_t data[2048]; size_t n=e.size-pos; if(n>2048)n=2048;
            if(!iso_read(d,start+pos,data,n))return false;
            for(size_t at=0;at<n;) {
                uint8_t *r=data+at; unsigned len=r[0];
                if(!len)break;
                if(len<34 || len>n-at || r[32]>len-33 || ++records>4096)return false;
                at+=len;
                if(r[32]==1 && (r[33]==0 || r[33]==1))continue;
                uint32_t lba=le32(r+2), bytes=le32(r+10); uint64_t off;
                if(lba!=be32(r+6) || bytes!=be32(r+14) || (r[25]&0x80) || !extent(d,lba,bytes,&off))return false;
                if(r[25]&2) {
                    if(count==64)return false;
                    for(unsigned j=0;j<count;j++)if(queue[j].lba==lba)return false;
                    queue[count++]=(Entry){lba,bytes,e.depth+1};
                } else {
                    char filename[256]; unsigned j=0;
                    for(;j<r[32] && r[33+j]!=';';j++) {
                        if(r[33+j]<32 || r[33+j]>126)return false;
                        filename[j]=(char)r[33+j];
                    }
                    filename[j]=0;
                    if(same_name(filename,name)){*offset=off;*size=bytes;return true;}
                }
            }
            pos+=(uint32_t)n;
        }
    }
    return false;
}
static bool disc_read(void *opaque, uint64_t offset, void *data, size_t size)
{
    return plain_read(opaque, offset, data, size);
}
const char *libretro_chihiro_disc_set(void)
{ return active && active->media ? active->media->set :
         active && active->layout.title ? active->layout.title->name : ""; }
const char *libretro_chihiro_disc_parent(void)
{ return active && active->media ? active->media->parent : ""; }
const char *libretro_chihiro_disc_key_source(void) { return active ? active->source : ""; }
const char *libretro_chihiro_disc_warning(void) { return active ? active->warning : ""; }
/* A key is accepted by the decrypted structure, not by its name/hash alone. */
static bool try_key(void *opaque, const XemuChihiroKey *key,
                    const char *source, const char *sha1, bool verified)
{
    Disc *d = opaque;
    d->key_attempts++;
    uint64_t loader; uint32_t len; uint8_t data[224]; char name[33];
    if (!find_file(d, key->loader, &loader, &len) || len < sizeof(data) ||
        !iso_read(d, loader, data, sizeof(data))) return false;
    memcpy(name, data + 192, 32); name[32] = 0;
    if (!find_file(d, name, &d->payload, &len) || !len || len % 8 ||
        len > 1024u * 1024 * 1024) return false;
    d->size = len; d->key = 0;
    for (unsigned i = 0; i < 8; i++) d->key |= (uint64_t)key->des[i] << (8*i);
    if (!chihiro_image_layout(disc_read, d, d->size, &d->layout)) return false;
    snprintf(d->source, sizeof(d->source), "%s", source);
    if (!verified) {
        if (d->media)
            snprintf(d->warning, sizeof(d->warning),
                "Chihiro: key SHA-1 differs from MAME for %s; accepted by content checks (SHA-1 %s, expected %s)",
                d->media->set, sha1, d->media->key_sha1);
        else
            snprintf(d->warning, sizeof(d->warning),
                "Chihiro: uncatalogued disc; key accepted by content checks, SHA-1 unverified (%s)", sha1);
    }
    return true;
}
bool libretro_chihiro_disc_prepare(const char *path,const char *keys_dir,char *error,size_t capacity)
{
    const char *why = "Cannot allocate Chihiro disc";
    Disc *d = calloc(1, sizeof(*d));
    if (!d) goto fail;
    d->optical = xemu_chihiro_optical_open(path, error, capacity);
    if (!d->optical) { release(d); return false; }
    d->base = xemu_chihiro_optical_base(d->optical);
    uint8_t sha1[20];
    bool hashed = xemu_chihiro_optical_sha1(d->optical, sha1);
    d->media = xemu_chihiro_media_lookup(path, hashed ? sha1 : NULL);
    bool found = false;
    if (d->media) {
        XemuChihiroKeysFound *keys = calloc(1, sizeof(*keys));
        if (!keys) goto fail;
        if (xemu_chihiro_find_keys(path, keys_dir, d->media, keys))
            for (size_t i = 0; i < keys->count && !found; i++)
                found = try_key(d, &keys->keys[i], keys->source[i], keys->sha1[i], keys->verified[i]);
        free(keys);
    }
    if (!found)
        found = xemu_chihiro_try_image_keys(path, keys_dir, d->media, try_key, d);
    if (!found) {
        why = d->key_attempts ? "Security dumps found, but none decrypt a valid Chihiro FATX/boot.id" :
              "Missing or invalid Chihiro DATA/PIC in MAME sets, beside the image, system keys or redump";
        goto fail;
    }
    d->path = malloc(strlen(path) + 1); if (!d->path) goto fail;
    strcpy(d->path, path); release(active); active = d; return true;
fail:
    release(d); if (error && capacity) snprintf(error, capacity, "%s", why); return false;
}

#else
const char *libretro_chihiro_disc_set(void) { return ""; }
const char *libretro_chihiro_disc_parent(void) { return ""; }
const char *libretro_chihiro_disc_key_source(void) { return ""; }
const char *libretro_chihiro_disc_warning(void) { return ""; }
void libretro_chihiro_disc_close(void) {}
int64_t libretro_chihiro_disc_size(const char *path) { (void)path; return -1; }
unsigned libretro_chihiro_disc_dimm_mib(const char *path) { (void)path; return 0; }
bool libretro_chihiro_disc_read(const char *path,uint64_t off,void *buf,size_t n)
{ (void)path;(void)off;(void)buf;(void)n;return false; }
bool libretro_chihiro_disc_prepare(const char *path,const char *dir,char *error,size_t n)
{ (void)path;(void)dir;if(error&&n)snprintf(error,n,"Optical Chihiro images require the Chihiro core");return false; }
#endif
