/* widgets.c - hot-spot registry and drawing widgets (buttons, cards, toggles, badges, gauges) */
#include "ui/ui.h"
#include "core/util.h"
#include "core/settings.h"

/* ============================ hot spots ============================ */
typedef struct Hot { RECT r; int id; } Hot;
static Hot g_hot[600];
static int g_nhot = 0;
int   g_hover = -1, g_pressed = -1;
float g_hv[HOT_MAX];

void hot_reset(void) { g_nhot = 0; }

void hot_add(const RECT* r, int id)
{
    if (g_nhot < (int)MT_ARRAY(g_hot)) { g_hot[g_nhot].r = *r; g_hot[g_nhot].id = id; g_nhot++; }
}

int hot_at(int x, int y)
{
    int i;
    POINT p;
    p.x = x; p.y = y;
    for (i = g_nhot - 1; i >= 0; i--) if (PtInRect(&g_hot[i].r, p)) return g_hot[i].id;
    return -1;
}

BOOL hot_tick(void)
{
    int i;
    BOOL moved = FALSE;
    for (i = 0; i < HOT_MAX; i++) {
        float target = (i == g_hover) ? 1.0f : 0.0f;
        float d = target - g_hv[i];
        if (d > 0.001f || d < -0.001f) {
            if (!g_set.animations) g_hv[i] = target;
            else g_hv[i] += d * 0.35f;
            moved = TRUE;
        } else g_hv[i] = target;
    }
    return moved;
}

static float hv(int id) { return (id >= 0 && id < HOT_MAX) ? g_hv[id] : 0.0f; }

/* ============================ colors ============================ */
Rgb sev_color(int sev)
{
    switch (sev) {
    case 0: return g_th.ok;
    case 1: return g_th.info;
    case 2: return g_th.warn;
    case 3: return RGBX(0xFB, 0x92, 0x3C);
    default: return g_th.bad;
    }
}

int w_text_width(const wchar_t* s, int font) { return gfx_text_w(s, font); }

int w_btn_width(const wchar_t* text, BOOL icon)
{
    return gfx_text_w(text, F_UI_B) + S(28) + (icon ? S(26) : 0);
}

/* ============================ widgets ============================ */
void w_card(float x, float y, float w, float h)
{
    gfx_rrect_grad(x, y, w, h, (float)S(12), rgb_mix(g_th.card, g_th.card2, 0.35f), g_th.card, 255);
    gfx_rrect_stroke(x, y, w, h, (float)S(12), 1.0f, g_th.border, 255);
}

void w_card_rc(const RECT* r) { w_card((float)r->left, (float)r->top, (float)(r->right - r->left), (float)(r->bottom - r->top)); }

void w_btn(const RECT* r, const wchar_t* text, int icon, int id, int style, int flags)
{
    float x = (float)r->left, y = (float)r->top, w = (float)(r->right - r->left), h = (float)(r->bottom - r->top);
    float rad = (float)S(8), hov = (flags & BF_DISABLED) ? 0.0f : hv(id);
    Rgb fill, txt, border;
    RECT tr = *r;
    int iw = 0;
    switch (style) {
    case BTN_PRIMARY: fill = rgb_mix(g_th.accent, RGBX(255, 255, 255), hov * 0.18f); txt = RGBX(0x06, 0x12, 0x0F); border = fill; break;
    case BTN_DANGER:  fill = rgb_mix(rgb_mix(g_th.card2, g_th.bad, 0.20f), g_th.bad, hov * 0.30f); txt = rgb_mix(g_th.bad, RGBX(255, 255, 255), 0.25f); border = rgb_mix(g_th.border2, g_th.bad, 0.5f); break;
    case BTN_GHOST:   fill = rgb_mix(g_th.card, g_th.card2, hov); txt = rgb_mix(g_th.dim, g_th.text, hov); border = rgb_mix(g_th.card, g_th.border2, hov); break;
    default:          fill = rgb_mix(g_th.card2, g_th.border2, hov * 0.8f); txt = g_th.text; border = rgb_mix(g_th.border, g_th.border2, 0.6f + hov * 0.4f); break;
    }
    if (flags & BF_ACTIVE) { fill = rgb_mix(g_th.card2, g_th.accent, 0.22f); txt = g_th.accent; border = g_th.accent; }
    if (flags & BF_DISABLED) { fill = rgb_mix(g_th.card, g_th.card2, 0.4f); txt = g_th.muted; border = g_th.border; }
    gfx_rrect(x, y, w, h, rad, fill, 255);
    if (style != BTN_PRIMARY || (flags & BF_DISABLED)) gfx_rrect_stroke(x, y, w, h, rad, 1.0f, border, 255);
    if (icon >= 0) {
        float is = (float)S(16);
        iw = S(16) + S(8);
        {
            int tw = gfx_text_w(text, F_UI_B);
            float cx = (float)(r->left + (r->right - r->left - tw - iw) / 2) + is / 2;
            if (!text || !*text) cx = (x + w / 2);
            gfx_icon(icon, cx, y + h / 2, is, txt);
            tr.left = (int)(cx + is / 2 + S(8));
            tr.right = r->right;
            if (text && *text) { tr.left = (int)(cx + is / 2 + S(8)); gfx_text_rc(&tr, text, F_UI_B, txt, DT_SINGLELINE | DT_VCENTER | DT_LEFT); }
        }
    } else gfx_text_rc(&tr, text, F_UI_B, txt, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_END_ELLIPSIS);
    if (!(flags & BF_DISABLED)) hot_add(r, id);
}

