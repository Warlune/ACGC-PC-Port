/* Actual terrain atlas: orthographic native meshes, separate from map art. */
#include "pc_minimap.h"
#include "pc_minimap_style.h"
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



int pc_minimap_layout(PCMinimapLayout* out, int output_w, int output_h,
                      int size, float acre_x, float acre_z) {
    if (!out || output_w<=0 || output_h<=0 || !isfinite(acre_x) || !isfinite(acre_z) ||
        acre_x<0.0f || acre_x>=BLOCK_X_NUM || acre_z<PC_MAP_NORTH_CROP || acre_z>=PC_MINIMAP_ROWS) return 0;
    if (size<0 || size>2) size=0;
    static const float heights[]={60.0f,78.0f,96.0f};
    /* Stretch coordinates anchor to the real right edge. Counter-scale X
     * so acre squares retain their shape on widescreen outputs. */
    float sx=(4.0f/3.0f)*output_h/output_w;
    out->height=heights[size];
    out->width=out->height*((float)BLOCK_X_NUM/(PC_MINIMAP_ROWS-PC_MAP_NORTH_CROP))*sx;
    static const float max_widths[]={110,145,180};
    float limit=fminf(max_widths[size]*sx,320-12*sx);
    if(out->width>limit) {
        out->height*=limit/out->width;
        out->width=limit;
    }
    out->left=320.0f-6.0f*sx-out->width;
    out->top=8.0f;
    out->marker_x=out->left+acre_x*out->width/BLOCK_X_NUM;
    out->marker_y=out->top+(acre_z-PC_MAP_NORTH_CROP)*out->height/(PC_MINIMAP_ROWS-PC_MAP_NORTH_CROP);
    return 1;
}

#ifndef PC_MINIMAP_LAYOUT_TEST
#include "pc_dev_east.h"
#include "pc_dev_coast_walls.h"
#include "sys_matrix.h"
#include "libultra/gu.h"
#include "pc_minimap_symbols.h"

static Gfx map_scroll[]={gsSPEndDisplayList()};
static Gfx map_beach_color[]={gsDPSetEnvColor(255,255,255,255),gsSPEndDisplayList()};
static Lights1 map_light=gdSPDefLights1(140,140,140,115,115,115,0,0,127);
extern Gfx grd_s_f_1_model[];

