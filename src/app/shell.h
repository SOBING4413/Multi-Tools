/* shell.h - application shell (sidebar, top bar, navigation) */
#ifndef MT_SHELL_H
#define MT_SHELL_H
#include "core/common.h"

int  shell_sidebar_w(void);
int  shell_topbar_h(void);
void shell_compute_layout(void);          /* recompute g_cont for the current window size */
void shell_draw(void);                    /* paint sidebar + top bar into the back buffer */
void shell_click(int id);                 /* handle clicks on shell hot spots, otherwise forward to the active page */
BOOL shell_nav_wheel(int x, int delta);   /* mouse wheel over the sidebar; TRUE if consumed */
BOOL shell_confirm(const wchar_t* text);  /* Yes/No message box */

/* implemented in main.c */
void app_apply_dark_titlebar(HWND h);
int  app_window_dpi(HWND h);

#endif
