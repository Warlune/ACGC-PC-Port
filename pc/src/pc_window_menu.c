/* Native Windows menu, sharing the same persisted settings as the game UI. */
#include "pc_platform.h"
#include "pc_settings.h"
#include "pc_settings_menu.h"
#include "pc_graphics.h"
#include "pc_window_menu.h"
#include "pc_texture_pack.h"
#ifdef _WIN32
#include <SDL_syswm.h>
#include <shellapi.h>

enum {
    CMD_SAVE = 1000, CMD_SAVE_FOLDER, CMD_QUIT, CMD_SUPPORT,
    CMD_VSYNC, CMD_FXAA, CMD_TEXTURE_FILTER, CMD_FPS_COUNTER, CMD_TEXTURE_COMPARE,
    CMD_DISPLAY = 1100, CMD_RES = 1200, CMD_MSAA = 1300,
    CMD_SCALE = 1400, CMD_FILTER = 1500, CMD_SHARP = 1600,
    CMD_FPS = 1700, CMD_PRESET = 1800, CMD_ANISO = 1900, CMD_TEXTURE_MODE = 2000
};
static HWND hwnd;
static HMENU bar;
static unsigned int pending[32];
static int pending_count;
static const int aa_values[] = {0, 2, 4, 8};
static const int aniso_values[] = {1, 2, 4, 8, 16};
static const int scale_values[] = {50, 67, 75, 100, 125, 150, 200};
static const int fps_values[] = {30, 60, 90, 120, 144, 165, 180, 240, 0};
static const int resolutions[][2] = {
    {640,480}, {800,600}, {1280,720}, {1280,960}, {1600,1200},
    {1920,1080}, {2560,1440}, {3840,2160}
};
#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

static HMENU submenu(HMENU parent, const char* label) {
    HMENU child = CreatePopupMenu();
    AppendMenuA(parent, MF_POPUP, (UINT_PTR)child, label);
    return child;
}
static void item(HMENU menu, unsigned id, const char* label) { AppendMenuA(menu, MF_STRING, id, label); }
static void check(unsigned id, int checked) {
    CheckMenuItem(bar, id, MF_BYCOMMAND | (checked ? MF_CHECKED : MF_UNCHECKED));
}

