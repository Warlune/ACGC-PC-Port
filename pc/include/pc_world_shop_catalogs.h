#ifndef PC_WORLD_SHOP_CATALOGS_H
#define PC_WORLD_SHOP_CATALOGS_H
#include "pc_world_shops.h"
#include "m_home.h"
struct game_play_s;
struct game_s;
extern int g_pc_dev_shop_preview;
int pc_world_commerce_dev_spawn(int service,float* x,float* z);
void pc_maritime_outfitter_open(void);
void pc_customization_shop_open(void);
void pc_world_commerce_tick(struct game_play_s* play);
void pc_world_commerce_draw(struct game_play_s* play);
void pc_world_commerce_hud(struct game_s* game);
pc_shop_result_t pc_customization_print_pattern(Private_c* player,int slot,u32 price);
pc_shop_result_t pc_customization_repaint(Private_c* player,mHm_hs_c* home,int palette,u32 price);
#endif
