#ifndef PC_DEV_COAST_WALLS_H
#define PC_DEV_COAST_WALLS_H
#ifdef ACO_DEV_EAST
void* pc_dev_outer_coast_model(int bx,int bz);
void* pc_dev_outer_ground_model(int bx,int bz,void* original);
void* pc_dev_outer_coast_water_model(int bx,int bz);
int pc_dev_world_void_block(int bx,int bz);
#endif
#endif
