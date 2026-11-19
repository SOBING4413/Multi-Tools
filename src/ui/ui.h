/* ui.h - shared UI declarations */
#ifndef MT_UI_H
#define MT_UI_H
#include "core/common.h"
#include "gfx/gfx.h"
#include "core/out.h"
#include "features/feat.h"

enum { PG_DASH, PG_MONITOR, PG_POWER, PG_SYSTEM, PG_PROCS, PG_NETWORK, PG_SECURITY, PG_MALWARE, PG_DRIVERS, PG_CLEANUP, PG_TOOLS, PG_SETTINGS, PG_ABOUT, PG_COUNT };

extern int  g_page;
extern int  g_cw, g_ch;          /* client size in pixels */
extern RECT g_cont;              /* content rectangle of the current page */
extern BOOL g_dirty;             /* request a repaint on next tick */

/* ------------------------------------------------------------------ hot spots */
#define HOT_MAX 1024
void  hot_reset(void);
void  hot_add(const RECT* r, int id);
int   hot_at(int x, int y);
BOOL  hot_tick(void);            /* animate hover values; TRUE if something moved */
extern int   g_hover, g_pressed;
extern float g_hv[HOT_MAX];

/* hot ids */
#define HID_NAV0      100        /* +page */
#define HID_EXIT      130
#define HID_ADMIN     131
#define HID_PILL0     200        /* feature tabs, up to 30 */
#define HID_OPT0      240        /* option segments, up to 16 */
#define HID_RUN       260
#define HID_STOP      261
#define HID_CLEAR     262
#define HID_COPY      263
#define HID_SAVE      264
#define HID_BROWSE    265
#define HID_TILE0     300        /* grid tiles, up to 100 */
#define HID_THEME0    400        /* 6 */
#define HID_LANG0     410        /* 18 */
#define HID_ANIM      430
#define HID_AUTOREF   431
#define HID_OPENDATA  432
#define HID_PW_MODE0  440        /* 4 */
#define HID_PW_CHIP0  445        /* 5 */
#define HID_PW_START  450
#define HID_PW_CANCEL 451
#define HID_PW_LOCK   452
#define HID_PW_SLEEP  453
#define HID_PW_HIB    454
#define HID_DB_REPORT 460
#define HID_DB_SCAN   461
#define HID_DB_DRV    462
#define HID_DB_CLEAN  463
#define HID_MON_PAUSE 470
#define HID_PR_TAB0   480        /* 3 */
#define HID_PR_REFRESH 485
#define HID_PR_KILL   486
#define HID_PR_KILLALL 487
#define HID_PR_AUTO   488
#define HID_PR_OPEN   489
#define HID_PR_UNINST 490
#define HID_MW_QUICK  500
#define HID_MW_FULL   501
#define HID_MW_FILE   502
#define HID_MW_FOLDER 503
#define HID_MW_STOP   504
#define HID_MW_DUPD   505
#define HID_MW_DQUICK 506
#define HID_MW_DFULL  507
#define HID_MW_DHIST  508
#define HID_MW_QUAR   509
#define HID_MW_OPENLOC 510
#define HID_MW_COPYHASH 511
#define HID_MW_VT     512
#define HID_MW_EXPORT 513
#define HID_MW_QVIEW  514
#define HID_MW_RESTORE 515
#define HID_MW_QDEL   516
#define HID_MW_QFOLDER 517
#define HID_MW_DCUSTOM 518
#define HID_DR_SCAN   540
#define HID_DR_WU     541
#define HID_DR_ONLINE 542
#define HID_DR_WUPAGE 543
#define HID_DR_DEVMGR 544
#define HID_DR_BACKUP 545
#define HID_DR_EXPORT 546
#define HID_DR_RESCAN 547
#define HID_DR_STOP   548
#define HID_DR_FILTER0 549      /* 5 summary cards act as filters (549..553) */
#define HID_DR_TABDET 555
#define HID_DR_TABLOG 556
#define HID_ABOUT_GH  560
#define HID_ABOUT_DC  561

/* ------------------------------------------------------------------ widgets */
enum { BTN_PRIMARY, BTN_SOFT, BTN_DANGER, BTN_GHOST };
enum { BF_DISABLED = 1, BF_ACTIVE = 2 };

