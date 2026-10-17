#ifndef PC_DEV_EAST_H
#define PC_DEV_EAST_H
#ifdef ACO_DEV_EAST
struct game_play_s;
extern int g_pc_dev_east;
extern int g_pc_dev_east_spawn;
void pc_dev_east_diagnostics(void);
void pc_dev_east_prepare(void);
void pc_dev_east_start(struct game_play_s* play);
#endif
#endif
