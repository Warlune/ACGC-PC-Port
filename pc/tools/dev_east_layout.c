#include "m_common_data.h"
#include <stddef.h>
#include <stdio.h>
#ifndef ACO_TOWN_X0
#define ACO_TOWN_X0 0
#endif
int main(void) {
    printf("{\"height\":%u,\"visible_rows\":%u,\"island_z\":%u,\"version\":3,\"town_start\":%u,",BLOCK_Z_NUM,BLOCK_Z_NUM-2,mISL_BLOCK_Z,ACO_TOWN_X0);
    printf("\"width\":%u,\"fg_width\":%u,\"save_size\":%u,\"sector_size\":%u,\"others\":%u,\"file_data_size\":%u,",
        BLOCK_X_NUM,FG_BLOCK_X_NUM,(unsigned)sizeof(Save_t),(unsigned)sizeof(Save),
        (unsigned)OTHERS_SIZE,(unsigned)(OTHERS_SIZE+2*sizeof(Save)));
#define FIELD(n) printf("\"" #n "\":%u,",(unsigned)offsetof(Save_t,n))
    FIELD(fg);FIELD(combi_table);FIELD(animals);FIELD(melody);FIELD(deposit);
    FIELD(last_grow_time);FIELD(bridge);FIELD(needlework);FIELD(time_delta);FIELD(island);
    printf("\"animal_count\":%u,\"animal_stride\":%u,\"animal_home\":%u,\"animal_home_x\":%u,\"island_animal\":%u,\"bridge_x\":%u,\"end\":%u}\n",ANIMAL_NUM_MAX,
        (unsigned)sizeof(Animal_c),(unsigned)offsetof(Animal_c,home_info),(unsigned)offsetof(Anmhome_c,block_x),
        (unsigned)offsetof(Island_c,animal),(unsigned)offsetof(PlusBridge_c,block_x),(unsigned)sizeof(Save_t));
    return 0;
}
