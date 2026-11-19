/* ui/internal.h - helpers shared between UI modules: background jobs, toasts, dialogs, edit controls */
#ifndef MT_UI_INTERNAL_H
#define MT_UI_INTERNAL_H
#include "ui/ui.h"
#include "core/util.h"

/* ---- background jobs: one slot per long-running activity ---- */
enum { JS_FP0 = 0, JS_SCAN = 5, JS_DEFENDER = 6, JS_DRIVER = 7, JS_APPS = 8, JS_STATUS = 9, JS_COUNT = 10 };
/* payload kinds of WM_APP_TABLE */
enum { TBL_APPS = 1, TBL_DRIVERS = 2, TBL_DEFSTATUS = 3 };

typedef struct Job { volatile LONG running; volatile LONG cancel; HANDLE th; int progress10; } Job;
extern Job g_jobs[JS_COUNT];
typedef unsigned (__stdcall *JobFn)(void*);
BOOL job_run(int slot, JobFn fn, void* arg);      /* FALSE if the slot is busy */
void job_stop(int slot);                          /* request cancellation */
void job_finish(int slot);                        /* called on WM_APP_DONE */
BOOL job_busy(int slot);

/* ---- toast notifications ---- */
void ui_toast(int sev, const wchar_t* fmt, ...);  /* sev: 0 ok, 1 info, 2 warn, 3 orange, 4 bad */
void ui_toast_paint(void);
BOOL ui_toast_active(void);

/* ---- misc helpers ---- */
RECT rc_make(int l, int t, int r, int b);
void edit_place(HWND ed, const RECT* frame, BOOL show);
void edit_get(HWND ed, wchar_t* out, size_t cch);
void edit_set(HWND ed, const wchar_t* s);
void fmt_rate(double bytes_per_sec, wchar_t* out, size_t cch);
BOOL pick_file(wchar_t* out, size_t cch, const wchar_t* title);
BOOL pick_folder(wchar_t* out, size_t cch, const wchar_t* title);
BOOL confirm_box(const wchar_t* text);

#endif
