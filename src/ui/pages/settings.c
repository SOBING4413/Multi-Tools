/* settings.c - Settings page: accent color, animations, auto-refresh, language */
#include "ui/internal.h"
#include "ui/pages/pages.h"
#include "core/settings.h"

void pg_settings_paint(void)
{
    RECT c = g_cont, a, b, t, r;
    int gap = S(16), lw = (c.right - c.left - gap) * 42 / 100, i, y, ch = MT_MIN(c.bottom - c.top, S(470));
    a = rc_make(c.left, c.top, c.left + lw, c.top + ch);
    b = rc_make(c.left + lw + gap, c.top, c.right, c.top + ch);
    w_card_rc(&a);
    t = rc_make(a.left + S(22), a.top + S(16), a.right - S(22), a.top + S(40));
    gfx_text_rc(&t, T2(L"Appearance", L"Tampilan"), F_H2, g_th.text, DT_SINGLELINE | DT_VCENTER);
    t = rc_make(a.left + S(22), a.top + S(52), a.right - S(22), a.top + S(72));
    gfx_text_rc(&t, T2(L"Accent color", L"Warna aksen"), F_UI_S, g_th.dim, DT_SINGLELINE | DT_VCENTER);
    for (i = 0; i < 6; i++) {
        int sw = S(40), x = a.left + S(22) + i * (sw + S(12));
        RECT sr = rc_make(x, a.top + S(80), x + sw, a.top + S(80) + sw);
        gfx_circle((float)(x + sw / 2), (float)(a.top + S(80) + sw / 2), (float)(sw / 2) - 2, g_theme_accent[i], 255);
        if (g_set.theme == i) gfx_ring((float)(x + sw / 2), (float)(a.top + S(80) + sw / 2), (float)(sw / 2) + 3, 2.0f, 0, 1, g_th.text, 255, 0);
        hot_add(&sr, HID_THEME0 + i);
    }
    y = a.top + S(150);
    t = rc_make(a.left + S(22), y, a.right - S(80), y + S(24));
    gfx_text_rc(&t, T2(L"Animations", L"Animasi"), F_UI, g_th.text, DT_SINGLELINE | DT_VCENTER);
    r = rc_make(a.right - S(78), y, a.right - S(22), y + S(26));
    w_toggle(&r, g_set.animations, HID_ANIM);
    y += S(44);
    t = rc_make(a.left + S(22), y, a.right - S(80), y + S(24));
    gfx_text_rc(&t, T2(L"Auto-refresh live data", L"Auto-refresh data langsung"), F_UI, g_th.text, DT_SINGLELINE | DT_VCENTER);
    r = rc_make(a.right - S(78), y, a.right - S(22), y + S(26));
    w_toggle(&r, g_set.auto_refresh, HID_AUTOREF);
    y += S(62);
    r = rc_make(a.left + S(22), y, a.right - S(22), y + S(40));
    w_btn(&r, T2(L"Open data folder", L"Buka folder data"), ICO_FOLDER, HID_OPENDATA, BTN_SOFT, 0);
    y += S(52);
    if (!g_admin) {
        r = rc_make(a.left + S(22), y, a.right - S(22), y + S(40));
        w_btn(&r, T2(L"Restart as Administrator", L"Jalankan ulang sebagai Administrator"), ICO_LOCK, HID_ADMIN, BTN_PRIMARY, 0);
    }

    w_card_rc(&b);
    t = rc_make(b.left + S(22), b.top + S(16), b.right - S(22), b.top + S(40));
    gfx_text_rc(&t, TX(K_menu_language), F_H2, g_th.text, DT_SINGLELINE | DT_VCENTER);
    t = rc_make(b.left + S(22), b.top + S(46), b.right - S(22), b.top + S(68));
    gfx_text_rc(&t, T2(L"Menu names follow your language. Other texts are available in English and Indonesian.", L"Nama menu mengikuti bahasa pilihanmu. Teks lain tersedia dalam bahasa Inggris dan Indonesia."), F_UI_S, g_th.dim, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    {
        int cols = 3, gw = (b.right - b.left - S(44) - (cols - 1) * S(10)) / cols, gh = S(48);
        for (i = 0; i < LANG_COUNT; i++) {
            int cx = b.left + S(22) + (i % cols) * (gw + S(10)), cy = b.top + S(82) + (i / cols) * (gh + S(10));
            RECT lr = rc_make(cx, cy, cx + gw, cy + gh), nr, er;
            float hov = g_hv[HID_LANG0 + i];
            BOOL act = (i == g_lang);
            gfx_rrect((float)lr.left, (float)lr.top, (float)gw, (float)gh, (float)S(10), act ? rgb_mix(g_th.card, g_th.accent, 0.2f) : rgb_mix(g_th.card, g_th.card2, 0.3f + hov * 0.7f), 255);
            gfx_rrect_stroke((float)lr.left, (float)lr.top, (float)gw, (float)gh, (float)S(10), 1.0f, act ? g_th.accent : rgb_mix(g_th.border, g_th.border2, hov), 255);
            nr = rc_make(lr.left + S(12), lr.top + S(6), lr.right - S(8), lr.top + S(26));
            gfx_text_rc(&nr, g_lang_native[i], F_UI_B, act ? g_th.accent : g_th.text, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            er = rc_make(lr.left + S(12), lr.top + S(25), lr.right - S(8), lr.top + S(43));
            gfx_text_rc(&er, g_lang_name[i], F_UI_XS, g_th.muted, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            hot_add(&lr, HID_LANG0 + i);
        }
    }
}

BOOL pg_settings_click(int id)
{
    if (id >= HID_THEME0 && id < HID_THEME0 + 6) { shell_set_theme(id - HID_THEME0); return TRUE; }
    if (id >= HID_LANG0 && id < HID_LANG0 + LANG_COUNT) {
        g_set.lang = id - HID_LANG0;
        lang_set(g_set.lang);
        settings_save();
        pages_lang_changed();
        shell_relayout();
        return TRUE;
    }
    switch (id) {
    case HID_ANIM: g_set.animations = !g_set.animations; settings_save(); g_dirty = TRUE; return TRUE;
    case HID_AUTOREF: g_set.auto_refresh = !g_set.auto_refresh; settings_save(); g_dirty = TRUE; return TRUE;
    case HID_OPENDATA: ensure_dir(g_data_dir); shell_open(g_data_dir, NULL); return TRUE;
    default: return FALSE;
    }
}
