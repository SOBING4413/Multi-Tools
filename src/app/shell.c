/* shell.c - application shell: sidebar navigation, top bar, page navigation, theme switching */
#include "app/shell.h"
#include "ui/ui.h"
#include "ui/internal.h"
#include "core/util.h"
#include "core/settings.h"

static RECT rc_make_shell(int l, int t, int r, int b) { RECT x; x.left = l; x.top = t; x.right = r; x.bottom = b; return x; }

BOOL shell_confirm(const wchar_t* text)
{
    return MessageBoxW(g_hwnd, text, MT_NAME, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES;
}

static int g_nav_scroll = 0;

/* ============================ shell: layout ============================ */
int shell_sidebar_w(void) { return S(236); }
int shell_topbar_h(void) { return S(80); }

void shell_compute_layout(void)
{
    g_cont = rc_make_shell(shell_sidebar_w() + S(28), shell_topbar_h() + S(6), g_cw - S(28), g_ch - S(22));
}

void shell_relayout(void)
{
    shell_compute_layout();
    pages_layout();
    g_dirty = TRUE;
}

void shell_navigate(int page)
{
    if (page < 0 || page >= PG_COUNT) return;
    if (page == g_page) { g_dirty = TRUE; return; }
    g_page = page;
    shell_compute_layout();
    pages_page_changed(page);
    g_dirty = TRUE;
}

void shell_set_theme(int idx)
{
    g_set.theme = idx;
    theme_apply(idx);
    settings_save();
    log_restyle();
    if (g_hwnd) app_apply_dark_titlebar(g_hwnd);
    g_dirty = TRUE;
}

/* ============================ shell: sidebar ============================ */
typedef struct NavRow { int is_item; int page; const wchar_t* text; } NavRow;

static int build_nav(NavRow* r)
{
    int n = 0;
    r[n].is_item = 0; r[n++].text = T2(L"OVERVIEW", L"RINGKASAN");
    r[n].is_item = 1; r[n].page = PG_DASH; r[n++].text = page_title(PG_DASH);
    r[n].is_item = 1; r[n].page = PG_MONITOR; r[n++].text = page_title(PG_MONITOR);
    r[n].is_item = 0; r[n++].text = T2(L"SYSTEM", L"SISTEM");
    r[n].is_item = 1; r[n].page = PG_POWER; r[n++].text = page_title(PG_POWER);
    r[n].is_item = 1; r[n].page = PG_SYSTEM; r[n++].text = page_title(PG_SYSTEM);
    r[n].is_item = 1; r[n].page = PG_PROCS; r[n++].text = page_title(PG_PROCS);
    r[n].is_item = 1; r[n].page = PG_NETWORK; r[n++].text = page_title(PG_NETWORK);
    r[n].is_item = 0; r[n++].text = T2(L"PROTECTION", L"PERLINDUNGAN");
    r[n].is_item = 1; r[n].page = PG_SECURITY; r[n++].text = page_title(PG_SECURITY);
    r[n].is_item = 1; r[n].page = PG_MALWARE; r[n++].text = page_title(PG_MALWARE);
    r[n].is_item = 1; r[n].page = PG_DRIVERS; r[n++].text = page_title(PG_DRIVERS);
    r[n].is_item = 0; r[n++].text = T2(L"TOOLS", L"ALAT");
    r[n].is_item = 1; r[n].page = PG_CLEANUP; r[n++].text = page_title(PG_CLEANUP);
    r[n].is_item = 1; r[n].page = PG_TOOLS; r[n++].text = page_title(PG_TOOLS);
    r[n].is_item = 1; r[n].page = PG_SETTINGS; r[n++].text = page_title(PG_SETTINGS);
    r[n].is_item = 1; r[n].page = PG_ABOUT; r[n++].text = page_title(PG_ABOUT);
    return n;
}

static void draw_sidebar(void)
{
    int sw = shell_sidebar_w(), n, i, y, item_h = S(33), cap_h = S(28);
    NavRow rows[24];
    int top = S(84), bottom = g_ch - S(92), total = 0;
    RECT t;
    wchar_t buf[96];
    gfx_rect(0, 0, sw, g_ch, g_th.side, 255);
    gfx_rect(sw - 1, 0, 1, g_ch, g_th.border, 255);

    /* brand */
    gfx_logo(S(18), S(18), S(40));
    t = rc_make_shell(S(68), S(18), sw - S(10), S(40));
    gfx_text_rc(&t, MT_NAME, F_H2, g_th.text, DT_SINGLELINE | DT_VCENTER);
    wfmt(buf, 96, L"v%ls  %ls  %ls", MT_VERSION, U_DOT, MT_AUTHOR);
    t = rc_make_shell(S(68), S(38), sw - S(8), S(58));
    gfx_text_rc(&t, buf, F_UI_XS, g_th.muted, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

    /* nav */
    n = build_nav(rows);
    for (i = 0; i < n; i++) total += rows[i].is_item ? item_h : cap_h;
    { int maxs = total - (bottom - top); if (maxs < 0) maxs = 0; if (g_nav_scroll > maxs) g_nav_scroll = maxs; if (g_nav_scroll < 0) g_nav_scroll = 0; }
    gfx_clip(0, top, sw - 1, bottom);
    y = top - g_nav_scroll;
    for (i = 0; i < n; i++) {
        if (!rows[i].is_item) {
            t = rc_make_shell(S(22), y + S(8), sw - S(10), y + cap_h);
            gfx_text_rc(&t, rows[i].text, F_UI_XS, g_th.muted, DT_SINGLELINE | DT_VCENTER);
            y += cap_h;
        } else {
            RECT r = rc_make_shell(S(10), y, sw - S(10), y + item_h - S(2)), vis, clip = rc_make_shell(0, top, sw, bottom);
            int id = HID_NAV0 + rows[i].page;
            float hov = g_hv[id];
            BOOL act = (g_page == rows[i].page);
            if (act) {
                gfx_rrect((float)r.left, (float)r.top, (float)(r.right - r.left), (float)(r.bottom - r.top), (float)S(8), g_th.accent, 30);
                gfx_rrect((float)r.left, (float)r.top + S(7), (float)S(3), (float)(r.bottom - r.top - S(14)), 1.5f, g_th.accent, 255);
            } else if (hov > 0.01f) {
                gfx_rrect((float)r.left, (float)r.top, (float)(r.right - r.left), (float)(r.bottom - r.top), (float)S(8), g_th.card2, (int)(200 * hov));
            }
            gfx_icon(page_icon(rows[i].page), (float)(r.left + S(24)), (float)((r.top + r.bottom) / 2), (float)S(17), act ? g_th.accent : rgb_mix(g_th.dim, g_th.text, hov));
            t = rc_make_shell(r.left + S(44), r.top, r.right - S(6), r.bottom);
            gfx_text_rc(&t, rows[i].text, act ? F_UI_B : F_UI, act ? g_th.text : rgb_mix(g_th.dim, g_th.text, hov), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            if (IntersectRect(&vis, &r, &clip)) hot_add(&vis, id);
            y += item_h;
        }
    }
    gfx_unclip();

    /* footer: admin chip + exit */
    {
        RECT chip = rc_make_shell(S(14), g_ch - S(88), sw - S(14), g_ch - S(54));
        Rgb col = g_admin ? g_th.ok : g_th.warn;
        gfx_rrect((float)chip.left, (float)chip.top, (float)(chip.right - chip.left), (float)(chip.bottom - chip.top), (float)S(9), col, 26);
        gfx_circle((float)(chip.left + S(16)), (float)((chip.top + chip.bottom) / 2), (float)S(4), col, 255);
        t = rc_make_shell(chip.left + S(30), chip.top, chip.right - S(8), chip.bottom);
        gfx_text_rc(&t, g_admin ? T2(L"Administrator", L"Administrator") : T2(L"Standard user", L"User standar"), F_UI_SB, col, DT_SINGLELINE | DT_VCENTER);
        if (!g_admin) hot_add(&chip, HID_ADMIN);
        {
            RECT r = rc_make_shell(S(10), g_ch - S(46), sw - S(10), g_ch - S(12));
            float hov = g_hv[HID_EXIT];
            if (hov > 0.01f) gfx_rrect((float)r.left, (float)r.top, (float)(r.right - r.left), (float)(r.bottom - r.top), (float)S(8), g_th.bad, (int)(34 * hov));
            gfx_icon(ICO_EXIT, (float)(r.left + S(24)), (float)((r.top + r.bottom) / 2), (float)S(16), rgb_mix(g_th.dim, g_th.bad, hov));
            t = rc_make_shell(r.left + S(44), r.top, r.right - S(6), r.bottom);
            gfx_text_rc(&t, TX(K_menu_exit), F_UI, rgb_mix(g_th.dim, g_th.text, hov), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            hot_add(&r, HID_EXIT);
        }
    }
}

static void draw_topbar(void)
{
    int sw = shell_sidebar_w(), tb = shell_topbar_h();
    RECT t;
    SYSTEMTIME st;
    wchar_t tm[32], dt[64];
    static const wchar_t* days_en[] = { L"Sun", L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat" };
    static const wchar_t* days_id[] = { L"Min", L"Sen", L"Sel", L"Rab", L"Kam", L"Jum", L"Sab" };
    gfx_rect(sw, 0, g_cw - sw, tb, g_th.bg, 255);
    t = rc_make_shell(sw + S(28), S(12), g_cw - S(220), S(52));
    gfx_text_rc(&t, page_title(g_page), F_TITLE, g_th.text, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    t = rc_make_shell(sw + S(30), S(50), g_cw - S(220), S(72));
    gfx_text_rc(&t, page_subtitle(g_page), F_UI_S, g_th.dim, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    GetLocalTime(&st);
    wfmt(tm, 32, L"%02d:%02d", st.wHour, st.wMinute);
    wfmt(dt, 64, L"%ls, %02d/%02d/%04d", T2(days_en[st.wDayOfWeek], days_id[st.wDayOfWeek]), st.wDay, st.wMonth, st.wYear);
    t = rc_make_shell(g_cw - S(210), S(14), g_cw - S(28), S(42));
    gfx_text_rc(&t, tm, F_NUM, g_th.text, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
    t = rc_make_shell(g_cw - S(210), S(44), g_cw - S(28), S(64));
    gfx_text_rc(&t, dt, F_UI_S, g_th.dim, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
    gfx_rect(sw + S(28), tb - S(1), g_cw - sw - S(56), 1, g_th.border, 255);
}

void shell_draw(void)
{
    draw_sidebar();
    draw_topbar();
}

BOOL shell_nav_wheel(int x, int delta)
{
    if (x >= shell_sidebar_w()) return FALSE;
    g_nav_scroll -= delta / 120 * S(48);
    g_dirty = TRUE;
    return TRUE;
}

void shell_click(int id)
{
    if (id >= HID_NAV0 && id < HID_NAV0 + PG_COUNT) { shell_navigate(id - HID_NAV0); return; }
    if (id == HID_EXIT) { PostMessageW(g_hwnd, WM_CLOSE, 0, 0); return; }
    if (id == HID_ADMIN) {
        if (shell_confirm(T2(L"Restart MultiTool with Administrator rights?", L"Jalankan ulang MultiTool dengan hak Administrator?"))) {
            if (relaunch_as_admin()) { pages_shutdown(); DestroyWindow(g_hwnd); }
        }
        return;
    }
    pages_click(id);
}
