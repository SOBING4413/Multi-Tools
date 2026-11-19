/* sys.h - native system data collection (no PowerShell needed) */
#ifndef MT_SYS_H
#define MT_SYS_H
#include "core/common.h"

typedef struct DriveInfo {
    wchar_t root[8];
    wchar_t label[64];
    wchar_t fs[16];
    UINT    type;
    ULONGLONG total, free;
} DriveInfo;

int    sys_drives(DriveInfo* out, int max);              /* fixed / removable / remote with media */
double sys_cpu_percent(void);                            /* usage since previous call (0..100) */
void   sys_cpu_name(wchar_t* out, size_t cch);
int    sys_cpu_mhz(void);
void   sys_cpu_cores(int* physical, int* logical);
void   sys_os_string(wchar_t* out, size_t cch);          /* "Windows 11 Pro 23H2 (build 22631.3007)" */
void   sys_arch_string(wchar_t* out, size_t cch);
BOOL   sys_mem(ULONGLONG* total, ULONGLONG* avail, int* load_pct);
void   sys_commit(ULONGLONG* limit, ULONGLONG* used);    /* virtual memory commit (swap-like) */
BOOL   sys_battery(int* pct, BOOL* plugged, long* secs_left);   /* FALSE when no battery */
ULONGLONG sys_uptime_secs(void);
void   sys_net_bytes(ULONGLONG* rx, ULONGLONG* tx);      /* totals across non-loopback adapters */
void   sys_hostname(wchar_t* out, size_t cch);
void   sys_username(wchar_t* out, size_t cch);
ULONGLONG dir_size_limited(const wchar_t* path, int max_depth, volatile LONG* cancel);

#endif
