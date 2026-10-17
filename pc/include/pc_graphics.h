#ifndef PC_GRAPHICS_H
#define PC_GRAPHICS_H

/* Window size and internal render size are intentionally independent. */
extern int g_pc_render_w;
extern int g_pc_render_h;
void pc_graphics_init(void);
void pc_graphics_shutdown(void);
void pc_graphics_begin_frame(void);
void pc_graphics_present(void);
void pc_graphics_readback_begin(void);
void pc_graphics_readback_end(void);
int pc_graphics_samples(void);
int pc_graphics_max_samples(void);
int pc_graphics_max_anisotropy(void);
int pc_graphics_anisotropy(void);
void pc_graphics_texture_sampler(void);
void pc_graphics_support_text(char* out, int size);

#endif
