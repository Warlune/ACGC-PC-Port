/* Shared transaction and menu support for PC world shops. */
#ifndef PC_WORLD_SHOPS_H
#define PC_WORLD_SHOPS_H

#include "pc_platform.h"
#include "m_private.h"

struct game_play_s;

#ifdef __cplusplus
extern "C" {
#endif

#define PC_SHOP_MAX_ITEMS 8

typedef struct pc_shop_item_s {
    mActor_name_t item;
    u32 price;
    const char* name;
    u8 night_exclusive;
} pc_shop_item_t;

typedef void (*pc_shop_confirm_fn)(Private_c* player, const pc_shop_item_t* item,
                                   void* context);

typedef struct pc_shop_catalog_s {
    const char* name;
    const char* hours;
    const pc_shop_item_t* items;
    int item_count;
    pc_shop_confirm_fn on_confirm; /* Optional action for service/customization menus. */
    void* context;
} pc_shop_catalog_t;

typedef enum pc_world_service_id_e {
    PC_WORLD_SERVICE_NIGHT_MARKET = 1,
    PC_WORLD_SERVICE_MARITIME_OUTFITTER = 2,
    PC_WORLD_SERVICE_CUSTOMIZATION = 3
} pc_world_service_id_t;

typedef struct pc_world_service_anchor_s {
    int service_id;
    int town_index;
    float x;
    float z;
    float interact_radius;
} pc_world_service_anchor_t;

typedef enum pc_shop_result_e {
    PC_SHOP_OK = 0,
    PC_SHOP_ERR_CLOSED,
    PC_SHOP_ERR_FUNDS,
    PC_SHOP_ERR_FULL,
    PC_SHOP_ERR_ITEM,
    PC_SHOP_ERR_NOT_FOR_SALE
} pc_shop_result_t;

/* Shared, atomic purchase operation. Inventory remains in the native save. */
pc_shop_result_t pc_shop_buy(Private_c* player, const pc_shop_item_t* item,
                             int shop_is_open);
int pc_world_shops_item_is_night_exclusive(mActor_name_t item);

/* Rotates the night market's stock from the in-game date. */
const pc_shop_catalog_t* pc_night_market_catalog(void);
void pc_night_market_set_day(int year, int month, int day);
int pc_night_market_is_open(int hour);

/* Call these from the native event/draw path. */
void pc_world_shops_open(const pc_shop_catalog_t* catalog);
void pc_world_shops_open_night_market(int hour);
void pc_world_shops_open_night_market_now(void);
void pc_world_shops_close(void);
void pc_world_shops_set_status(const char* message);
/* Called on the native interact button near the player's world position. */
int pc_world_shops_try_open_at(struct game_play_s* play, float x, float z);
int pc_world_service_anchors_get(struct game_play_s* play,
                                 pc_world_service_anchor_t* out, int capacity);
int pc_world_shops_active(void);
int pc_world_shops_handle_event(const SDL_Event* event);
void pc_world_shops_draw(struct game_s* game);

#ifdef __cplusplus
}
#endif
#endif
