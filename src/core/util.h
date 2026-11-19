/* util.h - strings, files, processes, http-free helpers */
#ifndef MT_UTIL_H
#define MT_UTIL_H
#include "core/common.h"

/* ---------- strings ---------- */
int      wfmt(wchar_t* buf, size_t cch, const wchar_t* fmt, ...);
int      wvfmt(wchar_t* buf, size_t cch, const wchar_t* fmt, va_list ap);
wchar_t* wdup(const wchar_t* s);
wchar_t* wdupf(const wchar_t* fmt, ...);
wchar_t* utf8_to_w(const char* s, int len);          /* malloc'd, len<0 => NUL-terminated */
char*    w_to_utf8(const wchar_t* s);                /* malloc'd */
void     wcopy(wchar_t* dst, size_t cch, const wchar_t* src);
void     wtrim(wchar_t* s);
wchar_t* wtok(wchar_t** ctx, const wchar_t* delims);   /* portable wcstok (in-place) */
BOOL     wcontains_i(const wchar_t* hay, const wchar_t* needle);
BOOL     wstarts_i(const wchar_t* s, const wchar_t* prefix);
BOOL     wends_i(const wchar_t* s, const wchar_t* suffix);
void     fmt_size(unsigned long long bytes, wchar_t* buf, size_t cch);   /* "1.23 GB" */
void     fmt_duration(unsigned long long secs, wchar_t* buf, size_t cch);
void     fmt_filetime(const FILETIME* ft, wchar_t* buf, size_t cch, BOOL with_time);
BOOL     host_is_safe(const wchar_t* s);              /* ^[A-Za-z0-9.\-:]+$ */
void     bar_string(wchar_t* buf, int width, double pct);   /* block bar of `width` chars */

/* growable byte buffer */
typedef struct Buf { char* data; size_t len, cap; } Buf;
void buf_init(Buf* b);
void buf_free(Buf* b);
void buf_add(Buf* b, const void* p, size_t n);
void buf_addc(Buf* b, char c);

/* tiny JSON helpers (flat objects only; enough for ip-api / ipify) */
BOOL json_get_str (const char* json, const char* key, char* out, size_t cch);   /* UTF-8 out */
BOOL json_get_num (const char* json, const char* key, double* out);
BOOL json_get_bool(const char* json, const char* key, BOOL* out);

/* ---------- system helpers ---------- */
BOOL is_elevated(void);
BOOL relaunch_as_admin(void);
BOOL get_folder(int csidl, wchar_t* out);            /* SHGetFolderPath wrapper */
BOOL file_exists(const wchar_t* p);
BOOL dir_exists(const wchar_t* p);
BOOL ensure_dir(const wchar_t* p);                   /* creates recursively */
void path_join(wchar_t* out, size_t cch, const wchar_t* a, const wchar_t* b);
BOOL reg_get_str(HKEY root, const wchar_t* sub, const wchar_t* name, wchar_t* out, size_t cch);
BOOL reg_get_dword(HKEY root, const wchar_t* sub, const wchar_t* name, DWORD* out);
BOOL reg_get_str_w(HKEY root, const wchar_t* sub, const wchar_t* name, REGSAM wow, wchar_t* out, size_t cch);
void open_url(const wchar_t* url);
void shell_open(const wchar_t* what, const wchar_t* params);
void open_in_explorer_select(const wchar_t* file);
BOOL copy_to_clipboard(HWND owner, const wchar_t* text);
void expand_env(const wchar_t* in, wchar_t* out, size_t cch);

/* ---------- process execution ---------- */
typedef void (*LineCb)(const wchar_t* line, void* user);
#define RC_FAILED_START  (-1)
#define RC_TIMEOUT       (-2)
#define RC_CANCELLED     (-3)
/* Runs a command line hidden, streams stdout+stderr line by line (decoded with `cp`).
   Returns process exit code or RC_*.  `raw` (optional) receives all raw bytes. */
int  run_capture(const wchar_t* cmdline, DWORD timeout_ms, UINT cp, LineCb cb, void* user,
                 volatile LONG* cancel, Buf* raw);
/* PowerShell (UTF-8 output). Script is passed with -EncodedCommand. */
int  run_ps(const wchar_t* script, DWORD timeout_ms, LineCb cb, void* user, volatile LONG* cancel, Buf* raw);
/* Convenience: whole output as malloc'd wide string (never NULL; may be empty) */
wchar_t* run_ps_text(const wchar_t* script, DWORD timeout_ms, volatile LONG* cancel, int* rc);
wchar_t* run_cmd_text(const wchar_t* cmdline, DWORD timeout_ms, UINT cp, int* rc);
/* Fire and forget (hidden) */
BOOL run_hidden_nowait(const wchar_t* cmdline);

/* ---------- time ---------- */
double qpc_seconds(void);

#endif
