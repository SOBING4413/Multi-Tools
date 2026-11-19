/* feat.h - feature table types + all feature runners */
#ifndef MT_FEAT_H
#define MT_FEAT_H
#include "core/common.h"
#include "core/out.h"

typedef struct FeatArgs {
    wchar_t text[MAX_PATH * 2];
    int     opt;
} FeatArgs;

typedef void (*FeatRun)(MtOut* o, const FeatArgs* a);

typedef struct GridItem {
    const wchar_t* en;
    const wchar_t* id;
    const wchar_t* cmd;       /* NULL => group heading */
    const wchar_t* params;
} GridItem;

enum { IN_NONE, IN_TEXT, IN_NUMBER, IN_FILE, IN_FOLDER };
enum { FF_CONFIRM = 1, FF_STOPPABLE = 2 };

typedef struct FeatureDef {
    int            key;                 /* I18nKey or -1 */
    const wchar_t* en;
    const wchar_t* idn;
    const wchar_t* desc_en;
    const wchar_t* desc_id;
    FeatRun        run;
    int            in_kind;
    const wchar_t* in_label_en;
    const wchar_t* in_label_id;
    const wchar_t* in_default;
    int            nopt;
    const wchar_t* opt_en[12];
    const wchar_t* opt_id[12];
    int            flags;
    const wchar_t* confirm_en;
    const wchar_t* confirm_id;
    const GridItem* grid;
    int            ngrid;
} FeatureDef;

/* category pages -> their feature lists (defined in features.c) */
enum { FP_SYSTEM, FP_NETWORK, FP_CLEANUP, FP_SECURITY, FP_TOOLS, FP_COUNT };
const FeatureDef* features_for(int fp, int* count);
const wchar_t* feat_name(const FeatureDef* f);
const wchar_t* feat_desc(const FeatureDef* f);

/* ---- system ---- */
void f_sysinfo(MtOut* o, const FeatArgs* a);
void f_disk(MtOut* o, const FeatArgs* a);
void f_battery(MtOut* o, const FeatArgs* a);
void f_uptime(MtOut* o, const FeatArgs* a);
void f_gpu(MtOut* o, const FeatArgs* a);
void f_userinfo(MtOut* o, const FeatArgs* a);
void f_hardware(MtOut* o, const FeatArgs* a);
void f_diskhealth(MtOut* o, const FeatArgs* a);
void f_healthreport(MtOut* o, const FeatArgs* a);
BOOL report_generate(wchar_t* path_out, size_t cch);

/* ---- network ---- */
void f_checkip(MtOut* o, const FeatArgs* a);
void f_ipdetail(MtOut* o, const FeatArgs* a);
void f_netinfo(MtOut* o, const FeatArgs* a);
void f_ping(MtOut* o, const FeatArgs* a);
void f_traceroute(MtOut* o, const FeatArgs* a);
void f_dnslookup(MtOut* o, const FeatArgs* a);
void f_flushdns(MtOut* o, const FeatArgs* a);
void f_wifitoggle(MtOut* o, const FeatArgs* a);
void f_wifipass(MtOut* o, const FeatArgs* a);
void f_speedtest(MtOut* o, const FeatArgs* a);

/* ---- cleanup ---- */
typedef struct DelStats { ULONGLONG files, dirs, failed, bytes; } DelStats;
void del_contents(const wchar_t* dir, DelStats* st, volatile LONG* cancel);   /* deletes everything inside dir */
void f_cleantemp(MtOut* o, const FeatArgs* a);
void f_cleanbin(MtOut* o, const FeatArgs* a);
void f_benchmark(MtOut* o, const FeatArgs* a);
void f_largefiles(MtOut* o, const FeatArgs* a);

/* ---- security ---- */
void f_secoverview(MtOut* o, const FeatArgs* a);
void f_firewall(MtOut* o, const FeatArgs* a);
void f_ports(MtOut* o, const FeatArgs* a);
void f_antivirus(MtOut* o, const FeatArgs* a);
void f_suspicious(MtOut* o, const FeatArgs* a);
void f_useraccounts(MtOut* o, const FeatArgs* a);
void f_privacy(MtOut* o, const FeatArgs* a);

/* ---- tools ---- */
void f_screenshot(MtOut* o, const FeatArgs* a);
void f_sysactions(MtOut* o, const FeatArgs* a);
void f_hashcalc(MtOut* o, const FeatArgs* a);

#endif
