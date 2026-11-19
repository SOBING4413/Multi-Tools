/* processes.c - Processes page: running programs, startup manager, installed applications */
#include "ui/internal.h"
#include "ui/pages/pages.h"
#include "modules/sys.h"
#include "modules/procs.h"
#include "modules/sysmgr.h"
#include <shlobj.h>
#include <shellapi.h>
#include <process.h>

static BOOL is_critical_proc(DWORD pid, const wchar_t* name)
{
    static const wchar_t* crit[] = { L"system", L"smss.exe", L"csrss.exe", L"wininit.exe", L"winlogon.exe", L"services.exe", L"lsass.exe", L"lsm.exe", L"registry", L"memory compression", L"secure system" };
    int i;
    if (pid <= 4 || pid == GetCurrentProcessId()) return TRUE;
    for (i = 0; i < MT_ARRAY(crit); i++) if (_wcsicmp(name, crit[i]) == 0) return TRUE;
    return FALSE;
}

static Table t_proc, t_start, t_apps;
static int pr_tab = 0;
static BOOL pr_auto = TRUE;
static StartupEntry* g_se = NULL; static int g_nse = 0;
static AppEntry* g_apps = NULL; static int g_napps = 0;
static int pr_tick = 0;
static wchar_t pr_status[160];
static BOOL apps_loaded = FALSE;

typedef struct AppsResult { AppEntry* a; int n; } AppsResult;

static Table* pr_table(void) { return pr_tab == 0 ? &t_proc : (pr_tab == 1 ? &t_start : &t_apps); }

static void proc_refresh(void)
{
    ProcInfo* a = NULL;
    int n = procs_snapshot(&a), i;
    DWORD selpid = 0;
    int scroll = t_proc.scroll;
    ULONGLONG total = 0;
    TblRow* sr = tbl_selected(&t_proc);
    wchar_t sz[48];
    if (sr) selpid = (DWORD)(uintptr_t)sr->user;
    tbl_clear(&t_proc);
    for (i = 0; i < n; i++) {
        TblRow* r;
        wchar_t b[64];
        if (a[i].pid == 0) continue;
        r = tbl_add(&t_proc, 0, (void*)(uintptr_t)a[i].pid);
        if (!r) break;
        wfmt(b, 64, L"%lu", (unsigned long)a[i].pid);            tbl_set(r, 0, b, (double)a[i].pid);
        tbl_set(r, 1, a[i].name, 0);
        wfmt(b, 64, L"%.1f", a[i].cpu);                          tbl_set(r, 2, b, a[i].cpu);
        fmt_size(a[i].ws, b, 64);                                tbl_set(r, 3, b, (double)a[i].ws);
        wfmt(b, 64, L"%lu", (unsigned long)a[i].threads);        tbl_set(r, 4, b, (double)a[i].threads);
        tbl_set(r, 5, a[i].path, 0);
        total += a[i].ws;
    }
    tbl_rebuild(&t_proc);
    t_proc.scroll = scroll;
    if (selpid) for (i = 0; i < t_proc.n; i++) if ((DWORD)(uintptr_t)t_proc.rows[i].user == selpid) { t_proc.sel = i; break; }
    fmt_size(total, sz, 48);
    wfmt(pr_status, 160, T2(L"%d processes  %ls  %ls total working set", L"%d proses  %ls  %ls total working set"), t_proc.n, U_DOT, sz);
    free(a);
}

static void startup_refresh(void)
{
    int i;
    free(g_se);
    g_se = NULL;
    g_nse = startup_list(&g_se);
    tbl_clear(&t_start);
    for (i = 0; i < g_nse; i++) {
        TblRow* r = tbl_add(&t_start, 0, (void*)(intptr_t)i);
        if (!r) break;
        tbl_set(r, 0, g_se[i].name, 0);
        tbl_set(r, 1, g_se[i].cmd, 0);
        tbl_set(r, 2, g_se[i].where, 0);
    }
    tbl_rebuild(&t_start);
    wfmt(pr_status, 160, T2(L"%d startup entries", L"%d entri startup"), g_nse);
}

