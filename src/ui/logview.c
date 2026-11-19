/* ui_log.c - colored read-only output views based on RichEdit */
#include "ui/ui.h"
#include "core/util.h"
#include <richedit.h>
#include <uxtheme.h>

static HWND g_logs[LOG_COUNT];
static HMODULE g_msftedit = NULL;

static COLORREF style_color(int style)
{
    switch (style) {
    case OS_DIM: case OS_KEY: return CR(g_th.dim);
    case OS_ACCENT: case OS_TITLE: return CR(g_th.accent);
    case OS_OK: return CR(g_th.ok);
    case OS_WARN: return CR(g_th.warn);
    case OS_ERR: return CR(g_th.bad);
    case OS_INFO: return CR(g_th.info);
    default: return CR(g_th.text);
    }
}

static void fill_cf(CHARFORMAT2W* cf, int style)
{
    LOGFONTW lf;
    ZeroMemory(cf, sizeof(*cf));
    cf->cbSize = sizeof(*cf);
    cf->dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE | CFM_BOLD | CFM_ITALIC;
    cf->yHeight = 195;                       /* 9.75 pt */
    cf->crTextColor = style_color(style);
    cf->dwEffects = (style == OS_TITLE) ? CFE_BOLD : 0;
    wcscpy(cf->szFaceName, L"Consolas");
    if (g_font[F_MONO] && GetObjectW(g_font[F_MONO], sizeof(lf), &lf)) wcsncpy(cf->szFaceName, lf.lfFaceName, LF_FACESIZE - 1);
}

void log_set_dpi(void)
{
    int i;
    for (i = 0; i < LOG_COUNT; i++)
        if (g_logs[i]) SendMessageW(g_logs[i], EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(S(12), S(12)));
}

void log_restyle(void)
{
    int i;
    CHARFORMAT2W cf;
    fill_cf(&cf, OS_NORMAL);
    for (i = 0; i < LOG_COUNT; i++) {
        if (!g_logs[i]) continue;
        SendMessageW(g_logs[i], EM_SETBKGNDCOLOR, 0, (LPARAM)CR(g_th.logbg));
        SendMessageW(g_logs[i], EM_SETCHARFORMAT, SCF_DEFAULT, (LPARAM)&cf);
    }
}

void log_init(HWND parent)
{
    int i;
    if (!g_msftedit) g_msftedit = LoadLibraryW(L"Msftedit.dll");
    for (i = 0; i < LOG_COUNT; i++) {
        HWND h = CreateWindowExW(0, L"RICHEDIT50W", L"", WS_CHILD | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_NOHIDESEL,
                                 0, 0, 10, 10, parent, (HMENU)(INT_PTR)(7000 + i), g_hInst, NULL);
        g_logs[i] = h;
        if (!h) continue;
        SendMessageW(h, EM_EXLIMITTEXT, 0, 0x7FFFFFF);
        SetWindowTheme(h, L"DarkMode_Explorer", NULL);
    }
    log_restyle();
    log_set_dpi();
}

void log_place(int id, const RECT* rc)
{
    int i;
    for (i = 0; i < LOG_COUNT; i++) {
        if (!g_logs[i]) continue;
        if (rc && i == id) {
            MoveWindow(g_logs[i], rc->left, rc->top, rc->right - rc->left, rc->bottom - rc->top, TRUE);
            ShowWindow(g_logs[i], SW_SHOWNA);
        } else ShowWindow(g_logs[i], SW_HIDE);
    }
}

void log_append(int id, int style, const wchar_t* text)
{
    HWND h;
    CHARFORMAT2W cf;
    wchar_t* t;
    size_t i, n;
    if (id < 0 || id >= LOG_COUNT || !(h = g_logs[id]) || !text) return;
    t = wdup(text);
    if (!t) return;
    n = wcslen(t);
    for (i = 0; i < n; i++) if (t[i] == L'\n') t[i] = L'\r';
    fill_cf(&cf, style);
    SendMessageW(h, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessageW(h, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    SendMessageW(h, EM_REPLACESEL, FALSE, (LPARAM)t);
    free(t);
    SendMessageW(h, WM_VSCROLL, SB_BOTTOM, 0);
    SendMessageW(h, WM_HSCROLL, SB_LEFT, 0);
}

void log_clear(int id)
{
    if (id >= 0 && id < LOG_COUNT && g_logs[id]) SetWindowTextW(g_logs[id], L"");
}

wchar_t* log_text(int id)
{
    HWND h;
    int n;
    wchar_t* t;
    if (id < 0 || id >= LOG_COUNT || !(h = g_logs[id])) return wdup(L"");
    n = GetWindowTextLengthW(h);
    t = (wchar_t*)malloc((size_t)(n + 2) * sizeof(wchar_t));
    if (!t) return wdup(L"");
    GetWindowTextW(h, t, n + 1);
    return t;
}