void w_pill(const RECT* r, const wchar_t* text, int id, BOOL active, int font)
{
    float x = (float)r->left, y = (float)r->top, w = (float)(r->right - r->left), h = (float)(r->bottom - r->top);
    float hov = hv(id);
    Rgb fill, txt, border;
    RECT tr = *r;
    if (active) { fill = rgb_mix(g_th.card, g_th.accent, 0.20f); txt = g_th.accent; border = g_th.accent; }
    else { fill = rgb_mix(g_th.card, g_th.card2, 0.4f + hov * 0.6f); txt = rgb_mix(g_th.dim, g_th.text, hov); border = rgb_mix(g_th.border, g_th.border2, hov); }
    gfx_rrect(x, y, w, h, h / 2, fill, 255);
    gfx_rrect_stroke(x, y, w, h, h / 2, 1.0f, border, 255);
    tr.left += S(6); tr.right -= S(6);
    gfx_text_rc(&tr, text, font, txt, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_END_ELLIPSIS);
    hot_add(r, id);
}

void w_toggle(const RECT* r, BOOL on, int id)
{
    float x = (float)r->left, y = (float)r->top, w = (float)(r->right - r->left), h = (float)(r->bottom - r->top);
    Rgb bg = on ? g_th.accent : g_th.border2;
    float knob = h - 6.0f;
    gfx_rrect(x, y, w, h, h / 2, bg, 255);
    gfx_circle(on ? x + w - h / 2 : x + h / 2, y + h / 2, knob / 2, on ? RGBX(0x06, 0x12, 0x0F) : g_th.text, 255);
    hot_add(r, id);
}

void w_badge(const RECT* r, const wchar_t* text, Rgb c)
{
    float x = (float)r->left, y = (float)r->top, w = (float)(r->right - r->left), h = (float)(r->bottom - r->top);
    gfx_rrect(x, y, w, h, h / 2, c, 38);
    gfx_rrect_stroke(x, y, w, h, h / 2, 1.0f, c, 110);
    gfx_text_rc(r, text, F_UI_SB, c, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_END_ELLIPSIS);
}

int w_badge_at(int x, int y, const wchar_t* text, Rgb c)
{
    int tw = gfx_text_w(text, F_UI_SB) + S(20), h = S(22);
    RECT r;
    r.left = x; r.top = y; r.right = x + tw; r.bottom = y + h;
    w_badge(&r, text, c);
    return tw;
}

void w_progress(const RECT* r, double frac, Rgb c)
{
    float x = (float)r->left, y = (float)r->top, w = (float)(r->right - r->left), h = (float)(r->bottom - r->top);
    gfx_rrect(x, y, w, h, h / 2, g_th.border, 255);
    if (frac < 0) {   /* indeterminate: moving block */
        float t = (float)((GetTickCount() % 1400) / 1400.0);
        float bw = w * 0.3f, bx = x + (w + bw) * t - bw;
        gfx_clip((int)x, (int)y - 1, (int)(x + w), (int)(y + h) + 1);
        gfx_rrect(bx, y, bw, h, h / 2, c, 255);
        gfx_unclip();
        return;
    }
    if (frac > 1) frac = 1;
    if (frac > 0.0) gfx_rrect(x, y, (float)MT_MAX(w * (float)frac, h), h, h / 2, c, 255);
}

void w_input_frame(const RECT* r, BOOL focus)
{
    float x = (float)r->left, y = (float)r->top, w = (float)(r->right - r->left), h = (float)(r->bottom - r->top);
    gfx_rrect(x, y, w, h, (float)S(8), g_th.logbg, 255);
    gfx_rrect_stroke(x, y, w, h, (float)S(8), 1.0f, focus ? g_th.accent : g_th.border2, 255);
}

void w_gauge(float cx, float cy, float r, float thick, double frac, Rgb c)
{
    gfx_ring(cx, cy, r, thick, 0, 1, g_th.border, 255, 0);
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    if (frac > 0.004) gfx_ring(cx, cy, r, thick, 0.0f, (float)frac, c, 255, 1);
}


void w_grid_lines(const RECT* r, int lines)
{
    int i;
    for (i = 0; i <= lines; i++) {
        int y = r->top + (r->bottom - r->top) * i / lines;
        gfx_rect(r->left, y, r->right - r->left, 1, g_th.border, 200);
    }
}