static void apps_fill(void)
{
    int i;
    tbl_clear(&t_apps);
    for (i = 0; i < g_napps; i++) {
        TblRow* r = tbl_add(&t_apps, 0, (void*)(intptr_t)i);
        wchar_t sz[48];
        if (!r) break;
        tbl_set(r, 0, g_apps[i].name, 0);
        tbl_set(r, 1, g_apps[i].ver, 0);
        tbl_set(r, 2, g_apps[i].pub, 0);
        tbl_set(r, 3, g_apps[i].date, 0);
        if (g_apps[i].size_kb) fmt_size(g_apps[i].size_kb * 1024ULL, sz, 48); else wcopy(sz, 48, L"-");
        tbl_set(r, 4, sz, (double)g_apps[i].size_kb);
    }
    tbl_rebuild(&t_apps);
    wfmt(pr_status, 160, T2(L"%d installed applications", L"%d aplikasi terpasang"), g_napps);
}

static unsigned __stdcall apps_thread(void* p)
{
    AppsResult* ar = (AppsResult*)calloc(1, sizeof(AppsResult));
    (void)p;
    if (ar) ar->n = apps_list(&ar->a);
    PostMessageW(g_hwnd, WM_APP_TABLE, TBL_APPS, (LPARAM)ar);
    PostMessageW(g_hwnd, WM_APP_DONE, JS_APPS, 0);
    return 0;
}

static void apps_reload(void)
{
    if (job_run(JS_APPS, apps_thread, NULL)) {
        wcopy(pr_status, 160, T2(L"Reading installed applications...", L"Membaca aplikasi terpasang..."));
        g_dirty = TRUE;
    }
}

static void pr_select_tab(int t)
{
    pr_tab = t;
    if (g_ed_proc) SetWindowTextW(g_ed_proc, L"");
    t_proc.filter[0] = t_start.filter[0] = t_apps.filter[0] = 0;
    if (t == 0) proc_refresh();
    else if (t == 1) startup_refresh();
    else { if (!apps_loaded) apps_reload(); else apps_fill(); }
    pages_layout();
}

static void pr_kill_selected(void)
{
    TblRow* r = tbl_selected(&t_proc);
    wchar_t err[160], q[256];
    DWORD pid;
    if (!r) { ui_toast(2, T2(L"Select a process first.", L"Pilih proses terlebih dahulu.")); return; }
    pid = (DWORD)(uintptr_t)r->user;
    if (is_critical_proc(pid, r->c[1])) { ui_toast(4, T2(L"%ls is a protected system process.", L"%ls adalah proses sistem yang dilindungi."), r->c[1]); return; }
    wfmt(q, 256, T2(L"End process %ls (PID %lu)?\nUnsaved data in it will be lost.", L"Akhiri proses %ls (PID %lu)?\nData yang belum disimpan akan hilang."), r->c[1], (unsigned long)pid);
    if (!confirm_box(q)) return;
    if (proc_kill(pid, err, 160)) ui_toast(0, T2(L"Process %lu ended.", L"Proses %lu diakhiri."), (unsigned long)pid);
    else ui_toast(4, L"%ls", err);
    proc_refresh();
}

static void pr_kill_matches(void)
{
    wchar_t q[256];
    int i, killed = 0, protected_n = 0, cnt = 0;
    if (!t_proc.filter[0]) { ui_toast(2, T2(L"Type part of a process name in the filter box first.", L"Ketik sebagian nama proses di kotak filter terlebih dahulu.")); return; }
    for (i = 0; i < t_proc.nview; i++) cnt++;
    if (cnt == 0) { ui_toast(2, T2(L"No process matches the filter.", L"Tidak ada proses yang cocok dengan filter.")); return; }
    wfmt(q, 256, T2(L"End ALL %d process(es) matching \"%ls\"?", L"Akhiri SEMUA %d proses yang cocok dengan \"%ls\"?"), cnt, t_proc.filter);
    if (!confirm_box(q)) return;
    for (i = 0; i < t_proc.nview; i++) {
        TblRow* r = &t_proc.rows[t_proc.view[i]];
        DWORD pid = (DWORD)(uintptr_t)r->user;
        wchar_t err[64];
        if (is_critical_proc(pid, r->c[1])) { protected_n++; continue; }
        if (proc_kill(pid, err, 64)) killed++;
    }
    ui_toast(killed ? 0 : 2, T2(L"%d process(es) ended, %d protected/skipped.", L"%d proses diakhiri, %d dilindungi/dilewati."), killed, cnt - killed);
    (void)protected_n;
    proc_refresh();
}

