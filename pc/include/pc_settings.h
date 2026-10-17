#ifndef PC_SETTINGS_H
#define PC_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int window_width;
    int window_height;
    int fullscreen;       /* 0=windowed, 1=fullscreen, 2=borderless */
    int vsync;            /* 0=off, 1=on */
    int max_fps;          /* 0=uncapped, otherwise frame limiter target */
    int msaa;             /* 0=off, 2/4/8=samples */
    int render_scale;     /* percent of output resolution, 50..200 */
    int upscale_filter;   /* 0=nearest, 1=bilinear, 2=Catmull-Rom bicubic */
    int fxaa;             /* spatial post-process AA */
    int sharpness;        /* bounded unsharp mask strength, 0..100 */
    int show_fps;         /* show FPS in the window title */
    int anisotropy;       /* 1=off, 2/4/8/16x; clamped to device support */
    int texture_replacements; /* 0=original, 1=pack, 2=pack + runtime 4x fallback */
    int texture_filtering; /* 0=force nearest-neighbor, 1=use the game's texture filtering */
    int preload_textures; /* 0=off (load on demand), 1=on (load all at startup), 2=on + cache file */
    int disable_resetti;  /* 0=normal (Resetti appears on reset), 1=disable reset penalty */
    int disable_shop_visitor_req; /* 0=normal (Nookington's needs a foreign-town shopper), 1=skip requirement */
    int borderless_acres; /* 0=original acre transitions (faster, draws less), 1=continuous camera/movement */
    int camera_distance;  /* normal gameplay camera distance percent, 75..150, default 100 */
    int minimap;          /* outdoor town minimap: 0=off, 1=on */
    int minimap_size;     /* 0=small, 1=medium, 2=large */
    int minimap_opacity;  /* percent 0..100; default 30 */
    int nes_aspect;       /* NES emulator aspect: 0=fullscreen stretch, 1=4:3 pillarbox (default) */
    int master_volume;    /* Applied at the PC audio output, 0-100 (default 100) */
    int stick_deadzone;   /* Gamepad main stick deadzone, percent 0-40 (default 12) */
    int cstick_deadzone;  /* Gamepad C-stick deadzone, percent 0-40 (default 12) */
} PCSettings;

extern PCSettings g_pc_settings;

void pc_settings_load(void);
void pc_settings_save(void);
void pc_settings_apply(void);
void pc_settings_cycle_resolution(int* width, int* height, int dir);

#ifdef __cplusplus
}
#endif

#endif /* PC_SETTINGS_H */
