/* pages/pages.h - interface implemented by every page module (called from ui/pages.c) */
#ifndef MT_PAGES_H
#define MT_PAGES_H
#include "ui/ui.h"

/* Dashboard ---------------------------------------------------------- */
void pg_dashboard_paint(void);
BOOL pg_dashboard_click(int id);
BOOL pg_dashboard_anim(void);

/* Live monitor ------------------------------------------------------- */
void pg_monitor_paint(void);
BOOL pg_monitor_click(int id);

/* Power -------------------------------------------------------------- */
void pg_power_init(void);
void pg_power_paint(void);
BOOL pg_power_click(int id);
void pg_power_tick(void);
BOOL pg_power_anim(void);

/* Generic category pages (System / Network / Cleanup / Security / Tools) */
int  pg_features_fp(int page);                 /* feature-page index of a page id, or -1 */
void pg_features_enter(void);
void pg_features_layout(void);
void pg_features_paint(void);
BOOL pg_features_click(int id);
BOOL pg_features_wheel(int delta);
BOOL pg_features_anim(void);
void pg_features_command(HWND ed);
void pg_features_select(int fp, int idx);
void pg_features_app_done(int slot);
void pg_features_dev_run(int fp, int idx, const wchar_t* outfile, const wchar_t* text, int opt);

/* Settings / About --------------------------------------------------- */
void pg_settings_paint(void);
BOOL pg_settings_click(int id);
void pg_about_paint(void);
BOOL pg_about_click(int id);

/* Processes / Startup / Installed apps ------------------------------- */
void   pg_processes_init(void);
void   pg_processes_free(void);
void   pg_processes_enter(void);
void   pg_processes_layout(void);
void   pg_processes_paint(void);
BOOL   pg_processes_click(int id);
void   pg_processes_tick(void);
BOOL   pg_processes_anim(void);
Table* pg_processes_table(void);
void   pg_processes_command(HWND ed);
void   pg_processes_app_msg(UINT msg, WPARAM wp, LPARAM lp);

/* Malware scan ------------------------------------------------------- */
void   pg_malware_init(void);
void   pg_malware_free(void);
void   pg_malware_enter(void);
void   pg_malware_layout(void);
void   pg_malware_paint(void);
BOOL   pg_malware_click(int id);
BOOL   pg_malware_anim(void);
Table* pg_malware_table(void);
void   pg_malware_app_msg(UINT msg, WPARAM wp, LPARAM lp);
void   pg_malware_start_quickscan(void);
void   pg_malware_refresh_defender(void);

/* Driver scanner ----------------------------------------------------- */
void   pg_drivers_init(void);
void   pg_drivers_free(void);
void   pg_drivers_enter(void);
void   pg_drivers_layout(void);
void   pg_drivers_paint(void);
BOOL   pg_drivers_click(int id);
BOOL   pg_drivers_anim(void);
Table* pg_drivers_table(void);
void   pg_drivers_command(HWND ed);
void   pg_drivers_app_msg(UINT msg, WPARAM wp, LPARAM lp);
void   pg_drivers_start_scan(void);

#endif
