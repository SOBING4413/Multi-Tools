/* power.c - Power page: scheduled shutdown / restart / sleep / hibernate with countdown */
#include "ui/internal.h"
#include "ui/pages/pages.h"
#include "modules/power_control.h"

static int g_pw_mode = 0;
static BOOL g_pw_active = FALSE;
static ULONGLONG g_pw_end = 0, g_pw_total = 0;
static wchar_t g_pw_msg[256];
static int g_pw_msg_sev = 1;

static void pw_msg(int sev, const wchar_t* m) { wcopy(g_pw_msg, 256, m); g_pw_msg_sev = sev; g_dirty = TRUE; }

static void pw_start(void)
{
    wchar_t t[32], msg[256];
    wchar_t* e;
    long mins;
    edit_get(g_ed_power, t, 32);
    wtrim(t);
    mins = wcstol(t, &e, 10);
    if (!t[0] || *e || mins < 1 || mins > 10000) { pw_msg(4, T2(L"Enter a whole number of minutes (1 - 10000).", L"Masukkan jumlah menit berupa angka bulat (1 - 10000).")); return; }
    if (g_pw_mode == PW_SHUTDOWN || g_pw_mode == PW_RESTART) {
        if (!power_schedule(g_pw_mode, (int)mins, msg, 256)) { pw_msg(4, msg); return; }
        pw_msg(0, msg);
    } else {
        pw_msg(0, g_pw_mode == PW_SLEEP ? T2(L"Sleep timer started. Keep MultiTool open.", L"Timer sleep dimulai. Biarkan MultiTool tetap terbuka.")
                                         : T2(L"Hibernate timer started. Keep MultiTool open.", L"Timer hibernate dimulai. Biarkan MultiTool tetap terbuka."));
    }
    g_pw_active = TRUE;
    g_pw_total = (ULONGLONG)mins * 60000ULL;
    g_pw_end = GetTickCount64() + g_pw_total;
}

static void pw_cancel(void)
{
    wchar_t msg[256];
    if (g_pw_mode == PW_SHUTDOWN || g_pw_mode == PW_RESTART || g_pw_active) {
        BOOL ok = power_cancel(msg, 256);
        if (!ok && g_pw_active && (g_pw_mode == PW_SLEEP || g_pw_mode == PW_HIBERNATE)) { wcopy(msg, 256, T2(L"Timer cancelled.", L"Timer dibatalkan.")); ok = TRUE; }
        pw_msg(ok ? 0 : 2, msg);
    }
    g_pw_active = FALSE;
}

static void pw_tick(void)
{
    if (g_pw_active && GetTickCount64() >= g_pw_end) {
        g_pw_active = FALSE;
        if (g_pw_mode == PW_SLEEP) power_sleep_now();
        else if (g_pw_mode == PW_HIBERNATE) power_hibernate_now();
        else pw_msg(1, T2(L"Time is up - the system is shutting down / restarting.", L"Waktu habis - sistem sedang shutdown / restart."));
    }
}

