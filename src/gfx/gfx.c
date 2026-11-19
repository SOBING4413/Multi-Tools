/* gfx.c */
#include "gfx/gfx.h"
#include "core/util.h"
#include "resource.h"

/* ============================ theme ============================ */
Theme g_th;
const wchar_t* const g_theme_names[6] = { L"Green", L"Cyan", L"Yellow", L"Red", L"Magenta", L"White" };
const Rgb g_theme_accent[6] = {
    RGBX(0x34, 0xD3, 0x99), RGBX(0x22, 0xD3, 0xEE), RGBX(0xFB, 0xBF, 0x24),
    RGBX(0xF8, 0x71, 0x71), RGBX(0xE8, 0x79, 0xF9), RGBX(0xE5, 0xE7, 0xEB)
};

Rgb rgb_mix(Rgb a, Rgb b, float t)
{
    int ar = (a >> 16) & 255, ag = (a >> 8) & 255, ab = a & 255;
    int br = (b >> 16) & 255, bg = (b >> 8) & 255, bb = b & 255;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    return RGBX((int)(ar + (br - ar) * t + 0.5f), (int)(ag + (bg - ag) * t + 0.5f), (int)(ab + (bb - ab) * t + 0.5f));
}

void theme_apply(int idx)
{
    if (idx < 0 || idx > 5) idx = 0;
    g_th.bg      = RGBX(0x0A, 0x0E, 0x13);
    g_th.side    = RGBX(0x0E, 0x13, 0x1A);
    g_th.card    = RGBX(0x12, 0x19, 0x22);
    g_th.card2   = RGBX(0x18, 0x21, 0x2C);
    g_th.border  = RGBX(0x21, 0x2C, 0x39);
    g_th.border2 = RGBX(0x2D, 0x3A, 0x4B);
    g_th.text    = RGBX(0xE7, 0xED, 0xF4);
    g_th.dim     = RGBX(0x8C, 0x99, 0xAA);
    g_th.muted   = RGBX(0x5A, 0x67, 0x79);
    g_th.logbg   = RGBX(0x0B, 0x10, 0x16);
    g_th.ok      = RGBX(0x34, 0xD3, 0x99);
    g_th.warn    = RGBX(0xFB, 0xBF, 0x24);
    g_th.bad     = RGBX(0xF8, 0x71, 0x71);
    g_th.info    = RGBX(0x60, 0xA5, 0xFA);
    g_th.accent  = g_theme_accent[idx];
}

/* ============================ surface ============================ */
int g_dpi = 96;
uint32_t* g_px = NULL;
int g_sw = 0, g_sh = 0;
HDC g_dc = NULL;
HFONT g_font[F_COUNT];
static HBITMAP g_bmp = NULL, g_oldbmp = NULL;
static int g_gdi_dirty = 0;
static int cl_l = 0, cl_t = 0, cl_r = 0, cl_b = 0;   /* clip rect: [l,r) x [t,b) */

static HICON g_icons[4];
static int   g_icon_sz[4];

void gfx_init(void)
{
    HDC scr = GetDC(NULL);
    g_dc = CreateCompatibleDC(scr);
    ReleaseDC(NULL, scr);
    SetBkMode(g_dc, TRANSPARENT);
    SetTextAlign(g_dc, TA_LEFT | TA_TOP);
    memset(g_font, 0, sizeof(g_font));
}

void gfx_resize(int w, int h)
{
    BITMAPINFO bi;
    void* bits = NULL;
    HBITMAP nb;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (w == g_sw && h == g_sh && g_bmp) return;
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    nb = CreateDIBSection(g_dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!nb) return;
    if (g_bmp) {
        SelectObject(g_dc, g_oldbmp);
        DeleteObject(g_bmp);
    }
    g_bmp = nb;
    g_oldbmp = (HBITMAP)SelectObject(g_dc, g_bmp);
    g_px = (uint32_t*)bits;
    g_sw = w;
    g_sh = h;
    cl_l = 0; cl_t = 0; cl_r = w; cl_b = h;
}

void gfx_present(HDC dst, int x, int y, int w, int h)
{
    GdiFlush();
    g_gdi_dirty = 0;
    BitBlt(dst, x, y, w, h, g_dc, x, y, SRCCOPY);
}

void gfx_shutdown(void)
{
    int i;
    for (i = 0; i < F_COUNT; i++) if (g_font[i]) DeleteObject(g_font[i]);
    for (i = 0; i < 4; i++) if (g_icons[i]) DestroyIcon(g_icons[i]);
}

