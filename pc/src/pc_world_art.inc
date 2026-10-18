/* Per-instance native building skins. Geometry, UVs and centre-town assets stay native. */
#include "pc_world_art.h"
#include "pc_world_shop_catalogs.h"
#include "pc_dev_east.h"
#include "m_actor.h"
#include "m_common_data.h"
#include "m_scene_table.h"
#include "m_name_table.h"
#include "libforest/gbi_extensions.h"
#include <string.h>
#ifdef ACO_DEV_EAST
#include "pc_world_art_data.inc"
extern u8 obj_s_tailor_1_tex_txt[],obj_w_tailor_1_tex_txt[];
extern u8 obj_s_shop1_roof_txt[],obj_w_shop1_roof_txt[];
extern u16 obj_s_tailor_pal[],obj_shop1_pal[];
static u8 art_textures[3][2][2048] ATTRIBUTE_ALIGN(32);
static int art_ready[3][2];
static int art_service(ACTOR* actor) {
    if(!actor || !g_pc_dev_east || Save_Get(scene_no)!=SCENE_FG)return 0;
    int acre=(int)(actor->world.position.x/640);
    if(acre<ACO_TOWN_X0 || acre>=ACO_TOWN_X0+21)return 0;
    int town=(acre-ACO_TOWN_X0)/7;
    if(town==1 || town<0 || town>2)return 0;
    if(actor->npc_id>=SHOP0 && actor->npc_id<=SHOP3)return town==0?1:2;
    return actor->npc_id==NEEDLEWORK_SHOP?3:0;
}
static u8* art_texture(int service,int winter) {
    u8* dst=art_textures[service-1][winter];
    if(!art_ready[service-1][winter]) {
        const u8* src=service==3?(winter?obj_w_tailor_1_tex_txt:obj_s_tailor_1_tex_txt):
            (winter?obj_w_shop1_roof_txt:obj_s_shop1_roof_txt);
        const u8* patch=service==1?art_night_sign:service==2?art_outfitter_sign:art_studio_sign;
        const u16* pal=service==3?obj_s_tailor_pal:obj_shop1_pal;
        int w=service==3?64:128,h=service==3?64:32,pw=service==3?30:60;
        memcpy(dst,src,2048);
        for(int y=0;y<h;y++)for(int x=0;x<pw;x++) {
            int p=((y/8)*(w/8)+x/8)*32+(y%8)*4+(x%8)/2;
            int shift=x%2?0:4,index=(src[p]>>shift)&15;
            if(!(pal[index]&0x8000) && !(pal[index]&0x7000))continue;
            /* Tailor UVs rotate the atlas and mirror its vertical axis. */
            int patch_y=service==3?h-1-y:y;
            dst[p]=(dst[p]&~(15<<shift))|(patch[patch_y*pw+x]<<shift);
        }
        art_ready[service-1][winter]=1;
    }
    return dst;
}
#endif
void* pc_world_art_model(void* actor_ptr,void* model_ptr) {
#ifdef ACO_DEV_EAST
    int service=art_service((ACTOR*)actor_ptr);if(!service || !model_ptr)return model_ptr;
    typedef struct {void* original;int service;Gfx commands[128];} Skin;
    static Skin skins[64];static int count;
    for(int i=0;i<count;i++)if(skins[i].original==model_ptr && skins[i].service==service)return skins[i].commands;
    if(count>=64)return model_ptr;
    Gfx* original=(Gfx*)model_ptr;Skin* skin=skins+count;
    for(int i=0;i<128;i++) {
        skin->commands[i]=original[i];
        for(int winter=0;winter<2;winter++) {
            void* source=service==3?(void*)(winter?obj_w_tailor_1_tex_txt:obj_s_tailor_1_tex_txt):
                (void*)(winter?obj_w_shop1_roof_txt:obj_s_shop1_roof_txt);
            if(original[i].words.w1==(u32)(uintptr_t)source)
                skin->commands[i].words.w1=(u32)(uintptr_t)art_texture(service,winter);
        }
        if((original[i].words.w0>>24)==G_ENDDL) {
            skin->original=model_ptr;skin->service=service;count++;return skin->commands;
        }
    }
#endif
    return model_ptr;
}
unsigned short* pc_world_art_palette(void* actor,unsigned short* original) {
#ifdef ACO_DEV_EAST
    int service=art_service((ACTOR*)actor);
    if(service==1)return (u16*)art_night_pal;
    if(service==2)return (u16*)art_outfitter_pal;
    if(service==3)return (u16*)art_studio_pal;
#endif
    return original;
}
unsigned short* pc_world_art_vendor_palette(void* actor,unsigned short* original) {
#ifdef ACO_DEV_EAST
    if(original && pc_world_commerce_is_night_vendor((ACTOR*)actor)) {
        static u16 palette[16];
        memcpy(palette,original,sizeof(palette));
        for(int i=0;i<16;i++) {
            u16 v=palette[i];if(!(v&0x8000))continue;
            int r=(v>>10)&31,g=(v>>5)&31,b=v&31;
            if(g>r+3 && g>b+2)palette[i]=0x8000|((g*3/4)<<10)|((g/3)<<5)|g;
        }
        return palette;
    }
#endif
    return original;
}
