/* gfx.h - software renderer on a 32-bit DIB: anti-aliased shapes + GDI text + vector icons */
#ifndef MT_GFX_H
#define MT_GFX_H
#include "core/common.h"

typedef uint32_t Rgb;                       /* 0x00RRGGBB */
#define RGBX(r,g,b) ((((uint32_t)(r)) << 16) | (((uint32_t)(g)) << 8) | ((uint32_t)(b)))
#define CR(c) RGB(((c) >> 16) & 255, ((c) >> 8) & 255, (c) & 255)

typedef struct Theme {
    Rgb bg, side, card, card2, border, border2, text, dim, muted, accent, ok, warn, bad, info, logbg;
} Theme;
extern Theme g_th;
extern const wchar_t* const g_theme_names[6];
extern const Rgb g_theme_accent[6];
void theme_apply(int idx);
Rgb  rgb_mix(Rgb a, Rgb b, float t);

extern int g_dpi;
static __inline int S(int px) { return MulDiv(px, g_dpi, 96); }

enum { F_UI, F_UI_B, F_UI_S, F_UI_SB, F_UI_XS, F_TITLE, F_H2, F_BIG, F_NUM, F_MONO, F_COUNT };

enum { ICO_DASH, ICO_MONITOR, ICO_POWER, ICO_INFO, ICO_NET, ICO_CLEAN, ICO_PROC, ICO_SHIELD, ICO_BUG,
       ICO_CHIP, ICO_TOOLS, ICO_GEAR, ICO_EXIT, ICO_RUN, ICO_STOP, ICO_REFRESH, ICO_COPY, ICO_SAVE,
       ICO_CLEAR, ICO_FOLDER, ICO_CHECK, ICO_WARN, ICO_SEARCH, ICO_GLOBE, ICO_USER, ICO_LOCK };

extern uint32_t* g_px;
extern int g_sw, g_sh;
extern HDC g_dc;
extern HFONT g_font[F_COUNT];

void gfx_init(void);
void gfx_set_dpi(int dpi);                 /* (re)creates fonts */
void gfx_resize(int w, int h);
void gfx_present(HDC dst, int x, int y, int w, int h);
void gfx_shutdown(void);

void gfx_clip(int l, int t, int r, int b);   /* pixel + GDI clip (intersected with surface) */
void gfx_unclip(void);

void gfx_clear(Rgb c);
void gfx_rect(int x, int y, int w, int h, Rgb c, int alpha);
void gfx_rrect(float x, float y, float w, float h, float r, Rgb c, int alpha);
void gfx_rrect_grad(float x, float y, float w, float h, float r, Rgb top, Rgb bottom, int alpha);
void gfx_rrect_stroke(float x, float y, float w, float h, float r, float t, Rgb c, int alpha);
void gfx_circle(float cx, float cy, float r, Rgb c, int alpha);
void gfx_ring(float cx, float cy, float r, float thick, float f0, float f1, Rgb c, int alpha, int round_caps);
void gfx_line(float x0, float y0, float x1, float y1, float thick, Rgb c, int alpha);
void gfx_poly(const float* pts, int n, Rgb c, int alpha);
void gfx_graph(float x, float y, float w, float h, const float* vals, int n, float vmax, Rgb c, float line_w);

void gfx_text(int x, int y, const wchar_t* s, int font, Rgb c);
void gfx_text_rc(const RECT* rc, const wchar_t* s, int font, Rgb c, UINT dt_flags);
int  gfx_text_w(const wchar_t* s, int font);
int  gfx_font_h(int font);

void gfx_icon(int icon, float cx, float cy, float size, Rgb c);
void gfx_logo(int x, int y, int size);

#endif
