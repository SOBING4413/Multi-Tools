/* pages.c - page router.
   Owns the shared edit controls and forwards every UI event to the module of the active page
   (see the modules in ui/pages/).  Per-page logic lives in those modules, not here. */
#include "ui/internal.h"
#include "ui/livedata.h"
#include "ui/pages/pages.h"
#include "core/settings.h"

HWND g_ed_feat, g_ed_power, g_ed_proc, g_ed_drv;

static BOOL is_feature_page(void) { return pg_features_fp(g_page) >= 0; }

static Table* active_table(void)
{
    switch (g_page) {
    case PG_PROCS:   return pg_processes_table();
    case PG_MALWARE: return pg_malware_table();
    case PG_DRIVERS: return pg_drivers_table();
    default:         return NULL;
    }
}

/* ------------------------------------------------------------------ lifetime */
void pages_init(HWND hwnd)
{
    DWORD st = WS_CHILD | ES_AUTOHSCROLL | ES_LEFT;
    log_init(hwnd);
    g_ed_feat  = CreateWindowExW(0, L"EDIT", L"", st, 0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)7101, g_hInst, NULL);
    g_ed_power = CreateWindowExW(0, L"EDIT", L"30", st | ES_NUMBER, 0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)7102, g_hInst, NULL);
    g_ed_proc  = CreateWindowExW(0, L"EDIT", L"", st, 0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)7103, g_hInst, NULL);
    g_ed_drv   = CreateWindowExW(0, L"EDIT", L"", st, 0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)7104, g_hInst, NULL);
    SendMessageW(g_ed_proc, EM_SETCUEBANNER, TRUE, (LPARAM)L"Filter...");
    SendMessageW(g_ed_drv, EM_SETCUEBANNER, TRUE, (LPARAM)L"Filter...");
    live_init();
    pg_power_init();
    pg_processes_init();
    pg_malware_init();
    pg_drivers_init();
}

void pages_shutdown(void)
{
    int i;
    for (i = 0; i < JS_COUNT; i++) job_stop(i);
    for (i = 0; i < JS_COUNT; i++) if (g_jobs[i].th) WaitForSingleObject(g_jobs[i].th, 800);
}

BOOL pages_busy(void)
{
    int i;
    for (i = 0; i < JS_COUNT; i++) if (g_jobs[i].running) return TRUE;
    return FALSE;
}

/* ------------------------------------------------------------------ layout / paint */
void pages_layout(void)
{
    log_place(-1, NULL);
    edit_place(g_ed_feat, NULL, FALSE);
    edit_place(g_ed_power, NULL, FALSE);
    edit_place(g_ed_proc, NULL, FALSE);
    edit_place(g_ed_drv, NULL, FALSE);
    if (is_feature_page()) pg_features_layout();
    else if (g_page == PG_PROCS) pg_processes_layout();
    else if (g_page == PG_MALWARE) pg_malware_layout();
    else if (g_page == PG_DRIVERS) pg_drivers_layout();
    /* the Power page positions its edit box while painting */
    g_dirty = TRUE;
}

void pages_paint(void)
{
    switch (g_page) {
    case PG_DASH:     pg_dashboard_paint(); break;
    case PG_MONITOR:  pg_monitor_paint();   break;
    case PG_POWER:    pg_power_paint();     break;
    case PG_SETTINGS: pg_settings_paint();  break;
    case PG_ABOUT:    pg_about_paint();     break;
    case PG_PROCS:    pg_processes_paint(); break;
    case PG_MALWARE:  pg_malware_paint();   break;
    case PG_DRIVERS:  pg_drivers_paint();   break;
    default:          pg_features_paint();  break;     /* System, Network, Cleanup, Security, Tools */
    }
    ui_toast_paint();
}

void pages_page_changed(int newpage)
{
    switch (newpage) {
    case PG_PROCS:   pg_processes_enter(); break;
    case PG_MALWARE: pg_malware_enter();   break;
    case PG_DRIVERS: pg_drivers_enter();   break;
    default: if (pg_features_fp(newpage) >= 0) pg_features_enter(); break;
    }
    pages_layout();
}

void pages_lang_changed(void) { pages_layout(); }

