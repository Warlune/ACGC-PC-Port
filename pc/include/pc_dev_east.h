#ifndef PC_DEV_EAST_H
#define PC_DEV_EAST_H
#ifdef ACO_DEV_EAST
struct game_play_s;
extern int g_pc_dev_east;
extern int g_pc_dev_east_spawn;
extern int g_pc_dev_coast_test;
extern int g_pc_dev_test_stick;
void pc_dev_coast_tick(struct game_play_s* play);
int pc_dev_ridge_side(int bx,int bz);
void pc_dev_coast_field(void);
void pc_dev_east_diagnostics(void);
void pc_dev_east_prepare(void);
void pc_dev_east_start(struct game_play_s* play);
#endif
#endif
