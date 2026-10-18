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
#include "ac_shop_indoor.h"
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
static int s_service_context;
static int s_service_scene;
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
static void commerce_seller_idle(ACTOR* actor,GAME* game){(void)actor;(void)game;}
#endif
int pc_world_commerce_is_repurposed_shop_actor(ACTOR* actor) {
#ifdef ACO_DEV_EAST
    int service = service_for_actor(actor);
    return service == PC_WORLD_SERVICE_NIGHT_MARKET ||
           service == PC_WORLD_SERVICE_MARITIME_OUTFITTER;
#else
    (void)actor;
    return 0;
#endif
}
int pc_world_commerce_hide_native_stock(void) {
#ifdef ACO_DEV_EAST
    return g_pc_dev_east && Save_Get(scene_no)==SCENE_SHOP0 &&
        (s_service_context==PC_WORLD_SERVICE_NIGHT_MARKET ||
         s_service_context==PC_WORLD_SERVICE_MARITIME_OUTFITTER);
#else
    return 0;
#endif
}
void pc_world_commerce_prepare_entry(ACTOR* actor,GAME_PLAY* play) {
#ifdef ACO_DEV_EAST
    int service_id;
    if(!actor || !play || !g_pc_dev_east || Save_Get(scene_no)!=SCENE_FG)return;
    service_id=service_for_actor(actor);
    if(service_id!=PC_WORLD_SERVICE_NIGHT_MARKET &&
       service_id!=PC_WORLD_SERVICE_MARITIME_OUTFITTER &&
       service_id!=PC_WORLD_SERVICE_CUSTOMIZATION)return;
    s_service_context=service_id;
    s_service_scene=service_id==PC_WORLD_SERVICE_CUSTOMIZATION?SCENE_NEEDLEWORK:SCENE_SHOP0;
#else
    (void)actor;(void)play;
#endif
}
int pc_world_commerce_override_npc(mActor_name_t* name) {
#ifdef ACO_DEV_EAST
    if(name && g_pc_dev_east && s_service_context==PC_WORLD_SERVICE_NIGHT_MARKET &&
       Save_Get(scene_no)==SCENE_SHOP0 && *name==SP_NPC_SHOP_MASTER) {
        *name=SP_NPC_MAMEDANUKI0;
        return 1;
    }
#else
    (void)name;
#endif
    return 0;
}
int pc_world_commerce_is_night_vendor(const ACTOR* actor) {
#ifdef ACO_DEV_EAST
    return actor && g_pc_dev_east && s_service_context==PC_WORLD_SERVICE_NIGHT_MARKET &&
           Save_Get(scene_no)==SCENE_SHOP0 && actor->part==ACTOR_PART_NPC &&
           actor->npc_id==SP_NPC_MAMEDANUKI0;
#else
    (void)actor;
    return 0;
#endif
}
void pc_world_commerce_tick(GAME_PLAY* play) {
#ifdef ACO_DEV_EAST
    if(!g_pc_dev_east || !play || mEv_IsTitleDemo())return;
    commerce_play=play;
    if(design_pending && !g_pc_paused && play->submenu.mode==mSM_MODE_IDLE) {
        design_pending=0;
        mSM_open_submenu(&play->submenu,mSM_OVL_NEEDLEWORK,mNW_OPEN_DESIGN,Common_Get(player_no));
        return;
    }
    int scene=Save_Get(scene_no);
    if(scene==SCENE_FG) {
        /* The native storefront door actor handles entry and writes its own
         * service-specific return point. Clear only after the interior exit. */
        if(s_service_context && (play->fb_mode!=FBDEMO_MODE_NONE || play->fb_wipe_mode!=WIPE_MODE_NONE ||
           play->fb_fade_type!=FADE_TYPE_NONE))return;
        s_service_context=0;
        s_service_scene=-1;
        return;
    }
    /* Disable the native seller's talk/buy state before NPC actor movement
     * runs. The shop goods actor is visual-only; purchases are offered solely
     * by the PC transaction catalog in repurposed service interiors. */
    if (s_service_context && scene == s_service_scene) {
        for (ACTOR* a=play->actor_info.list[ACTOR_PART_NPC].actor; a; a=a->next_actor) {
            if(scene==SCENE_SHOP0 && a->npc_id!=SP_NPC_SHOP_MASTER &&
            !(s_service_context==PC_WORLD_SERVICE_NIGHT_MARKET && a->npc_id==SP_NPC_MAMEDANUKI0))continue;
            a->mv_proc=commerce_seller_idle;
        }
    }
    if(!s_service_context || scene!=s_service_scene || g_pc_paused || pc_world_shops_active() ||
       play->submenu.mode!=mSM_MODE_IDLE || play->fb_mode!=FBDEMO_MODE_NONE ||
       play->fb_wipe_mode!=WIPE_MODE_NONE || mDemo_Get_talk_actor())return;
    PLAYER_ACTOR* p=get_player_actor_withoutCheck(play);
    if(!p)return;
    ACTOR* seller=NULL;
    float best=190.0f*190.0f;
    for(ACTOR* a=play->actor_info.list[ACTOR_PART_NPC].actor;a;a=a->next_actor) {
        if(scene==SCENE_SHOP0 && a->npc_id!=SP_NPC_SHOP_MASTER &&
           !(s_service_context==PC_WORLD_SERVICE_NIGHT_MARKET && a->npc_id==SP_NPC_MAMEDANUKI0))continue;
        float dx=p->actor_class.world.position.x-a->world.position.x;
        float dz=p->actor_class.world.position.z-a->world.position.z;
        float d=dx*dx+dz*dz;
        if(d<best){best=d;seller=a;}
    }
    if(!seller)return;
    /* Disable vanilla Nook/tailor dialogue in these cloned service rooms so
     * every sale remains routed through the PC transaction layer. */
    if(chkTrigger(BUTTON_A)) {
        if(s_service_context==PC_WORLD_SERVICE_NIGHT_MARKET)pc_world_shops_open_night_market_now();
        else if(s_service_context==PC_WORLD_SERVICE_MARITIME_OUTFITTER)pc_maritime_outfitter_open();
        else pc_customization_shop_open();
        play->game.pads[0].on.button&=~BUTTON_A;play->game.pads[0].now.button&=~BUTTON_A;
    }
#else
    (void)play;
#endif
}
void pc_world_commerce_hud(GAME* game) {
#ifdef ACO_DEV_EAST
    if(!g_pc_dev_east || !commerce_play || game!=&commerce_play->game || g_pc_paused ||
       (Save_Get(scene_no)!=SCENE_FG && !s_service_context) || commerce_play->submenu.mode!=mSM_MODE_IDLE)return;
    PLAYER_ACTOR* p=get_player_actor_withoutCheck(commerce_play);if(!p)return;
    const char* notice=pc_world_progression_notice_at(p->actor_class.world.position.x,p->actor_class.world.position.z);
    pc_world_service_anchor_t anchors[8];int count=Save_Get(scene_no)==SCENE_FG?pc_world_service_anchors_get(commerce_play,anchors,8):0;
    const char* names[]={"","Night Market - enter to browse","Maritime Outfitter - enter to browse","Color & Cloth Studio - enter to browse"};
    if(!notice)for(int i=0;i<count;i++) {
        float dx=p->actor_class.world.position.x-anchors[i].x,dz=p->actor_class.world.position.z-anchors[i].z;
        if(dx*dx+dz*dz<anchors[i].interact_radius*anchors[i].interact_radius){notice=names[anchors[i].service_id];break;}
    }
    if(notice){mFont_SetMatrix(game->graph,mFont_MODE_FONT);pc_menu_draw_centered(game,notice,216,255,240,180,255,.78f);mFont_UnSetMatrix(game->graph,mFont_MODE_FONT);}
#else
    (void)game;
#endif
}

/* Storefronts use their native meshes and per-building texture variants. */
void pc_world_commerce_draw(GAME_PLAY* play) { (void)play; }

#endif /* !PC_WORLD_SHOP_TEST */
