/* Opt-in native commerce integration test. All test mutations are restored
 * before exiting; this executable path is deliberately separate from play. */
#include "pc_world_shop_test.h"

#include "pc_world_shops.h"
#include "pc_world_shop_catalogs.h"
#include "pc_dev_east.h"
#include "pc_pause_menu.h"
#include "m_common_data.h"
#include "m_home.h"
#include "m_house.h"
#include "m_name_table.h"
#include "m_needlework_ovl.h"
#include "m_play.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int g_pc_dev_shop_test;

#ifdef ACO_DEV_EAST
enum {
    SHOP_TEST_WAIT_WORLD,
    SHOP_TEST_NIGHT_PURCHASE,
    SHOP_TEST_NIGHT_CLOSE,
    SHOP_TEST_DAY_PURCHASE,
    SHOP_TEST_DAY_CLOSE,
    SHOP_TEST_OUTFITTER_PURCHASE,
    SHOP_TEST_OUTFITTER_CLOSE,
    SHOP_TEST_STUDIO_OPEN_PAINT,
    SHOP_TEST_STUDIO_PAINT,
    SHOP_TEST_STUDIO_PAINT_CLOSE,
    SHOP_TEST_PRINT_DOWN,
    SHOP_TEST_PRINT_OPEN,
    SHOP_TEST_PRINT_BUY,
    SHOP_TEST_PRINT_CLOSE,
    SHOP_TEST_WORKBENCH_DOWN1,
    SHOP_TEST_WORKBENCH_DOWN2,
    SHOP_TEST_WORKBENCH_OPEN,
    SHOP_TEST_EDITOR_WAIT
};

static Save_t s_saved_save;
static Private_c s_saved_player;
static lbRTC_time_c s_saved_rtc;
static int s_saved_time_override;
static int s_saved_pause;
static int s_started;
static int s_stage;
static int s_frames;
static int s_menu_frames;
static int s_preview_opened;
static int s_home_idx = -1;
static pc_world_service_anchor_t s_anchors[8];
static int s_anchor_count;
static const pc_shop_item_t* s_night_item;
static Private_c* s_player;

static void shop_test_restore(void) {
    if (!s_started) return;
    pc_world_shops_close();
    memcpy(Common_GetPointer(save.save), &s_saved_save, sizeof(s_saved_save));
    Common_Set(time.rtc_time, s_saved_rtc);
    g_pc_time_override = s_saved_time_override;
    g_pc_paused = s_saved_pause;
    s_started = 0;
}

static void shop_test_finish(int pass, const char* reason) {
    shop_test_restore();
    fprintf(pass ? stdout : stderr, "[ShopTest] %s: %s\n", pass ? "PASS" : "FAIL", reason);
    fflush(pass ? stdout : stderr);
    exit(pass ? 0 : 1);
}

static int shop_test_send_key(SDL_Keycode key) {
    SDL_Event event;
    memset(&event, 0, sizeof(event));
    event.type = SDL_KEYDOWN;
    event.key.state = SDL_PRESSED;
    event.key.repeat = 0;
    event.key.keysym.sym = key;
    event.key.keysym.scancode = SDL_GetScancodeFromKey(key);
    return SDL_PushEvent(&event) == 1;
}

static pc_world_service_anchor_t* shop_test_anchor(int service_id) {
    for (int i = 0; i < s_anchor_count; ++i) {
        if (s_anchors[i].service_id == service_id) return &s_anchors[i];
    }
    return NULL;
}

static int shop_test_item_count(Private_c* player, mActor_name_t item) {
    int count = 0;
    for (int i = 0; i < mPr_POCKETS_SLOT_COUNT; ++i) {
        if (player->inventory.pockets[i] == item) ++count;
    }
    return count;
}

static void shop_test_empty_pockets(Private_c* player) {
    for (int i = 0; i < mPr_POCKETS_SLOT_COUNT; ++i) {
        player->inventory.pockets[i] = EMPTY_NO;
    }
    player->inventory.item_conditions = 0;
}

static void shop_test_reset_player(void) {
    *s_player = s_saved_player;
    shop_test_empty_pockets(s_player);
}

static void shop_test_set_hour(int hour) {
    g_pc_time_override = hour;
    Common_Set(time.rtc_time.hour, hour);
}

