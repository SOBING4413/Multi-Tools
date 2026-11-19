/* out.h - thread-safe styled output used by every feature (UI log or report file) */
#ifndef MT_OUT_H
#define MT_OUT_H
#include "core/common.h"

enum { LOG_SYSTEM, LOG_NETWORK, LOG_CLEANUP, LOG_SECURITY, LOG_TOOLS, LOG_MALWARE, LOG_DRIVER, LOG_COUNT };

enum { OS_NORMAL, OS_DIM, OS_ACCENT, OS_OK, OS_WARN, OS_ERR, OS_INFO, OS_TITLE, OS_KEY };

typedef struct LogMsg {
    int      style;
    int      clear;      /* 1 => clear the view instead of appending */
    wchar_t* text;       /* malloc'd, freed by receiver */
} LogMsg;

typedef struct MtOut {
    int            log_id;
    int            job_slot;     /* for progress messages, -1 if none */
    volatile LONG* cancel;       /* may be NULL */
    HANDLE         file;         /* if != NULL/INVALID, write UTF-8 text here instead of the UI */
} MtOut;

void out_init(MtOut* o, int log_id, int job_slot, volatile LONG* cancel);
void out_init_file(MtOut* o, HANDLE file);
#define OUT_CANCELLED(o) ((o)->cancel && *((o)->cancel))

void out_text(MtOut* o, int style, const wchar_t* text);                    /* no newline */
void out_line(MtOut* o, int style, const wchar_t* fmt, ...);                /* with newline */
void out_nl(MtOut* o);
void out_title(MtOut* o, const wchar_t* title);                             /* boxed title */
void out_section(MtOut* o, const wchar_t* title);
void out_sep(MtOut* o);
void out_kv(MtOut* o, const wchar_t* key, const wchar_t* fmt, ...);
void out_kv_style(MtOut* o, const wchar_t* key, int vstyle, const wchar_t* fmt, ...);
void out_ok(MtOut* o, const wchar_t* fmt, ...);
void out_warn(MtOut* o, const wchar_t* fmt, ...);
void out_err(MtOut* o, const wchar_t* fmt, ...);
void out_info(MtOut* o, const wchar_t* fmt, ...);
void out_bar(MtOut* o, const wchar_t* label, double pct, int width, const wchar_t* suffix);
void out_progress(MtOut* o, double pct);   /* 0..100, <0 hides */

/* UI thread helpers */
void log_post_clear(int log_id);
int  pct_style(double pct);              /* >85 err, >70 warn, else ok */

#endif
