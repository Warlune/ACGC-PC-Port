#include "pc_world_shops.h"

#include <string.h>
#include <stdio.h>

#include "m_common_data.h"
#include "lb_rtc.h"
#include "m_name_table.h"
#include "m_ftr_def.h"
#include "pc_menu_util.h"
#include "pc_text_draw.h"
#include "m_font.h"
#include "pc_pause_menu.h"
#include "m_field_info.h"
#include "m_play.h"
#include "m_actor.h"
#include "main.h"

static const pc_shop_item_t s_night_stock[] = {
    { FTR_START(FTR_FAMICOM_COMMON00), 12000, "Famicom game 1", 1 },
    { FTR_START(FTR_FAMICOM_COMMON01), 14000, "Famicom game 2", 1 },
    { FTR_START(FTR_SUM_HAL_LANP01), 9000, "Halloween Lamp", 1 },
    { FTR_START(FTR_SUM_HAL_PKIN), 7000, "Jack-o'-lantern", 1 },
    { FTR_START(FTR_TAK_SNOWLAMP), 11000, "Snow Lamp", 1 },
    { FTR_START(FTR_NOG_KAMAKURA), 15000, "Snow Hut", 1 },
    { FTR_START(FTR_SUM_X_LANP), 10000, "Holiday Lamp", 1 },
    { FTR_START(FTR_SUM_X_CLK), 8000, "Holiday Clock", 1 },
    { FTR_START(FTR_SUM_HAL_CLK01), 8500, "Halloween Clock", 1 },
    { FTR_START(FTR_SUM_HAL_MIRROR01), 9500, "Halloween Mirror", 1 },
    { FTR_START(FTR_KON_XTREE02), 16000, "Holiday Tree", 1 },
    { FTR_START(FTR_NOG_XTREE), 16000, "Snowy Tree", 1 },
};

static pc_shop_item_t s_rotating_items[6];
static pc_shop_catalog_t s_night_catalog = {
    "NIGHT MARKET", "Open 8 PM to 5 AM - Six curios daily",
    s_rotating_items, 6, NULL, NULL
};
static const pc_shop_catalog_t* s_active_catalog;
static int s_selected;
extern int g_pc_pause_input_drain;
static int s_shop_open = 1;
static int s_stock_initialized;
static int s_message_ticks;
static char s_message[80];

#ifndef PC_WORLD_SHOP_TEST
extern void pc_maritime_outfitter_open(void);
extern void pc_customization_shop_open(void);
#endif

static void pc_world_shops_format_bells(char* out, size_t capacity, u32 bells) {
    if (bells >= 1000) {
        snprintf(out, capacity, "%u,%03u", (unsigned)(bells / 1000),
                 (unsigned)(bells % 1000));
    } else {
        snprintf(out, capacity, "%u", (unsigned)bells);
    }
}

#ifndef PC_WORLD_SHOP_TEST
static int pc_world_shops_town_index(int bx) {
    if (bx < ACO_TOWN_X0 || bx >= ACO_TOWN_X0 + 21) return -1;
    return (bx - ACO_TOWN_X0) / 7;
}

static int pc_world_shops_service_for_item(mActor_name_t item, int town_index) {
    if (town_index == 1) return 0; /* Keep the starter town's native services. */
    if (item >= SHOP0 && item <= SHOP3) {
        return town_index == 0 ? PC_WORLD_SERVICE_NIGHT_MARKET
                               : PC_WORLD_SERVICE_MARITIME_OUTFITTER;
    }
    if (item == NEEDLEWORK_SHOP && town_index != 1) {
        return PC_WORLD_SERVICE_CUSTOMIZATION;
    }
    return 0;
}

