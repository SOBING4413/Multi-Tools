/* dashboard.c - Dashboard page: gauges, performance graph, system card, top processes, quick actions */
#include "ui/internal.h"
#include "ui/livedata.h"
#include "ui/pages/pages.h"
#include "core/settings.h"
#include "modules/sys.h"
#include <process.h>

static double g_gv[4];                       /* animated gauge fractions */

static Rgb pct_color(double pct) { return pct > 85 ? g_th.bad : (pct > 70 ? g_th.warn : g_th.ok); }

static void gauge_card(int x, int y, int w, int h, const wchar_t* label, int icon, double frac_anim, Rgb col, const wchar_t* center,
                       const wchar_t* l1, const wchar_t* l2, const wchar_t* l3)
{
    float R = (float)S(36), cx = (float)(x + S(18)) + R + S(4), cy = (float)(y + S(40)) + R;
    RECT t;
    w_card((float)x, (float)y, (float)w, (float)h);
    gfx_icon(icon, (float)(x + S(26)), (float)(y + S(22)), (float)S(16), g_th.dim);
    t = rc_make(x + S(42), y + S(12), x + w - S(12), y + S(32));
    gfx_text_rc(&t, label, F_UI_SB, g_th.dim, DT_SINGLELINE | DT_VCENTER);
    w_gauge(cx, cy, R, (float)S(9), frac_anim, col);
    t = rc_make((int)(cx - R), (int)(cy - S(12)), (int)(cx + R), (int)(cy + S(12)));
    gfx_text_rc(&t, center, F_H2, g_th.text, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
    {
        int tx = (int)(cx + R + S(18));
        t = rc_make(tx, (int)cy - S(34), x + w - S(10), (int)cy - S(12));
        gfx_text_rc(&t, l1, F_UI_B, g_th.text, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        t = rc_make(tx, (int)cy - S(10), x + w - S(10), (int)cy + S(10));
        gfx_text_rc(&t, l2, F_UI_S, g_th.dim, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        t = rc_make(tx, (int)cy + S(12), x + w - S(10), (int)cy + S(32));
        gfx_text_rc(&t, l3, F_UI_S, g_th.dim, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }
}




void pg_dashboard_paint(void)
{
    RECT c = g_cont;
    int gap = S(16), W = c.right - c.left, gw = (W - 3 * gap) / 4, gh = S(136), y2, h2, h3, i, x;
    wchar_t t1[96], t2[96], t3[96], t4[96], cen[32];
    ULONGLONG up = sys_uptime_secs();

    /* gauges */
    x = c.left;
    wfmt(t1, 96, T2(L"%d cores / %d threads", L"%d core / %d thread"), g_live.phys, g_live.logi);
    wfmt(t2, 96, g_live.mhz ? L"%.1f GHz base" : L"-", g_live.mhz / 1000.0);
    wfmt(cen, 32, L"%.0f%%", g_live.cpu);
    gauge_card(x, c.top, gw, gh, L"CPU", ICO_CHIP, g_gv[0] / 100.0, pct_color(g_live.cpu), cen, t1, t2, T2(L"Processor load", L"Beban prosesor"));
    x += gw + gap;
    fmt_size(g_live.ram_total - g_live.ram_avail, t3, 96); fmt_size(g_live.ram_total, t4, 96);
    wfmt(t1, 96, T2(L"%ls used", L"%ls terpakai"), t3);
    wfmt(t2, 96, T2(L"of %ls", L"dari %ls"), t4);
    fmt_size(g_live.ram_avail, t3, 96);
    wfmt(t4, 96, T2(L"%ls available", L"%ls tersedia"), t3);
    wfmt(cen, 32, L"%.0f%%", g_live.ram);
    gauge_card(x, c.top, gw, gh, T2(L"Memory", L"Memori"), ICO_PROC, g_gv[1] / 100.0, pct_color(g_live.ram), cen, t1, t2, t4);
    x += gw + gap;
    if (g_live.has_sysdrive && g_live.sysdrive.total) {
        double pc = 100.0 * (double)(g_live.sysdrive.total - g_live.sysdrive.free) / (double)g_live.sysdrive.total;
        wchar_t lab[64];
        fmt_size(g_live.sysdrive.total - g_live.sysdrive.free, t3, 96); fmt_size(g_live.sysdrive.total, t4, 96);
        wfmt(t1, 96, T2(L"%ls used", L"%ls terpakai"), t3);
        wfmt(t2, 96, T2(L"of %ls", L"dari %ls"), t4);
        fmt_size(g_live.sysdrive.free, t3, 96);
        wfmt(t4, 96, T2(L"%ls free", L"%ls sisa"), t3);
        wfmt(cen, 32, L"%.0f%%", pc);
        wfmt(lab, 64, T2(L"Storage (%ls)", L"Penyimpanan (%ls)"), g_live.sysdrive.root);
        gauge_card(x, c.top, gw, gh, lab, ICO_FOLDER, g_gv[2] / 100.0, pct_color(pc), cen, t1, t2, t4);
    }
    x += gw + gap;
    if (g_live.bat_present) {
        wfmt(cen, 32, L"%d%%", g_live.bat_pct);
        wcopy(t1, 96, g_live.bat_plug ? T2(L"Charging", L"Sedang di-charge") : T2(L"On battery", L"Memakai baterai"));
        if (g_live.bat_secs > 0 && !g_live.bat_plug) wfmt(t2, 96, T2(L"%ld h %ld min left", L"Sisa %ld jam %ld menit"), g_live.bat_secs / 3600, (g_live.bat_secs % 3600) / 60);
        else wcopy(t2, 96, g_live.bat_plug ? T2(L"Plugged in", L"Terhubung ke listrik") : L"-");
        gauge_card(x, c.top, gw, gh, T2(L"Battery", L"Baterai"), ICO_POWER, g_gv[3] / 100.0, g_live.bat_pct > 40 ? g_th.ok : (g_live.bat_pct > 20 ? g_th.warn : g_th.bad), cen, t1, t2, L"");
    } else {
        double h = (double)up / 3600.0;
        wfmt(cen, 32, h >= 24 ? L"%.0fd" : L"%.0fh", h >= 24 ? h / 24.0 : h);
        fmt_duration(up, t1, 96);
        wcopy(t2, 96, T2(L"Since last boot", L"Sejak boot terakhir"));
        gauge_card(x, c.top, gw, gh, L"Uptime", ICO_MONITOR, MT_MIN(1.0, h / 72.0), h > 72 ? g_th.warn : g_th.accent, cen, t1, t2, L"");
    }

    /* row 2 */
    y2 = c.top + gh + gap;
    { int rem = c.bottom - y2; h2 = MT_MAX(S(200), rem * 55 / 100); h3 = rem - h2 - gap; if (h3 < S(150)) { h3 = S(150); h2 = MT_MAX(S(160), rem - h3 - gap); } }
    {
        int lw = (W - gap) * 62 / 100;
        RECT card = rc_make(c.left, y2, c.left + lw, y2 + h2), gr, t;
        int sc = live_health_score();
        float v1[LIVE_HIST], v2[LIVE_HIST];
        w_card_rc(&card);
        t = rc_make(card.left + S(18), card.top + S(12), card.left + S(200), card.top + S(34));
        gfx_text_rc(&t, T2(L"Performance (last 2 minutes)", L"Performa (2 menit terakhir)"), F_UI_B, g_th.text, DT_SINGLELINE | DT_VCENTER);
        { int lx = card.right - S(18); wchar_t hs[32];
          wfmt(hs, 32, T2(L"Health %d/100", L"Kesehatan %d/100"), sc);
          lx -= w_text_width(hs, F_UI_SB) + S(20);
          w_badge_at(lx, card.top + S(11), hs, sc >= 80 ? g_th.ok : (sc >= 60 ? g_th.warn : g_th.bad));
          wfmt(hs, 32, L"RAM %.0f%%", g_live.ram); lx -= w_text_width(hs, F_UI_S) + S(28);
          gfx_circle((float)lx, (float)(card.top + S(22)), (float)S(4), g_th.info, 255);
          t = rc_make(lx + S(8), card.top + S(11), lx + S(100), card.top + S(33)); gfx_text_rc(&t, hs, F_UI_S, g_th.dim, DT_SINGLELINE | DT_VCENTER);
          wfmt(hs, 32, L"CPU %.0f%%", g_live.cpu); lx -= w_text_width(hs, F_UI_S) + S(30);
          gfx_circle((float)lx, (float)(card.top + S(22)), (float)S(4), g_th.accent, 255);
          t = rc_make(lx + S(8), card.top + S(11), lx + S(100), card.top + S(33)); gfx_text_rc(&t, hs, F_UI_S, g_th.dim, DT_SINGLELINE | DT_VCENTER); }
        gr = rc_make(card.left + S(48), card.top + S(46), card.right - S(18), card.bottom - S(18));
        w_grid_lines(&gr, 4);
        { int k; for (k = 0; k <= 4; k++) { wchar_t lb[8]; RECT lr; wfmt(lb, 8, L"%d%%", 100 - k * 25); lr = rc_make(card.left + S(8), gr.top + (gr.bottom - gr.top) * k / 4 - S(8), card.left + S(44), gr.top + (gr.bottom - gr.top) * k / 4 + S(8)); gfx_text_rc(&lr, lb, F_UI_XS, g_th.muted, DT_SINGLELINE | DT_VCENTER | DT_RIGHT); } }
        live_copy(v2, g_live.ram_h);
        gfx_clip(gr.left, gr.top - 2, gr.right, gr.bottom + 2);
        gfx_graph((float)gr.left, (float)gr.top, (float)(gr.right - gr.left), (float)(gr.bottom - gr.top), v2, LIVE_HIST, 100.0f, g_th.info, (float)S(2) * 0.75f);
        live_copy(v1, g_live.cpu_h);
        gfx_graph((float)gr.left, (float)gr.top, (float)(gr.right - gr.left), (float)(gr.bottom - gr.top), v1, LIVE_HIST, 100.0f, g_th.accent, (float)S(2) * 0.9f);
        gfx_unclip();

        /* system card */
        {
            RECT sc2 = rc_make(c.left + lw + gap, y2, c.right, y2 + h2);
            int ry = sc2.top + S(44), rh = S(26), kx = sc2.left + S(18), vx = sc2.left + S(96);
            struct { const wchar_t* k; const wchar_t* v; } rows[7];
            wchar_t upb[64], usr[160], ram[48];
            fmt_duration(up, upb, 64);
            wfmt(usr, 160, L"%ls%ls", g_live.user, g_admin ? T2(L"  (Administrator)", L"  (Administrator)") : L"");
            fmt_size(g_live.ram_total, ram, 48);
            rows[0].k = L"OS"; rows[0].v = g_live.os;
            rows[1].k = T2(L"Computer", L"Komputer"); rows[1].v = g_live.host;
            rows[2].k = L"CPU"; rows[2].v = g_live.cpuname;
            rows[3].k = L"RAM"; rows[3].v = ram;
            rows[4].k = T2(L"Local IP", L"IP Lokal"); rows[4].v = g_live.ip;
            rows[5].k = L"Uptime"; rows[5].v = upb;
            rows[6].k = T2(L"User", L"User"); rows[6].v = usr;
            w_card_rc(&sc2);
            t = rc_make(kx, sc2.top + S(12), sc2.right - S(18), sc2.top + S(34));
            gfx_text_rc(&t, T2(L"System", L"Sistem"), F_UI_B, g_th.text, DT_SINGLELINE | DT_VCENTER);
            for (i = 0; i < 7 && ry + rh <= sc2.bottom - S(4); i++) {
                t = rc_make(kx, ry, vx - S(6), ry + rh);
                gfx_text_rc(&t, rows[i].k, F_UI_S, g_th.muted, DT_SINGLELINE | DT_VCENTER);
                t = rc_make(vx, ry, sc2.right - S(14), ry + rh);
                gfx_text_rc(&t, rows[i].v, F_UI_S, g_th.text, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
                ry += rh;
            }
        }
    }

    /* row 3 */
    {
        int y3 = y2 + h2 + gap, lw = (W - gap) * 62 / 100;
        RECT card = rc_make(c.left, y3, c.left + lw, y3 + h3), t;
        RECT q = rc_make(c.left + lw + gap, y3, c.right, y3 + h3);
        ULONGLONG maxws = 1;
        int ry = card.top + S(42), rh = S(24);
        w_card_rc(&card);
        t = rc_make(card.left + S(18), card.top + S(10), card.right - S(18), card.top + S(32));
        gfx_text_rc(&t, T2(L"Top processes by memory", L"Proses teratas berdasarkan memori"), F_UI_B, g_th.text, DT_SINGLELINE | DT_VCENTER);
        for (i = 0; i < g_live.ntop; i++) if (g_live.top[i].ws > maxws) maxws = g_live.top[i].ws;
        for (i = 0; i < g_live.ntop && ry + rh <= card.bottom - S(4); i++) {
            RECT bar;
            wchar_t sz[48];
            t = rc_make(card.left + S(18), ry, card.left + S(190), ry + rh);
            gfx_text_rc(&t, g_live.top[i].name, F_UI_S, g_th.text, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            bar = rc_make(card.left + S(198), ry + S(9), card.right - S(110), ry + S(15));
            w_progress(&bar, (double)g_live.top[i].ws / (double)maxws, g_th.accent);
            fmt_size(g_live.top[i].ws, sz, 48);
            t = rc_make(card.right - S(100), ry, card.right - S(16), ry + rh);
            gfx_text_rc(&t, sz, F_UI_S, g_th.dim, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
            ry += rh;
        }
        w_card_rc(&q);
        t = rc_make(q.left + S(18), q.top + S(10), q.right - S(18), q.top + S(32));
        gfx_text_rc(&t, T2(L"Quick actions", L"Aksi cepat"), F_UI_B, g_th.text, DT_SINGLELINE | DT_VCENTER);
        {
            int bw = (q.right - q.left - S(18) * 2 - S(10)) / 2, bh = S(38), bx = q.left + S(18), by = q.top + S(44);
            RECT b;
            b = rc_make(bx, by, bx + bw, by + bh);                     w_btn(&b, T2(L"Health report", L"Laporan kesehatan"), ICO_SAVE, HID_DB_REPORT, BTN_SOFT, 0);
            b = rc_make(bx + bw + S(10), by, bx + 2 * bw + S(10), by + bh); w_btn(&b, T2(L"Quick scan", L"Quick scan"), ICO_BUG, HID_DB_SCAN, BTN_SOFT, 0);
            by += bh + S(10);
            b = rc_make(bx, by, bx + bw, by + bh);                     w_btn(&b, T2(L"Scan drivers", L"Scan driver"), ICO_CHIP, HID_DB_DRV, BTN_SOFT, 0);
            b = rc_make(bx + bw + S(10), by, bx + 2 * bw + S(10), by + bh); w_btn(&b, T2(L"Clean temp", L"Bersihkan temp"), ICO_CLEAN, HID_DB_CLEAN, BTN_SOFT, 0);
            by += bh + S(10);
            if (!g_admin && by + bh <= q.bottom) {
                b = rc_make(bx, by, bx + 2 * bw + S(10), by + bh);
                w_btn(&b, T2(L"Restart as Administrator", L"Jalankan ulang sebagai Administrator"), ICO_LOCK, HID_ADMIN, BTN_PRIMARY, 0);
            }
        }
    }
}

/* ---------------------------------------------------------------- behaviour */
BOOL pg_dashboard_anim(void)
{
    BOOL need = FALSE;
    double tgt[4];
    int i;
    tgt[0] = g_live.cpu; tgt[1] = g_live.ram;
    tgt[2] = (g_live.has_sysdrive && g_live.sysdrive.total) ? 100.0 * (double)(g_live.sysdrive.total - g_live.sysdrive.free) / (double)g_live.sysdrive.total : 0;
    tgt[3] = g_live.bat_present ? g_live.bat_pct : 0;
    for (i = 0; i < 4; i++) {
        double d = tgt[i] - g_gv[i];
        if (d > 0.05 || d < -0.05) { g_gv[i] += g_set.animations ? d * 0.14 : d; need = TRUE; } else g_gv[i] = tgt[i];
    }
    return need;
}

static unsigned __stdcall report_thread(void* p)
{
    wchar_t path[MAX_PATH * 2];
    BOOL ok;
    wchar_t* msg;
    (void)p;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    ok = report_generate(path, MT_ARRAY(path));
    msg = ok ? wdupf(T2(L"Report saved: %ls", L"Laporan disimpan: %ls"), path) : wdup(T2(L"Failed to create the report.", L"Gagal membuat laporan."));
    PostMessageW(g_hwnd, WM_APP_STATUS, ok ? 0 : 4, (LPARAM)msg);
    if (ok) { wchar_t* p2 = wdup(path); PostMessageW(g_hwnd, WM_APP_STATUS, 100, (LPARAM)p2); }
    CoUninitialize();
    return 0;
}

BOOL pg_dashboard_click(int id)
{
    switch (id) {
    case HID_DB_REPORT: { HANDLE h = (HANDLE)_beginthreadex(NULL, 0, report_thread, NULL, 0, NULL); if (h) CloseHandle(h); ui_toast(1, T2(L"Generating report...", L"Membuat laporan...")); return TRUE; }
    case HID_DB_SCAN:   shell_navigate(PG_MALWARE); pg_malware_start_quickscan(); return TRUE;
    case HID_DB_DRV:    shell_navigate(PG_DRIVERS); pg_drivers_start_scan(); return TRUE;
    case HID_DB_CLEAN:  shell_navigate(PG_CLEANUP); pg_features_select(FP_CLEANUP, 0); return TRUE;
    default: return FALSE;
    }
}
