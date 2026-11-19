#ifndef MT_PROCS_H
#define MT_PROCS_H
#include "core/common.h"

typedef struct ProcInfo {
    DWORD     pid, ppid, threads;
    wchar_t   name[64];
    wchar_t   path[MAX_PATH];
    double    cpu;        /* percent of total CPU since previous snapshot */
    ULONGLONG ws;         /* working set bytes */
} ProcInfo;

/* Returns count; *out is malloc'd (free with free()). CPU% needs two snapshots. */
int  procs_snapshot(ProcInfo** out);
BOOL proc_kill(DWORD pid, wchar_t* err, size_t cch);
BOOL proc_name_of(DWORD pid, wchar_t* out, size_t cch);   /* quick lookup via snapshot cache */
void proc_cache_refresh(void);

#endif
