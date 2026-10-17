#ifndef PC_MINIMAP_H
#define PC_MINIMAP_H
#ifdef __cplusplus
extern "C" {
#endif
#define PC_MINIMAP_ROWS 8
#define PC_MINIMAP_CAPTURE 0xAC6000u
#define PC_MINIMAP_CAPTURE_END 0xAC6100u
#define PC_MINIMAP_RESET 0xAC6200u
struct game_play_s;
typedef struct {
    float left, top, width, height, marker_x, marker_y;
} PCMinimapLayout;
/* Returns zero for positions outside the town or invalid output dimensions. */
int pc_minimap_layout(PCMinimapLayout* out, int output_w, int output_h,
                      int size, float acre_x, float acre_z);
void pc_minimap_draw(struct game_play_s* play);
void pc_minimap_set_overlay(const PCMinimapLayout* layout);
void pc_minimap_capture_begin(int tile);
void pc_minimap_capture_end(void);
void pc_minimap_reset(void);
void pc_minimap_present(void);
void pc_minimap_shutdown(void);
extern int g_pc_minimap_capture;
#ifdef __cplusplus
}
#endif
#endif