void pc_window_menu_refresh(void) {
    if (!bar) return;
    for (int i=0; i<3; i++) check(CMD_DISPLAY+i, g_pc_settings.fullscreen==i);
    for (int i=0; i<COUNT(aa_values); i++) {
        check(CMD_MSAA+i, g_pc_settings.msaa==aa_values[i]);
        EnableMenuItem(bar, CMD_MSAA+i, MF_BYCOMMAND |
            (aa_values[i]<=pc_graphics_max_samples() ? MF_ENABLED : MF_GRAYED));
    }
    for (int i=0; i<COUNT(scale_values); i++) check(CMD_SCALE+i, g_pc_settings.render_scale==scale_values[i]);
    for (int i=0; i<COUNT(fps_values); i++) check(CMD_FPS+i, g_pc_settings.max_fps==fps_values[i]);
    for (int i=0; i<COUNT(resolutions); i++) check(CMD_RES+i,
        g_pc_settings.window_width==resolutions[i][0] && g_pc_settings.window_height==resolutions[i][1]);
    for (int i=0; i<3; i++) check(CMD_FILTER+i, g_pc_settings.upscale_filter==i);
    for (int i=0; i<=4; i++) check(CMD_SHARP+i, g_pc_settings.sharpness==i*25);
    check(CMD_VSYNC, g_pc_settings.vsync);
    check(CMD_FXAA, g_pc_settings.fxaa);
    check(CMD_TEXTURE_FILTER, g_pc_settings.texture_filtering);
    check(CMD_FPS_COUNTER, g_pc_settings.show_fps);
    for (int i=0; i<3; i++) check(CMD_TEXTURE_MODE+i,g_pc_settings.texture_replacements==i);
    for (int i=0; i<COUNT(aniso_values); i++) {
        check(CMD_ANISO+i, g_pc_settings.anisotropy==aniso_values[i]);
        EnableMenuItem(bar, CMD_ANISO+i, MF_BYCOMMAND |
            (aniso_values[i]<=pc_graphics_max_anisotropy() ? MF_ENABLED : MF_GRAYED));
    }
    /* Keep menu usable in windowed mode. Alt+Enter restores it from fullscreen. */
    HMENU wanted = g_pc_settings.fullscreen == 0 ? bar : NULL;
    if (GetMenu(hwnd) != wanted) {
        SetMenu(hwnd, wanted);
        if (wanted) {
            /* Adding a non-client menu must not reduce the chosen GL output. */
            RECT bounds = {0,0,g_pc_settings.window_width,g_pc_settings.window_height};
            AdjustWindowRectEx(&bounds,(DWORD)GetWindowLongPtr(hwnd,GWL_STYLE),TRUE,
                (DWORD)GetWindowLongPtr(hwnd,GWL_EXSTYLE));
            SetWindowPos(hwnd,NULL,0,0,bounds.right-bounds.left,bounds.bottom-bounds.top,
                SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        }
    }
    DrawMenuBar(hwnd);
}

void pc_window_menu_init(void) {
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(g_pc_window, &info)) return;
    hwnd = info.info.win.window;
    bar = CreateMenu();
    HMENU file = submenu(bar, "&File");
    item(file, CMD_SAVE, "Save graphics settings");
    item(file, CMD_SAVE_FOLDER, "Open save folder...");
    AppendMenuA(file, MF_SEPARATOR, 0, NULL);
    item(file, CMD_QUIT, "Exit...");
    HMENU graphics = submenu(bar, "&Graphics");
    item(graphics, CMD_TEXTURE_COMPARE, "Cycle texture comparison\tF6");
    HMENU textures = submenu(graphics, "Texture source");
    item(textures, CMD_TEXTURE_MODE, "Original");
    item(textures, CMD_TEXTURE_MODE+1, "Current 4x pack");
    item(textures, CMD_TEXTURE_MODE+2, "Everything 4x (pack + runtime filter)");
    AppendMenuA(graphics, MF_SEPARATOR, 0, NULL);
    HMENU display = submenu(graphics, "Display mode");
    item(display, CMD_DISPLAY, "Windowed");
    item(display, CMD_DISPLAY+1, "Exclusive fullscreen");
    item(display, CMD_DISPLAY+2, "Borderless");
    HMENU res = submenu(graphics, "Output resolution");
    char label[96];
    for (int i=0; i<COUNT(resolutions); i++) {
        snprintf(label, sizeof(label), "%d x %d", resolutions[i][0], resolutions[i][1]);
        item(res, CMD_RES+i, label);
    }
    HMENU aa = submenu(graphics, "MSAA (live)");
    for (int i=0; i<COUNT(aa_values); i++) {
        if (!aa_values[i]) strcpy(label, "Off"); else snprintf(label, sizeof(label), "%dx MSAA", aa_values[i]);
        item(aa, CMD_MSAA+i, label);
    }
    item(graphics, CMD_FXAA, "FXAA post-process antialiasing");
    HMENU scale = submenu(graphics, "Internal render scale");
    for (int i=0; i<COUNT(scale_values); i++) {
        snprintf(label, sizeof(label), "%d%%%s", scale_values[i], scale_values[i]<100 ? " (upscale)" : scale_values[i]>100 ? " (supersample)" : " (native output)");
        item(scale, CMD_SCALE+i, label);
    }
    HMENU filter = submenu(graphics, "Spatial scaling filter");
    item(filter, CMD_FILTER, "Nearest neighbor");
    item(filter, CMD_FILTER+1, "Bilinear");
    item(filter, CMD_FILTER+2, "Bicubic (Catmull-Rom)");
    HMENU sharp = submenu(graphics, "Sharpening");
    for (int i=0; i<=4; i++) {
        snprintf(label, sizeof(label), "%d%%", i*25); item(sharp, CMD_SHARP+i, label);
    }
    item(graphics, CMD_TEXTURE_FILTER, "Game texture filtering");
    HMENU aniso = submenu(graphics, "Anisotropic texture filtering");
    for(int i=0;i<COUNT(aniso_values);i++) {
        if(i==0) strcpy(label,"Off"); else snprintf(label,sizeof(label),"%dx",aniso_values[i]);
        item(aniso,CMD_ANISO+i,label);
    }
    AppendMenuA(graphics, MF_SEPARATOR, 0, NULL);
    item(graphics, CMD_VSYNC, "VSync");
    HMENU fps = submenu(graphics, "Frame limit");
    for (int i=0; i<COUNT(fps_values); i++) {
        if (fps_values[i]) snprintf(label, sizeof(label), "%d FPS", fps_values[i]); else strcpy(label, "Uncapped");
        item(fps, CMD_FPS+i, label);
    }
    HMENU presets = submenu(graphics, "Graphics presets");
    item(presets, CMD_PRESET, "Native / 4x MSAA");
    item(presets, CMD_PRESET+1, "Performance / 75% bicubic + FXAA");
    item(presets, CMD_PRESET+2, "Supersampled / 150% + 2x MSAA");
    AppendMenuA(graphics, MF_SEPARATOR, 0, NULL);
    AppendMenuA(graphics, MF_STRING | MF_GRAYED, 1990, "DLSS / DLAA (additional renderer required)");
    item(graphics, CMD_SUPPORT, "Graphics capabilities / DLSS roadmap...");
    HMENU view = submenu(bar, "&View");
    item(view, CMD_FPS_COUNTER, "FPS in title bar");
    HMENU help = submenu(bar, "&Help");
    item(help, CMD_SUPPORT, "Graphics support...");
    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
    pc_window_menu_refresh();
    SDL_SetWindowSize(g_pc_window, g_pc_settings.window_width, g_pc_settings.window_height);
}

