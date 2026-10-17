#include "m_common_data.h"
#include <stddef.h>
#include <stdio.h>
int main(void) {
    printf("{\"width\":%u,\"fg_width\":%u,\"save_size\":%u,\"sector_size\":%u,\"others\":%u,\"file_data_size\":%u,",
        BLOCK_X_NUM,FG_BLOCK_X_NUM,(unsigned)sizeof(Save_t),(unsigned)sizeof(Save),
        (unsigned)OTHERS_SIZE,(unsigned)mCD_LAND_SAVE_SIZE);
#define FIELD(n) printf("\"" #n "\":%u,",(unsigned)offsetof(Save_t,n))
    FIELD(fg);FIELD(combi_table);FIELD(animals);FIELD(melody);FIELD(deposit);
    FIELD(last_grow_time);FIELD(time_delta);FIELD(island);
    printf("\"end\":%u}\n",(unsigned)sizeof(Save_t));
    return 0;
}
