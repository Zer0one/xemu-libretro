#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Native regressions for options and frontend state retained between loads."""
from pathlib import Path
import os, shlex, subprocess, tempfile
root = Path(__file__).resolve().parents[1]
front = (root / 'ui/libretro.c').read_text()
stubs = (root / 'ui/libretro-stubs.c').read_text()
shaders = (root / 'hw/xbox/nv2a/pgraph/vk/shaders.c').read_text()
def function(text, signature):
    start = text.index(signature)
    return text[start:text.index('\n}', start) + 2]
frame = front[front.index('    if (audio_batch_cb && !audio_async)', front.index('vk_audio:')):]
frame = frame[:frame.index('\n}')]
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "ui/libretro.h"
#define G_N_ELEMENTS(a) (sizeof(a)/sizeof((a)[0]))
#define qatomic_read(p) (*(p))
#define qatomic_set(p,v) (*(p)=(v))
static bool exchange(bool *p,bool v) { bool old=*p; *p=v; return old; }
#define qatomic_xchg(p,v) exchange(p,v)
#define le32_to_cpu(v) (v)
#define cpu_to_le32(v) (v)
static uint32_t ldl_le_p(const void *p) {uint32_t v;memcpy(&v,p,4);return v;}
static void stl_le_p(void *p,uint32_t v) {memcpy(p,&v,4);}
#define MAX(a,b) ((a)>(b)?(a):(b))
#define LRLOG_INFO(...) ((void)0)
#define qemu_fopen fopen
#define XEMU_REFRESH_RATE 59.94
#define EEPROM_VIDEO_FLAGS 0x94
#define EEPROM_VIDEO_WIDESCREEN 0x10000
#define EEPROM_VIDEO_LETTERBOX 0x100000
#define EEPROM_USER_CHECKSUM 0x60
#define EEPROM_USER_SECTION 0x64
#define EEPROM_USER_SECTION_LEN 0x5c
static uint8_t eeprom[256];
static bool smbus_storage_peek(uint32_t off,void *p,uint32_t n) { memcpy(p,eeprom+off,n);return true; }
static bool smbus_storage_poke(uint32_t off,const void *p,uint32_t n) { memcpy(eeprom+off,p,n);return true; }
static struct {struct {struct {const char *eeprom_path;} files;} sys;} g_config;
static bool emu_started_chihiro, dashboard_aspect_valid;
static uint32_t dashboard_aspect_flags;
static int opt_video_aspect;
static double audio_fraction;
static bool audio_async_needs_flush,audio_async;
static unsigned flushes,batches,last_batch;
static int64_t st_t,st_audio_sum,st_audio_max;
static int64_t g_get_monotonic_time(void) { return 1; }
void libretro_audio_flush(void) { flushes++; }
int libretro_audio_pull(int16_t *p,int n) { memset(p,0,(size_t)n*4);return n; }
static size_t audio_batch(const int16_t *p,size_t n) { assert(p);batches++;last_batch=n;return n; }
static retro_audio_sample_batch_t audio_batch_cb=audio_batch;
static uint32_t libretro_rumble[4],applied_rumble[4];
static bool libretro_rumble_enabled[4]={true,true,true,true};
static uint16_t motors[4][2];
static bool rumble(unsigned port,enum retro_rumble_effect effect,uint16_t strength)
{ motors[port][effect]=strength;return true; }
typedef struct {bool seed_service_done;} PGRAPHVkState;
typedef struct {PGRAPHVkState *vk_renderer_state;} PGRAPHState;
static const char *tag;
static unsigned seed_calls;
static const char *pgraph_seed_game_tag(void) {return tag;}
static void vk_seed_siblings(void) {seed_calls++;}
''' + '\n'.join([
    function(front, 'static uint32_t eeprom_checksum('),
    function(front, 'static void apply_video_aspect('),
    function(front, 'static unsigned audio_frames_per_run('),
    function(stubs, 'void libretro_input_reset_rumble(retro_set_rumble_state_t set_rumble_state)\n{'),
    function(stubs, 'void libretro_input_set_rumble('),
    function(stubs, 'void libretro_input_apply_rumble('),
    function(shaders, 'void pgraph_vk_seed_service('),
    'static void frame_audio(void) {\n' + frame + '\n}',
]) + r'''
static uint32_t flags(void) {uint32_t f;memcpy(&f,eeprom+0x94,4);return f;}
int main(int argc,char **argv)
{
    assert(argc==2);
    uint32_t original=0x80; memcpy(eeprom+0x94,&original,4);
    FILE *f=fopen(argv[1],"wb");assert(f);assert(fwrite(eeprom,1,256,f)==256);fclose(f);
    g_config.sys.files.eeprom_path=argv[1];
    opt_video_aspect=2;apply_video_aspect();assert(flags()==(original|0x10000));
    opt_video_aspect=0;apply_video_aspect();assert(flags()==original);
    opt_video_aspect=3;apply_video_aspect();assert(flags()==(original|0x100000));
    opt_video_aspect=1;apply_video_aspect();assert(flags()==original);
    /* The dashboard can change its saved video flags between loads. */
    original|=0x10000;f=fopen(argv[1],"r+b");assert(f);fseek(f,0x94,SEEK_SET);fwrite(&original,4,1,f);fclose(f);
    opt_video_aspect=0;apply_video_aspect();assert(flags()==original);
    uint32_t sum;memcpy(&sum,eeprom+0x60,4);
    assert(sum==eeprom_checksum(eeprom+0x64,0x5c));
    emu_started_chihiro=true;uint8_t copy[256];memcpy(copy,eeprom,256);
    opt_video_aspect=3;apply_video_aspect();assert(!memcmp(copy,eeprom,256));
    emu_started_chihiro=false;
    g_config.sys.files.eeprom_path="/nonexistent/xemu-eeprom.bin";
    dashboard_aspect_valid=false;original=0x80;memcpy(eeprom+0x94,&original,4);
    opt_video_aspect=2;apply_video_aspect();assert(flags()==(original|0x10000));
    opt_video_aspect=0;apply_video_aspect();assert(flags()==original);
    audio_async_needs_flush=true;frame_audio();frame_audio();assert(flushes==1 && batches==2);
    audio_fraction=0;audio_async_needs_flush=true;frame_audio();assert(flushes==2 && last_batch==800);
    audio_fraction=0;unsigned total=0;
    for(unsigned i=0;i<10000;i++){unsigned n=audio_frames_per_run();assert(n==800||n==801);total+=n;}
    assert(abs((int)total-(int)(10000*48000.0/59.94))<=1);
    libretro_input_set_rumble(0,1234,5678);libretro_input_apply_rumble(rumble);
    assert(motors[0][RETRO_RUMBLE_STRONG]==1234);
    libretro_rumble_enabled[0]=false;libretro_input_apply_rumble(rumble);assert(!motors[0][0]&&!motors[0][1]);
    libretro_rumble_enabled[0]=true;libretro_input_apply_rumble(rumble);assert(motors[0][0]==1234);
    libretro_input_reset_rumble(rumble);assert(!motors[0][0]&&!motors[0][1]&&!libretro_rumble[0]);
    libretro_input_set_rumble(0,1234,5678);libretro_input_apply_rumble(rumble);assert(motors[0][0]==1234);
    libretro_input_reset_rumble(NULL);assert(!libretro_rumble[0]&&!applied_rumble[0]);
    PGRAPHVkState a={0},b={0};PGRAPHState pg={&a};
    pgraph_vk_seed_service(&pg);assert(!seed_calls);
    tag="outr2";pgraph_vk_seed_service(&pg);pgraph_vk_seed_service(&pg);assert(seed_calls==1);
    pg.vk_renderer_state=&b;tag="outr2st";pgraph_vk_seed_service(&pg);assert(seed_calls==2);
    puts("Dashboard restore, per-load audio flush/cadence, rumble lifecycle and per-renderer seeding passed.");
}
'''
with tempfile.TemporaryDirectory(prefix='xemu-options-') as tmp:
    p=Path(tmp);(p/'test.c').write_text(code)
    subprocess.run(shlex.split(os.environ.get('CC','cc'))+['-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root),str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test'),str(p/'eeprom.bin')],check=True,env={**os.environ,"UBSAN_OPTIONS":"halt_on_error=1"})
