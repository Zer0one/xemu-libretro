/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hw/xbox/chihiro/chihiro-image-layout.h"
static unsigned char image[1024*1024];
static void put32(unsigned char *p,unsigned n)
{ for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(n>>(i*8)); }
static bool read_image(void *opaque,uint64_t offset,void *out,size_t n)
{
    (void)opaque;
    if(offset>sizeof(image)||n>sizeof(image)-offset)return false;
    memcpy(out,image+offset,n);return true;
}
static void add_layout(unsigned mib,const char *id)
{
    uint64_t entries=(((uint64_t)mib-16)<<20)/16384+1;
    uint64_t data=4096+((entries*(entries<0xfff0?2:4)+4095)&~UINT64_C(4095));
    unsigned char *entry=image+data;
    entry[0]=7;memcpy(entry+2,"BOOT.ID",7);put32(entry+44,2);put32(entry+48,192);
    unsigned char *bid=image+data+16384;
    memcpy(bid,"BTID",4);memcpy(bid+32,"XBAM",4);memcpy(bid+48,id,4);
}
int main(void)
{
    const unsigned capacities[]={128,256,512,1024};
    unsigned cases=0;
    for(unsigned game=0;game<=sizeof(chihiro_titles)/sizeof(chihiro_titles[0]);game++) {
        const char *id=game<sizeof(chihiro_titles)/sizeof(chihiro_titles[0])?chihiro_titles[game].id:"ZZZZ";
        for(unsigned p=0;p<4;p++) {
            memset(image,0,sizeof(image));memcpy(image,"FATX",4);put32(image+8,32);put32(image+12,1);
            add_layout(capacities[p],id);ChihiroImageLayout out;
            assert(chihiro_image_layout(read_image,NULL,sizeof(image),&out));
            assert(out.dimm_mib==capacities[p]&&!memcmp(out.game_id,id,4));
            assert((out.title!=NULL)==(game<sizeof(chihiro_titles)/sizeof(chihiro_titles[0])));
            cases++;
        }
    }
    /* Same title has two valid layouts: refuse ambiguity instead of guessing. */
    memset(image,0,sizeof(image));memcpy(image,"FATX",4);put32(image+8,32);put32(image+12,1);
    add_layout(512,"SGBZ");add_layout(1024,"SGBZ");ChihiroImageLayout out;
    assert(!chihiro_image_layout(read_image,NULL,sizeof(image),&out));cases++;
    memset(image,0,sizeof(image));assert(!chihiro_image_layout(read_image,NULL,sizeof(image),&out));cases++;
    memcpy(image,"FATX",4);put32(image+8,3);put32(image+12,1);
    assert(!chihiro_image_layout(read_image,NULL,sizeof(image),&out));cases++;
    put32(image+8,32);put32(image+12,0xffffffff);
    assert(!chihiro_image_layout(read_image,NULL,sizeof(image),&out));cases++;
    put32(image+12,1);assert(!chihiro_image_layout(read_image,NULL,16,&out));cases++;
    printf("%u layout cases passed across all 14 catalogued IDs, unknown ID and all four capacities\n",cases);
    return 0;
}
