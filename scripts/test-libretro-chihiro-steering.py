#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Check the actual Chihiro steering mapper across all signed axis values."""
from pathlib import Path
import os,shlex,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'ui/libretro-chihiro.c').read_text()
def function(signature):
 start=s.index(signature);return s[start:s.index('\n}',start)+2]
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define RETRO_DEVICE_INDEX_ANALOG_LEFT 0
#define RETRO_DEVICE_ID_ANALOG_X 0
#define RETRO_DEVICE_ID_JOYPAD_LEFT 6
#define RETRO_DEVICE_ID_JOYPAD_RIGHT 7
static int16_t raw;
static bool left,right;
static int16_t stick(unsigned p,unsigned i,unsigned a) {(void)p;(void)i;(void)a;return raw;}
static bool pad(unsigned p,unsigned id) {(void)p;return id==6?left:right;}
'''+function('static uint16_t axis_smooth(')+'\n'+function('static uint16_t steering(')+r'''
int main(void) {
 unsigned centered=0;
 const unsigned dzs[]={0,2,5,8,10,12,15,20,25,30};
 for(unsigned d=0;d<sizeof(dzs)/sizeof(dzs[0]);d++)
 for(int value=INT16_MIN;value<=INT16_MAX;value++) {
  raw=value;
  for(int invert=0;invert<2;invert++) {
   uint16_t ramp=0;
   bool dead=(abs(value)*100 < (int)dzs[d]*32767);
   int travel=dead?0:value;
   if(invert)travel=travel==INT16_MIN?INT16_MAX:-travel;
   assert(steering(0,&ramp,invert,dzs[d])==(uint16_t)(0x8000+travel));
   assert(ramp==0x8000);
   if(dead)centered++;
  }
 }
 for(int invert=0;invert<2;invert++) {
  raw=2000;left=true;right=false;uint16_t ramp=0x8000;
  for(unsigned i=0;i<8;i++)steering(0,&ramp,invert,12);
  assert(ramp==(invert?0xffff:0));
  left=false;right=true;ramp=0x8000;
  for(unsigned i=0;i<8;i++)steering(0,&ramp,invert,12);
  assert(ramp==(invert?0:0xffff));
  left=right=true;ramp=0x8000;assert(steering(0,&ramp,invert,12)==0x8000);
  left=right=false;assert(steering(0,&ramp,invert,12)==0x8000);
 }
 printf("1310720 analog mappings across all 10 settings and digital/inverted steering passed; %u centered mappings; endpoints retained\n",centered);
}
'''
with tempfile.TemporaryDirectory(prefix='chihiro-steering-') as tmp:
 p=Path(tmp);(p/'test.c').write_text(code)
 subprocess.run(shlex.split(os.environ.get('CC','cc'))+['-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
