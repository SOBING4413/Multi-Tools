/* drivers.c - Driver Scanner page: inventory, health status, Windows Update lookup, backup/export */
#include "ui/internal.h"
#include "ui/pages/pages.h"
#include "modules/driver_scan.h"
#include <shlobj.h>
#include <shellapi.h>
#include <process.h>

static void url_encode(const wchar_t* in, wchar_t* out, size_t cch)
{
    char* u = w_to_utf8(in);
    size_t i, k = 0;
    if (!u) { out[0] = 0; return; }
    for (i = 0; u[i] && k + 4 < cch; i++) {
        unsigned char c = (unsigned char)u[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.') out[k++] = (wchar_t)c;
        else if (c == ' ') out[k++] = L'+';
        else { wfmt(out + k, 4, L"%%%02X", c); k += 3; }
    }
    out[k] = 0;
    free(u);
}

static Table t_drv;
static DriverScan* g_ds = NULL;
static int dr_filter = 0;
static int dr_bottom = 0;
static wchar_t dr_status[200];
static int dr_first = 1;

static int dr_pred(const TblRow* r, void* ctx)
{
    int st;
    (void)ctx;
    if (dr_filter == 0 || !g_ds) return 1;
    st = g_ds->rows[(int)(intptr_t)r->user].state;
    switch (dr_filter) {
    case 1: return st == DS_OK;
    case 2: return st == DS_OLD || st == DS_OUTDATED || st == DS_GENERIC || st == DS_UPDATE;
    case 3: return st == DS_PROBLEM || st == DS_MISSING || st == DS_UNSIGNED;
    case 4: return g_ds->rows[(int)(intptr_t)r->user].wu_title[0] != 0;
    default: return 1;
    }
}

static void dr_fill(void)
{
    int i, selidx = -1;
    TblRow* sr = tbl_selected(&t_drv);
    if (sr) selidx = (int)(intptr_t)sr->user;
    tbl_clear(&t_drv);
    if (!g_ds) return;
    for (i = 0; i < g_ds->n; i++) {
        const DriverRow* d = &g_ds->rows[i];
        int sev = driver_state_severity(d->state);
        TblRow* r = tbl_add(&t_drv, sev == 3 ? 4 : sev, (void*)(intptr_t)i);
        wchar_t age[24], prov[200];
        if (!r) break;
        tbl_set(r, 0, driver_state_text(d->state), (double)(sev * 20 + d->state));
        tbl_set(r, 1, d->name, 0);
        tbl_set(r, 2, d->cls, 0);
        wcopy(prov, 200, d->provider[0] ? d->provider : d->mfg);
        tbl_set(r, 3, prov, 0);
        tbl_set(r, 4, d->version[0] ? d->version : L"-", 0);
        tbl_set(r, 5, d->date[0] ? d->date : L"-", (double)d->age_days);
        if (d->age_days >= 0) wfmt(age, 24, d->age_days >= 365 ? T2(L"%.1f y", L"%.1f th") : T2(L"%d d", L"%d hr"), d->age_days >= 365 ? d->age_days / 365.0 : (double)d->age_days);
        else wcopy(age, 24, L"-");
        tbl_set(r, 6, age, (double)d->age_days);
    }
    t_drv.pred = dr_pred;
    tbl_rebuild(&t_drv);
    if (selidx >= 0) for (i = 0; i < t_drv.n; i++) if ((int)(intptr_t)t_drv.rows[i].user == selidx) { t_drv.sel = i; break; }
}

static unsigned __stdcall drvscan_thread(void* p)
{
    DriverScan* ds = driver_scan_local(&g_jobs[JS_DRIVER].cancel);
    (void)p;
    PostMessageW(g_hwnd, WM_APP_TABLE, TBL_DRIVERS, (LPARAM)ds);
    PostMessageW(g_hwnd, WM_APP_DONE, JS_DRIVER, 0);
    return 0;
}

static unsigned __stdcall drvwu_thread(void* p)
{
    DriverScan* ds = (DriverScan*)p;      /* private copy owned by this thread until posted */
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    driver_scan_wu(ds, &g_jobs[JS_DRIVER].cancel);
    CoUninitialize();
    PostMessageW(g_hwnd, WM_APP_TABLE, TBL_DRIVERS, (LPARAM)ds);
    PostMessageW(g_hwnd, WM_APP_DONE, JS_DRIVER, 0);
    return 0;
}

static unsigned __stdcall drvtool_thread(void* p)
{
    int kind = (int)(intptr_t)p;
    MtOut o;
    out_init(&o, LOG_DRIVER, JS_DRIVER, &g_jobs[JS_DRIVER].cancel);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (kind == 0) driver_backup_job(&o); else driver_rescan_job(&o);
    CoUninitialize();
    PostMessageW(g_hwnd, WM_APP_DONE, JS_DRIVER, 0);
    return 0;
}

void pg_drivers_start_scan(void)
{
    if (job_busy(JS_DRIVER)) return;
    wcopy(dr_status, 200, T2(L"Scanning installed drivers...", L"Memindai driver terpasang..."));
    job_run(JS_DRIVER, drvscan_thread, NULL);
    g_dirty = TRUE;
}

static void dr_wu_check(void)
{
    DriverScan* copy;
    if (job_busy(JS_DRIVER)) return;
    if (!g_ds) { ui_toast(2, T2(L"Run a driver scan first.", L"Jalankan scan driver terlebih dahulu.")); return; }
    copy = (DriverScan*)malloc(sizeof(DriverScan));
    if (!copy) return;
    *copy = *g_ds;
    copy->rows = (DriverRow*)malloc(sizeof(DriverRow) * (size_t)(g_ds->n > 0 ? g_ds->n : 1));
    if (!copy->rows) { free(copy); return; }
    memcpy(copy->rows, g_ds->rows, sizeof(DriverRow) * (size_t)g_ds->n);
    wcopy(dr_status, 200, T2(L"Asking Windows Update for driver updates (this can take a minute)...", L"Menanyakan update driver ke Windows Update (bisa memakan waktu semenit)..."));
    if (!job_run(JS_DRIVER, drvwu_thread, copy)) { driver_scan_free(copy); }
    g_dirty = TRUE;
}

static void dr_tool(int kind)
{
    if (job_busy(JS_DRIVER)) return;
    dr_bottom = 1;
    log_clear(LOG_DRIVER);
    job_run(JS_DRIVER, drvtool_thread, (void*)(intptr_t)kind);
    pages_layout();
}

static const DriverRow* dr_selected(void)
{
    TblRow* r = tbl_selected(&t_drv);
    int idx;
    if (!r || !g_ds) return NULL;
    idx = (int)(intptr_t)r->user;
    return (idx >= 0 && idx < g_ds->n) ? &g_ds->rows[idx] : NULL;
}

void pg_drivers_layout(void)
{
    RECT c = g_cont;
    int bh = S(138);
    RECT fr = rc_make(c.right - S(260), c.top + S(104), c.right, c.top + S(104) + S(34));
    RECT lr = rc_make(c.left + 2, c.bottom - bh + S(44), c.right - 2, c.bottom - 2);
    (void)fr;
    edit_place(g_ed_drv, &fr, TRUE);
    if (dr_bottom == 1) log_place(LOG_DRIVER, &lr); else log_place(-1, NULL);
}

static void kv_line(int x, int y, int w, const wchar_t* k, const wchar_t* v)
{
    RECT r = rc_make(x, y, x + S(92), y + S(20));
    gfx_text_rc(&r, k, F_UI_XS, g_th.muted, DT_SINGLELINE | DT_VCENTER);
    r = rc_make(x + S(92), y, x + w, y + S(20));
    gfx_text_rc(&r, v[0] ? v : L"-", F_UI_S, g_th.text, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

void pg_drivers_paint(void)
{
    RECT c = g_cont, r, t, fr;
    int gap = S(12), cw = (c.right - c.left - 4 * gap) / 5, i, x = c.left, y;
    int bh = S(138);
    wchar_t num[32];
    struct { const wchar_t* label; int count; Rgb col; } cards[5];
    int total = g_ds ? g_ds->n : 0, ok = g_ds ? g_ds->cnt[DS_OK] : 0;
    int attn = g_ds ? g_ds->cnt[DS_OLD] + g_ds->cnt[DS_OUTDATED] + g_ds->cnt[DS_GENERIC] + g_ds->cnt[DS_UPDATE] : 0;
    int prob = g_ds ? g_ds->cnt[DS_PROBLEM] + g_ds->cnt[DS_MISSING] + g_ds->cnt[DS_UNSIGNED] : 0;
    int wu = 0;
    if (g_ds) for (i = 0; i < g_ds->n; i++) if (g_ds->rows[i].wu_title[0]) wu++;
    cards[0].label = T2(L"All devices", L"Semua perangkat"); cards[0].count = total; cards[0].col = g_th.text;
    cards[1].label = T2(L"Healthy", L"Sehat"); cards[1].count = ok; cards[1].col = g_th.ok;
    cards[2].label = T2(L"Needs attention", L"Perlu perhatian"); cards[2].count = attn; cards[2].col = g_th.warn;
    cards[3].label = T2(L"Problems", L"Bermasalah"); cards[3].count = prob; cards[3].col = g_th.bad;
    cards[4].label = T2(L"Windows Update", L"Windows Update"); cards[4].count = wu; cards[4].col = g_th.info;
    for (i = 0; i < 5; i++) {
        float hov = g_hv[HID_DR_FILTER0 + i];
        BOOL act = (dr_filter == i);
        r = rc_make(x, c.top, x + cw, c.top + S(84));
        gfx_rrect((float)r.left, (float)r.top, (float)cw, (float)S(84), (float)S(12), act ? rgb_mix(g_th.card, cards[i].col, 0.14f) : rgb_mix(g_th.card, g_th.card2, 0.3f + hov * 0.5f), 255);
        gfx_rrect_stroke((float)r.left, (float)r.top, (float)cw, (float)S(84), (float)S(12), 1.0f, act ? cards[i].col : rgb_mix(g_th.border, g_th.border2, hov), 255);
        wfmt(num, 32, L"%d", cards[i].count);
        t = rc_make(r.left + S(16), r.top + S(8), r.right - S(10), r.top + S(50));
        gfx_text_rc(&t, (i == 4 && !(g_ds && g_ds->wu_checked)) ? L"?" : num, F_BIG, cards[i].col, DT_SINGLELINE | DT_VCENTER);
        t = rc_make(r.left + S(16), r.top + S(52), r.right - S(10), r.top + S(74));
        gfx_text_rc(&t, cards[i].label, F_UI_S, g_th.dim, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        hot_add(&r, HID_DR_FILTER0 + i);
        x += cw + gap;
    }
    y = c.top + S(98);
    {
        int busy = job_busy(JS_DRIVER) ? BF_DISABLED : 0;
        x = c.left;
        r = rc_make(x, y, x + S(138), y + S(34)); w_btn(&r, T2(L"Scan drivers", L"Scan driver"), ICO_SEARCH, HID_DR_SCAN, BTN_PRIMARY, busy); x += S(146);
        r = rc_make(x, y, x + S(210), y + S(34)); w_btn(&r, T2(L"Check Windows Update", L"Cek Windows Update"), ICO_GLOBE, HID_DR_WU, BTN_SOFT, busy); x += S(218);
        if (job_busy(JS_DRIVER)) { r = rc_make(x, y, x + S(90), y + S(34)); w_btn(&r, T2(L"Stop", L"Stop"), ICO_STOP, HID_DR_STOP, BTN_DANGER, 0); }
    }
    fr = rc_make(c.right - S(260), y, c.right, y + S(34));
    w_input_frame(&fr, GetFocus() == g_ed_drv);
    SendMessageW(g_ed_drv, EM_SETCUEBANNER, TRUE, (LPARAM)T2(L"Filter devices...", L"Filter perangkat..."));
    y += S(42);
    {
        int busy = job_busy(JS_DRIVER) ? BF_DISABLED : 0, bw;
        const wchar_t* tx;
        x = c.left;
        tx = T2(L"Find driver online", L"Cari driver online");        bw = w_btn_width(tx, TRUE);
        r = rc_make(x, y, x + bw, y + S(34)); w_btn(&r, tx, ICO_SEARCH, HID_DR_ONLINE, BTN_SOFT, 0); x += bw + S(8);
        tx = T2(L"Windows Update page", L"Halaman Windows Update");   bw = w_btn_width(tx, TRUE);
        r = rc_make(x, y, x + bw, y + S(34)); w_btn(&r, tx, ICO_GLOBE, HID_DR_WUPAGE, BTN_SOFT, 0); x += bw + S(8);
        tx = T2(L"Device Manager", L"Device Manager");               bw = w_btn_width(tx, TRUE);
        r = rc_make(x, y, x + bw, y + S(34)); w_btn(&r, tx, ICO_CHIP, HID_DR_DEVMGR, BTN_SOFT, 0); x += bw + S(8);
        tx = T2(L"Backup drivers", L"Backup driver");                bw = w_btn_width(tx, TRUE);
        r = rc_make(x, y, x + bw, y + S(34)); w_btn(&r, tx, ICO_SAVE, HID_DR_BACKUP, BTN_SOFT, busy); x += bw + S(8);
        tx = T2(L"Export CSV", L"Ekspor CSV");                       bw = w_btn_width(tx, TRUE);
        r = rc_make(x, y, x + bw, y + S(34)); w_btn(&r, tx, ICO_COPY, HID_DR_EXPORT, BTN_SOFT, 0); x += bw + S(8);
        tx = T2(L"Rescan hardware", L"Pindai ulang HW");              bw = w_btn_width(tx, TRUE);
        r = rc_make(x, y, x + bw, y + S(34)); w_btn(&r, tx, ICO_REFRESH, HID_DR_RESCAN, BTN_SOFT, busy);
    }
    y += S(40);
    if (job_busy(JS_DRIVER)) { r = rc_make(c.left, y + S(6), c.right, y + S(12)); w_progress(&r, -1.0, g_th.accent); }
    t = rc_make(c.left, y, c.right, y + S(22));
    gfx_text_rc(&t, dr_status, F_UI_S, g_th.dim, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    y += S(26);
    r = rc_make(c.left, y, c.right, c.bottom - bh - gap);
    tbl_draw(&t_drv, &r);

    /* bottom panel */
    r = rc_make(c.left, c.bottom - bh, c.right, c.bottom);
    w_card_rc(&r);
    {
        RECT p1 = rc_make(r.left + S(14), r.top + S(8), r.left + S(14) + S(100), r.top + S(34));
        RECT p2 = rc_make(p1.right + S(8), r.top + S(8), p1.right + S(8) + S(120), r.top + S(34));
        w_pill(&p1, T2(L"Details", L"Detail"), HID_DR_TABDET, dr_bottom == 0, F_UI_S);
        w_pill(&p2, T2(L"Activity log", L"Log aktivitas"), HID_DR_TABLOG, dr_bottom == 1, F_UI_S);
    }
    if (dr_bottom == 0) {
        const DriverRow* d = dr_selected();
        int cx = r.left + S(18), cy = r.top + S(42), colw = (r.right - r.left - S(36) - S(20)) / 2;
        if (!d) {
            t = rc_make(r.left + S(18), r.top + S(48), r.right - S(18), r.bottom - S(10));
            gfx_text_rc(&t, T2(L"Select a device to see its driver details and the recommended action.", L"Pilih perangkat untuk melihat detail driver dan tindakan yang disarankan."), F_UI_S, g_th.muted, DT_WORDBREAK);
        } else {
            wchar_t ver[160], note[400];
            wfmt(ver, 160, L"%ls   %ls", d->version[0] ? d->version : L"-", d->date[0] ? d->date : L"");
            kv_line(cx, cy, colw, T2(L"Device", L"Perangkat"), d->name);
            kv_line(cx, cy + S(21), colw, T2(L"Provider", L"Provider"), d->provider[0] ? d->provider : d->mfg);
            kv_line(cx, cy + S(42), colw, T2(L"Version / date", L"Versi / tanggal"), ver);
            kv_line(cx, cy + S(63), colw, L"INF", d->inf);
            kv_line(cx + colw + S(20), cy, colw, T2(L"Hardware ID", L"Hardware ID"), d->hwid);
            kv_line(cx + colw + S(20), cy + S(21), colw, T2(L"Instance", L"Instance"), d->instance);
            kv_line(cx + colw + S(20), cy + S(42), colw, T2(L"Windows Update", L"Windows Update"), d->wu_title[0] ? d->wu_title : T2(L"no update known", L"tidak ada update diketahui"));
            wcopy(note, 400, d->note);
            t = rc_make(cx, cy + S(86), r.right - S(18), r.bottom - S(6));
            gfx_circle((float)(cx + S(5)), (float)(t.top + S(10)), (float)S(4), sev_color(driver_state_severity(d->state) == 3 ? 4 : driver_state_severity(d->state)), 255);
            t.left += S(16);
            gfx_text_rc(&t, note, F_UI_S, g_th.text, DT_WORDBREAK | DT_TOP);
        }
    }
}

static void dr_click(int id)
{
    wchar_t p[MAX_PATH * 2], q[600], enc[900], desk[MAX_PATH], name[64];
    SYSTEMTIME st;
    if (id >= HID_DR_FILTER0 && id < HID_DR_FILTER0 + 5) {
        dr_filter = id - HID_DR_FILTER0;
        tbl_rebuild(&t_drv);
        return;
    }
    switch (id) {
    case HID_DR_SCAN: pg_drivers_start_scan(); break;
    case HID_DR_WU: dr_wu_check(); break;
    case HID_DR_STOP: job_stop(JS_DRIVER); break;
    case HID_DR_TABDET: dr_bottom = 0; pages_layout(); break;
    case HID_DR_TABLOG: dr_bottom = 1; pages_layout(); break;
    case HID_DR_WUPAGE: shell_open(L"ms-settings:windowsupdate-optionalupdates", NULL); break;
    case HID_DR_DEVMGR: shell_open(L"devmgmt.msc", NULL); break;
    case HID_DR_BACKUP:
        if (confirm_box(T2(L"Export all third-party drivers to Documents\\MultiTool Driver Backup? (needs Administrator)", L"Ekspor semua driver pihak ketiga ke Documents\\MultiTool Driver Backup? (butuh Administrator)"))) dr_tool(0);
        break;
    case HID_DR_RESCAN: dr_tool(1); break;
    case HID_DR_ONLINE: {
        const DriverRow* d = dr_selected();
        if (!d) { ui_toast(2, T2(L"Select a device first.", L"Pilih perangkat terlebih dahulu.")); break; }
        wfmt(q, 600, L"%ls %ls driver download", d->provider[0] && !wcontains_i(d->provider, L"Microsoft") ? d->provider : d->mfg, d->name);
        url_encode(q, enc, 900);
        wfmt(p, MT_ARRAY(p), L"https://www.google.com/search?q=%ls", enc);
        open_url(p);
        break; }
    case HID_DR_EXPORT:
        if (!g_ds || g_ds->n == 0) { ui_toast(2, T2(L"Run a driver scan first.", L"Jalankan scan driver terlebih dahulu.")); break; }
        get_folder(CSIDL_DESKTOPDIRECTORY, desk);
        GetLocalTime(&st);
        wfmt(name, 64, L"driver_report_%04d%02d%02d_%02d%02d%02d.csv", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        path_join(p, MT_ARRAY(p), desk, name);
        if (driver_export_csv(g_ds, p)) { ui_toast(0, T2(L"Report saved to Desktop: %ls", L"Laporan disimpan di Desktop: %ls"), name); open_in_explorer_select(p); }
        else ui_toast(4, T2(L"Cannot write the report.", L"Tidak dapat menulis laporan."));
        break;
    default: break;
    }
}

void pg_drivers_init(void)
{
    tbl_init(&t_drv);
    t_drv.ncols = 7;
    t_drv.col[0].en = L"Status"; t_drv.col[0].id = L"Status"; t_drv.col[0].w = 13; t_drv.col[0].kind = 2;
    t_drv.col[1].en = L"Device"; t_drv.col[1].id = L"Perangkat"; t_drv.col[1].w = 30;
    t_drv.col[2].en = L"Class"; t_drv.col[2].id = L"Kelas"; t_drv.col[2].w = 11;
    t_drv.col[3].en = L"Provider"; t_drv.col[3].id = L"Provider"; t_drv.col[3].w = 17;
    t_drv.col[4].en = L"Version"; t_drv.col[4].id = L"Versi"; t_drv.col[4].w = 13;
    t_drv.col[5].en = L"Date"; t_drv.col[5].id = L"Tanggal"; t_drv.col[5].w = 10; t_drv.col[5].kind = 1;
    t_drv.col[6].en = L"Age"; t_drv.col[6].id = L"Umur"; t_drv.col[6].w = 7; t_drv.col[6].right = 1; t_drv.col[6].kind = 1;
    t_drv.sortcol = 0; t_drv.sortasc = 0;
    t_drv.empty_en = L"Press \"Scan drivers\" to inspect installed drivers."; t_drv.empty_id = L"Tekan \"Scan driver\" untuk memeriksa driver terpasang.";
    wcopy(dr_status, 200, L"");
}

void pg_drivers_free(void)
{
    tbl_free(&t_drv);
    driver_scan_free(g_ds);
}

void pg_drivers_enter(void)
{
    if (dr_first) { dr_first = 0; pg_drivers_start_scan(); }
}

BOOL pg_drivers_anim(void) { return job_busy(JS_DRIVER); }

Table* pg_drivers_table(void) { return &t_drv; }

void pg_drivers_command(HWND ed)
{
    if (ed == g_ed_drv) {
        GetWindowTextW(ed, t_drv.filter, 128);
        tbl_rebuild(&t_drv);
        t_drv.scroll = 0;
        g_dirty = TRUE;
    }
}

BOOL pg_drivers_click(int id)
{
    dr_click(id);
    g_dirty = TRUE;
    return TRUE;
}

void pg_drivers_app_msg(UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_APP_TABLE && wp == TBL_DRIVERS) {
            DriverScan* ds = (DriverScan*)lp;
            if (ds) {
                driver_scan_free(g_ds);
                g_ds = ds;
                dr_fill();
                if (ds->wu_checked) wfmt(dr_status, 200, ds->wu_ok ? T2(L"Windows Update lookup finished: %d driver update(s) available.", L"Pencarian Windows Update selesai: %d update driver tersedia.")
                                                                    : T2(L"Windows Update lookup could not complete (offline or service unavailable). Found %d.", L"Pencarian Windows Update tidak selesai (offline atau layanan tidak tersedia). Ditemukan %d."), ds->wu_found);
                else wfmt(dr_status, 200, T2(L"%d devices scanned. Click \"Check Windows Update\" to look for newer drivers online.", L"%d perangkat dipindai. Klik \"Cek Windows Update\" untuk mencari driver baru secara online."), ds->n);
            }
        g_dirty = TRUE;
    } else if (msg == WM_APP_DONE && (int)wp == JS_DRIVER) {
        g_dirty = TRUE;
    }
}
