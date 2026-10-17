/* Native inventory goods and paid design services for the expanded world. */
#include "pc_world_shop_catalogs.h"
#include "m_name_table.h"
#include "m_common_data.h"
#include "m_house.h"
#include "m_play.h"
#include "m_player_lib.h"
#include "m_scene_table.h"
#include "m_event.h"
#include "m_demo.h"
#include "m_controller.h"
#include "m_needlework_ovl.h"
#include "pc_dev_east.h"
#include "pc_world_progression.h"
#include "pc_pause_menu.h"
#include "pc_menu_util.h"
#include "m_font.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

pc_shop_result_t pc_customization_repaint(Private_c* player,mHm_hs_c* home,int palette,u32 price) {
    if(!player || !home || palette<0 || palette>=12 || !price)return PC_SHOP_ERR_ITEM;
    if(memcmp(&player->player_ID,&home->ownerID,sizeof(PersonalID_c)))return PC_SHOP_ERR_ITEM;
    if(home->next_outlook_pal==palette)return PC_SHOP_ERR_NOT_FOR_SALE;
    if(player->inventory.wallet<price)return PC_SHOP_ERR_FUNDS;
    home->next_outlook_pal=(u8)palette;
    player->state_flags|=mPr_FLAG_UPDATE_OUTLOOK_PENDING;
    player->inventory.wallet-=price;
    return PC_SHOP_OK;
}


pc_shop_result_t pc_customization_print_pattern(Private_c* player,int slot,u32 price) {
    if(!player || slot<0 || slot>=mPr_ORIGINAL_DESIGN_COUNT || !price || !player->my_org[slot].flag_design_set)return PC_SHOP_ERR_ITEM;
    pc_shop_item_t item={(mActor_name_t)(ITM_MY_ORG_UMBRELLA0+slot),price,"Pattern umbrella",0};
    return pc_shop_buy(player,&item,1);
}

