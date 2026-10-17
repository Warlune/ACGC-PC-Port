/* Read-only town HUD; uses the same acre art and palettes as the game's map. */
#include "pc_minimap.h"
#include "pc_platform.h"
#include "pc_settings.h"
#include "pc_pause_menu.h"
#include "m_play.h"
#include "m_player_lib.h"
#include "m_event.h"
#include "m_common_data.h"
#include "m_scene_table.h"
#include "m_rcp.h"
#include "graph.h"
#include <math.h>

extern int pc_minimap_get_tile(int bx, int bz, const u8** texture, const u16** palette);

int pc_minimap_layout(PCMinimapLayout* out, int output_w, int output_h,
                      int size, float acre_x, float acre_z) {
    if (!out || output_w<=0 || output_h<=0 || !isfinite(acre_x) || !isfinite(acre_z) ||
        acre_x<1.0f || acre_x>=FG_BLOCK_X_NUM+1.0f || acre_z<1.0f || acre_z>=FG_BLOCK_Z_NUM+1.0f) return 0;
    if (size<0 || size>2) size=0;
    static const float heights[]={60.0f,78.0f,96.0f};
    /* Stretch coordinates anchor to the real right edge. Counter-scale X
     * so acre squares retain their shape on widescreen outputs. */
    float sx=(4.0f/3.0f)*output_h/output_w;
    out->height=heights[size];
    out->width=out->height*((float)FG_BLOCK_X_NUM/FG_BLOCK_Z_NUM)*sx;
    if(out->width>110.0f*sx) {
        out->height*=110.0f*sx/out->width;
        out->width=110.0f*sx;
    }
    out->left=320.0f-6.0f*sx-out->width;
    out->top=8.0f;
    out->marker_x=out->left+(acre_x-1.0f)*out->width/FG_BLOCK_X_NUM;
    out->marker_y=out->top+(acre_z-1.0f)*out->height/FG_BLOCK_Z_NUM;
    return 1;
}

static Gfx* minimap_rect(Gfx* gfx, float x0,float y0,float x1,float y1) {
    return gfx_gSPTextureRectangle1(gfx,(int)(x0*4),(int)(y0*4),
        (int)(x1*4),(int)(y1*4),0,0,0,0,0);
}

void pc_minimap_draw(GAME_PLAY* play) {
    if (!g_pc_settings.minimap || g_pc_settings.minimap_opacity<=0 || g_pc_paused ||
        !play || !play->game.graph || play->submenu.mode!=mSM_MODE_IDLE ||
        play->fb_mode!=FBDEMO_MODE_NONE || play->fb_wipe_mode!=WIPE_MODE_NONE || mEv_IsTitleDemo() ||
        !mFI_CheckFieldData() || Save_Get(scene_no)!=SCENE_FG ||
        mFI_GetFieldId()!=mFI_FIELD_FG) return;
    PLAYER_ACTOR* player=get_player_actor_withoutCheck(play);
    if (!player) return;
    PCMinimapLayout map;
    if (!pc_minimap_layout(&map,g_pc_window_w,g_pc_window_h,g_pc_settings.minimap_size,
            player->actor_class.world.position.x/mFI_BK_WORLDSIZE_X_F,
            player->actor_class.world.position.z/mFI_BK_WORLDSIZE_Z_F)) return;
    int percent=g_pc_settings.minimap_opacity;
    if (percent>100) percent=100;
    int alpha=(percent*255+50)/100;
    float sx=(4.0f/3.0f)*g_pc_window_h/g_pc_window_w;
    GRAPH* graph=play->game.graph;
    OPEN_DISP(graph);
    Gfx* gfx=NOW_FONT_DISP;
    gDPNoOpTag(gfx++,PC_NOOP_WIDESCREEN_STRETCH);
    gDPPipeSync(gfx++);
    gDPSetOtherMode(gfx++,
        G_AD_DISABLE|G_CD_MAGICSQ|G_CK_NONE|G_TC_FILT|G_TF_BILERP|G_TT_RGBA16|
        G_TL_TILE|G_TD_CLAMP|G_TP_NONE|G_CYC_1CYCLE|G_PM_NPRIMITIVE,
        G_AC_NONE|G_ZS_PRIM|G_RM_XLU_SURF|G_RM_XLU_SURF2);
    gDPSetCombineMode(gfx++,G_CC_PRIMITIVE,G_CC_PRIMITIVE);
    gDPSetPrimColor(gfx++,0,0,10,18,22,alpha);
    gfx=minimap_rect(gfx,map.left-2*sx,map.top-2,map.left+map.width+2*sx,map.top+map.height+2);
    gDPPipeSync(gfx++);
    /* Texture color and alpha are modulated by the user's opacity. */
    gDPSetCombineMode(gfx++,G_CC_MODULATEIA_PRIM,G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(gfx++,0,0,255,255,255,alpha);
    for (int z=0;z<FG_BLOCK_Z_NUM;z++) for (int x=0;x<FG_BLOCK_X_NUM;x++) {
        const u8* texture;
        const u16* palette;
        if (!pc_minimap_get_tile(x+1,z+1,&texture,&palette)) continue;
        float x0=map.left+x*map.width/FG_BLOCK_X_NUM, x1=map.left+(x+1)*map.width/FG_BLOCK_X_NUM;
        float y0=map.top+z*map.height/FG_BLOCK_Z_NUM, y1=map.top+(z+1)*map.height/FG_BLOCK_Z_NUM;
        gDPPipeSync(gfx++);
        gDPLoadTLUT_Dolphin(gfx++,15,16,1,palette);
        gDPLoadTextureBlock_4b_Dolphin(gfx++,texture,G_IM_FMT_CI,32,32,15,0,0,0,0);
        gfx=gfx_gSPTextureRectangle1(gfx,(int)(x0*4),(int)(y0*4),(int)(x1*4),(int)(y1*4),
            0,0,0,(int)(32*1024/(x1-x0)),(int)(32*1024/(y1-y0)));
    }
    gDPPipeSync(gfx++);
    gDPSetCombineMode(gfx++,G_CC_PRIMITIVE,G_CC_PRIMITIVE);
    gDPSetPrimColor(gfx++,0,0,15,20,22,alpha);
    gfx=minimap_rect(gfx,map.marker_x-2*sx,map.marker_y-2,map.marker_x+2*sx,map.marker_y+2);
    gDPPipeSync(gfx++);
    gDPSetPrimColor(gfx++,0,0,255,245,145,alpha);
    gfx=minimap_rect(gfx,map.marker_x-sx,map.marker_y-1,map.marker_x+sx,map.marker_y+1);
    gDPPipeSync(gfx++);
    gDPNoOpTag(gfx++,PC_NOOP_WIDESCREEN_STRETCH_OFF);
    SET_FONT_DISP(gfx);
    CLOSE_DISP(graph);
}
