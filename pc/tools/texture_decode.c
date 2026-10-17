/* Batch texture codec using exactly the native client's original GC decoder.
 * Input: six little-endian u32 values (w,h,format,palformat,entries,size),
 * then raw texture bytes and BE palette. Output: linear RGBA bytes. */
#include "pc_platform.h"
#include <dolphin/gx/GXEnum.h>
/* Generated verbatim from pc_gx_texture.c by tools/build-texture-pack.py. */
#include "../build32/texture_decode.inc"
#include <fcntl.h>
#include <io.h>
int main(void) {
    _setmode(_fileno(stdin),_O_BINARY);
    _setmode(_fileno(stdout),_O_BINARY);
    unsigned int h[6];
    while (fread(h,sizeof(h),1,stdin)==1) {
        unsigned w=h[0],height=h[1],fmt=h[2],palformat=h[3],entries=h[4],size=h[5];
        if(!w||!height||w>1024||height>1024||entries>256||size>4194304) return 2;
        u8* src=calloc(1,size+65536);
        u8* rgba=malloc(w*height*4);
        u8 rawpal[512],palette[256][4];
        if(!src||!rgba) return 3;
        if(fread(src,1,size,stdin)!=size || fread(rawpal,2,entries,stdin)!=entries) return 4;
        build_palette(entries?rawpal:NULL,palformat,entries,palette,1);
        decode_gc_texture(src,rgba,w,height,fmt,palette);
        if(fwrite(rgba,4,w*height,stdout)!=w*height) return 5;
        fflush(stdout); free(src); free(rgba);
    }
    return 0;
}
