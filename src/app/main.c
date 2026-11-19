/* main.c - entry point and main window procedure */
#include "ui/ui.h"
#include "app/shell.h"
#include "app/splash.h"
#include "core/util.h"
#include "core/settings.h"
#include "resource.h"
#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commctrl.h>

#ifdef _MSC_VER   /* Visual Studio: pull in every import library from code, so the project links without extra settings */
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "netapi32.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "powrprof.lib")
#pragma comment(lib, "msimg32.lib")
#endif

/* ---- application-wide state ---- */
HINSTANCE g_hInst;
HWND      g_hwnd;
BOOL      g_admin;
wchar_t   g_exe_path[MAX_PATH], g_exe_dir[MAX_PATH], g_data_dir[MAX_PATH];
int       g_page = PG_DASH;
int       g_cw, g_ch;
RECT      g_cont;
BOOL      g_dirty = TRUE;

static BOOL g_tracking = FALSE;
static const wchar_t* k_class = L"MultiToolMainWnd";

/* ============================ dpi / dwm helpers ============================ */
int app_window_dpi(HWND h)
{
    typedef UINT (WINAPI *PFN_GetDpiForWindow)(HWND);
    HMODULE u = GetModuleHandleW(L"user32.dll");
    PFN_GetDpiForWindow fn = u ? (PFN_GetDpiForWindow)(void*)GetProcAddress(u, "GetDpiForWindow") : NULL;
    if (fn && h) { UINT d = fn(h); if (d) return (int)d; }
    {
        HDC dc = GetDC(NULL);
        int d = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96;
        if (dc) ReleaseDC(NULL, dc);
        return d > 0 ? d : 96;
    }
}

void app_apply_dark_titlebar(HWND h)
{
    BOOL on = TRUE;
    COLORREF cap = CR(g_th.side), txt = CR(g_th.text);
    DwmSetWindowAttribute(h, 20, &on, sizeof(on));       /* DWMWA_USE_IMMERSIVE_DARK_MODE */
    DwmSetWindowAttribute(h, 19, &on, sizeof(on));       /* older Windows 10 builds */
    DwmSetWindowAttribute(h, 35, &cap, sizeof(cap));     /* DWMWA_CAPTION_COLOR (Windows 11) */
    DwmSetWindowAttribute(h, 36, &txt, sizeof(txt));     /* DWMWA_TEXT_COLOR */
}