#ifndef PC_WORLD_SHOP_TEST
int g_pc_dev_shop_preview;
int pc_world_commerce_dev_spawn(int service,float* x,float* z) {
#ifdef ACO_DEV_EAST
    for(int bz=1;bz<=6;bz++)for(int bx=ACO_TOWN_X0+1;bx<ACO_TOWN_X0+20;bx++) {
        int town=(bx-ACO_TOWN_X0)/7;if(town==1)continue;
        mActor_name_t* items=Save_Get(fg[bz-1][bx-1]).items[0];
        for(int unit=0;unit<256;unit++) {
            int item=items[unit];int id=item>=SHOP0 && item<=SHOP3?(town==0?1:2):item==NEEDLEWORK_SHOP?3:0;
            if(id!=service)continue;
            *x=bx*640+(unit%16+.5f)*40-20-(id==3?64:68.29f);
            *z=bz*640+(unit/16+.5f)*40+20+(id==3?64:68.29f)+75;
            return 1;
        }
    }
#else
    (void)service;(void)x;(void)z;
#endif
    return 0;
}
static const pc_shop_item_t maritime_stock[]={
    {ITM_ROD,500,"Fishing rod",0},{ITM_NET,500,"Bug net",0},
    {ITM_SHOVEL,500,"Shovel",0},{ITM_AXE,500,"Axe",0},
    {ITM_SIGNBOARD,500,"Trail signboard",0},{ITM_UMBRELLA_START+3,350,"Travel umbrella",0}
};
static const pc_shop_catalog_t maritime={"MARITIME OUTFITTER","Every day - tools for the next shore",maritime_stock,6,NULL,NULL};
static const char* paint_names[]={"Red roof","Orange roof","Yellow roof","Pale green roof","Green roof","Sky blue roof","Blue roof","Purple roof","Pink roof","Black roof","White roof","Brown roof"};
static pc_shop_item_t paint_rows[8],print_rows[8];
static pc_shop_catalog_t paint_catalog,print_catalog;
static int paint_page,design_pending;
static GAME_PLAY* commerce_play;
static void studio_confirm(Private_c*,const pc_shop_item_t*,void*);
static void print_confirm(Private_c*,const pc_shop_item_t*,void*);
static const pc_shop_item_t studio_actions[]={
    {1,0,"Repaint my roof",0},{2,0,"Print a pattern umbrella",0},{3,0,"Custom design workbench",0}
};
static const pc_shop_catalog_t studio={"COLOR & CLOTH STUDIO","Roof colors and your own saved designs",studio_actions,3,studio_confirm,NULL};
static void paint_confirm(Private_c* player,const pc_shop_item_t* row,void* context);
static void open_paint(int page) {
    paint_page=page;
    for(int i=0;i<6;i++)paint_rows[i]=(pc_shop_item_t){(mActor_name_t)(page*6+i+1),1000,paint_names[page*6+i],0};
    paint_rows[6]=(pc_shop_item_t){100,0,page?"Previous colors":"More colors",0};
    paint_rows[7]=(pc_shop_item_t){101,0,"Back to studio",0};
    paint_catalog=(pc_shop_catalog_t){"ROOF PAINTING","Native roof repaint - applied on next town load",paint_rows,8,paint_confirm,NULL};
    pc_world_shops_open(&paint_catalog);
}
static void paint_confirm(Private_c* player,const pc_shop_item_t* row,void* context) {
    (void)context;
    if(row->item==100){open_paint(!paint_page);return;}
    if(row->item==101){pc_customization_shop_open();return;}
    int index=Common_Get(player_no);
    if(index<0 || index>=PLAYER_NUM){pc_world_shops_set_status("Only resident homeowners can order paint.");return;}
    mHm_hs_c* home=Save_GetPointer(homes[mHS_get_arrange_idx(index)]);
    pc_shop_result_t result=pc_customization_repaint(player,home,row->item-1,row->price);
    pc_world_shops_set_status(result==PC_SHOP_OK?"Paint ordered! Your roof updates on the next town load.":
        result==PC_SHOP_ERR_FUNDS?"You need 1,000 Bells.":result==PC_SHOP_ERR_NOT_FOR_SALE?"That color is already applied or ordered.":"A resident-owned home is required.");
}
static const char* pattern_names[]={"Pattern 1 umbrella","Pattern 2 umbrella","Pattern 3 umbrella","Pattern 4 umbrella","Pattern 5 umbrella","Pattern 6 umbrella","Pattern 7 umbrella","Pattern 8 umbrella"};
static void studio_confirm(Private_c* player,const pc_shop_item_t* row,void* context) {
    (void)player;(void)context;
    if(row->item==1){open_paint(0);return;}
    if(row->item==2){
        int count=0;
        for(int i=0;i<8;i++)if(player->my_org[i].flag_design_set)
            print_rows[count++]=(pc_shop_item_t){(mActor_name_t)(ITM_MY_ORG_UMBRELLA0+i),350,pattern_names[i],0};
        if(!count){pc_world_shops_set_status("Create a design at the workbench first.");return;}
        print_catalog=(pc_shop_catalog_t){"PATTERN PRINTING","350 Bells - prints your saved designs",print_rows,count,print_confirm,NULL};
        pc_world_shops_open(&print_catalog);return;
    }
    if(row->item==3){design_pending=1;pc_world_shops_close();}
}
static void print_confirm(Private_c* player,const pc_shop_item_t* row,void* context) {
    (void)context;
    pc_shop_result_t result=pc_customization_print_pattern(player,row->item-ITM_MY_ORG_UMBRELLA0,row->price);
    pc_world_shops_set_status(result==PC_SHOP_OK?"Your pattern umbrella is in your pockets!":result==PC_SHOP_ERR_FUNDS?"You need 350 Bells.":result==PC_SHOP_ERR_FULL?"Your pockets are full.":"Create that design at the workbench first.");
}
void pc_maritime_outfitter_open(void){pc_world_shops_open(&maritime);}
void pc_customization_shop_open(void){pc_world_shops_open(&studio);}

#ifdef ACO_DEV_EAST
static int service_for_actor(ACTOR* actor) {
    if(!g_pc_dev_east || Save_Get(scene_no)!=SCENE_FG)return 0;
    int bx,bz;mFI_Wpos2BlockNum(&bx,&bz,actor->world.position);
    if(bx<ACO_TOWN_X0 || bx>=ACO_TOWN_X0+21 || bz<0 || bz>6)return 0;
    int town=(bx-ACO_TOWN_X0)/7;
    if(town==1)return 0;
    if(actor->npc_id>=SHOP0 && actor->npc_id<=SHOP3)return town?PC_WORLD_SERVICE_MARITIME_OUTFITTER:PC_WORLD_SERVICE_NIGHT_MARKET;
    if(actor->npc_id==NEEDLEWORK_SHOP)return PC_WORLD_SERVICE_CUSTOMIZATION;
    return 0;
}
/* Counter service replaces the copied building's original shared Nook/Able
 * interior and hours. Keeping a live proc preserves the native actor lifecycle. */
