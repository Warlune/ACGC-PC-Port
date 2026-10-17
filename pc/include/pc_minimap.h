#ifndef PC_MINIMAP_H
#define PC_MINIMAP_H
struct game_play_s;
typedef struct {
    float left, top, width, height, marker_x, marker_y;
} PCMinimapLayout;
/* Returns zero for positions outside the town or invalid output dimensions. */
int pc_minimap_layout(PCMinimapLayout* out, int output_w, int output_h,
                      int size, float acre_x, float acre_z);
void pc_minimap_draw(struct game_play_s* play);
#endif
