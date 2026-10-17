#ifndef PC_WORLD_SHOP_TEST_H
#define PC_WORLD_SHOP_TEST_H

struct game_play_s;

/* 0=off, 1=automated native integration test, 2=hold a visible shop menu. */
extern int g_pc_dev_shop_test;

/* Call from play_main before mCon_main so the test also advances while paused. */
void pc_world_shop_test_tick(struct game_play_s* play);

#endif
