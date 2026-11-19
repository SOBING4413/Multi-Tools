/* about.c - About page: version, credits, links */
#include "ui/internal.h"
#include "ui/pages/pages.h"
#include "modules/sys.h"

void pg_about_paint(void)
{
    RECT c = g_cont, card, t, r;
    int cw = MT_MIN(c.right - c.left, S(760)), x = c.left, y = c.top, gap = S(16);
    int ch = MT_MIN(c.bottom - c.top, S(500));
    wchar_t os[256], line[256];
    card = rc_make(x, y, x + cw, y + ch);
    w_card_rc(&card);
    gfx_logo(card.left + S(28), card.top + S(28), S(96));
    t = rc_make(card.left + S(142), card.top + S(30), card.right - S(24), card.top + S(70));
    gfx_text_rc(&t, MT_NAME, F_TITLE, g_th.text, DT_SINGLELINE | DT_VCENTER);
    wfmt(line, 256, L"Version %ls  %ls  native C / Win32 build", MT_VERSION, U_DOT);
    t = rc_make(card.left + S(142), card.top + S(70), card.right - S(24), card.top + S(94));
    gfx_text_rc(&t, line, F_UI, g_th.accent, DT_SINGLELINE | DT_VCENTER);
    t = rc_make(card.left + S(142), card.top + S(96), card.right - S(24), card.top + S(124));
    gfx_text_rc(&t, T2(L"All-in-one Windows toolkit: system info, network, security, cleanup, drivers and malware scan.", L"Toolkit Windows serba ada: info sistem, jaringan, keamanan, pembersihan, driver, dan pemindai malware."), F_UI_S, g_th.dim, DT_WORDBREAK);
    y = card.top + S(150);
    gfx_rect(card.left + S(28), y, cw - S(56), 1, g_th.border, 255);
    y += S(18);
    t = rc_make(card.left + S(28), y, card.left + S(200), y + S(22));
    gfx_text_rc(&t, T2(L"Credits", L"Kredit"), F_UI_SB, g_th.muted, DT_SINGLELINE | DT_VCENTER);
    y += S(26);
    wfmt(line, 256, T2(L"Developed by %ls", L"Dikembangkan oleh %ls"), MT_AUTHOR);
    t = rc_make(card.left + S(28), y, card.right - S(28), y + S(26));
    gfx_text_rc(&t, line, F_H2, g_th.text, DT_SINGLELINE | DT_VCENTER);
    y += S(28);
    wfmt(line, 256, T2(L"Lead developer: %ls", L"Developer utama: %ls"), MT_DEVELOPER);
    t = rc_make(card.left + S(28), y, card.right - S(28), y + S(22));
    gfx_text_rc(&t, line, F_UI, g_th.dim, DT_SINGLELINE | DT_VCENTER);
    y += S(24);
    wfmt(line, 256, T2(L"Based on the original Python MultiTool by %ls (open source)", L"Berdasarkan MultiTool Python asli oleh %ls (open source)"), MT_ORIGIN);
    t = rc_make(card.left + S(28), y, card.right - S(28), y + S(22));
    gfx_text_rc(&t, line, F_UI, g_th.dim, DT_SINGLELINE | DT_VCENTER);
    y += S(40);
    r = rc_make(card.left + S(28), y, card.left + S(28) + S(160), y + S(40));
    w_btn(&r, L"GitHub", ICO_GLOBE, HID_ABOUT_GH, BTN_SOFT, 0);
    r = rc_make(card.left + S(28) + S(172), y, card.left + S(28) + S(172) + S(160), y + S(40));
    w_btn(&r, L"Discord", ICO_USER, HID_ABOUT_DC, BTN_SOFT, 0);
    y += S(62);
    gfx_rect(card.left + S(28), y, cw - S(56), 1, g_th.border, 255);
    y += S(16);
    sys_os_string(os, 256);
    wfmt(line, 256, L"%ls", os);
    t = rc_make(card.left + S(28), y, card.right - S(28), y + S(20));
    gfx_text_rc(&t, line, F_UI_S, g_th.muted, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    y += S(22);
    wfmt(line, 256, L"Build %hs %hs  %ls  %ls", __DATE__, __TIME__, U_DOT, g_admin ? T2(L"Running as Administrator", L"Berjalan sebagai Administrator") : T2(L"Standard user", L"User standar"));
    t = rc_make(card.left + S(28), y, card.right - S(28), y + S(20));
    gfx_text_rc(&t, line, F_UI_S, g_th.muted, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    y += S(22);
    wfmt(line, 256, L"%ls %ls %ls  %ls", U_COPY, L"2026", MT_AUTHOR, T2(L"Released under the GPL-3.0 license (see LICENSE).", L"Dirilis dengan lisensi GPL-3.0 (lihat LICENSE)."));
    t = rc_make(card.left + S(28), y, card.right - S(28), y + S(20));
    gfx_text_rc(&t, line, F_UI_S, g_th.muted, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    (void)gap;
}

BOOL pg_about_click(int id)
{
    if (id == HID_ABOUT_GH) { open_url(MT_GITHUB); return TRUE; }
    if (id == HID_ABOUT_DC) { open_url(MT_DISCORD); return TRUE; }
    return FALSE;
}
