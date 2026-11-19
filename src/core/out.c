/* out.c */
#include "core/out.h"
#include "core/util.h"

void out_init(MtOut* o, int log_id, int job_slot, volatile LONG* cancel)
{
    o->log_id = log_id;
    o->job_slot = job_slot;
    o->cancel = cancel;
    o->file = NULL;
}

void out_init_file(MtOut* o, HANDLE file)
{
    o->log_id = -1;
    o->job_slot = -1;
    o->cancel = NULL;
    o->file = file;
}

static BOOL file_sink(const MtOut* o) { return o->file && o->file != INVALID_HANDLE_VALUE; }

void out_text(MtOut* o, int style, const wchar_t* text)
{
    if (!text || !*text) return;
    if (file_sink(o)) {
        char* u = w_to_utf8(text);
        if (u) {
            DWORD w;
            WriteFile(o->file, u, (DWORD)strlen(u), &w, NULL);
            free(u);
        }
        return;
    }
    if (g_hwnd && o->log_id >= 0) {
        LogMsg* m = (LogMsg*)malloc(sizeof(LogMsg));
        if (!m) return;
        m->style = style;
        m->clear = 0;
        m->text = wdup(text);
        if (!PostMessageW(g_hwnd, WM_APP_LOG, (WPARAM)o->log_id, (LPARAM)m)) {
            free(m->text);
            free(m);
        }
    }
}

void out_nl(MtOut* o) { out_text(o, OS_NORMAL, L"\n"); }

void out_line(MtOut* o, int style, const wchar_t* fmt, ...)
{
    wchar_t buf[4096];
    va_list ap;
    size_t n;
    va_start(ap, fmt);
    wvfmt(buf, MT_ARRAY(buf) - 2, fmt, ap);
    va_end(ap);
    n = wcslen(buf);
    buf[n] = L'\n';
    buf[n + 1] = 0;
    out_text(o, style, buf);
}

static void rep(wchar_t* b, wchar_t c, int n)
{
    int i;
    for (i = 0; i < n; i++) b[i] = c;
    b[n] = 0;
}

void out_sep(MtOut* o)
{
    wchar_t b[80];
    rep(b, 0x2500, 64);
    out_line(o, OS_DIM, L"%ls", b);
}

void out_title(MtOut* o, const wchar_t* title)
{
    wchar_t b[80];
    rep(b, 0x2550, 64);
    out_line(o, OS_DIM, L"%ls", b);
    out_line(o, OS_TITLE, L"  %ls", title);
    out_line(o, OS_DIM, L"%ls", b);
}

void out_section(MtOut* o, const wchar_t* title)
{
    wchar_t b[80];
    out_nl(o);
    rep(b, 0x2500, 64);
    out_line(o, OS_ACCENT, L"  %ls", title);
    out_line(o, OS_DIM, L"%ls", b);
}

static void kv_core(MtOut* o, const wchar_t* key, int vstyle, const wchar_t* fmt, va_list ap)
{
    wchar_t k[160], v[3072];
    wfmt(k, MT_ARRAY(k), L"  %-26ls", key);
    if (wcslen(key) >= 26) wcscat(k, L" ");
    wvfmt(v, MT_ARRAY(v), fmt, ap);
    out_text(o, OS_KEY, k);
    out_text(o, vstyle, v);
    out_text(o, OS_NORMAL, L"\n");
}

void out_kv(MtOut* o, const wchar_t* key, const wchar_t* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    kv_core(o, key, OS_NORMAL, fmt, ap);
    va_end(ap);
}

void out_kv_style(MtOut* o, const wchar_t* key, int vstyle, const wchar_t* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    kv_core(o, key, vstyle, fmt, ap);
    va_end(ap);
}

#define DEF_PREFIXED(NAME, STYLE, PREFIX) \
void NAME(MtOut* o, const wchar_t* fmt, ...) \
{ \
    wchar_t b[4096]; \
    va_list ap; \
    va_start(ap, fmt); \
    wvfmt(b, MT_ARRAY(b), fmt, ap); \
    va_end(ap); \
    out_text(o, STYLE, PREFIX); \
    out_text(o, STYLE, b); \
    out_text(o, STYLE, L"\n"); \
}

DEF_PREFIXED(out_ok,   OS_OK,   L"  " U_CHECK L" ")
DEF_PREFIXED(out_warn, OS_WARN, L"  " U_WARNSIGN L" ")
DEF_PREFIXED(out_err,  OS_ERR,  L"  " U_CROSS L" ")
DEF_PREFIXED(out_info, OS_INFO, L"  " U_ARROW L" ")

int pct_style(double pct)
{
    return pct > 85.0 ? OS_ERR : (pct > 70.0 ? OS_WARN : OS_OK);
}

void out_bar(MtOut* o, const wchar_t* label, double pct, int width, const wchar_t* suffix)
{
    wchar_t bar[128], line[512];
    if (width > 100) width = 100;
    bar_string(bar, width, pct);
    wfmt(line, MT_ARRAY(line), L"  %-12ls [%ls] %5.1f%%%ls%ls\n", label ? label : L"", bar, pct,
         (suffix && *suffix) ? L"  " : L"", suffix ? suffix : L"");
    out_text(o, pct_style(pct), line);
}

void out_progress(MtOut* o, double pct)
{
    if (g_hwnd && o->job_slot >= 0 && !file_sink(o))
        PostMessageW(g_hwnd, WM_APP_PROGRESS, (WPARAM)o->job_slot, (LPARAM)(int)(pct * 10.0));
}

void log_post_clear(int log_id)
{
    LogMsg* m = (LogMsg*)malloc(sizeof(LogMsg));
    if (!m) return;
    m->style = OS_NORMAL;
    m->clear = 1;
    m->text = NULL;
    if (!PostMessageW(g_hwnd, WM_APP_LOG, (WPARAM)log_id, (LPARAM)m)) free(m);
}
