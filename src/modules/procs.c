#include "modules/procs.h"
#include "core/util.h"
#include "modules/sys.h"
#include <tlhelp32.h>
#include <psapi.h>

typedef struct CpuPrev { DWORD pid; ULONGLONG t; } CpuPrev;
static CpuPrev* g_prev = NULL;
static int g_nprev = 0;
static ULONGLONG g_prev_tick = 0;
static CRITICAL_SECTION g_cs;
static int g_cs_init = 0;

typedef struct NameCache { DWORD pid; wchar_t name[64]; } NameCache;
static NameCache* g_nc = NULL;
static int g_nnc = 0;

static void lock_init(void)
{
    if (!g_cs_init) { InitializeCriticalSection(&g_cs); g_cs_init = 1; }
}

int procs_snapshot(ProcInfo** out)
{
    HANDLE snap;
    PROCESSENTRY32W pe;
    ProcInfo* arr = NULL;
    int n = 0, cap = 0, ncpu = 1, i;
    CpuPrev* np = NULL;
    int nn = 0, npcap = 0;
    ULONGLONG now = GetTickCount64(), dt;
    *out = NULL;
    lock_init();
    sys_cpu_cores(NULL, &ncpu);
    if (ncpu < 1) ncpu = 1;
    snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    EnterCriticalSection(&g_cs);
    dt = g_prev_tick ? (now - g_prev_tick) * 10000ULL : 0;   /* ms -> 100ns */
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            ProcInfo* p;
            HANDLE h;
            if (n == cap) {
                ProcInfo* na;
                cap = cap ? cap * 2 : 256;
                na = (ProcInfo*)realloc(arr, (size_t)cap * sizeof(ProcInfo));
                if (!na) break;
                arr = na;
            }
            p = &arr[n++];
            ZeroMemory(p, sizeof(*p));
            p->pid = pe.th32ProcessID;
            p->ppid = pe.th32ParentProcessID;
            p->threads = pe.cntThreads;
            wcopy(p->name, 64, pe.szExeFile);
            if (p->pid == 0) { wcopy(p->name, 64, L"System Idle Process"); continue; }
            h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, p->pid);
            if (h) {
                PROCESS_MEMORY_COUNTERS pmc;
                FILETIME c, e, k, u;
                DWORD sz = MAX_PATH;
                if (GetProcessMemoryInfo(h, &pmc, sizeof(pmc))) p->ws = pmc.WorkingSetSize;
                QueryFullProcessImageNameW(h, 0, p->path, &sz);
                if (GetProcessTimes(h, &c, &e, &k, &u)) {
                    ULONGLONG t = (((ULONGLONG)k.dwHighDateTime << 32) | k.dwLowDateTime) +
                                  (((ULONGLONG)u.dwHighDateTime << 32) | u.dwLowDateTime);
                    int j;
                    if (nn == npcap) {
                        CpuPrev* t2 = (CpuPrev*)realloc(np, sizeof(CpuPrev) * (size_t)(npcap ? npcap * 2 : 256));
                        if (t2) { np = t2; npcap = npcap ? npcap * 2 : 256; }
                    }
                    if (np && nn < npcap) { np[nn].pid = p->pid; np[nn].t = t; nn++; }
                    if (dt) for (j = 0; j < g_nprev; j++)
                        if (g_prev[j].pid == p->pid) {
                            ULONGLONG d = t >= g_prev[j].t ? t - g_prev[j].t : 0;
                            p->cpu = 100.0 * (double)d / ((double)dt * ncpu);
                            if (p->cpu > 100) p->cpu = 100;
                            break;
                        }
                }
                CloseHandle(h);
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    free(g_prev);
    g_prev = np;
    g_nprev = nn;
    g_prev_tick = now;
    /* refresh name cache */
    free(g_nc);
    g_nc = (NameCache*)malloc(sizeof(NameCache) * (size_t)(n > 0 ? n : 1));
    g_nnc = 0;
    if (g_nc) for (i = 0; i < n; i++) { g_nc[g_nnc].pid = arr[i].pid; wcopy(g_nc[g_nnc].name, 64, arr[i].name); g_nnc++; }
    LeaveCriticalSection(&g_cs);
    *out = arr;
    return n;
}

void proc_cache_refresh(void)
{
    ProcInfo* a = NULL;
    procs_snapshot(&a);
    free(a);
}

BOOL proc_name_of(DWORD pid, wchar_t* out, size_t cch)
{
    int i;
    BOOL ok = FALSE;
    lock_init();
    EnterCriticalSection(&g_cs);
    for (i = 0; i < g_nnc; i++)
        if (g_nc[i].pid == pid) { wcopy(out, cch, g_nc[i].name); ok = TRUE; break; }
    LeaveCriticalSection(&g_cs);
    if (!ok) wcopy(out, cch, pid == 0 ? L"System Idle" : L"-");
    return ok;
}

BOOL proc_kill(DWORD pid, wchar_t* err, size_t cch)
{
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!h) {
        DWORD e = GetLastError();
        if (err) wfmt(err, cch, e == ERROR_ACCESS_DENIED ? L"Access denied (run as Administrator)" : L"Cannot open process (error %lu)", (unsigned long)e);
        return FALSE;
    }
    if (!TerminateProcess(h, 1)) {
        DWORD e = GetLastError();
        CloseHandle(h);
        if (err) wfmt(err, cch, L"Terminate failed (error %lu)", (unsigned long)e);
        return FALSE;
    }
    CloseHandle(h);
    return TRUE;
}
