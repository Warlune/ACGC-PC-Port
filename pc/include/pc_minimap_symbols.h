#ifndef PC_MINIMAP_SYMBOLS_H
#define PC_MINIMAP_SYMBOLS_H
#include "m_name_table.h"
enum {
    PC_MAP_HOME,PC_MAP_VILLAGER,PC_MAP_SHOP,PC_MAP_POST,PC_MAP_STATION,
    PC_MAP_MUSEUM,PC_MAP_POLICE,PC_MAP_TAILOR,PC_MAP_WELL,PC_MAP_PORT,
    PC_MAP_DUMP,PC_MAP_TENT,PC_MAP_LIGHTHOUSE,PC_MAP_SYMBOL_TYPES
};
#define PC_MAP_MAX_SYMBOLS 256
typedef struct {float x,z;int kind;} PCMapSymbol;
void pc_minimap_set_symbols(const PCMapSymbol* symbols,int count);
static inline int pc_minimap_symbol_type(unsigned item) {
    if(ITEM_IS_PLAYER_HOUSE(item))return PC_MAP_HOME;
    if(ITEM_IS_NPC_HOUSE(item)||ITEM_IS_DUMMY_NPC_HOUSE(item))return PC_MAP_VILLAGER;
    if(item>=SHOP0 && item<=SHOP3)return PC_MAP_SHOP;
    switch(item) {
        case POST_OFFICE:return PC_MAP_POST;
        case TRAIN_STATION:return PC_MAP_STATION;
        case MUSEUM:return PC_MAP_MUSEUM;
        case POLICE_STATION:return PC_MAP_POLICE;
        case NEEDLEWORK_SHOP:return PC_MAP_TAILOR;
        case WISHING_WELL:return PC_MAP_WELL;
        case BOAT:case PORT_SIGN:return PC_MAP_PORT;
        case DUMP:return PC_MAP_DUMP;
        case BROKER_TENT:case FORTUNE_TENT:case TENT:return PC_MAP_TENT;
        case TOUDAI:return PC_MAP_LIGHTHOUSE;
        default:return -1;
    }
}
#endif
