#include "modules/sys.h"
#include "core/util.h"
#include <iphlpapi.h>
#include <netioapi.h>
#include <psapi.h>
#include <lmcons.h>

int sys_drives(DriveInfo* out, int max)
{
    DWORD mask = GetLogicalDrives();
    int n = 0, i;
    for (i = 0; i < 26 && n < max; i++) {
        wchar_t root[4];
        UINT t;
        ULARGE_INTEGER fa, tot, fr;
        DriveInfo* d;
        if (!(mask & (1u << i))) continue;
        root[0] = (wchar_t)(L'A' + i); root[1] = L':'; root[2] = L'\\'; root[3] = 0;
        t = GetDriveTypeW(root);
        if (t != DRIVE_FIXED && t != DRIVE_REMOVABLE && t != DRIVE_REMOTE) continue;
        if (!GetDiskFreeSpaceExW(root, &fa, &tot, &fr)) continue;
        if (tot.QuadPart == 0) continue;
        d = &out[n++];
        ZeroMemory(d, sizeof(*d));
        wcopy(d->root, 8, root);
        d->type = t;
        d->total = tot.QuadPart;
        d->free = fr.QuadPart;
        GetVolumeInformationW(root, d->label, 64, NULL, NULL, NULL, d->fs, 16);
    }
    return n;
}

static ULONGLONG ft64(const FILETIME* f) { return ((ULONGLONG)f->dwHighDateTime << 32) | f->dwLowDateTime; }

double sys_cpu_percent(void)
{
    static ULONGLONG pi = 0, pk = 0, pu = 0;
    static int first = 1;
    FILETIME i, k, u;
    ULONGLONG ii, kk, uu, di, dk, du, total;
    double pct;
    if (!GetSystemTimes(&i, &k, &u)) return 0;
    ii = ft64(&i); kk = ft64(&k); uu = ft64(&u);
    if (first) { first = 0; pi = ii; pk = kk; pu = uu; return 0; }
    di = ii - pi; dk = kk - pk; du = uu - pu;
    pi = ii; pk = kk; pu = uu;
    total = dk + du;
    if (total == 0) return 0;
    pct = 100.0 * (double)(total - di) / (double)total;
    return pct < 0 ? 0 : (pct > 100 ? 100 : pct);
}

void sys_cpu_name(wchar_t* out, size_t cch)
{
    if (!reg_get_str(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"ProcessorNameString", out, cch))
        wcopy(out, cch, L"Unknown CPU");
    wtrim(out);
}

int sys_cpu_mhz(void)
{
    DWORD v = 0;
    if (reg_get_dword(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"~MHz", &v)) return (int)v;
    return 0;
}

void sys_cpu_cores(int* physical, int* logical)
{
    DWORD len = 0;
    int phys = 0;
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    if (logical) {
        DWORD ac = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
        *logical = ac ? (int)ac : (int)si.dwNumberOfProcessors;
    }
    GetLogicalProcessorInformationEx(RelationProcessorCore, NULL, &len);
    if (len) {
        BYTE* buf = (BYTE*)malloc(len);
        if (buf && GetLogicalProcessorInformationEx(RelationProcessorCore, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)buf, &len)) {
            DWORD off = 0;
            while (off < len) {
                PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX p = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)(buf + off);
                if (p->Size == 0) break;
                if (p->Relationship == RelationProcessorCore) phys++;
                off += p->Size;
            }
        }
        free(buf);
    }
    if (physical) *physical = phys ? phys : (int)si.dwNumberOfProcessors;
}

void sys_os_string(wchar_t* out, size_t cch)
{
    typedef LONG (WINAPI *RtlGetVersionFn)(PRTL_OSVERSIONINFOW);
    RTL_OSVERSIONINFOW vi;
    wchar_t name[128] = L"Windows", disp[32] = L"", ubr[16] = L"";
    DWORD build = 0, ubrv = 0;
    HMODULE nt = GetModuleHandleW(L"ntdll.dll");
    RtlGetVersionFn fn = nt ? (RtlGetVersionFn)GetProcAddress(nt, "RtlGetVersion") : NULL;
    ZeroMemory(&vi, sizeof(vi));
    vi.dwOSVersionInfoSize = sizeof(vi);
    if (fn) { fn(&vi); build = vi.dwBuildNumber; }
    reg_get_str(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"ProductName", name, 128);
    if (build >= 22000 && wcsstr(name, L"Windows 10")) {
        /* ProductName still says Windows 10 on Windows 11 */
        wchar_t tmp[128];
        wfmt(tmp, 128, L"Windows 11%ls", name + 10);
        wcopy(name, 128, tmp);
    }
    if (!reg_get_str(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"DisplayVersion", disp, 32))
        reg_get_str(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"ReleaseId", disp, 32);
    if (reg_get_dword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"UBR", &ubrv))
        wfmt(ubr, 16, L".%lu", (unsigned long)ubrv);
    if (build)
        wfmt(out, cch, L"%ls%ls%ls (build %lu%ls)", name, disp[0] ? L" " : L"", disp, (unsigned long)build, ubr);
    else
        wfmt(out, cch, L"%ls", name);
}

