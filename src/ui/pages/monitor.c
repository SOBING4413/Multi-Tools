/* monitor.c - Live Monitor page: CPU, memory and network graphs */
#include "ui/internal.h"
#include "ui/livedata.h"
#include "ui/pages/pages.h"

static double vmax_of(const float* a, double floor_v)
{
    int i;
    double m = floor_v;
    for (i = 0; i < LIVE_HIST; i++) if (a[i] > m) m = a[i];
    return m * 1.15;
}

static void mon_panel(const RECT* card, const wchar_t* title, const wchar_t* big, const wchar_t* sub, Rgb col,
                      const float* a, double vmax, const float* b, Rgb col2, const wchar_t* axis_top)
{
    RECT t, gr;
    float v[LIVE_HIST];
    w_card_rc(card);
    t = rc_make(card->left + S(18), card->top + S(10), card->right - S(300), card->top + S(30));
    gfx_text_rc(&t, title, F_UI_SB, g_th.dim, DT_SINGLELINE | DT_VCENTER);
    t = rc_make(card->left + S(18), card->top + S(28), card->left + S(260), card->top + S(66));
    gfx_text_rc(&t, big, F_NUM, col, DT_SINGLELINE | DT_VCENTER);
    t = rc_make(card->left + S(260), card->top + S(12), card->right - S(18), card->top + S(66));
    gfx_text_rc(&t, sub, F_UI_S, g_th.dim, DT_RIGHT | DT_VCENTER | DT_WORDBREAK);
    gr = rc_make(card->left + S(18), card->top + S(76), card->right - S(18), card->bottom - S(16));
    w_grid_lines(&gr, 4);
    t = rc_make(gr.right - S(120), gr.top + 2, gr.right - S(4), gr.top + S(18));
    gfx_text_rc(&t, axis_top, F_UI_XS, g_th.muted, DT_SINGLELINE | DT_RIGHT);
    gfx_clip(gr.left, gr.top - 2, gr.right, gr.bottom + 2);
    if (b) { live_copy(v, b); gfx_graph((float)gr.left, (float)gr.top, (float)(gr.right - gr.left), (float)(gr.bottom - gr.top), v, LIVE_HIST, (float)vmax, col2, (float)S(2) * 0.8f); }
    live_copy(v, a);
    gfx_graph((float)gr.left, (float)gr.top, (float)(gr.right - gr.left), (float)(gr.bottom - gr.top), v, LIVE_HIST, (float)vmax, col, (float)S(2) * 0.9f);
    gfx_unclip();
}

void pg_monitor_paint(void)
{
    RECT c = g_cont, r;
    int gap = S(14), ph = (c.bottom - c.top - S(50) - 2 * gap) / 3, y = c.top + S(46);
    wchar_t big[64], sub[160], ax[32], a1[48], a2[48], a3[48], a4[48];
    double vm;
    RECT tg = rc_make(c.right - S(52), c.top + S(4), c.right, c.top + S(28));
    RECT tl = rc_make(c.right - S(200), c.top + S(2), c.right - S(60), c.top + S(30));
    gfx_text_rc(&tl, g_live.paused ? T2(L"Paused", L"Dijeda") : T2(L"Live", L"Langsung"), F_UI_SB, g_live.paused ? g_th.warn : g_th.ok, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
    w_toggle(&tg, !g_live.paused, HID_MON_PAUSE);
    { RECT d = rc_make(c.left, c.top + S(2), c.right - S(210), c.top + S(30));
      gfx_text_rc(&d, T2(L"Updated every second. Use the switch to pause.", L"Diperbarui tiap detik. Gunakan saklar untuk menjeda."), F_UI_S, g_th.dim, DT_SINGLELINE | DT_VCENTER); }

    r = rc_make(c.left, y, c.right, y + ph);
    wfmt(big, 64, L"%.1f%%", g_live.cpu);
    wfmt(sub, 160, T2(L"%d cores / %d threads\n%ls", L"%d core / %d thread\n%ls"), g_live.phys, g_live.logi, g_live.cpuname);
    mon_panel(&r, L"CPU", big, sub, g_th.accent, g_live.cpu_h, 100.0, NULL, 0, L"100%");

    y += ph + gap;
    r = rc_make(c.left, y, c.right, y + ph);
    fmt_size(g_live.ram_total - g_live.ram_avail, a1, 48); fmt_size(g_live.ram_total, a2, 48);
    wfmt(big, 64, L"%.1f%%", g_live.ram);
    wfmt(sub, 160, T2(L"%ls used of %ls", L"%ls terpakai dari %ls"), a1, a2);
    mon_panel(&r, T2(L"Memory", L"Memori"), big, sub, g_th.info, g_live.ram_h, 100.0, NULL, 0, L"100%");

    y += ph + gap;
    r = rc_make(c.left, y, c.right, y + ph);
    fmt_rate(g_live.dn, a1, 48); fmt_rate(g_live.up, a2, 48);
    fmt_size(g_live.rx - g_live.rx0, a3, 48); fmt_size(g_live.tx - g_live.tx0, a4, 48);
    wfmt(big, 64, L"%ls %ls", U_DOWN, a1);
    wfmt(sub, 160, T2(L"Upload %ls\nReceived %ls / Sent %ls (since start)", L"Upload %ls\nDiterima %ls / Terkirim %ls (sejak mulai)"), a2, a3, a4);
    vm = MT_MAX(vmax_of(g_live.dn_h, 50 * 1024.0), vmax_of(g_live.up_h, 50 * 1024.0));
    fmt_rate(vm, ax, 32);
    mon_panel(&r, T2(L"Network - download (green) / upload (yellow)", L"Jaringan - download (hijau) / upload (kuning)"), big, sub, g_th.accent, g_live.dn_h, vm, g_live.up_h, g_th.warn, ax);
}

BOOL pg_monitor_click(int id)
{
    if (id == HID_MON_PAUSE) { g_live.paused = !g_live.paused; g_dirty = TRUE; return TRUE; }
    return FALSE;
}