static void shop_test_check_static_transactions(void) {
    const pc_shop_catalog_t* catalog = pc_night_market_catalog();
    pc_shop_result_t result;
    if (!catalog || catalog->item_count != 6) shop_test_finish(0, "night catalog does not expose six items");
    for (int i = 0; i < catalog->item_count; ++i) {
        if (!catalog->items[i].night_exclusive ||
            !pc_world_shops_item_is_night_exclusive(catalog->items[i].item)) {
            shop_test_finish(0, "night stock contains an item not marked night-exclusive");
        }
    }
    if (pc_night_market_is_open(4) != 1 || pc_night_market_is_open(5) != 0 ||
        pc_night_market_is_open(19) != 0 || pc_night_market_is_open(20) != 1) {
        shop_test_finish(0, "night-market hour boundaries are incorrect");
    }

    s_night_item = &catalog->items[0];
    shop_test_reset_player();
    s_player->inventory.wallet = 100000;
    result = pc_shop_buy(s_player, s_night_item, 0);
    if (result != PC_SHOP_ERR_CLOSED || s_player->inventory.wallet != 100000) {
        shop_test_finish(0, "closed-shop transaction changed the wallet");
    }
    shop_test_set_hour(12);
    result = pc_shop_buy(s_player, s_night_item, 1);
    if (result != PC_SHOP_ERR_NOT_FOR_SALE || s_player->inventory.wallet != 100000) {
        shop_test_finish(0, "daytime transaction accepted night-exclusive stock");
    }
    shop_test_set_hour(22);
    s_player->inventory.wallet = 0;
    result = pc_shop_buy(s_player, s_night_item, 1);
    if (result != PC_SHOP_ERR_FUNDS || s_player->inventory.wallet != 0) {
        shop_test_finish(0, "insufficient-funds transaction changed the wallet");
    }
    shop_test_reset_player();
    s_player->inventory.wallet = 100000;
    for (int i = 0; i < mPr_POCKETS_SLOT_COUNT; ++i) {
        s_player->inventory.pockets[i] = ITM_SHOVEL;
    }
    result = pc_shop_buy(s_player, s_night_item, 1);
    if (result != PC_SHOP_ERR_FULL || s_player->inventory.wallet != 100000) {
        shop_test_finish(0, "full-pocket transaction changed the wallet");
    }
    shop_test_reset_player();
}

static void shop_test_start(GAME_PLAY* play) {
    if (!g_pc_dev_east || !play || Save_Get(scene_no) != SCENE_FG) return;
    if (g_pc_dev_shop_test == 2) return;
    if (!Now_Private) shop_test_finish(0, "native resident data is unavailable");

    s_player = Now_Private;
    memcpy(&s_saved_save, Common_GetPointer(save.save), sizeof(s_saved_save));
    s_saved_player = *s_player;
    s_saved_rtc = Common_Get(time.rtc_time);
    s_saved_time_override = g_pc_time_override;
    s_saved_pause = g_pc_paused;
    s_started = 1;

    s_anchor_count = pc_world_service_anchors_get(play, s_anchors, 8);
    if (s_anchor_count > 8) s_anchor_count = 8;
    for (int id = PC_WORLD_SERVICE_NIGHT_MARKET; id <= PC_WORLD_SERVICE_CUSTOMIZATION; ++id) {
        if (!shop_test_anchor(id)) shop_test_finish(0, "world is missing a native service anchor");
    }

    int player_no = Common_Get(player_no);
    if (player_no >= 0 && player_no < PLAYER_NUM) {
        s_home_idx = mHS_get_arrange_idx(player_no);
    }
    if (s_home_idx < 0 || s_home_idx >= PLAYER_NUM) {
        shop_test_finish(0, "resident home arrangement is unavailable");
    }

    shop_test_set_hour(22);
    shop_test_check_static_transactions();
    s_stage = SHOP_TEST_WAIT_WORLD;
}

static void shop_test_open_at(GAME_PLAY* play, int service) {
    pc_world_service_anchor_t* anchor = shop_test_anchor(service);
    if (!anchor || !pc_world_shops_try_open_at(play, anchor->x, anchor->z) || !pc_world_shops_active()) {
        shop_test_finish(0, "interacting at a service anchor did not open its native catalog");
    }
}

