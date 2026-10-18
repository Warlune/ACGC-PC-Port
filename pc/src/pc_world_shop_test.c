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
#include "m_scene.h"
#include "m_controller.h"
#include "m_needlework_ovl.h"
#include "m_play.h"
#include "m_player_lib.h"
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
static int s_test_hour = -1;
static int s_saved_pause;
static int s_started;
static int s_stage;
static int s_frames;
static int s_stage_frames;
static int s_menu_frames;
static int s_preview_opened;
static int s_home_idx = -1;
static pc_world_service_anchor_t s_anchors[8];
static int s_anchor_count;
static int s_pending_service;
static int s_pending_transition;
static int s_return_service;
static int s_current_service;
static SDL_Keycode s_queued_key;
static const pc_shop_item_t* s_night_item;
static Private_c* s_player;

static const char* shop_test_stage_name(int stage) {
    static const char* names[]={"world-ready","night-purchase","night-close","day-purchase","day-close",
        "outfitter-purchase","outfitter-close","studio-paint-open","studio-paint-buy","studio-paint-close",
        "print-down","print-open","print-buy","print-close","workbench-down1","workbench-down2",
        "workbench-open","editor-wait"};
    return stage >= 0 && stage < (int)(sizeof(names)/sizeof(names[0])) ? names[stage] : "unknown";
}

static void shop_test_set_stage(int stage) {
    s_stage=stage;
    s_stage_frames=0;
    fprintf(stdout,"[ShopTest] stage: %s\n",shop_test_stage_name(stage));
    fflush(stdout);
}

static void shop_test_restore(void) {
    if (!s_started) return;
    pc_world_shops_close();
    memcpy(Common_GetPointer(save.save), &s_saved_save, sizeof(s_saved_save));
    Common_Set(time.rtc_time, s_saved_rtc);
    g_pc_time_override = s_saved_time_override;
    s_test_hour = -1;
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
    if (!pc_world_shops_active()) {
        s_queued_key = key;
        return 1;
    }
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
    s_test_hour = hour;
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
    shop_test_set_stage(SHOP_TEST_WAIT_WORLD);
}

static int shop_test_begin_entry(GAME_PLAY* play, int service) {
    pc_world_service_anchor_t* anchor = shop_test_anchor(service);
    Door_data_c entry;
    Door_data_c* exit;
    ACTOR service_actor;
    float offset;
    if (!anchor) return 0;
    offset = service == PC_WORLD_SERVICE_CUSTOMIZATION ? 64.0f : 68.29f;
    memset(&service_actor, 0, sizeof(service_actor));
    service_actor.npc_id = service == PC_WORLD_SERVICE_CUSTOMIZATION ? NEEDLEWORK_SHOP : SHOP0;
    service_actor.world.position.x = anchor->x + offset;
    service_actor.world.position.z = anchor->z - offset;
    pc_world_commerce_prepare_entry(&service_actor, play);
    exit = Common_GetPointer(structure_exit_door_data);
    memset(exit, 0, sizeof(*exit));
    exit->next_scene_id = SCENE_FG;
    exit->exit_orientation = mSc_DIRECT_SOUTH_WEST;
    exit->extra_data = service == PC_WORLD_SERVICE_CUSTOMIZATION ? 2 : 3;
    exit->exit_position.x = anchor->x;
    exit->exit_position.y = 0.0f;
    exit->exit_position.z = anchor->z;
    exit->door_actor_name = service == PC_WORLD_SERVICE_CUSTOMIZATION ? NEEDLEWORK_SHOP : SHOP0;
    exit->wipe_type = WIPE_TYPE_TRIFORCE;
    memset(&entry, 0, sizeof(entry));
    entry.next_scene_id = service == PC_WORLD_SERVICE_CUSTOMIZATION ? SCENE_NEEDLEWORK : SCENE_SHOP0;
    entry.exit_orientation = service == PC_WORLD_SERVICE_CUSTOMIZATION ? 4 : mSc_DIRECT_NORTH;
    entry.exit_position.x = 160.0f;
    entry.exit_position.z = 300.0f;
    entry.door_actor_name = EMPTY_NO;
    entry.wipe_type = WIPE_TYPE_TRIFORCE;
    return goto_other_scene(play, &entry, FALSE) == 1;
}