static void pc_world_shops_anchor_position(pc_world_service_anchor_t* anchor,
                                           int service, int town, float x, float z) {
    /* Actor position is the native structure origin; these offsets land at its door. */
    anchor->service_id = service;
    anchor->town_index = town;
    anchor->x = x - (service == PC_WORLD_SERVICE_CUSTOMIZATION ? 64.0f : 68.29f);
    anchor->z = z + (service == PC_WORLD_SERVICE_CUSTOMIZATION ? 64.0f : 68.29f);
    anchor->interact_radius = 100.0f;
}

static void pc_world_shops_anchor_add(pc_world_service_anchor_t* out, int* count,
                                      int capacity, int service, int town,
                                      float x, float z, int replace) {
    int i;
    for (i = 0; i < *count; ++i) {
        if (out[i].service_id == service && out[i].town_index == town) {
            if (replace) pc_world_shops_anchor_position(&out[i], service, town, x, z);
            return;
        }
    }
    if (*count < capacity) {
        pc_world_shops_anchor_position(&out[*count], service, town, x, z);
        ++*count;
    }
}

int pc_world_service_anchors_get(struct game_play_s* play,
                                 pc_world_service_anchor_t* out, int capacity) {
#if defined(TARGET_PC) && defined(ACO_DEV_EAST)
    mFM_block_info_c* blocks = mFI_GetBlockTopP();
    int block_x_max = mFI_GetBlockXMax();
    int block_z_max = mFI_GetBlockZMax();
    int count = 0;
    int bz;
    int part;
    if (!out || capacity <= 0) return 0;
    /* First use FG anchors, which remain available while structures are unloaded. */
    for (bz = 0; bz < block_z_max; ++bz) {
        int bx;
        for (bx = 0; bx < block_x_max; ++bx) {
            int town_index;
            mFM_block_info_c* block;
            mActor_name_t* items;
            int unit;
            town_index = pc_world_shops_town_index(bx);
            if (town_index < 0 || town_index == 1 || !blocks) continue;
            block = &blocks[mFI_GetBlockNum(bx, bz)];
            items = block->fg_info.items_p;
            if (!items) continue;
            for (unit = 0; unit < UT_TOTAL_NUM && count < capacity; ++unit) {
                mActor_name_t item = items[unit];
                int service;
                xyz_t pos;
                int ux;
                int uz;
                service = pc_world_shops_service_for_item(item, town_index);
                if (!service) continue;
                ux = unit % UT_X_NUM;
                uz = unit / UT_X_NUM;
                mFI_BkandUtNum2CenterWpos(&pos, bx, bz, ux, uz);
                /* FG unit center is shifted to the native structure actor origin. */
                pos.x -= 20.0f;
                pos.z += 20.0f;
                pc_world_shops_anchor_add(out, &count, capacity, service, town_index,
                                          pos.x, pos.z, 0);
            }
        }
    }
    /* Loaded storefront actors survive the temporary FG RSV_NO substitution. */
    if (play) {
        for (part = 0; part < ACTOR_PART_NUM; ++part) {
            ACTOR* actor;
            for (actor = play->actor_info.list[part].actor; actor; actor = actor->next_actor) {
                int bx;
                int bz_actor;
                int town_index;
                int service;
                if (!mFI_Wpos2BlockNum(&bx, &bz_actor, actor->world.position)) continue;
                town_index = pc_world_shops_town_index(bx);
                service = pc_world_shops_service_for_item(actor->npc_id, town_index);
                if (!service) continue;
                pc_world_shops_anchor_add(out, &count, capacity, service, town_index,
                                          actor->world.position.x, actor->world.position.z, 1);
            }
        }
    }
    return count;
#else
    (void)play;
    (void)out;
    (void)capacity;
    return 0;
#endif
}
#endif /* !PC_WORLD_SHOP_TEST */

