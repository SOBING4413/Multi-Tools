/* common.h - shared includes, constants and small helpers for MultiTool */
#ifndef MT_COMMON_H
#define MT_COMMON_H

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef WINVER
#define WINVER 0x0A00
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <objbase.h>
#include <shellapi.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <stdarg.h>
#include <stdint.h>
#include <math.h>

#include "i18n/i18n_keys.h"

/* ---- product info ---- */
#define MT_NAME      L"MultiTool"
#include "core/version.h"
#define MT_AUTHOR    L"Exter Interactive"
#define MT_DEVELOPER L"ALIF HAKIM"
#define MT_ORIGIN    L"Sobing4413"
#define MT_GITHUB    L"https://github.com/SOBING4413"
#define MT_DISCORD   L"https://discord.gg/9nsub2yx4V"

#define MT_PI 3.14159265358979323846f
#define MT_MIN(a,b) ((a)<(b)?(a):(b))
#define MT_MAX(a,b) ((a)>(b)?(a):(b))
#define MT_CLAMP(v,lo,hi) ((v)<(lo)?(lo):((v)>(hi)?(hi):(v)))
#define MT_ARRAY(a) ((int)(sizeof(a)/sizeof((a)[0])))

/* ---- unicode glyphs (kept as macros so sources stay pure ASCII) ---- */
#define U_BLOCK_FULL  L"\x2588"
#define U_BLOCK_LIGHT L"\x2591"
#define U_HLINE       L"\x2500"
#define U_HLINE2      L"\x2550"
#define U_BULLET      L"\x25CF"
#define U_ARROW       L"\x25BA"
#define U_CHECK       L"\x2714"
#define U_CROSS       L"\x2718"
#define U_WARNSIGN    L"\x26A0"
#define U_UP          L"\x2191"
#define U_DOWN        L"\x2193"
#define U_DEGREE      L"\x00B0"
#define U_ELLIPSIS    L"\x2026"
#define U_DOT         L"\x00B7"
#define U_COPY        L"\x00A9"

/* ---- custom window messages (posted from worker threads) ---- */
#define WM_APP_LOG       (WM_APP + 1)   /* wParam = log id, lParam = LogMsg*        */
#define WM_APP_DONE      (WM_APP + 2)   /* wParam = job slot                         */
#define WM_APP_PROGRESS  (WM_APP + 3)   /* wParam = job slot, lParam = percent*10    */
#define WM_APP_TABLE     (WM_APP + 4)   /* wParam = table kind, lParam = TableData*  */
#define WM_APP_SCAN      (WM_APP + 5)   /* malware scan events                       */
#define WM_APP_STATUS    (WM_APP + 6)   /* wParam = kind, lParam = malloc'd wchar_t* */

/* ---- globals ---- */
extern HINSTANCE g_hInst;
extern HWND      g_hwnd;
extern BOOL      g_admin;
extern wchar_t   g_exe_path[MAX_PATH];
extern wchar_t   g_exe_dir[MAX_PATH];
extern wchar_t   g_data_dir[MAX_PATH];

/* ---- i18n ---- */
extern int g_lang;                                   /* index into g_lang_code */
const wchar_t* TX(I18nKey key);                      /* translated original key */
const wchar_t* T2(const wchar_t* en, const wchar_t* id); /* en / id (id also for su, jv) */
int  lang_from_code(const wchar_t* code);
void lang_set(int idx);

#endif