void sys_arch_string(wchar_t* out, size_t cch)
{
    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);
    switch (si.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_AMD64: wcopy(out, cch, L"x64 (AMD64)"); break;
    case PROCESSOR_ARCHITECTURE_ARM64: wcopy(out, cch, L"ARM64"); break;
    case PROCESSOR_ARCHITECTURE_INTEL: wcopy(out, cch, L"x86"); break;
    default: wfmt(out, cch, L"Arch %u", (unsigned)si.wProcessorArchitecture); break;
    }
}

BOOL sys_mem(ULONGLONG* total, ULONGLONG* avail, int* load_pct)
{
    MEMORYSTATUSEX m;
    m.dwLength = sizeof(m);
    if (!GlobalMemoryStatusEx(&m)) return FALSE;
    if (total) *total = m.ullTotalPhys;
    if (avail) *avail = m.ullAvailPhys;
    if (load_pct) *load_pct = (int)m.dwMemoryLoad;
    return TRUE;
}

void sys_commit(ULONGLONG* limit, ULONGLONG* used)
{
    PERFORMANCE_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    pi.cb = sizeof(pi);
    if (GetPerformanceInfo(&pi, sizeof(pi))) {
        if (limit) *limit = (ULONGLONG)pi.CommitLimit * pi.PageSize;
        if (used) *used = (ULONGLONG)pi.CommitTotal * pi.PageSize;
    } else {
        if (limit) *limit = 0;
        if (used) *used = 0;
    }
}

BOOL sys_battery(int* pct, BOOL* plugged, long* secs_left)
{
    SYSTEM_POWER_STATUS sp;
    if (!GetSystemPowerStatus(&sp)) return FALSE;
    if (sp.BatteryFlag == 128 || sp.BatteryFlag == 255) return FALSE;   /* none / unknown */
    if (pct) *pct = sp.BatteryLifePercent == 255 ? 0 : sp.BatteryLifePercent;
    if (plugged) *plugged = (sp.ACLineStatus == 1);
    if (secs_left) *secs_left = (sp.BatteryLifeTime == 0xFFFFFFFF) ? -1 : (long)sp.BatteryLifeTime;
    return TRUE;
}

ULONGLONG sys_uptime_secs(void) { return GetTickCount64() / 1000ULL; }

void sys_net_bytes(ULONGLONG* rx, ULONGLONG* tx)
{
    PMIB_IF_TABLE2 t = NULL;
    ULONGLONG r = 0, x = 0;
    if (GetIfTable2(&t) == NO_ERROR && t) {
        ULONG i;
        for (i = 0; i < t->NumEntries; i++) {
            const MIB_IF_ROW2* row = &t->Table[i];
            if (row->Type == IF_TYPE_SOFTWARE_LOOPBACK) continue;
            if (row->OperStatus != IfOperStatusUp) continue;
            r += row->InOctets;
            x += row->OutOctets;
        }
        FreeMibTable(t);
    }
    if (rx) *rx = r;
    if (tx) *tx = x;
}

void sys_hostname(wchar_t* out, size_t cch)
{
    DWORD n = (DWORD)cch;
    if (!GetComputerNameExW(ComputerNameDnsHostname, out, &n)) wcopy(out, cch, L"?");
}

void sys_username(wchar_t* out, size_t cch)
{
    DWORD n = (DWORD)cch;
    if (!GetUserNameW(out, &n)) wcopy(out, cch, L"?");
}

static ULONGLONG dir_size_rec(const wchar_t* path, int depth, int max_depth, volatile LONG* cancel)
{
    wchar_t pat[MAX_PATH * 2], sub[MAX_PATH * 2];
    WIN32_FIND_DATAW fd;
    HANDLE h;
    ULONGLONG total = 0;
    if (depth >= max_depth) return 0;
    wfmt(pat, MT_ARRAY(pat), L"%ls\\*", path);
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (cancel && *cancel) break;
        if (fd.cFileName[0] == L'.' && (fd.cFileName[1] == 0 || (fd.cFileName[1] == L'.' && fd.cFileName[2] == 0))) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            wfmt(sub, MT_ARRAY(sub), L"%ls\\%ls", path, fd.cFileName);
            total += dir_size_rec(sub, depth + 1, max_depth, cancel);
        } else {
            total += ((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return total;
}

ULONGLONG dir_size_limited(const wchar_t* path, int max_depth, volatile LONG* cancel)
{
    return dir_size_rec(path, 0, max_depth, cancel);
}