pc_shop_result_t pc_shop_buy(Private_c* player, const pc_shop_item_t* item,
                             int shop_is_open) {
    int slot;
    lbRTC_time_c now = Common_Get(time.rtc_time);
    if (!shop_is_open) return PC_SHOP_ERR_CLOSED;
    if (!player || !item || item->item == EMPTY_NO || item->price == 0) {
        return PC_SHOP_ERR_ITEM;
    }
    if (item->night_exclusive) {
        if (!pc_night_market_is_open(now.hour)) return PC_SHOP_ERR_NOT_FOR_SALE;
    }
    if (player->inventory.wallet < item->price) return PC_SHOP_ERR_FUNDS;

    /* Add the item first. The wallet only changes after a successful slot write. */
    slot = mPr_SetFreePossessionItem(player, item->item, mPr_ITEM_COND_NORMAL);
    if (!slot) return PC_SHOP_ERR_FULL;
    player->inventory.wallet -= item->price;
    return PC_SHOP_OK;
}

const pc_shop_catalog_t* pc_night_market_catalog(void) {
    if (!s_stock_initialized) {
        lbRTC_time_c now = Common_Get(time.rtc_time);
        pc_night_market_set_day(now.year, now.month, now.day);
    }
    return &s_night_catalog;
}

int pc_world_shops_item_is_night_exclusive(mActor_name_t item) {
    return item == FTR_START(FTR_FAMICOM_COMMON00) ||
           item == FTR_START(FTR_FAMICOM_COMMON01) ||
           item == FTR_START(FTR_SUM_HAL_LANP01) ||
           item == FTR_START(FTR_SUM_HAL_PKIN) ||
           item == FTR_START(FTR_TAK_SNOWLAMP) ||
           item == FTR_START(FTR_NOG_KAMAKURA) ||
           item == FTR_START(FTR_SUM_X_LANP) ||
           item == FTR_START(FTR_SUM_X_CLK) ||
           item == FTR_START(FTR_SUM_HAL_CLK01) ||
           item == FTR_START(FTR_SUM_HAL_MIRROR01) ||
           item == FTR_START(FTR_KON_XTREE02) ||
           item == FTR_START(FTR_NOG_XTREE);
}

void pc_night_market_set_day(int year, int month, int day) {
    unsigned seed = (unsigned)(year * 372 + month * 31 + day);
    int decor_count = (int)(sizeof(s_night_stock) / sizeof(s_night_stock[0])) - 2;
    int first = (int)(seed % (unsigned)decor_count);
    int i;
    s_rotating_items[0] = s_night_stock[0];
    s_rotating_items[1] = s_night_stock[1];
    for (i = 0; i < 4; ++i) {
        s_rotating_items[i + 2] = s_night_stock[2 + ((first + i) % decor_count)];
    }
    s_stock_initialized = 1;
}

int pc_night_market_is_open(int hour) {
    return hour >= 20 || hour < 5;
}

void pc_world_shops_open(const pc_shop_catalog_t* catalog) {
    if (catalog && (catalog->item_count < 0 || catalog->item_count > PC_SHOP_MAX_ITEMS)) {
        catalog = NULL;
    }
    s_active_catalog = catalog;
    s_shop_open = 1;
    s_selected = 0;
    s_message_ticks = 0;
    s_message[0] = '\0';
    if (catalog) g_pc_paused = 1;
}

void pc_world_shops_set_status(const char* message) {
    if (!message) message = "";
    strncpy(s_message, message, sizeof(s_message) - 1);
    s_message[sizeof(s_message) - 1] = '\0';
    s_message_ticks = 180;
}

void pc_world_shops_open_night_market(int hour) {
    pc_world_shops_open(pc_night_market_catalog());
    s_shop_open = pc_night_market_is_open(hour);
}

void pc_world_shops_open_night_market_now(void) {
    lbRTC_time_c now = Common_Get(time.rtc_time);
    pc_night_market_set_day(now.year, now.month, now.day);
    pc_world_shops_open_night_market(now.hour);
}

int pc_world_shops_active(void) { return s_active_catalog != NULL; }

