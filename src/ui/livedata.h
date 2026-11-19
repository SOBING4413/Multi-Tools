/* livedata.h - rolling system statistics (CPU, RAM, network, battery...) sampled once per second */
#ifndef MT_LIVEDATA_H
#define MT_LIVEDATA_H
#include "core/common.h"
#include "modules/sys.h"
#include "modules/procs.h"

#define LIVE_HIST 120                       /* samples kept (2 minutes) */

typedef struct LiveData {
    float     cpu_h[LIVE_HIST], ram_h[LIVE_HIST], dn_h[LIVE_HIST], up_h[LIVE_HIST];   /* oldest -> newest */
    int       hist_n;
    double    cpu, ram, dn, up;               /* current values (percent / bytes per second) */
    ULONGLONG rx, tx, rx0, tx0;               /* network byte counters (now / at start) */
    ULONGLONG ram_total, ram_avail;
    DriveInfo sysdrive;
    BOOL      has_sysdrive;
    int       bat_pct;
    BOOL      bat_plug, bat_present;
    long      bat_secs;
    wchar_t   os[256], host[128], cpuname[200], user[128], ip[64];
    int       phys, logi, mhz;
    ProcInfo  top[5];                         /* top processes by memory */
    int       ntop;
    BOOL      paused;                         /* Live Monitor pause switch */
} LiveData;

extern LiveData g_live;

#define live_copy(dst, src) memcpy((dst), (src), sizeof(float) * LIVE_HIST)

void live_init(void);
void live_tick(void);                         /* call once per second */
void live_refresh_top(void);
int  live_health_score(void);                 /* 0..100 */

#endif