static int shop_test_trigger_seller(GAME_PLAY* play, int service) {
    PLAYER_ACTOR* player = get_player_actor_withoutCheck(play);
    int scene = Save_Get(scene_no);
    if (!player || scene != (service == PC_WORLD_SERVICE_CUSTOMIZATION ? SCENE_NEEDLEWORK : SCENE_SHOP0)) return 0;
    for (ACTOR* actor = play->actor_info.list[ACTOR_PART_NPC].actor; actor; actor = actor->next_actor) {
        if (scene == SCENE_SHOP0 && service == PC_WORLD_SERVICE_NIGHT_MARKET &&
            actor->npc_id != SP_NPC_MAMEDANUKI0) continue;
        if (scene == SCENE_SHOP0 && service == PC_WORLD_SERVICE_MARITIME_OUTFITTER &&
            actor->npc_id != SP_NPC_SHOP_MASTER) continue;
        player->actor_class.world.position.x = actor->world.position.x;
        player->actor_class.world.position.z = actor->world.position.z;
        play->game.pads[0].on.button |= BUTTON_A;
        pc_world_commerce_tick(play);
        play->game.pads[0].on.button &= ~BUTTON_A;
        return pc_world_shops_active();
    }
    fprintf(stderr, "[ShopTest] no seller actor for service=%d scene=%d ids:", service, scene);
    for (ACTOR* actor = play->actor_info.list[ACTOR_PART_NPC].actor; actor; actor = actor->next_actor) {
        fprintf(stderr, " %04x", (unsigned)actor->npc_id);
    }
    fputc('\n', stderr);
    fflush(stderr);
    return 0;
}

static void shop_test_open_at(GAME_PLAY* play, int service) {
    int target_scene=service==PC_WORLD_SERVICE_CUSTOMIZATION?SCENE_NEEDLEWORK:SCENE_SHOP0;
    if (s_pending_service) shop_test_finish(0, "a second interior entry was requested before the previous one completed");
    /* The day-boundary and studio submenu checks can reuse the interior we
     * already occupy; do not force an unnecessary round trip through the
     * field when the native service room is unchanged. */
    if (Save_Get(scene_no)==target_scene && s_current_service==service) {
        if (!shop_test_trigger_seller(play,service)) shop_test_finish(0,"seller interaction failed in the current service room");
        return;
    }
    s_pending_service = service;
    s_pending_transition = 0;
    if (Save_Get(scene_no) != SCENE_FG) {
        s_return_service = s_current_service;
        if (s_return_service == 0) shop_test_finish(0, "current native interior service was not recorded");
        if (goto_other_scene(play, Common_GetPointer(structure_exit_door_data), FALSE) != 1) {
            shop_test_finish(0, "native interior exit could not return to the field");
        }
        s_pending_transition = 1;
    } else if (!shop_test_begin_entry(play, service)) {
        shop_test_finish(0, "native service door transition could not begin");
    } else {
        s_pending_transition = 2;
    }
}

static int shop_test_progress_entry(GAME_PLAY* play) {
    if (!s_pending_service) return 0;
    if (s_pending_transition == 1) {
        if (Save_Get(scene_no) != SCENE_FG || play->fb_wipe_mode != WIPE_MODE_NONE ||
            play->fb_fade_type != FADE_TYPE_NONE) return 1;
        pc_world_service_anchor_t* return_anchor = shop_test_anchor(s_return_service);
        PLAYER_ACTOR* player = get_player_actor_withoutCheck(play);
        if (!return_anchor || !player) shop_test_finish(0, "native interior return did not restore the player");
        float return_dx = player->actor_class.world.position.x - return_anchor->x;
        float return_dz = player->actor_class.world.position.z - return_anchor->z;
        if (return_dx * return_dx + return_dz * return_dz > 320.0f * 320.0f) {
            fprintf(stderr, "[ShopTest] return mismatch service=%d expected=(%.1f,%.1f) actual=(%.1f,%.1f) distance=%.1f\n",
                    s_return_service, return_anchor->x, return_anchor->z,
                    player->actor_class.world.position.x, player->actor_class.world.position.z,
                    return_dx * return_dx + return_dz * return_dz);
            fflush(stderr);
            shop_test_finish(0, "native interior exit returned to the wrong storefront");
        }
        if (!shop_test_begin_entry(play, s_pending_service)) shop_test_finish(0, "native service door transition could not begin after exit");
        s_pending_transition = 2;
        return 1;
    }
    int expected_scene = s_pending_service == PC_WORLD_SERVICE_CUSTOMIZATION ? SCENE_NEEDLEWORK : SCENE_SHOP0;
    if (Save_Get(scene_no) != expected_scene || play->fb_wipe_mode != WIPE_MODE_NONE ||
        play->fb_fade_type != FADE_TYPE_NONE) return 1;
    int service = s_pending_service;
    s_current_service = service;
    s_pending_service = 0;
    s_pending_transition = 0;
    if (!shop_test_trigger_seller(play, service)) shop_test_finish(0, "A interaction at the native interior seller did not open its catalog");
    if (s_queued_key) {
        SDL_Keycode key = s_queued_key;
        s_queued_key = 0;
        if (!shop_test_send_key(key)) shop_test_finish(0, "could not enqueue delayed native shop confirmation");
    }
    return 1;
}

