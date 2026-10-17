/* Runs the game's unmodified five-acre town generator outside the client.
 * Compile WITHOUT ACO_DEV_EAST: its authored base templates are 7x10. */
#include "m_common_data.h"
#include "m_random_field_ovl.h"
#include "m_random_field.h"
#include "libc64/qrand.h"
#include <stdio.h>
#include <stdlib.h>
common_data_t common_data;
/* Same river-direction lookup as m_random_field.c. Keeping this small
 * adapter here avoids linking runtime collision/field code into the tool. */
int mRF_RiverIdx2NextDirect(int river_idx) {
    static const u8 next[]={mRF_DIRECT_SOUTH,mRF_DIRECT_EAST,mRF_DIRECT_WEST,
        mRF_DIRECT_EAST,mRF_DIRECT_SOUTH,mRF_DIRECT_WEST,mRF_DIRECT_SOUTH};
    return river_idx>=0 && river_idx<mRF_RIVER_NUM?next[river_idx]:mRF_DIRECT_SOUTH;
}
int main(int argc,char** argv) {
    if(argc!=2) {fprintf(stderr,"Usage: generate_dev_town SEED\n");return 2;}
    unsigned seed=strtoul(argv[1],NULL,0);
    mFM_combination_c acres[BLOCK_TOTAL_NUM];
    sqrand(seed);
    mRF_MakeRandomField_ovl(acres,data_combi_table,data_combi_table_number,NULL);
    printf("{\"seed\":%u,\"width\":7,\"height\":10,\"acres\":[",seed);
    for(int i=0;i<BLOCK_TOTAL_NUM;i++) {
        int c=acres[i].combination_type;
        if(c<0 || c>=data_combi_table_number)return 3;
        printf("%s[%u,%u,%u,%u]",i?",":"",c,acres[i].height,
            data_combi_table[c].fg_id,data_combi_table[c].type);
    }
    printf("]}\n");return 0;
}
