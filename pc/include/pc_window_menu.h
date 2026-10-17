#ifndef PC_WINDOW_MENU_H
#define PC_WINDOW_MENU_H
#include <SDL.h>
void pc_window_menu_init(void);
void pc_window_menu_shutdown(void);
void pc_window_menu_refresh(void);
void pc_window_menu_event(const SDL_Event* event);
void pc_window_menu_apply_pending(void);
#endif
