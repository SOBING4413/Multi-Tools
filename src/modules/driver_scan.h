/* driver_scan.h - installed driver inventory, health classification and Windows Update lookup */
#ifndef MT_DRIVER_SCAN_H
#define MT_DRIVER_SCAN_H
#include "core/common.h"
#include "core/out.h"

enum { DS_OK, DS_OLD, DS_OUTDATED, DS_GENERIC, DS_UPDATE, DS_DISABLED, DS_UNSIGNED, DS_PROBLEM, DS_MISSING, DS__COUNT };

typedef struct DriverRow {
    wchar_t name[256], cls[64], mfg[128], provider[128], version[64], date[16], inf[64];
    wchar_t hwid[512], instance[256], note[320];
    wchar_t wu_title[256], wu_ver[64], wu_date[16];
    int     age_days;          /* -1 unknown */
    int     state;
    DWORD   problem;
    int     from_wu_only;
} DriverRow;

typedef struct DriverScan {
    DriverRow* rows;
    int        n;
    int        cnt[DS__COUNT];
    int        wu_checked;      /* Windows Update lookup finished */
    int        wu_ok;           /* lookup succeeded */
    int        wu_found;
} DriverScan;

DriverScan*    driver_scan_local(volatile LONG* cancel);
void           driver_scan_wu(DriverScan* ds, volatile LONG* cancel);   /* adds Windows Update info; slow (online) */
void           driver_scan_free(DriverScan* ds);
void           driver_recount(DriverScan* ds);
const wchar_t* driver_state_text(int state);
int            driver_state_severity(int state);   /* 0 ok, 1 info, 2 warn, 3 bad */
BOOL           driver_export_csv(const DriverScan* ds, const wchar_t* path);
void           driver_backup_job(MtOut* o);          /* pnputil /export-driver */
void           driver_rescan_job(MtOut* o);          /* pnputil /scan-devices */

#endif