static void render_frame(HDC hdc, const RECT* dirty)
{
    hot_reset();
    gfx_clear(g_th.bg);
    shell_draw();
    pages_paint();
    gfx_present(hdc, dirty->left, dirty->top, dirty->right - dirty->left, dirty->bottom - dirty->top);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        if (g_px) render_frame(dc, &ps.rcPaint);
        EndPaint(hwnd, &ps);
        return 0; }
    case WM_SIZE:
        if (wp != SIZE_MINIMIZED) {
            g_cw = LOWORD(lp); g_ch = HIWORD(lp);
            gfx_resize(g_cw, g_ch);
            shell_compute_layout();
            if (g_ed_feat) pages_layout();
            g_dirty = TRUE;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO* mm = (MINMAXINFO*)lp;
        mm->ptMinTrackSize.x = S(1040);
        mm->ptMinTrackSize.y = S(720);
        return 0; }
    case WM_DPICHANGED: {
        RECT* r = (RECT*)lp;
        gfx_set_dpi(HIWORD(wp));
        log_set_dpi();
        SetWindowPos(hwnd, NULL, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
        shell_relayout();
        return 0; }
    case WM_MOUSEMOVE: {
        int x = (short)LOWORD(lp), y = (short)HIWORD(lp), id;
        if (!g_tracking) {
            TRACKMOUSEEVENT te;
            te.cbSize = sizeof(te); te.dwFlags = TME_LEAVE; te.hwndTrack = hwnd; te.dwHoverTime = 0;
            TrackMouseEvent(&te);
            g_tracking = TRUE;
        }
        id = hot_at(x, y);
        if (id != g_hover) { g_hover = id; g_dirty = TRUE; }
        if (pages_mouse(WM_MOUSEMOVE, x, y)) g_dirty = TRUE;
        return 0; }
    case WM_MOUSELEAVE:
        g_tracking = FALSE;
        g_hover = -1;
        pages_mouse(WM_MOUSEMOVE, -1, -1);
        g_dirty = TRUE;
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT && g_hover >= 0) { SetCursor(LoadCursorW(NULL, IDC_HAND)); return TRUE; }
        break;
    case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: {
        int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
        SetFocus(hwnd);
        SetCapture(hwnd);
        g_pressed = hot_at(x, y);
        if (pages_mouse(msg, x, y)) { g_dirty = TRUE; g_pressed = -1; }
        return 0; }
    case WM_LBUTTONUP: {
        int x = (short)LOWORD(lp), y = (short)HIWORD(lp), id = hot_at(x, y);
        BOOL consumed;
        ReleaseCapture();
        consumed = pages_mouse(WM_LBUTTONUP, x, y);
        if (!consumed && id >= 0 && id == g_pressed) shell_click(id);
        g_pressed = -1;
        g_dirty = TRUE;
        return 0; }
    case WM_MOUSEWHEEL: {
        POINT p;
        int d = (short)HIWORD(wp);
        p.x = (short)LOWORD(lp); p.y = (short)HIWORD(lp);
        ScreenToClient(hwnd, &p);
        if (shell_nav_wheel(p.x, d)) { /* sidebar scrolled */ }
        else if (pages_wheel(p.x, p.y, d)) g_dirty = TRUE;
        return 0; }
    case WM_KEYDOWN:
        if (pages_key(wp)) g_dirty = TRUE;
        return 0;
    case WM_TIMER:
        if (wp == 1) pages_tick();
        else if (wp == 2) {
            BOOL moved = hot_tick();
            BOOL anim = pages_anim();
            if (g_dirty || moved || anim) { g_dirty = FALSE; InvalidateRect(hwnd, NULL, FALSE); }
        }
        return 0;
    case WM_APP_LOG: case WM_APP_DONE: case WM_APP_PROGRESS: case WM_APP_TABLE: case WM_APP_SCAN: case WM_APP_STATUS:
        pages_app_msg(msg, wp, lp);
        return 0;
    case WM_COMMAND:
        pages_command(wp, lp);
        return 0;
    case WM_CTLCOLOREDIT: case WM_CTLCOLORSTATIC:
        return (LRESULT)pages_ctlcolor((HDC)wp, (HWND)lp);
    case WM_CLOSE:
        if (pages_busy() && !shell_confirm(T2(L"Some tasks are still running. Exit anyway?", L"Masih ada tugas yang berjalan. Tetap keluar?"))) return 0;
        pages_shutdown();
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        KillTimer(hwnd, 2);
        PostQuitMessage(0);
        return 0;
    default: break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ============================ entry point ============================ */
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int show)
{
    WNDCLASSEXW wc;
    MSG msg;
    RECT wa;
    int dpi, w, h, x, y;
    WSADATA wsa;
    INITCOMMONCONTROLSEX icc;
    wchar_t appdata[MAX_PATH];
    (void)hPrev; (void)cmd;

    g_hInst = hInst;
    GetModuleFileNameW(NULL, g_exe_path, MAX_PATH);
    wcopy(g_exe_dir, MAX_PATH, g_exe_path);
    { wchar_t* s = wcsrchr(g_exe_dir, L'\\'); if (s) *s = 0; }
    if (get_folder(CSIDL_APPDATA, appdata)) path_join(g_data_dir, MAX_PATH, appdata, L"MultiTool");
    else wcopy(g_data_dir, MAX_PATH, g_exe_dir);
    g_admin = is_elevated();

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    WSAStartup(MAKEWORD(2, 2), &wsa);
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);

    settings_load();
    theme_apply(g_set.theme);
    dpi = app_window_dpi(NULL);
    gfx_init();
    gfx_set_dpi(dpi);

    splash_show();

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);
    wc.hIconSm = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    wc.hbrBackground = NULL;
    wc.lpszClassName = k_class;
    RegisterClassExW(&wc);

    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    w = MT_MIN(S(1340), (wa.right - wa.left) * 96 / 100);
    h = MT_MIN(S(860), (wa.bottom - wa.top) * 96 / 100);
    x = wa.left + (wa.right - wa.left - w) / 2;
    y = wa.top + (wa.bottom - wa.top - h) / 2;
    g_hwnd = CreateWindowExW(0, k_class, MT_NAME L" " MT_VERSION, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                             x, y, w, h, NULL, NULL, hInst, NULL);
    if (!g_hwnd) return 1;
    app_apply_dark_titlebar(g_hwnd);
    {
        RECT cr;
        GetClientRect(g_hwnd, &cr);
        g_cw = cr.right; g_ch = cr.bottom;
        gfx_resize(g_cw, g_ch);
        shell_compute_layout();
    }
    pages_init(g_hwnd);
    {   /* developer option: multi-tool.exe --page=N opens directly on page N */
        const wchar_t* a = wcsstr(GetCommandLineW(), L"--page=");
        if (a) { int pg = _wtoi(a + 7); if (pg >= 0 && pg < PG_COUNT) g_page = pg; }
    }
    pages_page_changed(g_page);
    {   /* developer option: --run=FP,IDX,OPT,OUTFILE[,TEXT] runs one feature, dumps its output and exits (used for automated tests) */
        const wchar_t* a = wcsstr(GetCommandLineW(), L"--run=");
        if (a) {
            wchar_t buf[600], *p[5] = { 0 }, *tok;
            int n = 0;
            wcopy(buf, 600, a + 6);
            tok = buf;
            while (n < 5 && tok) { p[n++] = tok; tok = wcschr(tok, L','); if (tok) *tok++ = 0; }
            if (n >= 4) {
                int fp = _wtoi(p[0]);
                static const int pg_of[FP_COUNT] = { PG_SYSTEM, PG_NETWORK, PG_CLEANUP, PG_SECURITY, PG_TOOLS };
                if (fp >= 0 && fp < FP_COUNT) { g_page = pg_of[fp]; pages_page_changed(g_page); }
                pages_dev_run(fp, _wtoi(p[1]), p[3], n >= 5 ? p[4] : NULL, _wtoi(p[2]));
            }
        }
    }
    ShowWindow(g_hwnd, show == SW_HIDE ? SW_SHOWNORMAL : SW_SHOW);
    UpdateWindow(g_hwnd);
    SetTimer(g_hwnd, 1, 1000, NULL);
    SetTimer(g_hwnd, 2, 33, NULL);

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    settings_save();
    gfx_shutdown();
    WSACleanup();
    CoUninitialize();
    ExitProcess(0);
    return (int)msg.wParam;
}