/* ============================ fonts ============================ */
static int CALLBACK font_probe(const LOGFONTW* lf, const TEXTMETRICW* tm, DWORD type, LPARAM lp)
{
    (void)lf; (void)tm; (void)type;
    *(int*)lp = 1;
    return 0;
}

static BOOL font_exists(const wchar_t* face)
{
    LOGFONTW lf;
    int found = 0;
    HDC dc = GetDC(NULL);
    ZeroMemory(&lf, sizeof(lf));
    lf.lfCharSet = DEFAULT_CHARSET;
    wcsncpy(lf.lfFaceName, face, LF_FACESIZE - 1);
    EnumFontFamiliesExW(dc, &lf, (FONTENUMPROCW)font_probe, (LPARAM)&found, 0);
    ReleaseDC(NULL, dc);
    return found != 0;
}

static HFONT mkfont(const wchar_t* face, int px, int weight)
{
    return CreateFontW(-S(px), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
}

void gfx_set_dpi(int dpi)
{
    int i;
    const wchar_t* ui = L"Segoe UI";
    const wchar_t* light = font_exists(L"Segoe UI Light") ? L"Segoe UI Light" : L"Segoe UI";
    const wchar_t* mono = font_exists(L"Cascadia Mono") ? L"Cascadia Mono" : (font_exists(L"Consolas") ? L"Consolas" : L"Courier New");
    if (dpi < 48) dpi = 96;
    g_dpi = dpi;
    for (i = 0; i < F_COUNT; i++) if (g_font[i]) { DeleteObject(g_font[i]); g_font[i] = NULL; }
    g_font[F_UI]    = mkfont(ui, 13, FW_NORMAL);
    g_font[F_UI_B]  = mkfont(ui, 13, FW_SEMIBOLD);
    g_font[F_UI_S]  = mkfont(ui, 12, FW_NORMAL);
    g_font[F_UI_SB] = mkfont(ui, 12, FW_SEMIBOLD);
    g_font[F_UI_XS] = mkfont(ui, 11, FW_NORMAL);
    g_font[F_TITLE] = mkfont(ui, 26, FW_SEMIBOLD);
    g_font[F_H2]    = mkfont(ui, 16, FW_SEMIBOLD);
    g_font[F_BIG]   = mkfont(light, 34, FW_LIGHT);
    g_font[F_NUM]   = mkfont(ui, 22, FW_SEMIBOLD);
    g_font[F_MONO]  = mkfont(mono, 13, FW_NORMAL);
    for (i = 0; i < 4; i++) if (g_icons[i]) { DestroyIcon(g_icons[i]); g_icons[i] = NULL; g_icon_sz[i] = 0; }
}

/* ============================ pixel plumbing ============================ */
static __inline void sync_gdi(void)
{
    if (g_gdi_dirty) { GdiFlush(); g_gdi_dirty = 0; }
}

static __inline void blend(uint32_t* p, Rgb c, int a)
{
    uint32_t d;
    int dr, dg, db, sr, sg, sb;
    if (a <= 0) return;
    if (a >= 255) { *p = c; return; }
    d = *p;
    dr = (d >> 16) & 255; dg = (d >> 8) & 255; db = d & 255;
    sr = (c >> 16) & 255; sg = (c >> 8) & 255; sb = c & 255;
    dr += ((sr - dr) * a + 127) / 255;
    dg += ((sg - dg) * a + 127) / 255;
    db += ((sb - db) * a + 127) / 255;
    *p = ((uint32_t)dr << 16) | ((uint32_t)dg << 8) | (uint32_t)db;
}

void gfx_clip(int l, int t, int r, int b)
{
    if (l < 0) l = 0;
    if (t < 0) t = 0;
    if (r > g_sw) r = g_sw;
    if (b > g_sh) b = g_sh;
    if (r < l) r = l;
    if (b < t) b = t;
    cl_l = l; cl_t = t; cl_r = r; cl_b = b;
    {
        HRGN rg = CreateRectRgn(l, t, r, b);
        SelectClipRgn(g_dc, rg);
        DeleteObject(rg);
    }
}

void gfx_unclip(void)
{
    cl_l = 0; cl_t = 0; cl_r = g_sw; cl_b = g_sh;
    SelectClipRgn(g_dc, NULL);
}

static __inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

void gfx_clear(Rgb c)
{
    int i, n = g_sw * g_sh;
    sync_gdi();
    for (i = 0; i < n; i++) g_px[i] = c;
}

void gfx_rect(int x, int y, int w, int h, Rgb c, int alpha)
{
    int x0 = MT_MAX(x, cl_l), y0 = MT_MAX(y, cl_t), x1 = MT_MIN(x + w, cl_r), y1 = MT_MIN(y + h, cl_b), xx, yy;
    sync_gdi();
    if (x1 <= x0 || y1 <= y0) return;
    for (yy = y0; yy < y1; yy++) {
        uint32_t* row = g_px + (size_t)yy * g_sw;
        if (alpha >= 255) for (xx = x0; xx < x1; xx++) row[xx] = c;
        else for (xx = x0; xx < x1; xx++) blend(&row[xx], c, alpha);
    }
}

/* ============================ shapes ============================ */
static __inline float sd_rrect(float px, float py, float cx, float cy, float hx, float hy, float r)
{
    float qx = fabsf(px - cx) - (hx - r), qy = fabsf(py - cy) - (hy - r);
    float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0, outside, inside;
    outside = (ox > 0 && oy > 0) ? sqrtf(ox * ox + oy * oy) : (ox > oy ? ox : oy);
    inside = qx > qy ? qx : qy;
    if (inside > 0) inside = 0;
    return outside + inside - r;
}

static void rrect_core(float x, float y, float w, float h, float r, float stroke, Rgb c1, Rgb c2, int grad, int alpha)
{
    float cx = x + w * 0.5f, cy = y + h * 0.5f, hx = w * 0.5f, hy = h * 0.5f;
    int x0, y0, x1, y1, xx, yy;
    if (w <= 0 || h <= 0 || alpha <= 0) return;
    if (r > hx) r = hx;
    if (r > hy) r = hy;
    if (r < 0) r = 0;
    sync_gdi();
    x0 = MT_MAX((int)floorf(x) - 1, cl_l);
    y0 = MT_MAX((int)floorf(y) - 1, cl_t);
    x1 = MT_MIN((int)ceilf(x + w) + 1, cl_r);
    y1 = MT_MIN((int)ceilf(y + h) + 1, cl_b);
    for (yy = y0; yy < y1; yy++) {
        uint32_t* row = g_px + (size_t)yy * g_sw;
        Rgb col = c1;
        if (grad) col = rgb_mix(c1, c2, (yy + 0.5f - y) / h);
        for (xx = x0; xx < x1; xx++) {
            float d = sd_rrect(xx + 0.5f, yy + 0.5f, cx, cy, hx, hy, r);
            float cov;
            if (stroke > 0) cov = clamp01(0.5f - d) - clamp01(0.5f - (d + stroke));
            else cov = clamp01(0.5f - d);
            if (cov > 0.003f) blend(&row[xx], col, (int)(cov * alpha + 0.5f));
        }
    }
}

void gfx_rrect(float x, float y, float w, float h, float r, Rgb c, int alpha)
{
    rrect_core(x, y, w, h, r, 0, c, c, 0, alpha);
}

void gfx_rrect_grad(float x, float y, float w, float h, float r, Rgb top, Rgb bottom, int alpha)
{
    rrect_core(x, y, w, h, r, 0, top, bottom, 1, alpha);
}

void gfx_rrect_stroke(float x, float y, float w, float h, float r, float t, Rgb c, int alpha)
{
    rrect_core(x, y, w, h, r, t, c, c, 0, alpha);
}

void gfx_circle(float cx, float cy, float r, Rgb c, int alpha)
{
    int x0, y0, x1, y1, xx, yy;
    sync_gdi();
    x0 = MT_MAX((int)floorf(cx - r) - 1, cl_l);
    y0 = MT_MAX((int)floorf(cy - r) - 1, cl_t);
    x1 = MT_MIN((int)ceilf(cx + r) + 1, cl_r);
    y1 = MT_MIN((int)ceilf(cy + r) + 1, cl_b);
    for (yy = y0; yy < y1; yy++) {
        uint32_t* row = g_px + (size_t)yy * g_sw;
        for (xx = x0; xx < x1; xx++) {
            float dx = xx + 0.5f - cx, dy = yy + 0.5f - cy;
            float cov = clamp01(0.5f - (sqrtf(dx * dx + dy * dy) - r));
            if (cov > 0.003f) blend(&row[xx], c, (int)(cov * alpha + 0.5f));
        }
    }
}

void gfx_ring(float cx, float cy, float r, float thick, float f0, float f1, Rgb c, int alpha, int round_caps)
{
    int x0, y0, x1, y1, xx, yy, full = (f0 <= 0.0f && f1 >= 1.0f);
    float ext = r + thick * 0.5f;
    sync_gdi();
    x0 = MT_MAX((int)floorf(cx - ext) - 1, cl_l);
    y0 = MT_MAX((int)floorf(cy - ext) - 1, cl_t);
    x1 = MT_MIN((int)ceilf(cx + ext) + 1, cl_r);
    y1 = MT_MIN((int)ceilf(cy + ext) + 1, cl_b);
    for (yy = y0; yy < y1; yy++) {
        uint32_t* row = g_px + (size_t)yy * g_sw;
        for (xx = x0; xx < x1; xx++) {
            float dx = xx + 0.5f - cx, dy = yy + 0.5f - cy;
            float dist = sqrtf(dx * dx + dy * dy);
            float dd = fabsf(dist - r) - thick * 0.5f;
            float cov = clamp01(0.5f - dd);
            if (cov <= 0.003f) continue;
            if (!full) {
                float a = atan2f(dx, -dy);
                float fr;
                if (a < 0) a += 2.0f * MT_PI;
                fr = a / (2.0f * MT_PI);
                if (fr < f0 || fr > f1) continue;
            }
            blend(&row[xx], c, (int)(cov * alpha + 0.5f));
        }
    }
    if (!full && round_caps) {
        float a0 = f0 * 2.0f * MT_PI, a1 = f1 * 2.0f * MT_PI;
        gfx_circle(cx + sinf(a0) * r, cy - cosf(a0) * r, thick * 0.5f, c, alpha);
        gfx_circle(cx + sinf(a1) * r, cy - cosf(a1) * r, thick * 0.5f, c, alpha);
    }
}

void gfx_line(float x0f, float y0f, float x1f, float y1f, float thick, Rgb c, int alpha)
{
    float vx = x1f - x0f, vy = y1f - y0f, len2 = vx * vx + vy * vy, hw = thick * 0.5f;
    int x0, y0, x1, y1, xx, yy;
    sync_gdi();
    x0 = MT_MAX((int)floorf(MT_MIN(x0f, x1f) - hw) - 1, cl_l);
    y0 = MT_MAX((int)floorf(MT_MIN(y0f, y1f) - hw) - 1, cl_t);
    x1 = MT_MIN((int)ceilf(MT_MAX(x0f, x1f) + hw) + 1, cl_r);
    y1 = MT_MIN((int)ceilf(MT_MAX(y0f, y1f) + hw) + 1, cl_b);
    for (yy = y0; yy < y1; yy++) {
        uint32_t* row = g_px + (size_t)yy * g_sw;
        for (xx = x0; xx < x1; xx++) {
            float px = xx + 0.5f, py = yy + 0.5f, t = 0, dx, dy, cov;
            if (len2 > 0) t = clamp01(((px - x0f) * vx + (py - y0f) * vy) / len2);
            dx = px - (x0f + t * vx);
            dy = py - (y0f + t * vy);
            cov = clamp01(0.5f - (sqrtf(dx * dx + dy * dy) - hw));
            if (cov > 0.003f) blend(&row[xx], c, (int)(cov * alpha + 0.5f));
        }
    }
}

void gfx_poly(const float* p, int n, Rgb c, int alpha)
{
    float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
    int i, x0, y0, x1, y1, xx, yy;
    if (n < 3) return;
    sync_gdi();
    for (i = 0; i < n; i++) {
        if (p[i * 2] < minx) minx = p[i * 2];
        if (p[i * 2] > maxx) maxx = p[i * 2];
        if (p[i * 2 + 1] < miny) miny = p[i * 2 + 1];
        if (p[i * 2 + 1] > maxy) maxy = p[i * 2 + 1];
    }
    x0 = MT_MAX((int)floorf(minx) - 1, cl_l);
    y0 = MT_MAX((int)floorf(miny) - 1, cl_t);
    x1 = MT_MIN((int)ceilf(maxx) + 1, cl_r);
    y1 = MT_MIN((int)ceilf(maxy) + 1, cl_b);
    for (yy = y0; yy < y1; yy++) {
        uint32_t* row = g_px + (size_t)yy * g_sw;
        for (xx = x0; xx < x1; xx++) {
            float px = xx + 0.5f, py = yy + 0.5f, mind = 1e9f, cov;
            int inside = 0, j;
            for (i = 0, j = n - 1; i < n; j = i++) {
                float ax = p[j * 2], ay = p[j * 2 + 1], bx = p[i * 2], by = p[i * 2 + 1];
                float ex = bx - ax, ey = by - ay, l2 = ex * ex + ey * ey, t = 0, dx, dy, d;
                if (l2 > 0) t = clamp01(((px - ax) * ex + (py - ay) * ey) / l2);
                dx = px - (ax + t * ex);
                dy = py - (ay + t * ey);
                d = sqrtf(dx * dx + dy * dy);
                if (d < mind) mind = d;
                if (((ay > py) != (by > py)) && (px < (bx - ax) * (py - ay) / (by - ay) + ax)) inside = !inside;
            }
            cov = clamp01(0.5f - (inside ? -mind : mind));
            if (cov > 0.003f) blend(&row[xx], c, (int)(cov * alpha + 0.5f));
        }
    }
}

void gfx_graph(float x, float y, float w, float h, const float* vals, int n, float vmax, Rgb c, float line_w)
{
    int i, xx;
    float step;
    if (n < 2 || vmax <= 0) return;
    step = w / (float)(n - 1);
    sync_gdi();
    /* gradient fill under the curve, column by column */
    for (xx = (int)floorf(x); xx < (int)ceilf(x + w); xx++) {
        float fx = ((float)xx + 0.5f - x) / step;
        int i0 = (int)fx, yy, ytop, ybot;
        float t, v, py;
        if (xx < cl_l || xx >= cl_r) continue;
        if (i0 < 0) i0 = 0;
        if (i0 >= n - 1) i0 = n - 2;
        t = fx - (float)i0;
        v = vals[i0] + (vals[i0 + 1] - vals[i0]) * t;
        v = v < 0 ? 0 : (v > vmax ? vmax : v);
        py = y + h - (v / vmax) * h;
        ytop = MT_MAX((int)py, cl_t);
        ybot = MT_MIN((int)(y + h), cl_b);
        for (yy = ytop; yy < ybot; yy++) {
            float k = (float)(yy - (int)py) / (h > 1 ? h : 1);
            int a = (int)(70.0f * (1.0f - k));
            if (a > 0) blend(&g_px[(size_t)yy * g_sw + xx], c, a);
        }
    }
    for (i = 0; i + 1 < n; i++) {
        float v0 = MT_CLAMP(vals[i], 0.0f, vmax), v1 = MT_CLAMP(vals[i + 1], 0.0f, vmax);
        gfx_line(x + step * i, y + h - (v0 / vmax) * h, x + step * (i + 1), y + h - (v1 / vmax) * h, line_w, c, 255);
    }
}

/* ============================ text ============================ */
void gfx_text(int x, int y, const wchar_t* s, int font, Rgb c)
{
    HGDIOBJ old;
    if (!s || !*s) return;
    old = SelectObject(g_dc, g_font[font]);
    SetTextColor(g_dc, CR(c));
    TextOutW(g_dc, x, y, s, (int)wcslen(s));
    SelectObject(g_dc, old);
    g_gdi_dirty = 1;
}

void gfx_text_rc(const RECT* rc, const wchar_t* s, int font, Rgb c, UINT fl)
{
    HGDIOBJ old;
    RECT r = *rc;
    if (!s || !*s) return;
    old = SelectObject(g_dc, g_font[font]);
    SetTextColor(g_dc, CR(c));
    DrawTextW(g_dc, s, -1, &r, fl | DT_NOPREFIX);
    SelectObject(g_dc, old);
    g_gdi_dirty = 1;
}

int gfx_text_w(const wchar_t* s, int font)
{
    SIZE sz;
    HGDIOBJ old;
    if (!s || !*s) return 0;
    old = SelectObject(g_dc, g_font[font]);
    GetTextExtentPoint32W(g_dc, s, (int)wcslen(s), &sz);
    SelectObject(g_dc, old);
    return sz.cx;
}

int gfx_font_h(int font)
{
    TEXTMETRICW tm;
    HGDIOBJ old = SelectObject(g_dc, g_font[font]);
    GetTextMetricsW(g_dc, &tm);
    SelectObject(g_dc, old);
    return tm.tmHeight;
}

/* ============================ icons ============================ */
void gfx_icon(int icon, float cx, float cy, float s, Rgb c)
{
    float t = s * 0.09f, h = s * 0.5f;
    int i;
    if (t < 1.4f) t = 1.4f;
    switch (icon) {
    case ICO_DASH: {
        float q = s * 0.37f, g = s * 0.06f, r = s * 0.09f;
        gfx_rrect(cx - q - g, cy - q - g, q, q, r, c, 255);
        gfx_rrect(cx + g, cy - q - g, q, q, r, c, 255);
        gfx_rrect(cx - q - g, cy + g, q, q, r, c, 255);
        gfx_rrect(cx + g, cy + g, q, q, r, c, 255);
        break; }
    case ICO_MONITOR: {
        float px[6] = { -0.46f, -0.2f, -0.08f, 0.07f, 0.17f, 0.46f };
        float py[6] = { 0.05f, 0.05f, -0.32f, 0.32f, -0.02f, -0.02f };
        for (i = 0; i < 5; i++)
            gfx_line(cx + px[i] * s, cy + py[i] * s, cx + px[i + 1] * s, cy + py[i + 1] * s, t * 1.1f, c, 255);
        break; }
    case ICO_POWER:
        gfx_ring(cx, cy + s * 0.04f, s * 0.33f, t * 1.1f, 0.09f, 0.91f, c, 255, 1);
        gfx_line(cx, cy - s * 0.42f, cx, cy - s * 0.04f, t * 1.2f, c, 255);
        break;
    case ICO_INFO:
        gfx_ring(cx, cy, s * 0.41f, t, 0, 1, c, 255, 0);
        gfx_circle(cx, cy - s * 0.17f, t * 0.75f, c, 255);
        gfx_line(cx, cy - s * 0.04f, cx, cy + s * 0.21f, t * 1.1f, c, 255);
        break;
    case ICO_NET: case ICO_GLOBE: {
        float prev_x = 0, prev_y = 0;
        gfx_ring(cx, cy, s * 0.42f, t, 0, 1, c, 255, 0);
        gfx_line(cx - s * 0.42f, cy, cx + s * 0.42f, cy, t * 0.8f, c, 255);
        for (i = 0; i <= 20; i++) {
            float a = (float)i / 20.0f * 2.0f * MT_PI;
            float ex = cx + cosf(a) * s * 0.2f, ey = cy + sinf(a) * s * 0.42f;
            if (i > 0) gfx_line(prev_x, prev_y, ex, ey, t * 0.8f, c, 255);
            prev_x = ex; prev_y = ey;
        }
        break; }
    case ICO_CLEAN:
        gfx_line(cx - s * 0.38f, cy - s * 0.26f, cx + s * 0.38f, cy - s * 0.26f, t * 1.1f, c, 255);
        gfx_line(cx - s * 0.1f, cy - s * 0.26f, cx - s * 0.1f, cy - s * 0.4f, t, c, 255);
        gfx_line(cx + s * 0.1f, cy - s * 0.26f, cx + s * 0.1f, cy - s * 0.4f, t, c, 255);
        gfx_line(cx - s * 0.1f, cy - s * 0.4f, cx + s * 0.1f, cy - s * 0.4f, t, c, 255);
        gfx_line(cx - s * 0.28f, cy - s * 0.18f, cx - s * 0.22f, cy + s * 0.4f, t, c, 255);
        gfx_line(cx + s * 0.28f, cy - s * 0.18f, cx + s * 0.22f, cy + s * 0.4f, t, c, 255);
        gfx_line(cx - s * 0.22f, cy + s * 0.4f, cx + s * 0.22f, cy + s * 0.4f, t, c, 255);
        gfx_line(cx, cy - s * 0.08f, cx, cy + s * 0.28f, t * 0.8f, c, 255);
        break;
    case ICO_PROC: {
        float ys[3] = { -0.28f, 0.0f, 0.28f }, ks[3] = { -0.15f, 0.2f, -0.02f };
        for (i = 0; i < 3; i++) {
            gfx_line(cx - s * 0.42f, cy + ys[i] * s, cx + s * 0.42f, cy + ys[i] * s, t, c, 255);
            gfx_circle(cx + ks[i] * s, cy + ys[i] * s, s * 0.1f, c, 255);
        }
        break; }
    case ICO_SHIELD: {
        float p[12] = { 0, -0.46f, 0.38f, -0.32f, 0.38f, 0.04f, 0, 0.46f, -0.38f, 0.04f, -0.38f, -0.32f };
        for (i = 0; i < 6; i++) {
            int j = (i + 1) % 6;
            gfx_line(cx + p[i * 2] * s, cy + p[i * 2 + 1] * s, cx + p[j * 2] * s, cy + p[j * 2 + 1] * s, t * 1.1f, c, 255);
        }
        gfx_line(cx - s * 0.16f, cy, cx - s * 0.04f, cy + s * 0.13f, t * 1.1f, c, 255);
        gfx_line(cx - s * 0.04f, cy + s * 0.13f, cx + s * 0.18f, cy - s * 0.12f, t * 1.1f, c, 255);
        break; }
    case ICO_BUG:
        gfx_circle(cx, cy, s * 0.2f, c, 255);
        for (i = 0; i < 6; i++) {
            float a = (float)i / 6.0f * 2.0f * MT_PI + 0.26f;
            gfx_line(cx + cosf(a) * s * 0.2f, cy + sinf(a) * s * 0.2f, cx + cosf(a) * s * 0.36f, cy + sinf(a) * s * 0.36f, t, c, 255);
            gfx_circle(cx + cosf(a) * s * 0.39f, cy + sinf(a) * s * 0.39f, s * 0.07f, c, 255);
        }
        break;
    case ICO_CHIP:
        gfx_rrect_stroke(cx - s * 0.25f, cy - s * 0.25f, s * 0.5f, s * 0.5f, s * 0.06f, t, c, 255);
        gfx_rrect(cx - s * 0.1f, cy - s * 0.1f, s * 0.2f, s * 0.2f, s * 0.03f, c, 255);
        for (i = 0; i < 3; i++) {
            float o = (-0.14f + 0.14f * i) * s;
            gfx_line(cx + o, cy - s * 0.25f, cx + o, cy - s * 0.4f, t * 0.9f, c, 255);
            gfx_line(cx + o, cy + s * 0.25f, cx + o, cy + s * 0.4f, t * 0.9f, c, 255);
            gfx_line(cx - s * 0.25f, cy + o, cx - s * 0.4f, cy + o, t * 0.9f, c, 255);
            gfx_line(cx + s * 0.25f, cy + o, cx + s * 0.4f, cy + o, t * 0.9f, c, 255);
        }
        break;
    case ICO_TOOLS:
        gfx_line(cx - s * 0.34f, cy + s * 0.34f, cx + s * 0.1f, cy - s * 0.1f, t * 1.6f, c, 255);
        gfx_ring(cx + s * 0.2f, cy - s * 0.2f, s * 0.17f, t * 1.2f, 0.12f, 0.95f, c, 255, 0);
        break;
    case ICO_GEAR:
        gfx_ring(cx, cy, s * 0.24f, t * 1.2f, 0, 1, c, 255, 0);
        for (i = 0; i < 8; i++) {
            float a = (float)i / 8.0f * 2.0f * MT_PI;
            gfx_line(cx + cosf(a) * s * 0.3f, cy + sinf(a) * s * 0.3f, cx + cosf(a) * s * 0.44f, cy + sinf(a) * s * 0.44f, t * 1.9f, c, 255);
        }
        break;
    case ICO_EXIT:
        gfx_line(cx + s * 0.05f, cy - s * 0.38f, cx - s * 0.3f, cy - s * 0.38f, t, c, 255);
        gfx_line(cx - s * 0.3f, cy - s * 0.38f, cx - s * 0.3f, cy + s * 0.38f, t, c, 255);
        gfx_line(cx - s * 0.3f, cy + s * 0.38f, cx + s * 0.05f, cy + s * 0.38f, t, c, 255);
        gfx_line(cx - s * 0.05f, cy, cx + s * 0.4f, cy, t, c, 255);
        gfx_line(cx + s * 0.22f, cy - s * 0.17f, cx + s * 0.4f, cy, t, c, 255);
        gfx_line(cx + s * 0.22f, cy + s * 0.17f, cx + s * 0.4f, cy, t, c, 255);
        break;
    case ICO_RUN: {
        float p[6] = { cx - s * 0.2f, cy - s * 0.3f, cx + s * 0.32f, cy, cx - s * 0.2f, cy + s * 0.3f };
        gfx_poly(p, 3, c, 255);
        break; }
    case ICO_STOP:
        gfx_rrect(cx - s * 0.24f, cy - s * 0.24f, s * 0.48f, s * 0.48f, s * 0.06f, c, 255);
        break;
    case ICO_REFRESH: {
        float ang = 0.82f * 2.0f * MT_PI;
        float ax = cx + sinf(ang) * s * 0.32f, ay = cy - cosf(ang) * s * 0.32f;
        float p[6] = { ax + s * 0.02f, ay - s * 0.2f, ax + s * 0.2f, ay + s * 0.02f, ax - s * 0.12f, ay + s * 0.1f };
        gfx_ring(cx, cy, s * 0.32f, t * 1.1f, 0.05f, 0.82f, c, 255, 0);
        gfx_poly(p, 3, c, 255);
        break; }
    case ICO_COPY:
        gfx_rrect_stroke(cx - s * 0.1f, cy - s * 0.36f, s * 0.46f, s * 0.5f, s * 0.07f, t, c, 255);
        gfx_rrect_stroke(cx - s * 0.36f, cy - s * 0.14f, s * 0.46f, s * 0.5f, s * 0.07f, t, c, 255);
        break;
    case ICO_SAVE:
        gfx_rrect_stroke(cx - s * 0.36f, cy - s * 0.36f, s * 0.72f, s * 0.72f, s * 0.08f, t, c, 255);
        gfx_rrect(cx - s * 0.2f, cy - s * 0.36f, s * 0.36f, s * 0.2f, s * 0.02f, c, 255);
        gfx_rrect_stroke(cx - s * 0.2f, cy + s * 0.04f, s * 0.4f, s * 0.32f, s * 0.04f, t * 0.9f, c, 255);
        break;
    case ICO_CLEAR:
        gfx_line(cx - s * 0.28f, cy - s * 0.28f, cx + s * 0.28f, cy + s * 0.28f, t * 1.2f, c, 255);
        gfx_line(cx + s * 0.28f, cy - s * 0.28f, cx - s * 0.28f, cy + s * 0.28f, t * 1.2f, c, 255);
        break;
    case ICO_FOLDER:
        gfx_rrect_stroke(cx - s * 0.42f, cy - s * 0.24f, s * 0.84f, s * 0.6f, s * 0.07f, t, c, 255);
        gfx_line(cx - s * 0.42f, cy - s * 0.24f, cx - s * 0.3f, cy - s * 0.36f, t, c, 255);
        gfx_line(cx - s * 0.3f, cy - s * 0.36f, cx - s * 0.05f, cy - s * 0.36f, t, c, 255);
        gfx_line(cx - s * 0.05f, cy - s * 0.36f, cx + s * 0.04f, cy - s * 0.24f, t, c, 255);
        break;
    case ICO_CHECK:
        gfx_line(cx - s * 0.32f, cy + s * 0.02f, cx - s * 0.1f, cy + s * 0.26f, t * 1.3f, c, 255);
        gfx_line(cx - s * 0.1f, cy + s * 0.26f, cx + s * 0.34f, cy - s * 0.24f, t * 1.3f, c, 255);
        break;
    case ICO_WARN:
        gfx_line(cx, cy - s * 0.4f, cx + s * 0.44f, cy + s * 0.34f, t, c, 255);
        gfx_line(cx + s * 0.44f, cy + s * 0.34f, cx - s * 0.44f, cy + s * 0.34f, t, c, 255);
        gfx_line(cx - s * 0.44f, cy + s * 0.34f, cx, cy - s * 0.4f, t, c, 255);
        gfx_line(cx, cy - s * 0.13f, cx, cy + s * 0.1f, t * 1.1f, c, 255);
        gfx_circle(cx, cy + s * 0.22f, t * 0.7f, c, 255);
        break;
    case ICO_SEARCH:
        gfx_ring(cx - s * 0.08f, cy - s * 0.08f, s * 0.27f, t * 1.1f, 0, 1, c, 255, 0);
        gfx_line(cx + s * 0.12f, cy + s * 0.12f, cx + s * 0.4f, cy + s * 0.4f, t * 1.4f, c, 255);
        break;
    case ICO_USER:
        gfx_circle(cx, cy - s * 0.17f, s * 0.18f, c, 255);
        gfx_rrect(cx - s * 0.32f, cy + s * 0.1f, s * 0.64f, s * 0.3f, s * 0.15f, c, 255);
        break;
    case ICO_LOCK:
        gfx_rrect(cx - s * 0.3f, cy - s * 0.04f, s * 0.6f, s * 0.44f, s * 0.07f, c, 255);
        gfx_ring(cx, cy - s * 0.06f, s * 0.2f, t * 1.2f, 0.75f, 1.0f, c, 255, 0);
        gfx_ring(cx, cy - s * 0.06f, s * 0.2f, t * 1.2f, 0.0f, 0.25f, c, 255, 0);
        break;
    default:
        gfx_circle(cx, cy, h * 0.5f, c, 255);
        break;
    }
}

void gfx_logo(int x, int y, int size)
{
    int slot = -1, i;
    for (i = 0; i < 4; i++) if (g_icons[i] && g_icon_sz[i] == size) { slot = i; break; }
    if (slot < 0) {
        for (i = 0; i < 4; i++) if (!g_icons[i]) { slot = i; break; }
        if (slot < 0) { slot = 0; DestroyIcon(g_icons[0]); }
        g_icons[slot] = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, size, size, LR_DEFAULTCOLOR);
        g_icon_sz[slot] = size;
    }
    if (g_icons[slot]) {
        DrawIconEx(g_dc, x, y, g_icons[slot], size, size, 0, NULL, DI_NORMAL);
        g_gdi_dirty = 1;
    }
}