#ifndef PC_WORLD_SHOP_TEST
int pc_world_shops_try_open_at(struct game_play_s* play, float x, float z) {
    pc_world_service_anchor_t anchors[8];
    int count = pc_world_service_anchors_get(play, anchors, 8);
    int best = -1;
    float best_distance = 0.0f;
    int i;
    if (count > 8) count = 8;
    for (i = 0; i < count; ++i) {
        float dx = x - anchors[i].x;
        float dz = z - anchors[i].z;
        float distance = dx * dx + dz * dz;
        float radius2 = anchors[i].interact_radius * anchors[i].interact_radius;
        if (distance <= radius2 && (best < 0 || distance < best_distance)) {
            best = i;
            best_distance = distance;
        }
    }
    if (best < 0) return 0;
    if (anchors[best].service_id == PC_WORLD_SERVICE_NIGHT_MARKET) {
        pc_world_shops_open_night_market_now();
        return 1;
    }
    if (anchors[best].service_id == PC_WORLD_SERVICE_MARITIME_OUTFITTER) {
        pc_maritime_outfitter_open();
        return 1;
    }
    if (anchors[best].service_id == PC_WORLD_SERVICE_CUSTOMIZATION) {
        pc_customization_shop_open();
        return 1;
    }
    return 0;
}
#endif /* !PC_WORLD_SHOP_TEST */

void pc_world_shops_close(void) {
    s_active_catalog = NULL;
    g_pc_paused = 0;
    g_pc_pause_input_drain=1;
    SDL_FlushEvent(SDL_KEYDOWN);
    SDL_FlushEvent(SDL_KEYUP);
    SDL_FlushEvent(SDL_CONTROLLERBUTTONDOWN);
    SDL_FlushEvent(SDL_CONTROLLERBUTTONUP);
}

static void pc_world_shops_refresh_hours(void) {
    if (s_active_catalog == &s_night_catalog) {
        lbRTC_time_c now = Common_Get(time.rtc_time);
        pc_night_market_set_day(now.year, now.month, now.day);
        s_shop_open = pc_night_market_is_open(now.hour);
    }
}

static void pc_world_shops_result(pc_shop_result_t result) {
    const char* text = "Purchase complete. Enjoy!";
    switch (result) {
        case PC_SHOP_ERR_CLOSED: text = "The market is closed."; break;
        case PC_SHOP_ERR_FUNDS: text = "You need more Bells."; break;
        case PC_SHOP_ERR_FULL: text = "Your pockets are full."; break;
        case PC_SHOP_ERR_ITEM: text = "That item is unavailable."; break;
        case PC_SHOP_ERR_NOT_FOR_SALE: text = "That item is not for sale tonight."; break;
        default: break;
    }
    strncpy(s_message, text, sizeof(s_message) - 1);
    s_message[sizeof(s_message) - 1] = '\0';
    s_message_ticks = 180;
}

int pc_world_shops_handle_event(const SDL_Event* event) {
    int action = 0;
    if (!s_active_catalog || !event) return 0;
    pc_world_shops_refresh_hours();
    if (event->type == SDL_KEYDOWN && !event->key.repeat) {
        switch (event->key.keysym.sym) {
            case SDLK_UP: case SDLK_w: action = 1; break;
            case SDLK_DOWN: case SDLK_s: action = 2; break;
            case SDLK_RETURN: case SDLK_SPACE: action = 3; break;
            case SDLK_ESCAPE: action = 4; break;
            default: return 1; /* Modal shop owns keyboard input. */
        }
    } else if (event->type == SDL_CONTROLLERBUTTONDOWN) {
        switch (event->cbutton.button) {
            case SDL_CONTROLLER_BUTTON_DPAD_UP: action = 1; break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN: action = 2; break;
            case SDL_CONTROLLER_BUTTON_A: action = 3; break;
            case SDL_CONTROLLER_BUTTON_B: action = 4; break;
            default: return 1;
        }
    } else if (event->type == SDL_CONTROLLERAXISMOTION || event->type == SDL_CONTROLLERBUTTONUP ||
               event->type == SDL_KEYUP) {
        return 1;
    } else {
        return 0;
    }
    if (s_active_catalog->item_count <= 0) {
        if (action == 4) pc_world_shops_close();
        return 1;
    }
    switch (action) {
        case 1:
            s_selected = (s_selected + s_active_catalog->item_count - 1) % s_active_catalog->item_count;
            return 1;
        case 2:
            s_selected = (s_selected + 1) % s_active_catalog->item_count;
            return 1;
        case 3:
            if (s_active_catalog->item_count > 0) {
                if (!s_shop_open) {
                    pc_world_shops_result(PC_SHOP_ERR_CLOSED);
                } else if (s_active_catalog->on_confirm) {
                    s_active_catalog->on_confirm(Now_Private,
                        &s_active_catalog->items[s_selected], s_active_catalog->context);
                } else {
                    pc_world_shops_result(pc_shop_buy(Now_Private,
                        &s_active_catalog->items[s_selected], s_shop_open));
                }
            }
            return 1;
        case 4:
            pc_world_shops_close();
            return 1;
        default:
            return 0;
    }
}