/* ------------------------------------------------------------------ input */
void pages_click(int id)
{
    switch (g_page) {
    case PG_DASH:     pg_dashboard_click(id);  break;
    case PG_MONITOR:  pg_monitor_click(id);    break;
    case PG_POWER:    pg_power_click(id);      break;
    case PG_SETTINGS: pg_settings_click(id);   break;
    case PG_ABOUT:    pg_about_click(id);      break;
    case PG_PROCS:    pg_processes_click(id);  break;
    case PG_MALWARE:  pg_malware_click(id);    break;
    case PG_DRIVERS:  pg_drivers_click(id);    break;
    default:          pg_features_click(id);   break;
    }
}

BOOL pages_wheel(int x, int y, int delta)
{
    Table* t;
    POINT p;
    if (is_feature_page()) return pg_features_wheel(delta);
    t = active_table();
    p.x = x; p.y = y;
    if (!t || !PtInRect(&t->rc, p)) return FALSE;
    tbl_wheel(t, delta);
    return TRUE;
}

BOOL pages_mouse(UINT msg, int x, int y)
{
    Table* t = active_table();
    return t ? tbl_mouse(t, msg, x, y) : FALSE;
}

BOOL pages_key(WPARAM vk)
{
    Table* t = active_table();
    return t ? tbl_key(t, vk) : FALSE;
}

void pages_command(WPARAM wp, LPARAM lp)
{
    HWND ed = (HWND)lp;
    if (HIWORD(wp) == EN_CHANGE) {
        pg_features_command(ed);
        pg_processes_command(ed);
        pg_drivers_command(ed);
    }
    if (HIWORD(wp) == EN_SETFOCUS || HIWORD(wp) == EN_KILLFOCUS) g_dirty = TRUE;
}

HBRUSH pages_ctlcolor(HDC dc, HWND ctl)
{
    static HBRUSH br = NULL;
    static Rgb last = 0xFFFFFFFF;
    if (!br || last != g_th.logbg) {
        if (br) DeleteObject(br);
        br = CreateSolidBrush(CR(g_th.logbg));
        last = g_th.logbg;
    }
    (void)ctl;
    SetTextColor(dc, CR(g_th.text));
    SetBkColor(dc, CR(g_th.logbg));
    return br;
}

/* ------------------------------------------------------------------ timers */
void pages_tick(void)
{
    live_tick();
    pg_power_tick();
    pg_processes_tick();
    if (g_page == PG_DASH || g_page == PG_MONITOR || g_page == PG_POWER || g_page == PG_PROCS) g_dirty = TRUE;
}

BOOL pages_anim(void)
{
    BOOL need = FALSE;
    if (g_page == PG_DASH) need = pg_dashboard_anim();
    if (ui_toast_active()) need = TRUE;
    if (g_page == PG_POWER && pg_power_anim()) need = TRUE;
    if (is_feature_page() && pg_features_anim()) need = TRUE;
    if (g_page == PG_PROCS && pg_processes_anim()) need = TRUE;
    if (g_page == PG_MALWARE && pg_malware_anim()) need = TRUE;
    if (g_page == PG_DRIVERS && pg_drivers_anim()) need = TRUE;
    return need;
}

/* ------------------------------------------------------------------ messages posted by worker threads */
void pages_app_msg(UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_APP_LOG: {
        LogMsg* m = (LogMsg*)lp;
        if (m) {
            if (m->clear) log_clear((int)wp); else if (m->text) log_append((int)wp, m->style, m->text);
            free(m->text);
            free(m);
        }
        return; }
    case WM_APP_PROGRESS: {
        int slot = (int)wp;
        if (slot >= 0 && slot < JS_COUNT) g_jobs[slot].progress10 = (int)lp;
        g_dirty = TRUE;
        return; }
    case WM_APP_STATUS: {
        wchar_t* s = (wchar_t*)lp;
        if (wp == 101) pg_malware_refresh_defender();
        else if (wp == 100) { if (s) open_in_explorer_select(s); }
        else if (s) ui_toast((int)wp, L"%ls", s);
        free(s);
        return; }
    case WM_APP_DONE: {
        int slot = (int)wp;
        if (slot >= 0 && slot < JS_COUNT) job_finish(slot);
        g_dirty = TRUE;
        pg_features_app_done(slot);
        break; }
    default: break;
    }
    /* payloads for the data pages (scan events, tables, job completion) */
    pg_processes_app_msg(msg, wp, lp);
    pg_malware_app_msg(msg, wp, lp);
    pg_drivers_app_msg(msg, wp, lp);
}

void pages_dev_run(int fp, int idx, const wchar_t* outfile, const wchar_t* text, int opt)
{
    pg_features_dev_run(fp, idx, outfile, text, opt);
}