void pc_minimap_draw(GAME_PLAY* play) {
    pc_minimap_set_overlay(NULL);
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
    pc_minimap_set_overlay(&map);
    static const void* symbol_field;
    static Uint32 symbol_time;
    Uint32 now=SDL_GetTicks();
    if(symbol_field!=g_fdinfo || now-symbol_time>=500) {
        PCMapSymbol symbols[PC_MAP_MAX_SYMBOLS];int count=0;
        for(int z=0;z<PC_MINIMAP_ROWS;z++)for(int x=0;x<BLOCK_X_NUM;x++) {
            /* A loaded boat/sign actor can temporarily replace its FG anchor.
             * The authored port terrain remains a stable dock location. */
            if(g_fdinfo->block_info[z*BLOCK_X_NUM+x].bg_info.block_type==mFM_BLOCK_TYPE_PORT &&
               count<PC_MAP_MAX_SYMBOLS)
                symbols[count++]=(PCMapSymbol){x+8.5f/16,z+7.5f/16,PC_MAP_PORT};
            mActor_name_t* items=g_fdinfo->block_info[z*BLOCK_X_NUM+x].fg_info.items_p;
            if(!items)continue;
            for(int unit=0;unit<UT_X_NUM*UT_Z_NUM && count<PC_MAP_MAX_SYMBOLS;unit++) {
                int kind=pc_minimap_symbol_type(items[unit]);
                if(kind<0)continue;
                float px=x+(unit%UT_X_NUM+0.5f)/UT_X_NUM;
                float pz=z+(unit/UT_X_NUM+0.5f)/UT_Z_NUM;
                /* BOAT and PORT_SIGN can represent the same dock. */
                int duplicate=0;
                float separation=kind==PC_MAP_PORT?0.30f:0.15f;
                for(int j=0;j<count;j++)if(symbols[j].kind==kind &&
                    fabsf(symbols[j].x-px)<separation && fabsf(symbols[j].z-pz)<separation)duplicate=1;
                if(!duplicate)symbols[count++]=(PCMapSymbol){px,pz,kind};
            }
        }
        pc_minimap_set_symbols(symbols,count);symbol_field=g_fdinfo;symbol_time=now;
    }
    static const void* field;
    static int next_tile;
    static unsigned cached_signature;
    unsigned signature=2166136261u;
    for(int tile=0;tile<BLOCK_X_NUM*PC_MINIMAP_ROWS;tile++) {
        mFM_combination_c id=g_fdinfo->block_info[tile].bg_info.bg_id;
        signature=(signature^id.combination_type^(id.height<<16))*16777619u;
    }
    signature=(signature^g_pc_settings.texture_replacements)*16777619u;
    signature=(signature^g_pc_settings.texture_filtering^g_pc_settings.anisotropy)*16777619u;
    signature=(signature^Common_Get(time.rtc_time.year)^Common_Get(time.rtc_time.month)*32^
        Common_Get(time.rtc_time.day))*16777619u;
    GRAPH* graph=play->game.graph;
    OPEN_DISP(graph);
    Gfx* gfx=NOW_FONT_DISP;
    if(field!=g_fdinfo || signature!=cached_signature) {
        field=g_fdinfo;cached_signature=signature;next_tile=0;
        gDPNoOpTag(gfx++,PC_MINIMAP_RESET);
    }
    /* Bake at most two acres per frame after a field/terrain/texture change.
     * Once complete, normal frames only composite the cached terrain. */
    for(int i=0;i<2 && next_tile<BLOCK_X_NUM*PC_MINIMAP_ROWS;i++) {
        int tile=next_tile++;
        int x=tile%BLOCK_X_NUM,z=tile/BLOCK_X_NUM;
        mFM_bg_info_c* bg=&g_fdinfo->block_info[z*BLOCK_X_NUM+x].bg_info;
        gDPNoOpTag(gfx++,PC_MINIMAP_CAPTURE+tile);
        gDPPipeSync(gfx++);
        gSPTexture(gfx++,65535,65535,0,0,G_ON);
        gDPSetOtherMode(gfx++,G_AD_NOTPATTERN|G_CD_MAGICSQ|G_CK_NONE|G_TC_FILT|
            G_TF_BILERP|G_TT_NONE|G_TL_TILE|G_TD_CLAMP|G_TP_PERSP|G_CYC_2CYCLE|G_PM_NPRIMITIVE,
            G_AC_NONE|G_ZS_PIXEL|G_RM_AA_ZB_OPA_SURF|G_RM_AA_ZB_OPA_SURF2);
        gSPSegment(gfx++,8,map_scroll);gSPSegment(gfx++,9,map_scroll);
        gSPSegment(gfx++,11,map_scroll);gSPSegment(gfx++,12,map_beach_color);gSPSegment(gfx++,13,map_scroll);
        gSPSetLights1(gfx++,map_light);
        Mtx* projection=GRAPH_ALLOC_TYPE(graph,Mtx,1);
        guOrtho(projection,0,640,-640,0,-1000,1000,1);
        gSPMatrix(gfx++,projection,G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_PROJECTION);
        Matrix_push();
        Matrix_RotateX(0x4000,MTX_LOAD);
        Matrix_translate(0,bg->bg_id.height*120,0,MTX_MULT);
        Matrix_scale(0.0625f,0.0625f,0.0625f,MTX_MULT);
        gSPMatrix(gfx++,_Matrix_to_Mtx_new(graph),G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);
        Matrix_pull();
        Gfx* opaque=bg->opaque_gfx;
        Gfx* translucent=bg->translucent_gfx;
#ifdef ACO_DEV_EAST
        int ridge=pc_dev_ridge_side(x,z);
        if(ridge) {
            gSPDisplayList(gfx++,grd_s_f_1_model);
            opaque=pc_dev_ridge_model(ridge,z);translucent=NULL;
        }
        Gfx* outer_coast=pc_dev_outer_coast_model(x,z);
        if(outer_coast){opaque=outer_coast;translucent=pc_dev_outer_coast_water_model(x,z);}
        opaque=pc_dev_outer_ground_model(x,z,opaque);
        if(pc_dev_world_void_block(x,z)){opaque=NULL;translucent=NULL;}
#endif
        if(opaque)gSPDisplayList(gfx++,opaque);
        if(translucent)gSPDisplayList(gfx++,translucent);
        gDPNoOpTag(gfx++,PC_MINIMAP_CAPTURE_END);
        if(next_tile==BLOCK_X_NUM*PC_MINIMAP_ROWS && g_pc_verbose)
            printf("[Minimap] Terrain atlas ready: %d x %d acres, signature=%08x\n",
                BLOCK_X_NUM,PC_MINIMAP_ROWS,cached_signature);
    }
    SET_FONT_DISP(gfx);
    CLOSE_DISP(graph);
}
#endif
