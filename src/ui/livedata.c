/* livedata.c - samples CPU / RAM / network / battery / disk once per second for the dashboard and monitor */
#include "ui/livedata.h"
#include "ui/ui.h"
#include "core/util.h"
#include "core/settings.h"
#include <iphlpapi.h>

LiveData g_live;
static ULONGLONG s_last_tick = 0;
static int s_tick = 0;

static void push(float* a, float v)
{
    int i;
    for (i = 0; i < LIVE_HIST - 1; i++) a[i] = a[i + 1];
    a[LIVE_HIST - 1] = v;
}

static void local_ip(wchar_t* out, size_t cch)
{
    ULONG sz = 16384, r, tries = 0;
    PIP_ADAPTER_ADDRESSES aa = NULL, p;
    wcopy(out, cch, L"-");
    do {
        free(aa);
        aa = (PIP_ADAPTER_ADDRESSES)malloc(sz);
        if (!aa) return;
        r = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, NULL, aa, &sz);
    } while (r == ERROR_BUFFER_OVERFLOW && ++tries < 3);
    if (r == NO_ERROR) {
        for (p = aa; p; p = p->Next) {
            PIP_ADAPTER_UNICAST_ADDRESS ua;
            if (p->OperStatus != IfOperStatusUp || p->IfType == IF_TYPE_SOFTWARE_LOOPBACK || !p->FirstGatewayAddress) continue;
            for (ua = p->FirstUnicastAddress; ua; ua = ua->Next)
                if (ua->Address.lpSockaddr && ua->Address.lpSockaddr->sa_family == AF_INET) {
                    InetNtopW(AF_INET, &((struct sockaddr_in*)ua->Address.lpSockaddr)->sin_addr, out, cch);
                    free(aa);
                    return;
                }
        }
    }
    free(aa);
}

static void refresh_slow(void)
{
    DriveInfo d[26];
    int n = sys_drives(d, 26), i;
    wchar_t sysroot[8];
    g_live.has_sysdrive = FALSE;
    GetWindowsDirectoryW(sysroot, 8);
    for (i = 0; i < n; i++)
        if (towupper(d[i].root[0]) == towupper(sysroot[0])) { g_live.sysdrive = d[i]; g_live.has_sysdrive = TRUE; break; }
    if (!g_live.has_sysdrive && n > 0) { g_live.sysdrive = d[0]; g_live.has_sysdrive = TRUE; }
    g_live.bat_present = sys_battery(&g_live.bat_pct, &g_live.bat_plug, &g_live.bat_secs);
}

static int top_cmp(const void* a, const void* b)
{
    const ProcInfo* x = (const ProcInfo*)a; const ProcInfo* y = (const ProcInfo*)b;
    return x->ws < y->ws ? 1 : (x->ws > y->ws ? -1 : 0);
}

void live_refresh_top(void)
{
    ProcInfo* a = NULL;
    int n = procs_snapshot(&a), i;
    if (n > 0) {
        qsort(a, (size_t)n, sizeof(ProcInfo), top_cmp);
        g_live.ntop = n < 5 ? n : 5;
        for (i = 0; i < g_live.ntop; i++) g_live.top[i] = a[i];
    }
    free(a);
}

void live_init(void)
{
    sys_os_string(g_live.os, 256);
    sys_hostname(g_live.host, 128);
    sys_cpu_name(g_live.cpuname, 200);
    sys_username(g_live.user, 128);
    sys_cpu_cores(&g_live.phys, &g_live.logi);
    g_live.mhz = sys_cpu_mhz();
    sys_net_bytes(&g_live.rx, &g_live.tx);
    g_live.rx0 = g_live.rx; g_live.tx0 = g_live.tx;
    s_last_tick = GetTickCount64();
    sys_cpu_percent();
    refresh_slow();
    local_ip(g_live.ip, 64);
    sys_mem(&g_live.ram_total, &g_live.ram_avail, NULL);
}

void live_tick(void)
{
    ULONGLONG now = GetTickCount64(), rx, tx;
    double el = (double)(now - s_last_tick) / 1000.0;
    if (el < 0.2) el = 1.0;
    s_last_tick = now;
    s_tick++;
    if (!g_live.paused && g_set.auto_refresh) {
        g_live.cpu = sys_cpu_percent();
        sys_mem(&g_live.ram_total, &g_live.ram_avail, NULL);
        g_live.ram = g_live.ram_total ? 100.0 * (double)(g_live.ram_total - g_live.ram_avail) / (double)g_live.ram_total : 0;
        sys_net_bytes(&rx, &tx);
        g_live.dn = rx >= g_live.rx ? (double)(rx - g_live.rx) / el : 0;
        g_live.up = tx >= g_live.tx ? (double)(tx - g_live.tx) / el : 0;
        g_live.rx = rx; g_live.tx = tx;
        push(g_live.cpu_h, (float)g_live.cpu); push(g_live.ram_h, (float)g_live.ram);
        push(g_live.dn_h, (float)g_live.dn);   push(g_live.up_h, (float)g_live.up);
        if (g_live.hist_n < LIVE_HIST) g_live.hist_n++;
    } else {
        sys_net_bytes(&g_live.rx, &g_live.tx);
    }
    if (s_tick % 5 == 0) refresh_slow();
    if (s_tick % 30 == 0) local_ip(g_live.ip, 64);
    if (g_page == PG_DASH && (s_tick % 3 == 1 || g_live.ntop == 0)) live_refresh_top();
}

int live_health_score(void)
{
    int sc = 100;
    ULONGLONG up = sys_uptime_secs();
    if (g_live.cpu > 85) sc -= 15; else if (g_live.cpu > 70) sc -= 7;
    if (g_live.ram > 85) sc -= 15; else if (g_live.ram > 70) sc -= 7;
    if (g_live.has_sysdrive && g_live.sysdrive.total) {
        double pc = 100.0 * (double)(g_live.sysdrive.total - g_live.sysdrive.free) / (double)g_live.sysdrive.total;
        if (pc > 90) sc -= 20; else if (pc > 80) sc -= 8;
    }
    if (up > 72 * 3600ULL) sc -= 10;
    return sc < 0 ? 0 : sc;
}