static void commerce_structure_wait(ACTOR* actor,GAME* game){(void)actor;(void)game;}
#endif
void pc_world_commerce_tick(GAME_PLAY* play) {
#ifdef ACO_DEV_EAST
    if(!g_pc_dev_east || !play || Save_Get(scene_no)!=SCENE_FG || mEv_IsTitleDemo())return;
    commerce_play=play;
    for(int part=0;part<ACTOR_PART_NUM;part++)for(ACTOR* a=play->actor_info.list[part].actor;a;a=a->next_actor)
        if(service_for_actor(a))a->mv_proc=commerce_structure_wait;
    if(design_pending && !g_pc_paused && play->submenu.mode==mSM_MODE_IDLE) {
        design_pending=0;
        mSM_open_submenu(&play->submenu,mSM_OVL_NEEDLEWORK,mNW_OPEN_DESIGN,Common_Get(player_no));
        return;
    }
    if(g_pc_paused || pc_world_shops_active() || play->submenu.mode!=mSM_MODE_IDLE ||
       play->fb_mode!=FBDEMO_MODE_NONE || play->fb_wipe_mode!=WIPE_MODE_NONE || mDemo_Get_talk_actor())return;
    PLAYER_ACTOR* p=get_player_actor_withoutCheck(play);
    if(p && chkTrigger(BUTTON_A) && pc_world_shops_try_open_at(play,p->actor_class.world.position.x,p->actor_class.world.position.z)) {
        play->game.pads[0].on.button&=~BUTTON_A;play->game.pads[0].now.button&=~BUTTON_A;
    }
#else
    (void)play;
#endif
}
void pc_world_commerce_hud(GAME* game) {
#ifdef ACO_DEV_EAST
    if(!g_pc_dev_east || !commerce_play || game!=&commerce_play->game || g_pc_paused ||
       Save_Get(scene_no)!=SCENE_FG || commerce_play->submenu.mode!=mSM_MODE_IDLE)return;
    PLAYER_ACTOR* p=get_player_actor_withoutCheck(commerce_play);if(!p)return;
    const char* notice=pc_world_progression_notice_at(p->actor_class.world.position.x,p->actor_class.world.position.z);
    pc_world_service_anchor_t anchors[8];int count=pc_world_service_anchors_get(commerce_play,anchors,8);
    const char* names[]={"","Night Market - interact to browse","Maritime Outfitter - interact to browse","Color & Cloth Studio - interact to browse"};
    if(!notice)for(int i=0;i<count;i++) {
        float dx=p->actor_class.world.position.x-anchors[i].x,dz=p->actor_class.world.position.z-anchors[i].z;
        if(dx*dx+dz*dz<anchors[i].interact_radius*anchors[i].interact_radius){notice=names[anchors[i].service_id];break;}
    }
    if(notice){mFont_SetMatrix(game->graph,mFont_MODE_FONT);pc_menu_draw_centered(game,notice,216,255,240,180,255,.78f);mFont_UnSetMatrix(game->graph,mFont_MODE_FONT);}
#else
    (void)game;
#endif
}

