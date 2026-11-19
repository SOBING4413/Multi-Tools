/* util.c - implementation of util.h */
#include "core/util.h"
#include <shlobj.h>
#include <shellapi.h>
#include <process.h>

/* ======================= strings ======================= */
int wvfmt(wchar_t* buf, size_t cch, const wchar_t* fmt, va_list ap)
{
    int n;
    if (!buf || cch == 0) return 0;
    n = _vsnwprintf(buf, cch, fmt, ap);
    buf[cch - 1] = 0;
    return n;
}

int wfmt(wchar_t* buf, size_t cch, const wchar_t* fmt, ...)
{
    int n;
    va_list ap;
    va_start(ap, fmt);
    n = wvfmt(buf, cch, fmt, ap);
    va_end(ap);
    return n;
}

wchar_t* wdup(const wchar_t* s)
{
    size_t n;
    wchar_t* r;
    if (!s) s = L"";
    n = wcslen(s);
    r = (wchar_t*)malloc((n + 1) * sizeof(wchar_t));
    if (r) memcpy(r, s, (n + 1) * sizeof(wchar_t));
    return r;
}

wchar_t* wdupf(const wchar_t* fmt, ...)
{
    wchar_t tmp[2048];
    va_list ap;
    va_start(ap, fmt);
    wvfmt(tmp, MT_ARRAY(tmp), fmt, ap);
    va_end(ap);
    return wdup(tmp);
}

wchar_t* utf8_to_w(const char* s, int len)
{
    int n;
    wchar_t* r;
    if (!s) s = "";
    if (len < 0) len = (int)strlen(s);
    n = MultiByteToWideChar(CP_UTF8, 0, s, len, NULL, 0);
    r = (wchar_t*)malloc((size_t)(n + 1) * sizeof(wchar_t));
    if (!r) return NULL;
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s, len, r, n);
    r[n] = 0;
    return r;
}

char* w_to_utf8(const wchar_t* s)
{
    int n;
    char* r;
    if (!s) s = L"";
    n = WideCharToMultiByte(CP_UTF8, 0, s, -1, NULL, 0, NULL, NULL);
    r = (char*)malloc((size_t)n + 1);
    if (!r) return NULL;
    WideCharToMultiByte(CP_UTF8, 0, s, -1, r, n, NULL, NULL);
    r[n] = 0;
    return r;
}

void wcopy(wchar_t* dst, size_t cch, const wchar_t* src)
{
    if (!dst || cch == 0) return;
    if (!src) src = L"";
    wcsncpy(dst, src, cch - 1);
    dst[cch - 1] = 0;
}

void wtrim(wchar_t* s)
{
    size_t n, i = 0;
    if (!s) return;
    n = wcslen(s);
    while (n > 0 && (s[n - 1] == L' ' || s[n - 1] == L'\t' || s[n - 1] == L'\r' || s[n - 1] == L'\n')) s[--n] = 0;
    while (s[i] == L' ' || s[i] == L'\t') i++;
    if (i) memmove(s, s + i, (n - i + 1) * sizeof(wchar_t));
}

wchar_t* wtok(wchar_t** ctx, const wchar_t* delims)
{
    wchar_t* s = *ctx;
    wchar_t* start;
    if (!s) return NULL;
    while (*s && wcschr(delims, *s)) s++;
    if (!*s) { *ctx = NULL; return NULL; }
    start = s;
    while (*s && !wcschr(delims, *s)) s++;
    if (*s) { *s = 0; *ctx = s + 1; } else *ctx = NULL;
    return start;
}

BOOL wcontains_i(const wchar_t* hay, const wchar_t* needle)
{
    size_t nl, hl, i;
    if (!needle || !*needle) return TRUE;
    if (!hay) return FALSE;
    nl = wcslen(needle);
    hl = wcslen(hay);
    if (nl > hl) return FALSE;
    for (i = 0; i + nl <= hl; i++)
        if (CompareStringOrdinal(hay + i, (int)nl, needle, (int)nl, TRUE) == CSTR_EQUAL) return TRUE;
    return FALSE;
}

