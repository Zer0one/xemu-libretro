#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise real Chihiro factory-state and transactional save functions, ROM-free."""
from pathlib import Path
import os,shlex,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
src=(root/'hw/xbox/chihiro/chihiro-usb.c').read_text()
board=(root/'hw/xbox/chihiro/chihiro.c').read_text()
def function(signature):
 start=src.index(signature);return src[start:src.index('\n}',start)+2]
constants=src[src.index('#define CHIHIRO_SAVE_MAGIC'):src.index('/* The firmware dump carries')]
restore_start=board.index('static void chihiro_restore_game_state(')
restore=board[restore_start:board.index('\n}',restore_start)+2]
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define qemu_fopen fopen
#define g_malloc0(n) calloc(1,n)
#define g_free free
static uint16_t lduw_le_p(const void *p) {const uint8_t *b=p;return b[0]|b[1]<<8;}
static void stw_le_p(void *p,uint16_t v) {uint8_t *b=p;b[0]=v;b[1]=v>>8;}
typedef struct {uint8_t ic11[512],extmem[2][65536];struct {bool backup_dirty;} an2131;} ChihiroUSBState;
static ChihiroUSBState state,*chihiro_qc_instance=&state;
static uint8_t factory[128],*chihiro_ic11_data=factory;
static unsigned chihiro_ic11_size=sizeof factory;
static struct {struct {struct {bool freeplay;} settings;} chihiro;} g_config;
static void chihiro_apply_link_settings(ChihiroUSBState *s) {(void)s;}
'''+constants+'\n'+ '\n'.join(function(f) for f in ('static void chihiro_apply_freeplay(', 'static void chihiro_seed_game_id(', 'void chihiro_usb_save_reset(', 'static bool chihiro_usb_save_read(', 'bool chihiro_usb_save_load('))+r'''
#define CHIHIRO_BOOTID_LEN 128
static const char *restore_path;
static uint8_t restore_id[4];
static char chihiro_save_path[1024];
static bool chihiro_read_image_bootid(uint8_t *bid) {memset(bid,0,CHIHIRO_BOOTID_LEN);memcpy(bid+0x30,restore_id,4);return true;}
static bool chihiro_resolve_save_path(void) {snprintf(chihiro_save_path,sizeof chihiro_save_path,"%s",restore_path);return true;}
static void chihiro_dimm_sys_load(void) {}
''' + restore + r'''
static void checksum(uint8_t *r) {unsigned sum=0;for(unsigned i=12;i<64;i++)sum+=r[i];stw_le_p(r+10,sum);}
static void verify(const char *id,bool freeplay) {
 for(unsigned o=0;o<128;o+=64) {
  assert(!memcmp(state.ic11+o+20,id,4));
  assert(state.ic11[o+34]==(freeplay?10:1));assert(state.ic11[o+35]==freeplay);
  unsigned sum=0;for(unsigned i=12;i<64;i++)sum+=state.ic11[o+i];assert(lduw_le_p(state.ic11+o+10)==sum);
 }
}
static void write_save(const char *p,unsigned version) {
 FILE *f=fopen(p,"wb");assert(f);uint32_t header[]={CHIHIRO_SAVE_MAGIC,version};
 assert(fwrite(header,sizeof header,1,f)==1);
 assert(fwrite(state.ic11,512,1,f)==1);
 assert(fwrite(state.extmem[1]+0x8000,0x8000,1,f)==1);
 if(version==2)assert(fwrite(state.extmem[1]+0x2000,0x5b40,1,f)==1);
 assert(!fclose(f));
}
int main(int argc,char **argv) {
 assert(argc==3);const char *valid=argv[1],*bad=argv[2];
 for(unsigned o=0;o<128;o+=64) {
  memcpy(factory+o,"ACBU0001",8);stw_le_p(factory+o+8,13);memcpy(factory+o+20,"SBJE",4);
  factory[o+28]=0x63;factory[o+34]=10;factory[o+35]=1;checksum(factory+o);
 }
 uint8_t original[128];memcpy(original,factory,128);
 memset(&state,0x42,sizeof state);g_config.chihiro.settings.freeplay=true;
 chihiro_usb_save_reset((const uint8_t*)"SGBZ");verify("SGBZ",true);
 assert(!memcmp(factory,original,128));
 for(unsigned i=0;i<65536;i++) {
  bool backup=(i>=0x8000)||(i>=0x2000&&i<0x7b40);
  assert(state.extmem[0][i]==0x42);assert(state.extmem[1][i]==(backup?0:0x42));
 }
 assert(!state.an2131.backup_dirty);
 /* Another title with a missing save starts fresh, not with SGBZ state. */
 memset(state.extmem[1]+0x8000,0x77,0x8000);
 g_config.chihiro.settings.freeplay=false;
 restore_path=bad;memcpy(restore_id,"SBHF",4);
 chihiro_restore_game_state();verify("SBHF",false);assert(!state.extmem[1][0x8000]);
 /* Valid v2 is loaded without retagging its owner or altering non-coin data. */
 state.ic11[200]=0x95;state.extmem[1][0x2000]=0x67;state.extmem[1][0x8000]=0x89;
 write_save(valid,2);chihiro_usb_save_reset((const uint8_t*)"SGBZ");
 g_config.chihiro.settings.freeplay=true;assert(chihiro_usb_save_load(valid));verify("SBHF",true);
 assert(state.ic11[200]==0x95 && state.extmem[1][0x2000]==0x67 && state.extmem[1][0x8000]==0x89);
 /* A truncated file must leave every byte of live state untouched. */
 ChihiroUSBState before=state;
 FILE *f=fopen(bad,"wb");assert(f);uint32_t hdr[]={CHIHIRO_SAVE_MAGIC,2};fwrite(hdr,sizeof hdr,1,f);
 uint8_t partial[600];memset(partial,0x99,sizeof partial);fwrite(partial,sizeof partial,1,f);fclose(f);
 assert(!chihiro_usb_save_load(bad));assert(!memcmp(&before,&state,sizeof state));
 /* Version 1 clears only the absent saved low window, preserving the mailbox. */
 write_save(valid,1);state.extmem[1][0x7c00]=0x55;assert(chihiro_usb_save_load(valid));
 assert(!state.extmem[1][0x2000] && state.extmem[1][0x8000]==0x89 && state.extmem[1][0x7c00]==0x55);
 uint8_t unknown[512]={0},copy[512]={0};chihiro_seed_game_id(unknown,(const uint8_t*)"SGBZ");assert(!memcmp(unknown,copy,512));
 memcpy(unknown,factory,128);unknown[10]^=1;memcpy(copy,unknown,512);
 chihiro_seed_game_id(unknown,(const uint8_t*)"SGBZ");assert(!memcmp(unknown,copy,64)); /* invalid slot */
 puts("Factory owners/checksums, missing-title isolation, v1/v2, truncated-load atomicity, firmware/work-RAM/mailbox preservation passed.");
}
'''
with tempfile.TemporaryDirectory(prefix='xemu-persistence-') as t:
 p=Path(t);(p/'test.c').write_text(code)
 subprocess.run(shlex.split(os.environ.get('CC','cc'))+['-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test'),str(p/'valid.sav'),str(p/'missing.sav')],check=True)