#ifdef ACO_DEV_EAST
#include "sys_matrix.h"
#include "m_rcp.h"
#include "graph.h"
#include "libforest/gbi_extensions.h"
typedef struct {Vtx vertices[512];Gfx commands[512];int nv,nc;} ShopFacade;
static ShopFacade facades[3];static int facades_ready;
static void facade_color(ShopFacade* f,int r,int g,int b) {
    Gfx* p=f->commands+f->nc;gDPSetPrimColor(p,0,255,r,g,b,255);f->nc++;
}
static void facade_quad(ShopFacade* f,float x0,float y0,float z0,float x1,float y1,float z1,
                        float x2,float y2,float z2,float x3,float y3,float z3) {
    float v[4][3]={{x0,y0,z0},{x1,y1,z1},{x2,y2,z2},{x3,y3,z3}};
    Vtx* verts=f->vertices+f->nv;
    for(int i=0;i<4;i++){memset(verts+i,0,sizeof(*verts));for(int j=0;j<3;j++)verts[i].v.ob[j]=(short)lroundf(v[i][j]*.55f*16);verts[i].n.a=255;}
    Gfx* p=f->commands+f->nc;gSPVertex(p++,verts,4,0);gSPNTrianglesInit_5b(p++,2,0,1,2,0,2,3,0,0,0);
    f->nc=(int)(p-f->commands);f->nv+=4;
}
static void facade_box(ShopFacade* f,float x,float y,float z,float w,float h,float d) {
    facade_quad(f,x,y,z,x+w,y,z,x+w,y+h,z,x,y+h,z);
    facade_quad(f,x,y,z+d,x+w,y,z+d,x+w,y+h,z+d,x,y+h,z+d);
    facade_quad(f,x,y,z,x,y,z+d,x,y+h,z+d,x,y+h,z);
    facade_quad(f,x+w,y,z,x+w,y,z+d,x+w,y+h,z+d,x+w,y+h,z);
    facade_quad(f,x,y+h,z,x+w,y+h,z,x+w,y+h,z+d,x,y+h,z+d);
}
static void build_facade(int kind) {
    ShopFacade* f=facades+kind;Gfx* p=f->commands;
    gDPPipeSync(p++);gSPTexture(p++,0,0,0,0,G_OFF);
    gDPSetCycleType(p++,G_CYC_1CYCLE);gDPSetCombineMode(p++,G_CC_PRIMITIVE,G_CC_PRIMITIVE);
    gDPSetRenderMode(p++,G_RM_AA_ZB_OPA_SURF,G_RM_AA_ZB_OPA_SURF2);
    gSPLoadGeometryMode(p++,G_ZBUFFER|G_SHADE);f->nc=(int)(p-f->commands);
    static const int themes[3][3]={{65,52,101},{35,83,111},{163,76,61}};
    /* Slender wood supports keep the counter canopy grounded beside its door. */
    facade_color(f,91,65,43);
    facade_box(f,-126,0,77,6,137,6);
    facade_box(f,120,0,77,6,137,6);
    for(int stripe=0;stripe<6;stripe++) {
        const int* c=themes[kind];facade_color(f,stripe%2?225:c[0],stripe%2?215:c[1],stripe%2?175:c[2]);
        float x=-132+stripe*44;
        facade_quad(f,x,161,12,x+44,161,12,x+44,137,91,x,137,91);
        facade_quad(f,x,137,91,x+44,137,91,x+44,120,91,x,120,91);
    }
    facade_color(f,71,48,34);facade_box(f,-93,168,15,186,47,9);
    facade_color(f,220,192,135);facade_box(f,-85,174,25,170,35,2);
    if(kind==0) { /* Moon and two hanging lanterns. */
        facade_color(f,245,198,77);
        for(int i=0;i<12;i++) {
            float a=(72+i*18)*.0174532925f,b=(90+i*18)*.0174532925f;
            float c=(108+i*12)*.0174532925f,d=(120+i*12)*.0174532925f;
            facade_quad(f,13*cosf(a),191+13*sinf(a),29,
                13*cosf(b),191+13*sinf(b),29,
                8+13*cosf(d),191+13*sinf(d),29,
                8+13*cosf(c),191+13*sinf(c),29);
        }
        for(int side=-1;side<=1;side+=2){
            facade_color(f,74,48,36);facade_box(f,side*104-2,109,67,4,46,4);
            facade_color(f,246,184,72);facade_box(f,side*104-10,97,67,20,27,18);
            facade_color(f,73,43,33);facade_box(f,side*104-12,121,65,24,5,22);
        }
    } else if(kind==1) { /* Sailboat sign. */
        facade_color(f,30,78,108);
        facade_quad(f,-31,181,29,32,181,29,20,175,29,-20,175,29);
        facade_quad(f,-3,183,29,-3,205,29,-29,183,29,-29,183,29);
        facade_quad(f,2,183,29,2,204,29,26,183,29,26,183,29);
    } else { /* Painted color swatches. */
        static const int colors[5][3]={{181,50,54},{220,167,47},{77,139,81},{65,113,171},{139,83,161}};
        for(int i=0;i<5;i++){facade_color(f,colors[i][0],colors[i][1],colors[i][2]);facade_box(f,-58+i*24,183,29,18,17,2);}
    }
    p=f->commands+f->nc;gSPEndDisplayList(p++);f->nc=(int)(p-f->commands);
}
#endif
void pc_world_commerce_draw(GAME_PLAY* play) {
#ifdef ACO_DEV_EAST
    if(!g_pc_dev_east || !play || Save_Get(scene_no)!=SCENE_FG)return;
    if(!facades_ready){for(int i=0;i<3;i++)build_facade(i);facades_ready=1;}
    PLAYER_ACTOR* player=get_player_actor_withoutCheck(play);if(!player)return;
    pc_world_service_anchor_t anchors[8];int count=pc_world_service_anchors_get(play,anchors,8);
    for(int i=0;i<count;i++) {
        float dx=anchors[i].x-player->actor_class.world.position.x,dz=anchors[i].z-player->actor_class.world.position.z;
        if(dx*dx+dz*dz>1500*1500)continue;
        xyz_t pos={anchors[i].x,0,anchors[i].z};pos.y=mCoBG_GetBgY_OnlyCenter_FromWpos2(pos,0);
        Matrix_push();Matrix_translate(pos.x,pos.y,pos.z,MTX_LOAD);Matrix_scale(.0625f,.0625f,.0625f,MTX_MULT);
        OPEN_DISP(play->game.graph);
        gSPMatrix(NEXT_POLY_OPA_DISP,_Matrix_to_Mtx_new(play->game.graph),G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);
        gSPDisplayList(NEXT_POLY_OPA_DISP,facades[anchors[i].service_id-1].commands);
        CLOSE_DISP(play->game.graph);Matrix_pull();
    }
#else
    (void)play;
#endif
}

#endif /* !PC_WORLD_SHOP_TEST */
