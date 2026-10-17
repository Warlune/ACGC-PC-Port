#include "pc_minimap.h"
#include "pc_platform.h"
#include "pc_settings.h"
#include "m_play.h"
#include "m_player_lib.h"
#include "m_common_data.h"
#include "m_scene_table.h"
#include "graph.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

PCSettings g_pc_settings;
common_data_t common_data;
int g_pc_paused, g_pc_window_w=1920, g_pc_window_h=1080;
static PLAYER_ACTOR player;
static int tile_count, rectangle_count, title_demo;
unsigned int pc_gbi_pack_runtime_ptr(uintptr_t addr,int is_ptr,const char* expr,const char* file,int line) {
    (void)is_ptr;(void)expr;(void)file;(void)line;return (unsigned int)addr;
}
int mFI_CheckFieldData(void) { return 1; }
mActor_name_t mFI_GetFieldId(void) { return mFI_FIELD_FG; }
int mEv_CheckTitleDemo(void) { return title_demo; }
PLAYER_ACTOR* get_player_actor_withoutCheck(GAME_PLAY* play) { (void)play; return &player; }
int pc_minimap_get_tile(int bx,int bz,const u8** texture,const u16** palette) {
    static u8 pixels[512];static u16 colors[16];
    if (bx<1||bx>FG_BLOCK_X_NUM||bz<1||bz>FG_BLOCK_Z_NUM) abort();
    tile_count++;*texture=pixels;*palette=colors;return 1;
}
Gfx* gfx_gSPTextureRectangle1(Gfx* gfx,int xl,int yl,int xh,int yh,int tile,int s,int t,int dx,int dy) {
    (void)tile;(void)s;(void)t;
    if (xh<=xl||yh<=yl||xl<0||yl<0||xh>1280||yh>960) abort();
    if (dx<0||dy<0) abort();
    rectangle_count++;
    memset(gfx,0,3*sizeof(*gfx));
    return gfx+3;
}

static void require(int ok,const char* message) {
    if (!ok) { fprintf(stderr,"FAIL: %s\n",message); exit(1); }
}
static int close_to(float a,float b) { return fabsf(a-b)<0.002f; }
int main(void) {
    const int outputs[][2]={{640,480},{1920,1080},{3440,1440},{3840,1080},{1080,1920}};
    for (int i=0;i<5;i++) for (int size=0;size<3;size++) {
        PCMinimapLayout m,nw,se;
        int w=outputs[i][0],h=outputs[i][1];
        require(pc_minimap_layout(&m,w,h,size,1.0f+FG_BLOCK_X_NUM/2.0f,1.0f+FG_BLOCK_Z_NUM/2.0f),"valid town position");
        require(m.left>=0&&m.left+m.width<320&&m.top+m.height<240,"map within screen");
        require(close_to(m.width*w/320/FG_BLOCK_X_NUM,m.height*h/240/FG_BLOCK_Z_NUM),"acre squares retain physical aspect");
        require(close_to((320-m.left-m.width)*w/320,6.0f*h/240),"constant right-edge margin");
        require(close_to(m.marker_x,m.left+m.width/2)&&close_to(m.marker_y,m.top+m.height/2),"town center maps to center");
        pc_minimap_layout(&nw,w,h,size,1,1);
        pc_minimap_layout(&se,w,h,size,FG_BLOCK_X_NUM+0.999f,FG_BLOCK_Z_NUM+0.999f);
        require(close_to(nw.marker_x,m.left)&&close_to(nw.marker_y,m.top),"northwest town boundary");
        require(se.marker_x>m.left+m.width*0.99f&&se.marker_y>m.top+m.height*0.99f&&
                se.marker_x<m.left+m.width&&se.marker_y<m.top+m.height,"southeast town boundary");
    }
    PCMinimapLayout m;
    require(!pc_minimap_layout(&m,640,480,0,FG_BLOCK_X_NUM+1,1),"reject east border");
    require(!pc_minimap_layout(&m,640,480,0,1,FG_BLOCK_Z_NUM+1),"reject south border");
    require(!pc_minimap_layout(&m,640,480,0,0,1),"reject non-town position");
    require(!pc_minimap_layout(&m,0,480,0,1,1),"reject unavailable output");
    require(!pc_minimap_layout(&m,640,480,0,NAN,1),"reject non-finite position");
    require(pc_minimap_layout(&m,640,480,99,1,1)&&close_to(m.height,fminf(60.0f,110.0f*FG_BLOCK_Z_NUM/FG_BLOCK_X_NUM)),"invalid size falls back to small");
    /* Exercise the actual HUD command producer, without a window or game assets. */
    static GAME_PLAY play;
    static GRAPH graph;
    static Gfx commands[4096];
    play.game.graph=&graph;
    play.submenu.mode=mSM_MODE_IDLE;
    Save_Get(scene_no)=SCENE_FG;
    player.actor_class.world.position.x=3.5f*mFI_BK_WORLDSIZE_X_F;
    player.actor_class.world.position.z=4.0f*mFI_BK_WORLDSIZE_Z_F;
    g_pc_settings.minimap=1;
    for(int opacity=10;opacity<=100;opacity+=10) {
        memset(commands,0,sizeof(commands));
        graph.font_thaga.thaGfx.head_p=commands;
        g_pc_settings.minimap_opacity=opacity;
        tile_count=rectangle_count=0;
        pc_minimap_draw(&play);
        require(tile_count==FG_BLOCK_TOTAL_NUM&&rectangle_count==FG_BLOCK_TOTAL_NUM+3,"all town acres, panel and player marker emitted");
        int colors=0;
        for(Gfx* g=commands;g<graph.font_thaga.thaGfx.head_p;g++) {
            if ((g->words.w0>>24)==G_SETPRIMCOLOR) {
                colors++;
                require((g->words.w1&255)==(opacity*255+50)/100,"opacity reaches all map/panel/marker colors");
            }
        }
        require(colors>0,"opacity color commands present");
    }
    for(int guard=0;guard<5;guard++) {
        graph.font_thaga.thaGfx.head_p=commands;
        g_pc_settings.minimap=guard!=0;
        g_pc_settings.minimap_opacity=guard==1?0:30;
        g_pc_paused=guard==2;
        title_demo=guard==3;
        Save_Get(scene_no)=guard==4?SCENE_MY_ROOM_S:SCENE_FG;
        pc_minimap_draw(&play);
        require(graph.font_thaga.thaGfx.head_p==commands,"hidden for disabled/transparent/paused/title/indoor states");
    }
    puts("PASS: 15 minimap layouts; town/marker command emission, opacity propagation and visibility guards");
    return 0;
}