#ifndef PC_WORLD_SHOP_TEST
void pc_world_shops_draw(struct game_s* game) {
    int i;
    char balance[48];
    char balance_value[24];
    if (!s_active_catalog || !game || !game->graph) return;
    pc_world_shops_refresh_hours();
    mFont_SetMatrix(game->graph, mFont_MODE_FONT);
    pc_menu_dim_rect(game->graph, 150);
    pc_menu_draw_centered(game, s_active_catalog->name, 35.0f, 255, 231, 158, 255, 1.2f);
    if (s_active_catalog->hours && s_active_catalog->hours[0]) {
        pc_menu_draw_centered(game, s_active_catalog->hours, 55.0f,
                              220, 220, 210, 255, 0.75f);
    }
    pc_world_shops_format_bells(balance_value, sizeof(balance_value),
                                Now_Private ? Now_Private->inventory.wallet : 0);
    snprintf(balance, sizeof(balance), "Your Bells: %s", balance_value);
    pc_menu_draw_centered(game, balance, 72.0f, 180, 225, 190, 255, 0.75f);
    if (!s_shop_open) {
        pc_menu_draw_centered(game, "Closed right now - come back tonight", 118.0f,
                              255, 210, 170, 255, 0.85f);
        pc_menu_draw_centered(game, "ESC or B leave", 225.0f, 220, 220, 220, 255, 0.75f);
        mFont_UnSetMatrix(game->graph, mFont_MODE_FONT);
        return;
    }
    for (i = 0; i < s_active_catalog->item_count; ++i) {
        char row[96];
        int r, g, b, a;
        const pc_shop_item_t* item = &s_active_catalog->items[i];
        pc_menu_row_colors(i == s_selected, &r, &g, &b, &a);
        if (s_active_catalog->on_confirm && item->price == 0) {
            snprintf(row, sizeof(row), "%s  -  Choose", item->name);
        } else {
            char price[24];
            pc_world_shops_format_bells(price, sizeof(price), item->price);
            snprintf(row, sizeof(row), "%s  -  %s Bells%s", item->name,
                     price, item->night_exclusive ? "  \xB9" : "");
        }
        pc_menu_draw_centered(game, row, 88.0f + 15.0f * i, r, g, b, a,
                              i == s_selected ? 1.0f : 0.85f);
    }
    pc_menu_draw_centered(game,
        s_active_catalog->on_confirm ? "UP and DOWN choose   ENTER select   ESC leave"
                                     : "UP and DOWN select   ENTER buy   ESC leave",
        225.0f, 220, 220, 220, 255, 0.7f);
    if (s_message_ticks > 0) {
        pc_menu_draw_centered(game, s_message, 208.0f, 255, 235, 120, 255, 0.75f);
        --s_message_ticks;
    }
    mFont_UnSetMatrix(game->graph, mFont_MODE_FONT);
}
#endif /* !PC_WORLD_SHOP_TEST */
