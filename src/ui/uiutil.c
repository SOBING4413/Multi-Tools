/* uiutil.c - background jobs, toast notifications, edit-control and dialog helpers shared by all pages */
#include "ui/internal.h"
#include <shlobj.h>
#include <commdlg.h>
#include <shellapi.h>
#include <process.h>

RECT rc_make(int l, int t, int r, int b) { RECT x; x.left = l; x.top = t; x.right = r; x.bottom = b; return x; }

Job g_jobs[JS_COUNT];

BOOL job_busy(int slot) { return g_jobs[slot].running != 0; }

BOOL job_run(int slot, JobFn fn, void* arg)
{
    unsigned tid;
    HANDLE h;
    if (g_jobs[slot].running) return FALSE;
    g_jobs[slot].running = 1;
    g_jobs[slot].cancel = 0;
    g_jobs[slot].progress10 = -10;
    h = (HANDLE)_beginthreadex(NULL, 0, fn, arg, 0, &tid);
    if (!h) { g_jobs[slot].running = 0; return FALSE; }
    g_jobs[slot].th = h;
    return TRUE;
}

void job_stop(int slot) { InterlockedExchange(&g_jobs[slot].cancel, 1); }

void job_finish(int slot)
{
    g_jobs[slot].running = 0;
    g_jobs[slot].progress10 = -10;
    if (g_jobs[slot].th) { CloseHandle(g_jobs[slot].th); g_jobs[slot].th = NULL; }
}

/* ---- toast ---- */
static wchar_t g_toast[256];
static int g_toast_sev;
static ULONGLONG g_toast_until = 0;

void ui_toast(int sev, const wchar_t* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    wvfmt(g_toast, MT_ARRAY(g_toast), fmt, ap);
    va_end(ap);
    g_toast_sev = sev;
    g_toast_until = GetTickCount64() + 3800;
    g_dirty = TRUE;
}

/* ---- edit helpers ---- */
void edit_place(HWND ed, const RECT* frame, BOOL show)
{
    if (!ed) return;
    if (!show || !frame) { ShowWindow(ed, SW_HIDE); return; }
    {
        int fh = gfx_font_h(F_UI), h = frame->bottom - frame->top;
        int ey = frame->top + (h - fh) / 2;
        SendMessageW(ed, WM_SETFONT, (WPARAM)g_font[F_UI], FALSE);
        MoveWindow(ed, frame->left + S(12), ey, frame->right - frame->left - S(24), fh, TRUE);
        ShowWindow(ed, SW_SHOWNA);
    }
}

void edit_get(HWND ed, wchar_t* out, size_t cch)
{
    out[0] = 0;
    if (ed) GetWindowTextW(ed, out, (int)cch);
}

void edit_set(HWND ed, const wchar_t* s)
{
    if (ed) SetWindowTextW(ed, s ? s : L"");
}

void fmt_rate(double b, wchar_t* out, size_t cch)
{
    if (b >= 1024.0 * 1024.0) wfmt(out, cch, L"%.2f MB/s", b / (1024.0 * 1024.0));
    else if (b >= 1024.0) wfmt(out, cch, L"%.1f KB/s", b / 1024.0);
    else wfmt(out, cch, L"%.0f B/s", b);
}

BOOL pick_file(wchar_t* out, size_t cch, const wchar_t* title)
{
    OPENFILENAMEW ofn;
    out[0] = 0;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.lpstrFile = out;
    ofn.nMaxFile = (DWORD)cch;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_HIDEREADONLY;
    return GetOpenFileNameW(&ofn);
}

BOOL pick_folder(wchar_t* out, size_t cch, const wchar_t* title)
{
    BROWSEINFOW bi;
    LPITEMIDLIST pidl;
    BOOL ok = FALSE;
    ZeroMemory(&bi, sizeof(bi));
    bi.hwndOwner = g_hwnd;
    bi.lpszTitle = title;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_EDITBOX;
    pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        wchar_t p[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, p)) { wcopy(out, cch, p); ok = TRUE; }
        CoTaskMemFree(pidl);
    }
    return ok;
}

BOOL confirm_box(const wchar_t* text)
{
    return MessageBoxW(g_hwnd, text, MT_NAME, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES;
}

BOOL ui_toast_active(void) { return g_toast[0] && GetTickCount64() < g_toast_until + 200; }

void ui_toast_paint(void)
{
    int tw, th, x, y;
    Rgb col;
    RECT r, tr;
    if (!g_toast[0] || GetTickCount64() >= g_toast_until) return;
    tw = w_text_width(g_toast, F_UI_B) + S(52);
    th = S(40);
    x = (g_cont.left + g_cont.right - tw) / 2;
    y = g_cont.bottom - th - S(6);
    col = sev_color(g_toast_sev);
    r = rc_make(x, y, x + tw, y + th);
    tr = r;
    gfx_rrect((float)x, (float)y, (float)tw, (float)th, (float)(th / 2), rgb_mix(g_th.card2, col, 0.12f), 255);
    gfx_rrect_stroke((float)x, (float)y, (float)tw, (float)th, (float)(th / 2), 1.0f, col, 220);
    gfx_circle((float)(x + S(20)), (float)(y + th / 2), (float)S(5), col, 255);
    tr.left += S(34);
    gfx_text_rc(&tr, g_toast, F_UI_B, g_th.text, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
}