void pc_world_shop_test_tick(struct game_play_s* play) {
    if (!g_pc_dev_shop_test || !play) return;

    /* The ROM's clock may resample the host RTC between title and field
     * callbacks. Pin the test hour here, after that update and before the next
     * SDL event/draw pass, so production shop-hour checks see this value. */
    if (s_started && s_test_hour >= 0) Common_Set(time.rtc_time.hour, s_test_hour);

    if (g_pc_dev_shop_test == 2) {
        if (!g_pc_dev_east) return;
        if (s_preview_opened) { shop_test_progress_entry(play); return; }
        if (Save_Get(scene_no) != SCENE_FG) return;
        if (++s_menu_frames < 120) return;
        int service = g_pc_dev_shop_preview;
        if (service < PC_WORLD_SERVICE_NIGHT_MARKET || service > PC_WORLD_SERVICE_CUSTOMIZATION) {
            service = PC_WORLD_SERVICE_NIGHT_MARKET;
        }
        s_anchor_count = pc_world_service_anchors_get(play, s_anchors, 8);
        if (s_anchor_count > 8) s_anchor_count = 8;
        shop_test_open_at(play, service);
        s_preview_opened = 1;
        fprintf(stdout, "[ShopTest] VISUAL: entering native service %d after 120 frames; menu will be held for inspection\n", service);
        fflush(stdout);
        return;
    }

    if (!s_started) {
        shop_test_start(play);
        return;
    }
    if (++s_frames > 6000) shop_test_finish(0, "native integration test exceeded its total frame budget");
    if (++s_stage_frames > 720) {
        char reason[192];
        snprintf(reason,sizeof(reason),"stage '%s' timed out (service=%d transition=%d scene=%d)",
                 shop_test_stage_name(s_stage),s_pending_service,s_pending_transition,Save_Get(scene_no));
        shop_test_finish(0,reason);
    }
    if (shop_test_progress_entry(play)) return;

    switch (s_stage) {
        case SHOP_TEST_WAIT_WORLD: {
            /* Dev-world preparation can finish before the field player actor
             * has been installed. Do not ask the native transition helper to
             * leave until it can capture a real player and the entry wipe is
             * fully idle. */
            if (Save_Get(scene_no) != SCENE_FG || !get_player_actor_withoutCheck(play) ||
                play->fb_wipe_mode != WIPE_MODE_NONE || play->fb_fade_type != FADE_TYPE_NONE ||
                play->fb_mode != FBDEMO_MODE_NONE) return;
            shop_test_open_at(play, PC_WORLD_SERVICE_NIGHT_MARKET);
            shop_test_reset_player();
            s_player->inventory.wallet = 100000;
            if (!shop_test_send_key(SDLK_RETURN)) shop_test_finish(0, "could not enqueue native shop confirmation");
            shop_test_set_stage(SHOP_TEST_NIGHT_PURCHASE);
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
            shop_test_set_stage(SHOP_TEST_NIGHT_CLOSE);
            break;
        }
        case SHOP_TEST_NIGHT_CLOSE:
            if (pc_world_shops_active() || g_pc_paused) shop_test_finish(0, "Escape did not close the night-market UI");
            shop_test_set_hour(12);
            shop_test_open_at(play, PC_WORLD_SERVICE_NIGHT_MARKET);
            shop_test_reset_player();
            s_player->inventory.wallet = 100000;
            if (!shop_test_send_key(SDLK_RETURN)) shop_test_finish(0, "could not enqueue daytime market confirmation");
            shop_test_set_stage(SHOP_TEST_DAY_PURCHASE);
            break;
        case SHOP_TEST_DAY_PURCHASE:
            if (s_player->inventory.wallet != 100000 || shop_test_item_count(s_player, s_night_item->item) != 0) {
                fprintf(stderr, "[ShopTest] daytime check hour=%d wallet=%u item_count=%d active=%d\n",
                        Common_Get(time.rtc_time.hour), (unsigned)s_player->inventory.wallet,
                        shop_test_item_count(s_player, s_night_item->item), pc_world_shops_active());
                fflush(stderr);
                shop_test_finish(0, "daytime night-market menu sold a night-exclusive item");
            }
            if (!shop_test_send_key(SDLK_ESCAPE)) shop_test_finish(0, "could not enqueue daytime market close");
            shop_test_set_stage(SHOP_TEST_DAY_CLOSE);
            break;
        case SHOP_TEST_DAY_CLOSE:
            if (pc_world_shops_active() || g_pc_paused) shop_test_finish(0, "Escape did not close the daytime market UI");
            shop_test_set_hour(22);
            shop_test_reset_player();
            s_player->inventory.wallet = 10000;
            shop_test_open_at(play, PC_WORLD_SERVICE_MARITIME_OUTFITTER);
            if (!shop_test_send_key(SDLK_RETURN)) shop_test_finish(0, "could not enqueue outfitter confirmation");
            shop_test_set_stage(SHOP_TEST_OUTFITTER_PURCHASE);
            break;
        case SHOP_TEST_OUTFITTER_PURCHASE:
            if (s_player->inventory.wallet != 9500 || shop_test_item_count(s_player, ITM_ROD) != 1) {
                shop_test_finish(0, "native outfitter event did not charge and grant its tool");
            }
            if (!shop_test_send_key(SDLK_ESCAPE)) shop_test_finish(0, "could not enqueue outfitter close");
            shop_test_set_stage(SHOP_TEST_OUTFITTER_CLOSE);
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
            shop_test_set_stage(SHOP_TEST_STUDIO_OPEN_PAINT);
            break;
        }
        case SHOP_TEST_STUDIO_OPEN_PAINT:
            if (!pc_world_shops_active()) shop_test_finish(0, "studio did not open its paint menu");
            if (!shop_test_send_key(SDLK_RETURN)) shop_test_finish(0, "could not enqueue paint purchase");
            shop_test_set_stage(SHOP_TEST_STUDIO_PAINT);
            break;
        case SHOP_TEST_STUDIO_PAINT: {
            mHm_hs_c* home = Save_GetPointer(homes[s_home_idx]);
            if (s_player->inventory.wallet != 9000 || home->next_outlook_pal != 0) {
                shop_test_finish(0, "native studio menu did not apply paid roof paint");
            }
            if (!shop_test_send_key(SDLK_ESCAPE)) shop_test_finish(0, "could not enqueue paint menu close");
            shop_test_set_stage(SHOP_TEST_STUDIO_PAINT_CLOSE);
            break;
        }
        case SHOP_TEST_STUDIO_PAINT_CLOSE:
            if (pc_world_shops_active() || g_pc_paused) shop_test_finish(0, "Escape did not close the paint UI");
            shop_test_reset_player();
            s_player->inventory.wallet = 10000;
            s_player->my_org[0].flag_design_set = TRUE; /* Temporary design fixture; Save_t is restored at exit. */
            shop_test_open_at(play, PC_WORLD_SERVICE_CUSTOMIZATION);
            if (!shop_test_send_key(SDLK_DOWN)) shop_test_finish(0, "could not enqueue pattern menu selection");
            shop_test_set_stage(SHOP_TEST_PRINT_DOWN);
            break;
        case SHOP_TEST_PRINT_DOWN:
            if (!shop_test_send_key(SDLK_RETURN)) shop_test_finish(0, "could not enqueue pattern list selection");
            shop_test_set_stage(SHOP_TEST_PRINT_OPEN);
            break;
        case SHOP_TEST_PRINT_OPEN:
            if (!shop_test_send_key(SDLK_RETURN)) shop_test_finish(0, "could not enqueue pattern umbrella purchase");
            shop_test_set_stage(SHOP_TEST_PRINT_BUY);
            break;
        case SHOP_TEST_PRINT_BUY: {
            mActor_name_t umbrella = (mActor_name_t)(ITM_MY_ORG_UMBRELLA0);
            if (s_player->inventory.wallet != 9650 || shop_test_item_count(s_player, umbrella) != 1) {
                shop_test_finish(0, "native studio did not print the saved pattern umbrella");
            }
            if (!shop_test_send_key(SDLK_ESCAPE)) shop_test_finish(0, "could not enqueue print menu close");
            shop_test_set_stage(SHOP_TEST_PRINT_CLOSE);
            break;
        }
        case SHOP_TEST_PRINT_CLOSE:
            if (pc_world_shops_active() || g_pc_paused) shop_test_finish(0, "Escape did not close the print menu");
            shop_test_reset_player();
            shop_test_open_at(play, PC_WORLD_SERVICE_CUSTOMIZATION);
            if (!shop_test_send_key(SDLK_DOWN)) shop_test_finish(0, "could not enqueue workbench menu navigation");
            shop_test_set_stage(SHOP_TEST_WORKBENCH_DOWN1);
            break;
        case SHOP_TEST_WORKBENCH_DOWN1:
            if (!shop_test_send_key(SDLK_DOWN)) shop_test_finish(0, "could not enqueue workbench menu navigation");
            shop_test_set_stage(SHOP_TEST_WORKBENCH_DOWN2);
            break;
        case SHOP_TEST_WORKBENCH_DOWN2:
            if (!shop_test_send_key(SDLK_RETURN)) shop_test_finish(0, "could not enqueue native design-workbench selection");
            shop_test_set_stage(SHOP_TEST_WORKBENCH_OPEN);
            break;
        case SHOP_TEST_WORKBENCH_OPEN:
            if (pc_world_shops_active()) shop_test_finish(0, "workbench selection left the commerce menu open");
            shop_test_set_stage(SHOP_TEST_EDITOR_WAIT);
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