BOOL wstarts_i(const wchar_t* s, const wchar_t* prefix)
{
    size_t pl = wcslen(prefix);
    if (wcslen(s) < pl) return FALSE;
    return CompareStringOrdinal(s, (int)pl, prefix, (int)pl, TRUE) == CSTR_EQUAL;
}

BOOL wends_i(const wchar_t* s, const wchar_t* suffix)
{
    size_t sl = wcslen(s), xl = wcslen(suffix);
    if (sl < xl) return FALSE;
    return CompareStringOrdinal(s + sl - xl, (int)xl, suffix, (int)xl, TRUE) == CSTR_EQUAL;
}

void fmt_size(unsigned long long bytes, wchar_t* buf, size_t cch)
{
    static const wchar_t* units[] = { L"B", L"KB", L"MB", L"GB", L"TB", L"PB" };
    double v = (double)bytes;
    int u = 0;
    while (v >= 1024.0 && u < 5) { v /= 1024.0; u++; }
    if (u == 0) wfmt(buf, cch, L"%.0f %ls", v, units[u]);
    else        wfmt(buf, cch, L"%.2f %ls", v, units[u]);
}

void fmt_duration(unsigned long long secs, wchar_t* buf, size_t cch)
{
    unsigned long long d = secs / 86400ULL, h = (secs % 86400ULL) / 3600ULL,
                       m = (secs % 3600ULL) / 60ULL, s = secs % 60ULL;
    if (d) wfmt(buf, cch, L"%I64u d %I64u h %I64u m %I64u s", d, h, m, s);
    else   wfmt(buf, cch, L"%I64u h %I64u m %I64u s", h, m, s);
}

void fmt_filetime(const FILETIME* ft, wchar_t* buf, size_t cch, BOOL with_time)
{
    SYSTEMTIME st, lt;
    if (!ft || (ft->dwLowDateTime == 0 && ft->dwHighDateTime == 0) || !FileTimeToSystemTime(ft, &st)) {
        wcopy(buf, cch, L"-");
        return;
    }
    if (!SystemTimeToTzSpecificLocalTime(NULL, &st, &lt)) lt = st;
    if (with_time) wfmt(buf, cch, L"%04d-%02d-%02d %02d:%02d", lt.wYear, lt.wMonth, lt.wDay, lt.wHour, lt.wMinute);
    else           wfmt(buf, cch, L"%04d-%02d-%02d", lt.wYear, lt.wMonth, lt.wDay);
}

BOOL host_is_safe(const wchar_t* s)
{
    size_t i, n;
    if (!s) return FALSE;
    n = wcslen(s);
    if (n == 0 || n > 253) return FALSE;
    for (i = 0; i < n; i++) {
        wchar_t c = s[i];
        if (!((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9') ||
              c == L'.' || c == L'-' || c == L':')) return FALSE;
    }
    return TRUE;
}

void bar_string(wchar_t* buf, int width, double pct)
{
    int filled, i;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    filled = (int)(width * pct / 100.0);
    for (i = 0; i < width; i++) buf[i] = (i < filled) ? 0x2588 : 0x2591;
    buf[width] = 0;
}

/* ======================= Buf ======================= */
void buf_init(Buf* b) { b->data = NULL; b->len = b->cap = 0; }
void buf_free(Buf* b) { free(b->data); b->data = NULL; b->len = b->cap = 0; }
void buf_add(Buf* b, const void* p, size_t n)
{
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap : 1024;
        char* nd;
        while (nc < b->len + n + 1) nc *= 2;
        nd = (char*)realloc(b->data, nc);
        if (!nd) return;
        b->data = nd;
        b->cap = nc;
    }
    memcpy(b->data + b->len, p, n);
    b->len += n;
    b->data[b->len] = 0;
}
void buf_addc(Buf* b, char c) { buf_add(b, &c, 1); }

/* ======================= JSON ======================= */
static const char* json_find_value(const char* json, const char* key)
{
    char pat[128];
    const char* p = json;
    size_t kl;
    if (!json || !key) return NULL;
    _snprintf(pat, sizeof(pat) - 1, "\"%s\"", key);
    pat[sizeof(pat) - 1] = 0;
    kl = strlen(pat);
    while ((p = strstr(p, pat)) != NULL) {
        const char* q = p + kl;
        while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n') q++;
        if (*q == ':') {
            q++;
            while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n') q++;
            return q;
        }
        p += kl;
    }
    return NULL;
}