Rgb  sev_color(int sev);                       /* 0 ok, 1 info, 2 warn, 3 orange, 4 bad */
void w_card(float x, float y, float w, float h);
void w_card_rc(const RECT* r);
void w_btn(const RECT* r, const wchar_t* text, int icon, int id, int style, int flags);
void w_pill(const RECT* r, const wchar_t* text, int id, BOOL active, int font);
void w_toggle(const RECT* r, BOOL on, int id);
void w_badge(const RECT* r, const wchar_t* text, Rgb c);
int  w_badge_at(int x, int y, const wchar_t* text, Rgb c);   /* returns width */
void w_progress(const RECT* r, double frac, Rgb c);
void w_input_frame(const RECT* r, BOOL focus);
void w_gauge(float cx, float cy, float r, float thick, double frac, Rgb c);
int  w_text_width(const wchar_t* s, int font);
void w_grid_lines(const RECT* r, int lines);   /* horizontal chart grid */
int  w_btn_width(const wchar_t* text, BOOL icon);   /* width that fits a button label */

/* ------------------------------------------------------------------ table */
#define TBL_MAXC 8
typedef struct TblCol { const wchar_t* en; const wchar_t* id; int w; int right; int kind; } TblCol;   /* kind: 0 text, 1 numeric sort, 2 badge */
typedef struct TblRow { wchar_t* c[TBL_MAXC]; double key[TBL_MAXC]; int sev; void* user; } TblRow;
typedef int (*TblPred)(const TblRow* r, void* ctx);
typedef struct Table {
    int    ncols;
    TblCol col[TBL_MAXC];
    TblRow* rows; int n, cap;
    int*   view; int nview;
    int    sel;                /* row index into rows[] or -1 */
    int    hover;              /* row index or -1 */
    int    scroll;             /* pixel offset */
    int    sortcol, sortasc;
    RECT   rc;
    int    drag, drag_dy;
    wchar_t filter[128];
    TblPred pred; void* pred_ctx;
    const wchar_t* empty_en; const wchar_t* empty_id;
} Table;

void    tbl_init(Table* t);
void    tbl_clear(Table* t);
TblRow* tbl_add(Table* t, int sev, void* user);
void    tbl_set(TblRow* r, int col, const wchar_t* text, double key);
void    tbl_rebuild(Table* t);
void    tbl_draw(Table* t, const RECT* rc);
BOOL    tbl_mouse(Table* t, UINT msg, int x, int y);       /* TRUE when consumed (needs redraw) */
void    tbl_wheel(Table* t, int delta);
BOOL    tbl_key(Table* t, WPARAM vk);
TblRow* tbl_selected(Table* t);
void    tbl_free(Table* t);

/* ------------------------------------------------------------------ pages API (ui_pages.c) */
void  pages_init(HWND hwnd);
void  pages_layout(void);
void  pages_paint(void);
void  pages_click(int id);
void  pages_tick(void);                 /* 1 Hz */
BOOL  pages_anim(void);                 /* ~30 fps; TRUE => redraw */
BOOL  pages_wheel(int x, int y, int delta);
BOOL  pages_mouse(UINT msg, int x, int y);
BOOL  pages_key(WPARAM vk);
void  pages_app_msg(UINT msg, WPARAM wp, LPARAM lp);
void  pages_command(WPARAM wp, LPARAM lp);
HBRUSH pages_ctlcolor(HDC dc, HWND ctl);
void  pages_page_changed(int newpage);
void  pages_shutdown(void);
void  pages_dev_run(int fp, int idx, const wchar_t* outfile, const wchar_t* text, int opt);   /* developer test hook (--run=) */
BOOL  pages_busy(void);
void  pages_lang_changed(void);
const wchar_t* page_title(int p);
const wchar_t* page_subtitle(int p);
int   page_icon(int p);

/* log view (ui_log.c) */
void  log_init(HWND parent);
void  log_place(int id, const RECT* rc);     /* shows exactly one log at rc, hides others; rc NULL hides all */
void  log_append(int id, int style, const wchar_t* text);
void  log_clear(int id);
wchar_t* log_text(int id);                   /* malloc'd */
void  log_restyle(void);                     /* colors after theme change */
void  log_set_dpi(void);

/* settings of the shell */
void  shell_set_theme(int idx);
void  shell_relayout(void);
void  shell_navigate(int page);
extern HWND g_ed_feat, g_ed_power, g_ed_proc, g_ed_drv;

#endif
