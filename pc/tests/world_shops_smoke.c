#include "pc_world_shops.h"
#include "m_common_data.h"
#include "m_name_table.h"
#include "pc_world_shop_catalogs.h"
#include "m_house.h"
#include "pc_pause_menu.h"
#include <SDL.h>

#include <stdio.h>
#include <string.h>

common_data_t common_data;
int g_pc_paused;
int g_pc_pause_input_drain;

#define REQUIRE(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "world shop check failed at line %d: %s\n", __LINE__, #condition); \
        return 1; \
    } \
} while (0)

/* Keep the transaction test independent from unrelated game collection state. */
int mPr_SetFreePossessionItem(Private_c* player, mActor_name_t item, u32 cond) {
    int i;
    (void)cond;
    if (!player) return 0;
    for (i = 0; i < mPr_POCKETS_SLOT_COUNT; ++i) {
        if (player->inventory.pockets[i] == EMPTY_NO) {
            player->inventory.pockets[i] = item;
            return 1;
        }
    }
    return 0;
}

int main(void) {
    const pc_shop_catalog_t* catalog;
    pc_shop_catalog_t next_day;
    pc_shop_item_t purchase = { ITM_NET, 300, "Net", 0 };
    Private_c player;
    mActor_name_t first_item;
    mActor_name_t observed[12];
    int observed_count = 0;
    int i;

    REQUIRE(!pc_night_market_is_open(19));
    REQUIRE(pc_night_market_is_open(20));
    REQUIRE(pc_night_market_is_open(23));
    REQUIRE(pc_night_market_is_open(0));
    REQUIRE(pc_night_market_is_open(4));
    REQUIRE(!pc_night_market_is_open(5));

    pc_night_market_set_day(2026, 10, 17);
    catalog = pc_night_market_catalog();
    REQUIRE(catalog != NULL);
    REQUIRE(catalog->item_count == 6);
    for (i = 0; i < catalog->item_count; ++i) {
        int j;
        REQUIRE(catalog->items[i].price > 0);
        REQUIRE(pc_world_shops_item_is_night_exclusive(catalog->items[i].item));
        for (j = i + 1; j < catalog->item_count; ++j) {
            REQUIRE(catalog->items[i].item != catalog->items[j].item);
        }
    }
    first_item = catalog->items[2].item;
    next_day = *catalog;
    pc_night_market_set_day(2026, 10, 18);
    REQUIRE(next_day.items[2].item != first_item);

    /* Over a full rotation, both curios and every seasonal decor item appear. */
    for (i = 0; i < 10; ++i) {
        int row;
        pc_night_market_set_day(2026, 10, 17 + i);
        catalog = pc_night_market_catalog();
        REQUIRE(catalog->item_count == 6);
        REQUIRE(catalog->items[0].item == FTR_START(FTR_FAMICOM_COMMON00));
        REQUIRE(catalog->items[1].item == FTR_START(FTR_FAMICOM_COMMON01));
        for (row = 0; row < catalog->item_count; ++row) {
            int seen;
            for (seen = 0; seen < observed_count; ++seen) {
                if (observed[seen] == catalog->items[row].item) break;
            }
            if (seen == observed_count && observed_count < 12) {
                observed[observed_count++] = catalog->items[row].item;
            }
        }
    }
    REQUIRE(observed_count == 12);

    memset(&player, 0, sizeof(player));
    for (i = 0; i < mPr_POCKETS_SLOT_COUNT; ++i) {
        player.inventory.pockets[i] = EMPTY_NO;
    }
    player.inventory.wallet = 500;
    REQUIRE(pc_shop_buy(&player, &purchase, 1) == PC_SHOP_OK);
    REQUIRE(player.inventory.wallet == 200);
    REQUIRE(player.inventory.pockets[0] == ITM_NET);

    player.inventory.pockets[0] = EMPTY_NO;
    player.inventory.wallet = 299;
    REQUIRE(pc_shop_buy(&player, &purchase, 1) == PC_SHOP_ERR_FUNDS);
    REQUIRE(player.inventory.wallet == 299);
    REQUIRE(player.inventory.pockets[0] == EMPTY_NO);

    for (i = 0; i < mPr_POCKETS_SLOT_COUNT; ++i) {
        player.inventory.pockets[i] = ITM_NET;
    }
    player.inventory.wallet = 500;
    REQUIRE(pc_shop_buy(&player, &purchase, 1) == PC_SHOP_ERR_FULL);
    REQUIRE(player.inventory.wallet == 500);

    /* The transaction checks the current native clock, not only menu-open time. */
    for (i = 0; i < mPr_POCKETS_SLOT_COUNT; ++i) player.inventory.pockets[i] = EMPTY_NO;
    player.inventory.wallet = 20000;
    Common_Get(time.rtc_time).hour = 5;
    REQUIRE(pc_shop_buy(&player, &catalog->items[0], 1) == PC_SHOP_ERR_NOT_FOR_SALE);
    REQUIRE(player.inventory.wallet == 20000);
    REQUIRE(player.inventory.pockets[0] == EMPTY_NO);
    Common_Get(time.rtc_time).hour = 20;
    REQUIRE(pc_shop_buy(&player, &catalog->items[0], 1) == PC_SHOP_OK);
    REQUIRE(player.inventory.wallet == 8000);
    REQUIRE(player.inventory.pockets[0] == catalog->items[0].item);

    /* Controller close returns simulation ownership and drops queued stale input. */
    REQUIRE(SDL_Init(SDL_INIT_EVENTS) == 0);
    {
        pc_shop_item_t row = { ITM_NET, 10, "Net", 0 };
        pc_shop_catalog_t menu = { "Test", "", &row, 1, NULL, NULL };
        SDL_Event event;
        memset(&event, 0, sizeof(event));
        pc_world_shops_open(&menu);
        REQUIRE(pc_world_shops_active());
        REQUIRE(g_pc_paused);
        event.type = SDL_CONTROLLERBUTTONDOWN;
        event.cbutton.button = SDL_CONTROLLER_BUTTON_B;
        REQUIRE(pc_world_shops_handle_event(&event));
        REQUIRE(!pc_world_shops_active());
        REQUIRE(!g_pc_paused);
        event.type = SDL_KEYDOWN;
        event.key.keysym.sym = SDLK_RETURN;
        REQUIRE(SDL_PushEvent(&event) == 1);
        pc_world_shops_close();
        REQUIRE(SDL_PollEvent(&event) == 0);
    }
    SDL_Quit();

    /* Repaint is owner-only, charges once, and cannot repeat an applied order. */
    {
        mHm_hs_c home;
        memset(&player, 0, sizeof(player));
        memset(&home, 0, sizeof(home));
        memcpy(&home.ownerID, &player.player_ID, sizeof(home.ownerID));
        player.inventory.wallet = 2000;
        home.next_outlook_pal = 0;
        REQUIRE(pc_customization_repaint(&player, &home, 4, 1000) == PC_SHOP_OK);
        REQUIRE(home.next_outlook_pal == 4);
        REQUIRE(player.inventory.wallet == 1000);
        REQUIRE(pc_customization_repaint(&player, &home, 4, 1000) == PC_SHOP_ERR_NOT_FOR_SALE);
        REQUIRE(player.inventory.wallet == 1000);
        player.inventory.wallet = 999;
        REQUIRE(pc_customization_repaint(&player, &home, 5, 1000) == PC_SHOP_ERR_FUNDS);
        REQUIRE(home.next_outlook_pal == 4);
        REQUIRE(player.inventory.wallet == 999);
        home.ownerID.player_name[0] ^= 1;
        player.inventory.wallet = 2000;
        REQUIRE(pc_customization_repaint(&player, &home, 5, 1000) == PC_SHOP_ERR_ITEM);
        REQUIRE(player.inventory.wallet == 2000);
        REQUIRE(home.next_outlook_pal == 4);
    }

    REQUIRE(g_pc_pause_input_drain);
    memset(&player,0,sizeof(player));player.inventory.wallet=1000;
    for(i=0;i<mPr_POCKETS_SLOT_COUNT;i++)player.inventory.pockets[i]=EMPTY_NO;
    REQUIRE(pc_customization_print_pattern(&player,0,350)==PC_SHOP_ERR_ITEM);
    REQUIRE(player.inventory.wallet==1000 && player.inventory.pockets[0]==EMPTY_NO);
    player.my_org[2].flag_design_set=1;
    REQUIRE(pc_customization_print_pattern(&player,2,350)==PC_SHOP_OK);
    REQUIRE(player.inventory.wallet==650 && player.inventory.pockets[0]==ITM_MY_ORG_UMBRELLA2);
    for(i=0;i<mPr_POCKETS_SLOT_COUNT;i++)player.inventory.pockets[i]=ITM_NET;
    REQUIRE(pc_customization_print_pattern(&player,2,350)==PC_SHOP_ERR_FULL);
    REQUIRE(player.inventory.wallet==650);
    REQUIRE(pc_customization_print_pattern(&player,8,350)==PC_SHOP_ERR_ITEM);
    REQUIRE(!g_pc_paused);
    puts("world shops smoke passed");
    return 0;
}
