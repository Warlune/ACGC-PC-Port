#ifndef PC_MINIMAP_STYLE_H
#define PC_MINIMAP_STYLE_H
/* Clip the off-map river sources north of the railway, in acre units. */
#define PC_MAP_NORTH_CROP 0.75f
#ifdef ACO_DEV_EAST
#include <stdlib.h>
/* Planning exports include the normally hidden boat-island row. */
#undef PC_MINIMAP_ROWS
#define PC_MINIMAP_ROWS (getenv("ACO_MAP_EXPORT")?9:8)
#endif
#endif
