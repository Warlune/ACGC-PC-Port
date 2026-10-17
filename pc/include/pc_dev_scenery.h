#ifndef PC_DEV_SCENERY_H
#define PC_DEV_SCENERY_H
#ifdef ACO_DEV_EAST
typedef struct {float x,y,z;unsigned short item;} PCDevScenery;
int pc_dev_outer_scenery(PCDevScenery* out,int capacity,float player_x,float player_z);
#endif
#endif