static void utf8_put(char** o, char* end, unsigned cp)
{
    char* p = *o;
    if (cp < 0x80) { if (p + 1 < end) *p++ = (char)cp; }
    else if (cp < 0x800) { if (p + 2 < end) { *p++ = (char)(0xC0 | (cp >> 6)); *p++ = (char)(0x80 | (cp & 0x3F)); } }
    else if (cp < 0x10000) { if (p + 3 < end) { *p++ = (char)(0xE0 | (cp >> 12)); *p++ = (char)(0x80 | ((cp >> 6) & 0x3F)); *p++ = (char)(0x80 | (cp & 0x3F)); } }
    else { if (p + 4 < end) { *p++ = (char)(0xF0 | (cp >> 18)); *p++ = (char)(0x80 | ((cp >> 12) & 0x3F)); *p++ = (char)(0x80 | ((cp >> 6) & 0x3F)); *p++ = (char)(0x80 | (cp & 0x3F)); } }
    *o = p;
}

static int hex4(const char* s, unsigned* out)
{
    unsigned v = 0;
    int i;
    for (i = 0; i < 4; i++) {
        char c = s[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
        else return 0;
    }
    *out = v;
    return 1;
}

BOOL json_get_str(const char* json, const char* key, char* out, size_t cch)
{
    const char* p = json_find_value(json, key);
    char* o = out;
    char* end = out + cch;
    if (!p || *p != '"' || cch == 0) { if (cch) out[0] = 0; return FALSE; }
    p++;
    while (*p && *p != '"') {
        if (*p == '\\') {
            p++;
            switch (*p) {
            case 'n': utf8_put(&o, end, '\n'); p++; break;
            case 't': utf8_put(&o, end, '\t'); p++; break;
            case 'r': p++; break;
            case 'b': case 'f': p++; break;
            case 'u': {
                unsigned cp, lo;
                if (hex4(p + 1, &cp)) {
                    p += 5;
                    if (cp >= 0xD800 && cp < 0xDC00 && p[0] == '\\' && p[1] == 'u' && hex4(p + 2, &lo) && lo >= 0xDC00 && lo < 0xE000) {
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        p += 6;
                    }
                    utf8_put(&o, end, cp);
                } else p++;
                break; }
            case 0: break;
            default: utf8_put(&o, end, (unsigned char)*p); p++; break;
            }
        } else {
            if (o + 1 < end) *o++ = *p;
            p++;
        }
    }
    *o = 0;
    return TRUE;
}

BOOL json_get_num(const char* json, const char* key, double* out)
{
    const char* p = json_find_value(json, key);
    char* e;
    double v;
    if (!p) return FALSE;
    v = strtod(p, &e);
    if (e == p) return FALSE;
    *out = v;
    return TRUE;
}

BOOL json_get_bool(const char* json, const char* key, BOOL* out)
{
    const char* p = json_find_value(json, key);
    if (!p) return FALSE;
    if (strncmp(p, "true", 4) == 0) { *out = TRUE; return TRUE; }
    if (strncmp(p, "false", 5) == 0) { *out = FALSE; return TRUE; }
    return FALSE;
}

/* ======================= system helpers ======================= */
BOOL is_elevated(void)
{
    HANDLE tok = NULL;
    TOKEN_ELEVATION te;
    DWORD sz = sizeof(te);
    BOOL res = FALSE;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) {
        if (GetTokenInformation(tok, TokenElevation, &te, sizeof(te), &sz)) res = te.TokenIsElevated != 0;
        CloseHandle(tok);
    }
    return res;
}

BOOL relaunch_as_admin(void)
{
    HINSTANCE r = ShellExecuteW(NULL, L"runas", g_exe_path, NULL, g_exe_dir, SW_SHOWNORMAL);
    return (INT_PTR)r > 32;
}

BOOL get_folder(int csidl, wchar_t* out)
{
    out[0] = 0;
    return SUCCEEDED(SHGetFolderPathW(NULL, csidl | CSIDL_FLAG_CREATE, NULL, SHGFP_TYPE_CURRENT, out));
}