void pc_world_shop_test_tick(struct game_play_s* play) {
    if (!g_pc_dev_shop_test || !play) return;

    if (g_pc_dev_shop_test == 2) {
        if (!g_pc_dev_east || Save_Get(scene_no) != SCENE_FG || s_preview_opened) return;
        if (++s_menu_frames < 120) return;
        int service = g_pc_dev_shop_preview;
        if (service < PC_WORLD_SERVICE_NIGHT_MARKET || service > PC_WORLD_SERVICE_CUSTOMIZATION) {
            service = PC_WORLD_SERVICE_NIGHT_MARKET;
        }
        s_anchor_count = pc_world_service_anchors_get(play, s_anchors, 8);
        if (s_anchor_count > 8) s_anchor_count = 8;
        shop_test_open_at(play, service);
        s_preview_opened = 1;
        fprintf(stdout, "[ShopTest] VISUAL: opened service %d after 120 frames; menu held for inspection\n", service);
        fflush(stdout);
        return;
    }

    if (!s_started) {
        shop_test_start(play);
        return;
    }
    if (++s_frames > 1200) shop_test_finish(0, "native integration test timed out");

    switch (s_stage) {
        case SHOP_TEST_WAIT_WORLD: {
            shop_test_open_at(play, PC_WORLD_SERVICE_NIGHT_MARKET);
            shop_test_reset_player();
            s_player->inventory.wallet = 100000;
            if (!shop_test_send_key(SDLK_RETURN)) shop_test_finish(0, "could not enqueue native shop confirmation");
            s_stage = SHOP_TEST_NIGHT_PURCHASE;
            break;
        }
        case SHOP_TEST_NIGHT_PURCHASE: {
            int before;
            if (!pc_world_shops_active()) shop_test_finish(0, "night market closed before purchase event");
            before = shop_test_item_count(s_player, s_night_item->item);
            /* Event dispatch happened before this frame's play_main callback. */
            if (s_player->inventory.wallet != 100000 - s_night_item->price || before != 1) {
                shop_test_finish(0, "native night-market event did not charge and grant its item");
            }
            if (!shop_test_send_key(SDLK_ESCAPE)) shop_test_finish(0, "could not enqueue night-market close");
            s_stage = SHOP_TEST_NIGHT_CLOSE;
            break;
        }
        case SHOP_TEST_NIGHT_CLOSE:
            if (pc_world_shops_active() || g_pc_paused) shop_test_finish(0, "Escape did not close the night-market UI");
            shop_test_set_hour(12);
            shop_test_open_at(play, PC_WORLD_SERVICE_NIGHT_MARKET);
            shop_test_reset_player();
            s_player->inventory.wallet = 100000;
            if (!shop_test_send_key(SDLK_RETURN)) shop_test_finish(0, "could not enqueue daytime market confirmation");
            s_stage = SHOP_TEST_DAY_PURCHASE;
            break;
        case SHOP_TEST_DAY_PURCHASE:
            if (s_player->inventory.wallet != 100000 || shop_test_item_count(s_player, s_night_item->item) != 0) {
                shop_test_finish(0, "daytime night-market menu sold a night-exclusive item");
            }
            if (!shop_test_send_key(SDLK_ESCAPE)) shop_test_finish(0, "could not enqueue daytime market close");
            s_stage = SHOP_TEST_DAY_CLOSE;
            break;
        case SHOP_TEST_DAY_CLOSE:
            if (pc_world_shops_active() || g_pc_paused) shop_test_finish(0, "Escape did not close the daytime market UI");
            shop_test_set_hour(22);
            shop_test_reset_player();
            s_player->inventory.wallet = 10000;
            shop_test_open_at(play, PC_WORLD_SERVICE_MARITIME_OUTFITTER);
            if (!shop_test_send_key(SDLK_RETURN)) shop_test_finish(0, "could not enqueue outfitter confirmation");
            s_stage = SHOP_TEST_OUTFITTER_PURCHASE;
            break;
        case SHOP_TEST_OUTFITTER_PURCHASE:
            if (s_player->inventory.wallet != 9500 || shop_test_item_count(s_player, ITM_ROD) != 1) {
                shop_test_finish(0, "native outfitter event did not charge and grant its tool");
            }
            if (!shop_test_send_key(SDLK_ESCAPE)) shop_test_finish(0, "could not enqueue outfitter close");
            s_stage = SHOP_TEST_OUTFITTER_CLOSE;
            break;
        case SHOP_TEST_OUTFITTER_CLOSE: {
            if (pc_world_shops_active() || g_pc_paused) shop_test_finish(0, "Escape did not close the outfitter UI");
            shop_test_reset_player();
            s_player->inventory.wallet = 10000;
            mHm_hs_c* home = Save_GetPointer(homes[s_home_idx]);
            home->ownerID = s_player->player_ID;
            home->next_outlook_pal = 11;
            shop_test_open_at(play, PC_WORLD_SERVICE_CUSTOMIZATION);
            if (!shop_test_send_key(SDLK_RETURN)) shop_test_finish(0, "could not enqueue studio repaint selection");
            s_stage = SHOP_TEST_STUDIO_OPEN_PAINT;
            break;
        }
        case SHOP_TEST_STUDIO_OPEN_PAINT:
            if (!pc_world_shops_active()) shop_test_finish(0, "studio did not open its paint menu");
            if (!shop_test_send_key(SDLK_RETURN)) shop_test_finish(0, "could not enqueue paint purchase");
            s_stage = SHOP_TEST_STUDIO_PAINT;
            break;
        case SHOP_TEST_STUDIO_PAINT: {
            mHm_hs_c* home = Save_GetPointer(homes[s_home_idx]);
            if (s_player->inventory.wallet != 9000 || home->next_outlook_pal != 0) {
                shop_test_finish(0, "native studio menu did not apply paid roof paint");
            }
            if (!shop_test_send_key(SDLK_ESCAPE)) shop_test_finish(0, "could not enqueue paint menu close");
            s_stage = SHOP_TEST_STUDIO_PAINT_CLOSE;
            break;
        }
        case SHOP_TEST_STUDIO_PAINT_CLOSE:
            if (pc_world_shops_active() || g_pc_paused) shop_test_finish(0, "Escape did not close the paint UI");
            shop_test_reset_player();
            s_player->inventory.wallet = 10000;
            s_player->my_org[0].flag_design_set = TRUE; /* Temporary design fixture; Save_t is restored at exit. */
            shop_test_open_at(play, PC_WORLD_SERVICE_CUSTOMIZATION);
            if (!shop_test_send_key(SDLK_DOWN)) shop_test_finish(0, "could not enqueue pattern menu selection");
            s_stage = SHOP_TEST_PRINT_DOWN;
            break;
        case SHOP_TEST_PRINT_DOWN:
            if (!shop_test_send_key(SDLK_RETURN)) shop_test_finish(0, "could not enqueue pattern list selection");
            s_stage = SHOP_TEST_PRINT_OPEN;
            break;
        case SHOP_TEST_PRINT_OPEN:
            if (!shop_test_send_key(SDLK_RETURN)) shop_test_finish(0, "could not enqueue pattern umbrella purchase");
            s_stage = SHOP_TEST_PRINT_BUY;
            break;
        case SHOP_TEST_PRINT_BUY: {
            mActor_name_t umbrella = (mActor_name_t)(ITM_MY_ORG_UMBRELLA0);
            if (s_player->inventory.wallet != 9650 || shop_test_item_count(s_player, umbrella) != 1) {
                shop_test_finish(0, "native studio did not print the saved pattern umbrella");
            }
            if (!shop_test_send_key(SDLK_ESCAPE)) shop_test_finish(0, "could not enqueue print menu close");
            s_stage = SHOP_TEST_PRINT_CLOSE;
            break;
        }
        case SHOP_TEST_PRINT_CLOSE:
            if (pc_world_shops_active() || g_pc_paused) shop_test_finish(0, "Escape did not close the print menu");
            shop_test_reset_player();
            shop_test_open_at(play, PC_WORLD_SERVICE_CUSTOMIZATION);
            if (!shop_test_send_key(SDLK_DOWN)) shop_test_finish(0, "could not enqueue workbench menu navigation");
            s_stage = SHOP_TEST_WORKBENCH_DOWN1;
            break;
        case SHOP_TEST_WORKBENCH_DOWN1:
            if (!shop_test_send_key(SDLK_DOWN)) shop_test_finish(0, "could not enqueue workbench menu navigation");
            s_stage = SHOP_TEST_WORKBENCH_DOWN2;
            break;
        case SHOP_TEST_WORKBENCH_DOWN2:
            if (!shop_test_send_key(SDLK_RETURN)) shop_test_finish(0, "could not enqueue native design-workbench selection");
            s_stage = SHOP_TEST_WORKBENCH_OPEN;
            break;
        case SHOP_TEST_WORKBENCH_OPEN:
            if (pc_world_shops_active()) shop_test_finish(0, "workbench selection left the commerce menu open");
            s_stage = SHOP_TEST_EDITOR_WAIT;
            break;
        case SHOP_TEST_EDITOR_WAIT:
            if (play->submenu.menu_type == mSM_OVL_NEEDLEWORK &&
                play->submenu.param0 == mNW_OPEN_DESIGN &&
                play->submenu.process_status != mSM_PROCESS_WAIT) {
                shop_test_finish(1, "service anchors, SDL menu input, purchases, failure gates, roof paint, printed pattern and native workbench passed");
            }
            break;
        default:
            shop_test_finish(0, "invalid test state");
    }
}
#else
void pc_world_shop_test_tick(struct game_play_s* play) { (void)play; }
#endif