void pc_window_menu_event(const SDL_Event* event) {
    if (event->type==SDL_SYSWMEVENT && event->syswm.msg &&
        event->syswm.msg->subsystem==SDL_SYSWM_WINDOWS &&
        event->syswm.msg->msg.win.msg==WM_COMMAND &&
        event->syswm.msg->msg.win.lParam==0 && pending_count<COUNT(pending)) {
        pending[pending_count++] = LOWORD(event->syswm.msg->msg.win.wParam);
    }
}

static void apply_command(unsigned cmd) {
    if (cmd==CMD_SUPPORT) {
        char text[2048]; pc_graphics_support_text(text, sizeof(text));
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Graphics support", text, g_pc_window);
        return;
    }
    if (cmd==CMD_SAVE_FOLDER) { ShellExecuteA(hwnd,"open","save",NULL,NULL,SW_SHOWNORMAL); return; }
    if (cmd==CMD_QUIT) {
        const SDL_MessageBoxButtonData buttons[] = {{SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT,0,"Cancel"},{0,1,"Exit game"}};
        SDL_MessageBoxData box = {SDL_MESSAGEBOX_WARNING,g_pc_window,"Exit Animal Crossing?",
            "Save your town using the game's save function before exiting. Unsaved progress will be lost.",2,buttons,NULL};
        int selected=0;
        if (SDL_ShowMessageBox(&box,&selected)==0 && selected==1) g_pc_running=0;
        return;
    }
    if (cmd==CMD_SAVE) { pc_settings_save(); return; }
    /* The in-game editor owns a pending transaction. Do not overwrite it. */
    if (pc_settings_menu_active()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,"Settings open",
            "Apply or discard your in-game settings first, then use the top menu.",g_pc_window);
        return;
    }
    if (cmd==CMD_TEXTURE_COMPARE || (cmd>=CMD_TEXTURE_MODE && cmd<CMD_TEXTURE_MODE+3)) {
        pc_gx_texture_set_replacements(cmd==CMD_TEXTURE_COMPARE ? (g_pc_settings.texture_replacements+1)%3 : cmd-CMD_TEXTURE_MODE);
        pc_settings_save();
        pc_window_menu_refresh();
        return;
    }
    if (cmd>=CMD_DISPLAY && cmd<CMD_DISPLAY+3) g_pc_settings.fullscreen=cmd-CMD_DISPLAY;
    else if (cmd>=CMD_RES && cmd<CMD_RES+COUNT(resolutions)) {
        int i=cmd-CMD_RES; g_pc_settings.window_width=resolutions[i][0]; g_pc_settings.window_height=resolutions[i][1];
    } else if (cmd>=CMD_MSAA && cmd<CMD_MSAA+COUNT(aa_values)) g_pc_settings.msaa=aa_values[cmd-CMD_MSAA];
    else if (cmd>=CMD_SCALE && cmd<CMD_SCALE+COUNT(scale_values)) g_pc_settings.render_scale=scale_values[cmd-CMD_SCALE];
    else if (cmd>=CMD_FILTER && cmd<CMD_FILTER+3) g_pc_settings.upscale_filter=cmd-CMD_FILTER;
    else if (cmd>=CMD_SHARP && cmd<CMD_SHARP+5) g_pc_settings.sharpness=(cmd-CMD_SHARP)*25;
    else if (cmd>=CMD_FPS && cmd<CMD_FPS+COUNT(fps_values)) g_pc_settings.max_fps=fps_values[cmd-CMD_FPS];
    else if (cmd==CMD_VSYNC) g_pc_settings.vsync=!g_pc_settings.vsync;
    else if (cmd==CMD_FXAA) g_pc_settings.fxaa=!g_pc_settings.fxaa;
    else if (cmd==CMD_TEXTURE_FILTER) g_pc_settings.texture_filtering=!g_pc_settings.texture_filtering;
    else if (cmd==CMD_FPS_COUNTER) g_pc_settings.show_fps=!g_pc_settings.show_fps;
    else if (cmd>=CMD_ANISO && cmd<CMD_ANISO+COUNT(aniso_values)) g_pc_settings.anisotropy=aniso_values[cmd-CMD_ANISO];
    else if (cmd>=CMD_PRESET && cmd<CMD_PRESET+3) {
        int i=cmd-CMD_PRESET;
        g_pc_settings.render_scale=i==1?75:i==2?150:100;
        g_pc_settings.msaa=i==0?4:2;
        g_pc_settings.fxaa=i==1;
        g_pc_settings.upscale_filter=i==1?2:1;
        g_pc_settings.sharpness=i==1?25:0;
    } else return;
    pc_settings_apply();
    pc_settings_save();
}

void pc_window_menu_apply_pending(void) {
    for(int i=0;i<pending_count;i++) apply_command(pending[i]);
    pending_count=0;
}
void pc_window_menu_shutdown(void) {
    if (hwnd) SetMenu(hwnd,NULL);
    if (bar) DestroyMenu(bar);
    bar=NULL; hwnd=NULL;
}
#else
void pc_window_menu_init(void) {}
void pc_window_menu_shutdown(void) {}
void pc_window_menu_refresh(void) {}
void pc_window_menu_event(const SDL_Event* event) { (void)event; }
void pc_window_menu_apply_pending(void) {}
#endif