BOOL file_exists(const wchar_t* p)
{
    DWORD a = GetFileAttributesW(p);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

BOOL dir_exists(const wchar_t* p)
{
    DWORD a = GetFileAttributesW(p);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

BOOL ensure_dir(const wchar_t* p)
{
    wchar_t tmp[MAX_PATH * 2];
    size_t i, n;
    wcopy(tmp, MT_ARRAY(tmp), p);
    n = wcslen(tmp);
    for (i = 3; i < n; i++) {
        if (tmp[i] == L'\\' || tmp[i] == L'/') {
            tmp[i] = 0;
            CreateDirectoryW(tmp, NULL);
            tmp[i] = L'\\';
        }
    }
    CreateDirectoryW(tmp, NULL);
    return dir_exists(tmp);
}

void path_join(wchar_t* out, size_t cch, const wchar_t* a, const wchar_t* b)
{
    size_t n;
    wcopy(out, cch, a);
    n = wcslen(out);
    if (n && out[n - 1] != L'\\' && out[n - 1] != L'/') wcsncat(out, L"\\", cch - n - 1);
    wcsncat(out, b, cch - wcslen(out) - 1);
}

BOOL reg_get_str_w(HKEY root, const wchar_t* sub, const wchar_t* name, REGSAM wow, wchar_t* out, size_t cch)
{
    HKEY k;
    DWORD type = 0, sz = (DWORD)((cch - 1) * sizeof(wchar_t));
    LONG r;
    out[0] = 0;
    if (RegOpenKeyExW(root, sub, 0, KEY_READ | wow, &k) != ERROR_SUCCESS) return FALSE;
    r = RegQueryValueExW(k, name, NULL, &type, (LPBYTE)out, &sz);
    RegCloseKey(k);
    if (r != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) { out[0] = 0; return FALSE; }
    out[sz / sizeof(wchar_t)] = 0;
    out[cch - 1] = 0;
    return TRUE;
}

BOOL reg_get_str(HKEY root, const wchar_t* sub, const wchar_t* name, wchar_t* out, size_t cch)
{
    return reg_get_str_w(root, sub, name, 0, out, cch);
}

BOOL reg_get_dword(HKEY root, const wchar_t* sub, const wchar_t* name, DWORD* out)
{
    HKEY k;
    DWORD type = 0, sz = sizeof(DWORD), v = 0;
    LONG r;
    if (RegOpenKeyExW(root, sub, 0, KEY_READ, &k) != ERROR_SUCCESS) return FALSE;
    r = RegQueryValueExW(k, name, NULL, &type, (LPBYTE)&v, &sz);
    RegCloseKey(k);
    if (r != ERROR_SUCCESS || type != REG_DWORD) return FALSE;
    *out = v;
    return TRUE;
}

void open_url(const wchar_t* url) { ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOWNORMAL); }

void shell_open(const wchar_t* what, const wchar_t* params)
{
    ShellExecuteW(NULL, L"open", what, params, NULL, SW_SHOWNORMAL);
}

void open_in_explorer_select(const wchar_t* file)
{
    wchar_t arg[MAX_PATH * 2 + 32];
    wfmt(arg, MT_ARRAY(arg), L"/select,\"%ls\"", file);
    ShellExecuteW(NULL, L"open", L"explorer.exe", arg, NULL, SW_SHOWNORMAL);
}

BOOL copy_to_clipboard(HWND owner, const wchar_t* text)
{
    size_t n = wcslen(text) + 1;
    HGLOBAL h;
    wchar_t* p;
    if (!OpenClipboard(owner)) return FALSE;
    EmptyClipboard();
    h = GlobalAlloc(GMEM_MOVEABLE, n * sizeof(wchar_t));
    if (!h) { CloseClipboard(); return FALSE; }
    p = (wchar_t*)GlobalLock(h);
    memcpy(p, text, n * sizeof(wchar_t));
    GlobalUnlock(h);
    SetClipboardData(CF_UNICODETEXT, h);
    CloseClipboard();
    return TRUE;
}

void expand_env(const wchar_t* in, wchar_t* out, size_t cch)
{
    DWORD r = ExpandEnvironmentStringsW(in, out, (DWORD)cch);
    if (r == 0 || r > cch) wcopy(out, cch, in);
}

/* ======================= process execution ======================= */
static void emit_line(Buf* line, UINT cp, LineCb cb, void* user)
{
    wchar_t* w;
    size_t n;
    if (!cb) { line->len = 0; return; }
    while (line->len && (line->data[line->len - 1] == '\r' || line->data[line->len - 1] == '\n')) line->len--;
    if (line->len == 0) {
        cb(L"", user);
        return;
    }
    {
        int wl = MultiByteToWideChar(cp, 0, line->data, (int)line->len, NULL, 0);
        w = (wchar_t*)malloc((size_t)(wl + 1) * sizeof(wchar_t));
        if (!w) { line->len = 0; return; }
        if (wl > 0) MultiByteToWideChar(cp, 0, line->data, (int)line->len, w, wl);
        w[wl] = 0;
        n = (size_t)wl;
        /* strip a leading BOM */
        if (n && w[0] == 0xFEFF) memmove(w, w + 1, n * sizeof(wchar_t));
        cb(w, user);
        free(w);
    }
    line->len = 0;
}

int run_capture(const wchar_t* cmdline, DWORD timeout_ms, UINT cp, LineCb cb, void* user,
                volatile LONG* cancel, Buf* raw)
{
    SECURITY_ATTRIBUTES sa;
    HANDLE rd = NULL, wr = NULL, nul = INVALID_HANDLE_VALUE;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    wchar_t* cmd;
    BOOL ok;
    DWORD start = GetTickCount();
    int rc = 0;
    Buf line;
    char chunk[4096];
    BOOL done = FALSE;

    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return RC_FAILED_START;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = wr;
    si.hStdError = wr;
    si.hStdInput = (nul != INVALID_HANDLE_VALUE) ? nul : GetStdHandle(STD_INPUT_HANDLE);
    ZeroMemory(&pi, sizeof(pi));

    cmd = wdup(cmdline);
    ok = CreateProcessW(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    free(cmd);
    CloseHandle(wr);
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    if (!ok) { CloseHandle(rd); return RC_FAILED_START; }

    buf_init(&line);
    while (!done) {
        DWORD avail = 0, got = 0;
        if (cancel && *cancel) {
            TerminateProcess(pi.hProcess, 1);
            rc = RC_CANCELLED;
            break;
        }
        if (timeout_ms && (GetTickCount() - start) > timeout_ms) {
            TerminateProcess(pi.hProcess, 1);
            rc = RC_TIMEOUT;
            break;
        }
        if (!PeekNamedPipe(rd, NULL, 0, NULL, &avail, NULL)) {
            done = TRUE;   /* pipe closed */
            avail = 0;
        }
        if (avail > 0) {
            DWORD want = avail > sizeof(chunk) ? (DWORD)sizeof(chunk) : avail;
            if (ReadFile(rd, chunk, want, &got, NULL) && got > 0) {
                DWORD i;
                if (raw) buf_add(raw, chunk, got);
                for (i = 0; i < got; i++) {
                    if (chunk[i] == '\n') emit_line(&line, cp, cb, user);
                    else buf_addc(&line, chunk[i]);
                }
            }
        } else if (!done) {
            if (WaitForSingleObject(pi.hProcess, 25) == WAIT_OBJECT_0) {
                /* drain whatever is left */
                for (;;) {
                    DWORD av2 = 0, g2 = 0, i;
                    if (!PeekNamedPipe(rd, NULL, 0, NULL, &av2, NULL) || av2 == 0) break;
                    if (!ReadFile(rd, chunk, av2 > sizeof(chunk) ? (DWORD)sizeof(chunk) : av2, &g2, NULL) || g2 == 0) break;
                    if (raw) buf_add(raw, chunk, g2);
                    for (i = 0; i < g2; i++) {
                        if (chunk[i] == '\n') emit_line(&line, cp, cb, user);
                        else buf_addc(&line, chunk[i]);
                    }
                }
                done = TRUE;
            }
        }
    }
    if (line.len) emit_line(&line, cp, cb, user);
    buf_free(&line);

    if (rc == 0) {
        DWORD ec = 0;
        WaitForSingleObject(pi.hProcess, 2000);
        GetExitCodeProcess(pi.hProcess, &ec);
        rc = (int)ec;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(rd);
    return rc;
}

static wchar_t* b64_utf16(const wchar_t* s)
{
    static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t n = wcslen(s) * 2, i, o = 0;
    const unsigned char* d = (const unsigned char*)s;
    wchar_t* out = (wchar_t*)malloc(((n + 2) / 3 * 4 + 1) * sizeof(wchar_t));
    if (!out) return NULL;
    for (i = 0; i < n; i += 3) {
        unsigned v = (unsigned)d[i] << 16;
        if (i + 1 < n) v |= (unsigned)d[i + 1] << 8;
        if (i + 2 < n) v |= d[i + 2];
        out[o++] = (wchar_t)T[(v >> 18) & 63];
        out[o++] = (wchar_t)T[(v >> 12) & 63];
        out[o++] = (i + 1 < n) ? (wchar_t)T[(v >> 6) & 63] : L'=';
        out[o++] = (i + 2 < n) ? (wchar_t)T[v & 63] : L'=';
    }
    out[o] = 0;
    return out;
}

int run_ps(const wchar_t* script, DWORD timeout_ms, LineCb cb, void* user, volatile LONG* cancel, Buf* raw)
{
    static const wchar_t* prefix =
        L"[Console]::OutputEncoding=New-Object System.Text.UTF8Encoding $false;"
        L"$ProgressPreference='SilentlyContinue';$ErrorActionPreference='SilentlyContinue';";
    size_t n = wcslen(prefix) + wcslen(script) + 1;
    wchar_t* full = (wchar_t*)malloc(n * sizeof(wchar_t));
    wchar_t* b64;
    wchar_t* cmd;
    int rc;
    if (!full) return RC_FAILED_START;
    wcscpy(full, prefix);
    wcscat(full, script);
    b64 = b64_utf16(full);
    free(full);
    if (!b64) return RC_FAILED_START;
    cmd = (wchar_t*)malloc((wcslen(b64) + 160) * sizeof(wchar_t));
    if (!cmd) { free(b64); return RC_FAILED_START; }
    wcscpy(cmd, L"powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand ");
    wcscat(cmd, b64);
    free(b64);
    rc = run_capture(cmd, timeout_ms, CP_UTF8, cb, user, cancel, raw);
    free(cmd);
    return rc;
}

typedef struct TextAcc { Buf b; } TextAcc;
static void acc_line(const wchar_t* line, void* u)
{
    TextAcc* a = (TextAcc*)u;
    char* s = w_to_utf8(line);
    if (s) { buf_add(&a->b, s, strlen(s)); buf_addc(&a->b, '\n'); free(s); }
}

wchar_t* run_ps_text(const wchar_t* script, DWORD timeout_ms, volatile LONG* cancel, int* rc)
{
    TextAcc a;
    wchar_t* w;
    int r;
    buf_init(&a.b);
    r = run_ps(script, timeout_ms, acc_line, &a, cancel, NULL);
    if (rc) *rc = r;
    w = utf8_to_w(a.b.data ? a.b.data : "", -1);
    buf_free(&a.b);
    return w ? w : wdup(L"");
}

wchar_t* run_cmd_text(const wchar_t* cmdline, DWORD timeout_ms, UINT cp, int* rc)
{
    TextAcc a;
    wchar_t* w;
    int r;
    buf_init(&a.b);
    r = run_capture(cmdline, timeout_ms, cp, acc_line, &a, NULL, NULL);
    if (rc) *rc = r;
    w = utf8_to_w(a.b.data ? a.b.data : "", -1);
    buf_free(&a.b);
    return w ? w : wdup(L"");
}

BOOL run_hidden_nowait(const wchar_t* cmdline)
{
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    wchar_t* cmd = wdup(cmdline);
    BOOL ok;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    ok = CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    free(cmd);
    if (ok) { CloseHandle(pi.hProcess); CloseHandle(pi.hThread); }
    return ok;
}

double qpc_seconds(void)
{
    static LARGE_INTEGER f;
    LARGE_INTEGER c;
    if (!f.QuadPart) QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)f.QuadPart;
}
