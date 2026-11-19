/* splash.c - start-up splash screen (drawn with the same software renderer as the main window) */
#include "app/splash.h"
#include "ui/ui.h"
#include "core/util.h"

static RECT rc_make_shell(int l, int t, int r, int b) { RECT x; x.left = l; x.top = t; x.right = r; x.bottom = b; return x; }

static const wchar_t* k_splash_class = L"MultiToolSplash";
static LRESULT CALLBACK SplashProc(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcW(h, m, w, l); }

void splash_show(void)
{
    WNDCLASSW wc;
    int w = S(520), hgt = S(300), x, y, steps = 40, i;
    RECT wa;
    HWND sp;
    HDC dc;
    static const wchar_t* msgs_en[] = { L"Loading modules...", L"Reading system information...", L"Preparing interface...", L"Ready" };
    static const wchar_t* msgs_id[] = { L"Memuat modul...", L"Membaca informasi sistem...", L"Menyiapkan antarmuka...", L"Siap" };
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = SplashProc;
    wc.hInstance = g_hInst;
    wc.lpszClassName = k_splash_class;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    RegisterClassW(&wc);
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    x = wa.left + (wa.right - wa.left - w) / 2;
    y = wa.top + (wa.bottom - wa.top - hgt) / 2;
    sp = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, k_splash_class, MT_NAME, WS_POPUP, x, y, w, hgt, NULL, NULL, g_hInst, NULL);
    if (!sp) return;
    SetWindowRgn(sp, CreateRoundRectRgn(0, 0, w + 1, hgt + 1, S(24), S(24)), TRUE);
    gfx_resize(w, hgt);
    ShowWindow(sp, SW_SHOWNOACTIVATE);
    dc = GetDC(sp);
    for (i = 0; i <= steps; i++) {
        MSG m;
        RECT t, pr;
        double f = (double)i / steps;
        int mi = (int)(f * 3.99);
        gfx_clear(g_th.bg);
        gfx_rrect(1, 1, (float)w - 2, (float)hgt - 2, (float)S(22), g_th.card, 255);
        gfx_rrect_stroke(1, 1, (float)w - 2, (float)hgt - 2, (float)S(22), 1.5f, g_th.border2, 255);
        gfx_circle((float)(w / 2), (float)S(88), (float)S(78), g_th.accent, 14);
        gfx_logo((w - S(96)) / 2, S(40), S(96));
        t = rc_make_shell(0, S(146), w, S(186));
        gfx_text_rc(&t, MT_NAME, F_TITLE, g_th.text, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
        { wchar_t line[128]; wfmt(line, 128, L"v%ls  %ls  %ls", MT_VERSION, U_DOT, MT_AUTHOR);
          t = rc_make_shell(0, S(184), w, S(206));
          gfx_text_rc(&t, line, F_UI_S, g_th.dim, DT_SINGLELINE | DT_VCENTER | DT_CENTER); }
        pr = rc_make_shell(S(60), S(240), w - S(60), S(246));
        w_progress(&pr, f, g_th.accent);
        t = rc_make_shell(0, S(252), w, S(276));
        gfx_text_rc(&t, T2(msgs_en[mi], msgs_id[mi]), F_UI_S, g_th.muted, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
        gfx_present(dc, 0, 0, w, hgt);
        while (PeekMessageW(&m, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); }
        Sleep(i < steps ? 28 : 120);
    }
    ReleaseDC(sp, dc);
    DestroyWindow(sp);
}