void pg_processes_layout(void)
{
    RECT c = g_cont, fr;
    int x = c.right - S(300);
    (void)x;
    fr = rc_make(c.left + S(0), c.top + S(48), c.left + S(300), c.top + S(48) + S(36));
    edit_place(g_ed_proc, &fr, TRUE);
}

void pg_processes_paint(void)
{
    RECT c = g_cont, r, tr, fr;
    int x = c.left, i, y = c.top;
    const wchar_t* names[3];
    int ids[3] = { HID_PR_TAB0, HID_PR_TAB0 + 1, HID_PR_TAB0 + 2 };
    { static wchar_t n0[96], n1[96]; wchar_t* cut;
      wcopy(n0, 96, TX(K_menu_running)); cut = wcsstr(n0, L" ("); if (cut) *cut = 0;
      wcopy(n1, 96, TX(K_menu_startup)); cut = wcsstr(n1, L" ("); if (cut) *cut = 0;
      names[0] = n0; names[1] = n1; }
    names[2] = T2(L"Installed Apps", L"Aplikasi Terpasang");
    for (i = 0; i < 3; i++) {
        int w = w_text_width(names[i], F_UI_B) + S(30);
        r = rc_make(x, y, x + w, y + S(32));
        w_pill(&r, names[i], ids[i], pr_tab == i, F_UI_B);
        x += w + S(8);
    }
    y += S(48);
    fr = rc_make(c.left, y, c.left + S(300), y + S(36));
    w_input_frame(&fr, GetFocus() == g_ed_proc);
    x = fr.right + S(10);
    {
        const wchar_t* ph = T2(L"Filter by name...", L"Filter berdasarkan nama...");
        SendMessageW(g_ed_proc, EM_SETCUEBANNER, TRUE, (LPARAM)ph);
    }
    r = rc_make(x, y, x + S(112), y + S(36));
    w_btn(&r, T2(L"Refresh", L"Refresh"), ICO_REFRESH, HID_PR_REFRESH, BTN_SOFT, 0);
    x += S(120);
    if (pr_tab == 0) {
        r = rc_make(x, y, x + S(132), y + S(36)); w_btn(&r, T2(L"End process", L"Akhiri proses"), ICO_CLEAR, HID_PR_KILL, BTN_DANGER, 0); x += S(140);
        r = rc_make(x, y, x + S(150), y + S(36)); w_btn(&r, T2(L"End all matches", L"Akhiri semua cocok"), ICO_CLEAR, HID_PR_KILLALL, BTN_DANGER, 0); x += S(158);
        r = rc_make(x, y, x + S(132), y + S(36)); w_btn(&r, T2(L"Open location", L"Buka lokasi"), ICO_FOLDER, HID_PR_OPEN, BTN_SOFT, 0); x += S(140);
        tr = rc_make(x + S(4), y, x + S(110), y + S(36));
        gfx_text_rc(&tr, T2(L"Auto-refresh", L"Auto-refresh"), F_UI_S, g_th.dim, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
        r = rc_make(x + S(118), y + S(5), x + S(118) + S(46), y + S(31));
        w_toggle(&r, pr_auto, HID_PR_AUTO);
    } else if (pr_tab == 1) {
        r = rc_make(x, y, x + S(160), y + S(36)); w_btn(&r, T2(L"Remove entry", L"Hapus entri"), ICO_CLEAR, HID_PR_KILL, BTN_DANGER, 0); x += S(168);
        r = rc_make(x, y, x + S(170), y + S(36)); w_btn(&r, T2(L"Open Startup folder", L"Buka folder Startup"), ICO_FOLDER, HID_PR_OPEN, BTN_SOFT, 0);
    } else {
        r = rc_make(x, y, x + S(130), y + S(36)); w_btn(&r, T2(L"Uninstall", L"Uninstall"), ICO_CLEAR, HID_PR_UNINST, BTN_DANGER, 0);
    }
    tr = rc_make(c.left, y + S(44), c.right, y + S(66));
    gfx_text_rc(&tr, pr_status, F_UI_S, g_th.dim, DT_SINGLELINE | DT_VCENTER);
    if (job_busy(JS_APPS)) { RECT pb = rc_make(c.right - S(180), y + S(54), c.right, y + S(60)); w_progress(&pb, -1.0, g_th.accent); }
    r = rc_make(c.left, y + S(70), c.right, c.bottom);
    tbl_draw(pr_table(), &r);
}

void pg_processes_init(void)
{
    tbl_init(&t_proc);
    t_proc.ncols = 6;
    t_proc.col[0].en = L"PID";      t_proc.col[0].id = L"PID";      t_proc.col[0].w = 7;  t_proc.col[0].right = 1; t_proc.col[0].kind = 1;
    t_proc.col[1].en = L"Name";     t_proc.col[1].id = L"Nama";     t_proc.col[1].w = 20;
    t_proc.col[2].en = L"CPU %";    t_proc.col[2].id = L"CPU %";    t_proc.col[2].w = 8;  t_proc.col[2].right = 1; t_proc.col[2].kind = 1;
    t_proc.col[3].en = L"Memory";   t_proc.col[3].id = L"Memori";   t_proc.col[3].w = 11; t_proc.col[3].right = 1; t_proc.col[3].kind = 1;
    t_proc.col[4].en = L"Threads";  t_proc.col[4].id = L"Thread";   t_proc.col[4].w = 8;  t_proc.col[4].right = 1; t_proc.col[4].kind = 1;
    t_proc.col[5].en = L"Path";     t_proc.col[5].id = L"Lokasi";   t_proc.col[5].w = 46;
    t_proc.sortcol = 3; t_proc.sortasc = 0;
    t_proc.empty_en = L"No processes"; t_proc.empty_id = L"Tidak ada proses";

    tbl_init(&t_start);
    t_start.ncols = 3;
    t_start.col[0].en = L"Name"; t_start.col[0].id = L"Nama"; t_start.col[0].w = 22;
    t_start.col[1].en = L"Command"; t_start.col[1].id = L"Perintah"; t_start.col[1].w = 56;
    t_start.col[2].en = L"Location"; t_start.col[2].id = L"Lokasi"; t_start.col[2].w = 22;
    t_start.empty_en = L"No startup entries found"; t_start.empty_id = L"Tidak ada entri startup";

    tbl_init(&t_apps);
    t_apps.ncols = 5;
    t_apps.col[0].en = L"Name"; t_apps.col[0].id = L"Nama"; t_apps.col[0].w = 38;
    t_apps.col[1].en = L"Version"; t_apps.col[1].id = L"Versi"; t_apps.col[1].w = 14;
    t_apps.col[2].en = L"Publisher"; t_apps.col[2].id = L"Penerbit"; t_apps.col[2].w = 26;
    t_apps.col[3].en = L"Installed"; t_apps.col[3].id = L"Terpasang"; t_apps.col[3].w = 12;
    t_apps.col[4].en = L"Size"; t_apps.col[4].id = L"Ukuran"; t_apps.col[4].w = 10; t_apps.col[4].right = 1; t_apps.col[4].kind = 1;
    t_apps.sortcol = 0; t_apps.sortasc = 1;
    t_apps.empty_en = L"Loading..."; t_apps.empty_id = L"Memuat...";
    wcopy(pr_status, 160, L"");
}

void pg_processes_free(void)
{
    tbl_free(&t_proc); tbl_free(&t_start); tbl_free(&t_apps);
    free(g_se); free(g_apps);
}

void pg_processes_enter(void)
{
    t_proc.filter[0] = 0;
    if (g_ed_proc) SetWindowTextW(g_ed_proc, L"");
    pr_select_tab(pr_tab);
}

void pg_processes_tick(void)
{
    pr_tick++;
    if (g_page == PG_PROCS && pr_tab == 0 && pr_auto && (pr_tick % 2 == 0)) proc_refresh();
}

BOOL pg_processes_anim(void) { return job_busy(JS_APPS); }

Table* pg_processes_table(void) { return pr_table(); }

void pg_processes_command(HWND ed)
{
    if (ed == g_ed_proc) {
        Table* t = pr_table();
        GetWindowTextW(ed, t->filter, 128);
        tbl_rebuild(t);
        t->scroll = 0;
        g_dirty = TRUE;
    }
}

void pg_processes_app_msg(UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_APP_TABLE && wp == TBL_APPS) {
        AppsResult* ar = (AppsResult*)lp;
        if (ar) {
            free(g_apps);
            g_apps = ar->a; g_napps = ar->n;
            free(ar);
            apps_loaded = TRUE;
            if (pr_tab == 2) apps_fill();
        }
        g_dirty = TRUE;
    } else if (msg == WM_APP_DONE && (int)wp == JS_APPS) {
        if (pr_tab == 2) apps_fill();
        g_dirty = TRUE;
    }
}