void pg_power_paint(void)
{
    RECT c = g_cont, left, right, t, r;
    int gap = S(16), lw = (c.right - c.left - gap) * 60 / 100, i, x, y;
    static const wchar_t* modes_en[4] = { L"Shutdown", L"Restart", L"Sleep", L"Hibernate" };
    static const wchar_t* modes_id[4] = { L"Shutdown", L"Restart", L"Sleep", L"Hibernate" };
    static const int chips[5] = { 5, 15, 30, 60, 120 };
    int ch = c.bottom - c.top;
    int card_h = MT_MIN(ch, S(400));
    left = rc_make(c.left, c.top, c.left + lw, c.top + card_h);
    right = rc_make(c.left + lw + gap, c.top, c.right, c.top + card_h);
    w_card_rc(&left);
    t = rc_make(left.left + S(22), left.top + S(16), left.right - S(22), left.top + S(40));
    gfx_text_rc(&t, T2(L"Schedule a power action", L"Jadwalkan aksi power"), F_H2, g_th.text, DT_SINGLELINE | DT_VCENTER);
    t = rc_make(left.left + S(22), left.top + S(44), left.right - S(22), left.top + S(66));
    gfx_text_rc(&t, T2(L"Shutdown and restart are scheduled in Windows and survive closing this app.", L"Shutdown dan restart dijadwalkan di Windows dan tetap berjalan walau aplikasi ditutup."), F_UI_S, g_th.dim, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    x = left.left + S(22); y = left.top + S(82);
    for (i = 0; i < 4; i++) {
        int w = (left.right - left.left - S(44) - 3 * S(8)) / 4;
        r = rc_make(x, y, x + w, y + S(36));
        w_pill(&r, T2(modes_en[i], modes_id[i]), HID_PW_MODE0 + i, g_pw_mode == i, F_UI_B);
        x += w + S(8);
    }
    y += S(54);
    t = rc_make(left.left + S(22), y, left.left + S(200), y + S(22));
    gfx_text_rc(&t, T2(L"After how many minutes?", L"Setelah berapa menit?"), F_UI_S, g_th.dim, DT_SINGLELINE | DT_VCENTER);
    y += S(28);
    r = rc_make(left.left + S(22), y, left.left + S(22) + S(170), y + S(40));
    w_input_frame(&r, GetFocus() == g_ed_power);
    edit_place(g_ed_power, &r, TRUE);
    x = r.right + S(14);
    for (i = 0; i < 5; i++) {
        wchar_t lb[16];
        RECT cr;
        wfmt(lb, 16, L"%d", chips[i]);
        cr = rc_make(x, y + S(4), x + S(46), y + S(36));
        w_pill(&cr, lb, HID_PW_CHIP0 + i, FALSE, F_UI_S);
        x += S(52);
    }
    y += S(64);
    r = rc_make(left.left + S(22), y, left.left + S(22) + S(160), y + S(42));
    w_btn(&r, T2(L"Start timer", L"Mulai timer"), ICO_RUN, HID_PW_START, BTN_PRIMARY, 0);
    r = rc_make(left.left + S(22) + S(172), y, left.left + S(22) + S(172) + S(160), y + S(42));
    w_btn(&r, T2(L"Cancel", L"Batalkan"), ICO_CLEAR, HID_PW_CANCEL, BTN_DANGER, 0);
    y += S(62);
    if (g_pw_msg[0]) {
        t = rc_make(left.left + S(22), y, left.right - S(22), y + S(44));
        gfx_text_rc(&t, g_pw_msg, F_UI_S, sev_color(g_pw_msg_sev), DT_WORDBREAK | DT_TOP);
    }

    /* right card: countdown + quick actions */
    w_card_rc(&right);
    t = rc_make(right.left + S(22), right.top + S(16), right.right - S(22), right.top + S(40));
    gfx_text_rc(&t, T2(L"Status", L"Status"), F_H2, g_th.text, DT_SINGLELINE | DT_VCENTER);
    {
        float R = (float)S(54), cx = (float)(right.left + (right.right - right.left) / 2), cy = (float)(right.top + S(70)) + R + S(6);
        ULONGLONG now = GetTickCount64(), remain = (g_pw_active && g_pw_end > now) ? g_pw_end - now : 0;
        double frac = g_pw_active && g_pw_total ? (double)remain / (double)g_pw_total : 0;
        wchar_t tm[32];
        unsigned long s = (unsigned long)(remain / 1000);
        wfmt(tm, 32, L"%02lu:%02lu:%02lu", s / 3600, (s / 60) % 60, s % 60);
        w_gauge(cx, cy, R, (float)S(9), g_pw_active ? frac : 0.0, g_pw_active ? g_th.accent : g_th.border2);
        t = rc_make((int)(cx - R), (int)(cy - S(14)), (int)(cx + R), (int)(cy + S(14)));
        gfx_text_rc(&t, g_pw_active ? tm : L"--:--:--", F_H2, g_pw_active ? g_th.text : g_th.muted, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
        t = rc_make(right.left + S(16), (int)(cy + R + S(10)), right.right - S(16), (int)(cy + R + S(32)));
        gfx_text_rc(&t, g_pw_active ? T2(modes_en[g_pw_mode], modes_id[g_pw_mode]) : T2(L"No timer running", L"Tidak ada timer berjalan"), F_UI_B, g_pw_active ? g_th.accent : g_th.dim, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
        y = (int)(cy + R + S(46));
    }
    {
        int bw = right.right - right.left - S(44), bx = right.left + S(22);
        r = rc_make(bx, y, bx + bw, y + S(38));      w_btn(&r, TX(K_menu_lock), ICO_LOCK, HID_PW_LOCK, BTN_SOFT, 0);
        y += S(46);
        r = rc_make(bx, y, bx + bw, y + S(38));      w_btn(&r, T2(L"Sleep now", L"Sleep sekarang"), ICO_MONITOR, HID_PW_SLEEP, BTN_SOFT, 0);
        y += S(46);
        r = rc_make(bx, y, bx + bw, y + S(38));      w_btn(&r, T2(L"Hibernate now", L"Hibernate sekarang"), ICO_POWER, HID_PW_HIB, BTN_SOFT, 0);
    }
}

void pg_power_init(void) { wcopy(g_pw_msg, 256, L""); }

void pg_power_tick(void) { pw_tick(); }

BOOL pg_power_anim(void) { return g_pw_active; }

BOOL pg_power_click(int id)
{
    static const int chips[5] = { 5, 15, 30, 60, 120 };
    if (id >= HID_PW_MODE0 && id < HID_PW_MODE0 + 4) { g_pw_mode = id - HID_PW_MODE0; g_dirty = TRUE; return TRUE; }
    if (id >= HID_PW_CHIP0 && id < HID_PW_CHIP0 + 5) {
        wchar_t b[16];
        wfmt(b, 16, L"%d", chips[id - HID_PW_CHIP0]);
        edit_set(g_ed_power, b);
        return TRUE;
    }
    switch (id) {
    case HID_PW_START: pw_start(); return TRUE;
    case HID_PW_CANCEL: pw_cancel(); return TRUE;
    case HID_PW_LOCK: power_lock(); return TRUE;
    case HID_PW_SLEEP: if (confirm_box(T2(L"Put the computer to sleep now?", L"Tidurkan komputer sekarang?"))) power_sleep_now(); return TRUE;
    case HID_PW_HIB: if (confirm_box(T2(L"Hibernate the computer now?", L"Hibernate komputer sekarang?"))) power_hibernate_now(); return TRUE;
    default: return FALSE;
    }
}