BOOL pg_processes_click(int id)
{
        if (id >= HID_PR_TAB0 && id < HID_PR_TAB0 + 3) { pr_select_tab(id - HID_PR_TAB0); return TRUE; }
        switch (id) {
        case HID_PR_REFRESH: if (pr_tab == 0) proc_refresh(); else if (pr_tab == 1) startup_refresh(); else apps_reload(); break;
        case HID_PR_AUTO: pr_auto = !pr_auto; break;
        case HID_PR_KILL:
            if (pr_tab == 0) pr_kill_selected();
            else if (pr_tab == 1) {
                TblRow* r = tbl_selected(&t_start);
                wchar_t q[600], err[160];
                int idx;
                if (!r) { ui_toast(2, T2(L"Select an entry first.", L"Pilih entri terlebih dahulu.")); break; }
                idx = (int)(intptr_t)r->user;
                if (idx < 0 || idx >= g_nse) break;
                wfmt(q, 600, T2(L"Remove \"%ls\" from startup?\nThe program itself is not uninstalled.", L"Hapus \"%ls\" dari startup?\nProgramnya sendiri tidak di-uninstall."), g_se[idx].name);
                if (!confirm_box(q)) break;
                if (startup_remove(&g_se[idx], err, 160)) { ui_toast(0, T2(L"Startup entry removed.", L"Entri startup dihapus.")); startup_refresh(); }
                else ui_toast(4, L"%ls", err);
            }
            break;
        case HID_PR_KILLALL: pr_kill_matches(); break;
        case HID_PR_OPEN:
            if (pr_tab == 0) {
                TblRow* r = tbl_selected(&t_proc);
                if (r && r->c[5] && r->c[5][0]) open_in_explorer_select(r->c[5]); else ui_toast(2, T2(L"No file location for this process.", L"Proses ini tidak punya lokasi file."));
            } else if (pr_tab == 1) {
                wchar_t d[MAX_PATH];
                if (get_folder(CSIDL_STARTUP, d)) shell_open(d, NULL);
            }
            break;
        case HID_PR_UNINST: {
            TblRow* r = tbl_selected(&t_apps);
            int idx;
            wchar_t q[600];
            if (!r) { ui_toast(2, T2(L"Select an application first.", L"Pilih aplikasi terlebih dahulu.")); break; }
            idx = (int)(intptr_t)r->user;
            if (idx < 0 || idx >= g_napps || !g_apps[idx].uninstall[0]) { ui_toast(2, T2(L"This app has no uninstaller.", L"Aplikasi ini tidak punya uninstaller.")); break; }
            wfmt(q, 600, T2(L"Run the uninstaller for \"%ls\"?", L"Jalankan uninstaller untuk \"%ls\"?"), g_apps[idx].name);
            if (!confirm_box(q)) break;
            { wchar_t args[1100]; wfmt(args, 1100, L"/c \"%ls\"", g_apps[idx].uninstall); ShellExecuteW(NULL, L"open", L"cmd.exe", args, NULL, SW_SHOWNORMAL); }
            break; }
        default: break;
        }
        g_dirty = TRUE;
        return TRUE;
}
