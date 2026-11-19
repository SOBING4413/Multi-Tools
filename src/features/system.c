/* f_system.c - system info / disk / battery / uptime / GPU / user / hardware / health report */
#include "features/feat.h"
#include "core/util.h"
#include "modules/sys.h"
#include "modules/procs.h"
#include <shlobj.h>
#include <lmaccess.h>
#include <lm.h>
#include <iphlpapi.h>

static void gb(ULONGLONG b, wchar_t* out, size_t cch) { fmt_size(b, out, cch); }

/* ---------- Full System Info ---------- */
void f_sysinfo(MtOut* o, const FeatArgs* a)
{
    wchar_t t[256], t2[128], t3[128];
    int phys = 0, logi = 0, load = 0, mhz;
    ULONGLONG tot = 0, avail = 0, lim = 0, used = 0, st = 0, su = 0, sf = 0;
    DriveInfo d[26];
    int nd, i;
    (void)a;
    out_title(o, T2(L"FULL SYSTEM INFORMATION", L"INFO SISTEM LENGKAP"));

    out_section(o, T2(L"COMPUTER", L"KOMPUTER"));
    sys_hostname(t, 256);               out_kv(o, L"Computer name", L"%ls", t);
    sys_os_string(t, 256);              out_kv(o, L"Operating system", L"%ls", t);
    sys_arch_string(t, 256);            out_kv(o, L"Architecture", L"%ls", t);
    sys_username(t, 256);               out_kv(o, L"Username", L"%ls", t);
    out_kv(o, L"Administrator", L"%ls", g_admin ? T2(L"Yes", L"Ya") : T2(L"No", L"Tidak"));

    out_section(o, L"CPU");
    sys_cpu_name(t, 256);               out_kv(o, L"Processor", L"%ls", t);
    sys_cpu_cores(&phys, &logi);
    out_kv(o, T2(L"Cores (physical)", L"Core (fisik)"), L"%d", phys);
    out_kv(o, T2(L"Cores (logical)", L"Core (logical)"), L"%d", logi);
    mhz = sys_cpu_mhz();
    if (mhz) out_kv(o, L"Base frequency", L"%d MHz", mhz);
    sys_cpu_percent();
    Sleep(500);
    out_kv(o, L"Usage", L"%.1f%%", sys_cpu_percent());

    out_section(o, T2(L"MEMORY", L"MEMORI"));
    sys_mem(&tot, &avail, &load);
    gb(tot, t, 256); gb(tot - avail, t2, 128); gb(avail, t3, 128);
    out_kv(o, L"Total RAM", L"%ls", t);
    out_kv(o, T2(L"RAM used", L"RAM terpakai"), L"%ls (%d%%)", t2, load);
    out_kv(o, T2(L"RAM available", L"RAM tersedia"), L"%ls", t3);
    sys_commit(&lim, &used);
    if (lim) {
        gb(lim, t, 256); gb(used, t2, 128);
        out_kv(o, T2(L"Virtual memory (commit)", L"Memori virtual (commit)"), L"%ls / %ls", t2, t);
    }

    out_section(o, T2(L"STORAGE", L"PENYIMPANAN"));
    nd = sys_drives(d, 26);
    for (i = 0; i < nd; i++) { st += d[i].total; sf += d[i].free; }
    su = st - sf;
    gb(st, t, 256); gb(su, t2, 128); gb(sf, t3, 128);
    out_kv(o, T2(L"Total storage", L"Total penyimpanan"), L"%ls", t);
    out_kv(o, T2(L"Storage used", L"Storage terpakai"), L"%ls", t2);
    out_kv(o, T2(L"Storage free", L"Storage tersedia"), L"%ls", t3);
    out_nl(o);
}

/* ---------- Disk Space ---------- */
void f_disk(MtOut* o, const FeatArgs* a)
{
    DriveInfo d[26];
    int n = sys_drives(d, 26), i;
    ULONGLONG ta = 0, ua = 0, fa = 0;
    double pct;
    wchar_t s1[64], s2[64], s3[64], bar[64], extra[160];
    (void)a;
    out_title(o, T2(L"DISK SPACE", L"RUANG DISK"));
    if (n == 0) { out_warn(o, T2(L"No drives found.", L"Tidak ada drive ditemukan.")); return; }
    for (i = 0; i < n; i++) {
        ULONGLONG used = d[i].total - d[i].free;
        pct = 100.0 * (double)used / (double)d[i].total;
        ta += d[i].total; ua += used; fa += d[i].free;
        fmt_size(d[i].total, s1, 64); fmt_size(used, s2, 64); fmt_size(d[i].free, s3, 64);
        out_nl(o);
        wfmt(extra, 160, L"  Drive   : %ls  %ls", d[i].root, d[i].label[0] ? d[i].label : L"");
        out_line(o, OS_ACCENT, L"%ls", extra);
        out_kv(o, L"File system", L"%ls", d[i].fs[0] ? d[i].fs : L"-");
        out_kv(o, L"Type", L"%ls", d[i].type == DRIVE_FIXED ? L"Fixed" : (d[i].type == DRIVE_REMOVABLE ? L"Removable" : L"Network"));
        out_kv(o, L"Total", L"%ls", s1);
        out_kv(o, T2(L"Used", L"Terpakai"), L"%ls", s2);
        out_kv(o, T2(L"Free", L"Sisa"), L"%ls", s3);
        out_bar(o, T2(L"Usage", L"Pemakaian"), pct, 30, L"");
    }
    out_section(o, T2(L"TOTAL STORAGE SUMMARY", L"RINGKASAN TOTAL STORAGE"));
    pct = ta ? 100.0 * (double)ua / (double)ta : 0;
    fmt_size(ta, s1, 64); fmt_size(ua, s2, 64); fmt_size(fa, s3, 64);
    out_kv(o, L"Total", L"%ls", s1);
    out_kv(o, T2(L"Used", L"Terpakai"), L"%ls", s2);
    out_kv(o, T2(L"Free", L"Sisa"), L"%ls", s3);
    bar_string(bar, 30, pct);
    out_text(o, pct_style(pct), L"  ");
    out_line(o, pct_style(pct), L"[%ls] %.1f%%", bar, pct);
    if (pct > 90) out_warn(o, T2(L"Storage almost full! Consider cleaning up files.", L"Storage hampir penuh! Pertimbangkan membersihkan file."));
}

/* ---------- Battery ---------- */
void f_battery(MtOut* o, const FeatArgs* a)
{
    int pct = 0;
    BOOL plugged = FALSE;
    long secs = -1;
    wchar_t bar[64];
    (void)a;
    out_title(o, T2(L"BATTERY STATUS", L"STATUS BATERAI"));
    if (!sys_battery(&pct, &plugged, &secs)) {
        out_warn(o, T2(L"No battery detected. This may be a desktop computer.", L"Baterai tidak terdeteksi. Mungkin ini komputer desktop."));
        return;
    }
    bar_string(bar, 30, pct);
    out_nl(o);
    out_text(o, pct > 60 ? OS_OK : (pct > 20 ? OS_WARN : OS_ERR), L"  ");
    out_line(o, pct > 60 ? OS_OK : (pct > 20 ? OS_WARN : OS_ERR), L"Battery: [%ls] %d%%", bar, pct);
    out_nl(o);
    out_kv(o, L"Status", L"%ls", plugged ? T2(L"Charging (plugged in)", L"Charging (sedang di-charge)") : T2(L"Discharging (on battery)", L"Discharging (tidak di-charge)"));
    if (secs > 0 && !plugged) out_kv(o, T2(L"Time left", L"Sisa waktu"), T2(L"%ld h %ld min", L"%ld jam %ld menit"), secs / 3600, (secs % 3600) / 60);
    else if (plugged) out_kv(o, T2(L"Time left", L"Sisa waktu"), L"%ls", T2(L"Charging", L"Sedang mengisi daya"));
    else out_kv(o, T2(L"Time left", L"Sisa waktu"), L"%ls", T2(L"Unknown", L"Tidak diketahui"));

    out_nl(o);
    out_info(o, T2(L"Generating detailed battery report...", L"Membuat laporan detail baterai..."));
    {
        wchar_t desk[MAX_PATH], rp[MAX_PATH * 2], cmd[MAX_PATH * 3];
        int rc;
        wchar_t* txt;
        get_folder(CSIDL_DESKTOPDIRECTORY, desk);
        path_join(rp, MAX_PATH * 2, desk, L"battery-report.html");
        wfmt(cmd, MT_ARRAY(cmd), L"powercfg.exe /batteryreport /output \"%ls\"", rp);
        txt = run_cmd_text(cmd, 30000, CP_OEMCP, &rc);
        free(txt);
        if (file_exists(rp)) out_ok(o, T2(L"Report saved to: %ls", L"Laporan disimpan di: %ls"), rp);
        else out_info(o, T2(L"Battery report could not be created.", L"Laporan baterai tidak dapat dibuat."));
    }
}

/* ---------- Uptime ---------- */
void f_uptime(MtOut* o, const FeatArgs* a)
{
    ULONGLONG up = sys_uptime_secs();
    ULONGLONG d = up / 86400, h = (up % 86400) / 3600, m = (up % 3600) / 60, s = up % 60;
    double hours = (double)up / 3600.0, pct = hours / 24.0 * 100.0;
    wchar_t bar[64], when[64];
    FILETIME ft;
    ULARGE_INTEGER u;
    (void)a;
    out_title(o, T2(L"COMPUTER UPTIME", L"UPTIME KOMPUTER"));
    GetSystemTimeAsFileTime(&ft);
    u.LowPart = ft.dwLowDateTime; u.HighPart = ft.dwHighDateTime;
    u.QuadPart -= up * 10000000ULL;
    ft.dwLowDateTime = u.LowPart; ft.dwHighDateTime = u.HighPart;
    fmt_filetime(&ft, when, 64, TRUE);
    out_nl(o);
    out_kv(o, T2(L"Last boot", L"Boot terakhir"), L"%ls", when);
    out_kv(o, L"Uptime", T2(L"%I64u days, %I64u hours, %I64u minutes, %I64u seconds", L"%I64u hari, %I64u jam, %I64u menit, %I64u detik"), d, h, m, s);
    out_nl(o);
    if (pct > 100) pct = 100;
    bar_string(bar, 30, pct);
    {
        int st = hours < 12 ? OS_OK : (hours < 48 ? OS_WARN : OS_ERR);
        out_text(o, st, L"  ");
        out_line(o, st, T2(L"Uptime (24h): [%ls] %.1f hours", L"Uptime (24j): [%ls] %.1f jam"), bar, hours);
    }
    if (hours > 72) {
        out_nl(o);
        out_warn(o, T2(L"The computer has been on for more than 3 days. Consider restarting.", L"Komputer sudah menyala lebih dari 3 hari. Pertimbangkan untuk restart."));
    }
}

/* ---------- GPU ---------- */
typedef struct GpuCtx { MtOut* o; int count; } GpuCtx;
static void gpu_line(const wchar_t* line, void* u)
{
    GpuCtx* g = (GpuCtx*)u;
    wchar_t* eq;
    wchar_t key[96], val[512];
    if (!line[0]) return;
    if (wcscmp(line, L"--") == 0) { g->count++; out_nl(g->o); out_line(g->o, OS_ACCENT, L"  GPU #%d", g->count); out_sep(g->o); return; }
    eq = wcschr((wchar_t*)line, L'=');
    if (!eq) return;
    wcopy(key, 96, line);
    key[eq - line] = 0;
    wcopy(val, 512, eq + 1);
    wtrim(val);
    if (!val[0]) return;
    if (wcscmp(key, L"AdapterRAM") == 0) {
        double v = _wtof(val);
        if (v > 0) {
            wchar_t s[64];
            fmt_size((ULONGLONG)v, s, 64);
            if (v >= 4290000000.0) wfmt(val, 512, L">= %ls (WMI limit)", s);
            else wcopy(val, 512, s);
        } else wcopy(val, 512, L"N/A (shared memory)");
    }
    out_kv(g->o, key, L"%ls", val);
}

static void nvidia_line(const wchar_t* line, void* u)
{
    MtOut* o = (MtOut*)u;
    wchar_t buf[512], *p, *parts[8];
    int n = 0;
    if (!line[0]) return;
    wcopy(buf, 512, line);
    p = buf;
    while (n < 8) {
        parts[n++] = p;
        p = wcschr(p, L',');
        if (!p) break;
        *p++ = 0;
    }
    if (n >= 6) {
        int i;
        for (i = 0; i < 6; i++) wtrim(parts[i]);
        out_nl(o);
        out_kv(o, L"GPU Name", L"%ls", parts[0]);
        out_kv(o, L"VRAM Total", L"%ls MB", parts[1]);
        out_kv(o, L"VRAM Used", L"%ls MB", parts[2]);
        out_kv(o, L"VRAM Free", L"%ls MB", parts[3]);
        out_kv(o, L"Temperature", L"%ls" U_DEGREE L"C", parts[4]);
        out_kv(o, L"GPU Usage", L"%ls%%", parts[5]);
    }
}

void f_gpu(MtOut* o, const FeatArgs* a)
{
    GpuCtx g;
    int rc, i;
    (void)a;
    g.o = o; g.count = 0;
    out_title(o, T2(L"GPU / VGA INFORMATION", L"INFO GPU / VGA"));
    out_info(o, T2(L"Reading graphics adapters...", L"Mengambil info GPU..."));
    rc = run_ps(
        L"Get-CimInstance Win32_VideoController | ForEach-Object { Write-Output '--'; "
        L"Write-Output ('Name='+$_.Name); Write-Output ('DriverVersion='+$_.DriverVersion); "
        L"Write-Output ('DriverDate='+$_.DriverDate); Write-Output ('AdapterRAM='+$_.AdapterRAM); "
        L"Write-Output ('VideoMode='+$_.VideoModeDescription); Write-Output ('RefreshRate='+$_.CurrentRefreshRate); "
        L"Write-Output ('Status='+$_.Status) }", 30000, gpu_line, &g, o->cancel, NULL);
    if (g.count == 0 || rc == RC_FAILED_START) {
        DISPLAY_DEVICEW dd;
        out_warn(o, T2(L"Detailed query failed, showing basic list.", L"Query detail gagal, menampilkan daftar dasar."));
        ZeroMemory(&dd, sizeof(dd));
        dd.cb = sizeof(dd);
        for (i = 0; EnumDisplayDevicesW(NULL, (DWORD)i, &dd, 0); i++) {
            out_kv(o, L"Adapter", L"%ls", dd.DeviceString);
            dd.cb = sizeof(dd);
        }
    }
    {
        Buf raw;
        buf_init(&raw);
        rc = run_capture(L"nvidia-smi.exe --query-gpu=name,memory.total,memory.used,memory.free,temperature.gpu,utilization.gpu --format=csv,noheader,nounits",
                         10000, CP_OEMCP, NULL, NULL, o->cancel, &raw);
        if (rc == 0 && raw.data && raw.len > 4) {
            wchar_t* w = utf8_to_w(raw.data, (int)raw.len);
            if (w) {
                wchar_t* ctx = NULL;
                wchar_t* ln;
                out_section(o, T2(L"NVIDIA GPU DETAIL (nvidia-smi)", L"DETAIL NVIDIA GPU (nvidia-smi)"));
                ctx = w;
                ln = wtok(&ctx, L"\r\n");
                while (ln) { nvidia_line(ln, o); ln = wtok(&ctx, L"\r\n"); }
                free(w);
            }
        }
        buf_free(&raw);
    }
}

/* ---------- User info ---------- */
static void print_folder_size(MtOut* o, const wchar_t* name, const wchar_t* path, int depth, ULONGLONG* total, const wchar_t* note)
{
    ULONGLONG sz;
    wchar_t sbuf[64], bar[32];
    double gbv;
    if (!dir_exists(path)) { out_kv(o, name, T2(L"(not found)", L"(tidak ditemukan)")); return; }
    sz = dir_size_limited(path, depth, o->cancel);
    *total += sz;
    fmt_size(sz, sbuf, 64);
    gbv = (double)sz / (1024.0 * 1024.0 * 1024.0);
    bar_string(bar, 15, gbv / 50.0 * 100.0);
    out_kv(o, name, L"[%ls] %ls%ls", bar, sbuf, note);
}

void f_userinfo(MtOut* o, const FeatArgs* a)
{
    wchar_t home[MAX_PATH], t[MAX_PATH], p[MAX_PATH * 2], e[256];
    ULONGLONG total = 0;
    wchar_t sbuf[64];
    NET_API_STATUS st;
    LPUSER_INFO_2 ui = NULL;
    DWORD er = 0, tot = 0, resume = 0;
    (void)a;
    out_title(o, T2(L"USER INFORMATION", L"INFO USER DETAIL"));
    out_section(o, T2(L"CURRENT USER", L"USER SAAT INI"));
    sys_username(t, MAX_PATH);           out_kv(o, L"Username", L"%ls", t);
    get_folder(CSIDL_PROFILE, home);     out_kv(o, T2(L"Home directory", L"Home directory"), L"%ls", home);
    GetTempPathW(MAX_PATH, t);           out_kv(o, T2(L"Temp directory", L"Temp directory"), L"%ls", t);
    sys_hostname(e, 256);                out_kv(o, T2(L"Computer name", L"Nama komputer"), L"%ls", e);
    if (GetEnvironmentVariableW(L"USERDOMAIN", e, 256)) out_kv(o, T2(L"User domain", L"Domain user"), L"%ls", e);
    out_kv_style(o, L"Administrator", g_admin ? OS_OK : OS_WARN, L"%ls %ls", U_BULLET, g_admin ? T2(L"Yes", L"Ya") : T2(L"No", L"Tidak"));

    out_section(o, T2(L"USER FOLDER SIZES", L"UKURAN FOLDER USER"));
    out_info(o, T2(L"Calculating sizes (quick scan, limited depth)...", L"Menghitung ukuran folder (scan cepat, kedalaman terbatas)..."));
    out_nl(o);
    {
        static const wchar_t* names[] = { L"Desktop", L"Documents", L"Downloads", L"Pictures", L"Videos", L"Music" };
        int i;
        for (i = 0; i < 6 && !OUT_CANCELLED(o); i++) {
            path_join(p, MAX_PATH * 2, home, names[i]);
            print_folder_size(o, names[i], p, 3, &total, L"");
        }
        path_join(p, MAX_PATH * 2, home, L"AppData");
        print_folder_size(o, L"AppData", p, 1, &total, T2(L" (estimate)", L" (estimasi)"));
    }
    fmt_size(total, sbuf, 64);
    out_nl(o);
    out_kv_style(o, L"TOTAL", OS_WARN, T2(L"%ls (estimate)", L"%ls (estimasi)"), sbuf);

    out_section(o, T2(L"ALL USER ACCOUNTS ON THIS COMPUTER", L"SEMUA AKUN USER DI KOMPUTER INI"));
    st = NetUserEnum(NULL, 2, FILTER_NORMAL_ACCOUNT, (LPBYTE*)&ui, MAX_PREFERRED_LENGTH, &er, &tot, &resume);
    if ((st == NERR_Success || st == ERROR_MORE_DATA) && ui) {
        DWORD i;
        out_line(o, OS_ACCENT, L"  %-24ls %-10ls %ls", L"Name", L"Enabled", L"Last logon");
        for (i = 0; i < er; i++) {
            wchar_t when[64];
            if (ui[i].usri2_last_logon) {
                FILETIME ft;
                ULARGE_INTEGER u;
                u.QuadPart = 116444736000000000ULL + (ULONGLONG)ui[i].usri2_last_logon * 10000000ULL;
                ft.dwLowDateTime = u.LowPart; ft.dwHighDateTime = u.HighPart;
                fmt_filetime(&ft, when, 64, TRUE);
            } else wcopy(when, 64, T2(L"never", L"belum pernah"));
            out_line(o, (ui[i].usri2_flags & UF_ACCOUNTDISABLE) ? OS_DIM : OS_NORMAL, L"  %-24ls %-10ls %ls", ui[i].usri2_name,
                     (ui[i].usri2_flags & UF_ACCOUNTDISABLE) ? L"No" : L"Yes", when);
        }
        NetApiBufferFree(ui);
    } else {
        out_info(o, T2(L"Cannot list user accounts (error %lu).", L"Tidak dapat membaca daftar akun (error %lu)."), (unsigned long)st);
    }
}

/* ---------- Hardware summary ---------- */
static void ps_serial_line(const wchar_t* line, void* u)
{
    MtOut* o = (MtOut*)u;
    wchar_t* eq;
    wchar_t k[64], v[256];
    if (!(eq = wcschr((wchar_t*)line, L'='))) return;
    wcopy(k, 64, line);
    k[eq - line] = 0;
    wcopy(v, 256, eq + 1);
    wtrim(v);
    if (v[0]) out_kv(o, k, L"%ls", v);
}

void f_hardware(MtOut* o, const FeatArgs* a)
{
    wchar_t t[256], t2[128], t3[128], bar[64];
    int phys = 0, logi = 0, load = 0, i, nd, pct = 0;
    ULONGLONG tot = 0, avail = 0, stor = 0;
    DriveInfo d[26];
    BOOL plugged = FALSE;
    long secs;
    PIP_ADAPTER_ADDRESSES aa = NULL, p;
    ULONG sz = 0;
    GpuCtx g;
    (void)a;
    out_title(o, T2(L"COMPLETE HARDWARE SUMMARY", L"RINGKASAN HARDWARE LENGKAP"));
    out_section(o, T2(L"OPERATING SYSTEM", L"SISTEM OPERASI"));
    sys_os_string(t, 256); out_kv(o, L"OS", L"%ls", t);
    sys_arch_string(t, 256); out_kv(o, T2(L"Architecture", L"Arsitektur"), L"%ls", t);

    out_section(o, L"PROCESSOR (CPU)");
    sys_cpu_name(t, 256); out_kv(o, T2(L"Name", L"Nama"), L"%ls", t);
    sys_cpu_cores(&phys, &logi);
    out_kv(o, T2(L"Physical cores", L"Cores fisik"), L"%d", phys);
    out_kv(o, T2(L"Logical cores", L"Cores logical"), L"%d", logi);
    if (sys_cpu_mhz()) out_kv(o, L"Frequency", L"%d MHz", sys_cpu_mhz());
    sys_cpu_percent(); Sleep(400);
    out_kv(o, L"Usage", L"%.1f%%", sys_cpu_percent());

    out_section(o, T2(L"MEMORY (RAM)", L"MEMORI (RAM)"));
    sys_mem(&tot, &avail, &load);
    gb(tot, t, 256); gb(tot - avail, t2, 128); gb(avail, t3, 128);
    out_kv(o, L"Total", L"%ls", t);
    out_kv(o, T2(L"Used", L"Terpakai"), L"%ls (%d%%)", t2, load);
    out_kv(o, T2(L"Available", L"Tersedia"), L"%ls", t3);
    out_bar(o, L"RAM", (double)load, 25, L"");

    out_section(o, T2(L"STORAGE (DISK)", L"PENYIMPANAN (DISK)"));
    nd = sys_drives(d, 26);
    for (i = 0; i < nd; i++) {
        double pc = 100.0 * (double)(d[i].total - d[i].free) / (double)d[i].total;
        wchar_t tt[64], ff[64], line[128];
        stor += d[i].total;
        fmt_size(d[i].total, tt, 64); fmt_size(d[i].free, ff, 64);
        wfmt(line, 128, L"%ls (%ls)", d[i].root, d[i].fs);
        wfmt(t2, 128, T2(L"%ls, %ls free", L"%ls, sisa %ls"), tt, ff);
        out_bar(o, line, pc, 15, t2);
    }
    fmt_size(stor, t, 256);
    out_kv_style(o, L"Total Storage", OS_WARN, L"%ls", t);

    out_section(o, L"GPU / VGA");
    g.o = o; g.count = 0;
    run_ps(L"Get-CimInstance Win32_VideoController | ForEach-Object { Write-Output '--'; Write-Output ('Name='+$_.Name); Write-Output ('AdapterRAM='+$_.AdapterRAM) }",
           30000, gpu_line, &g, o->cancel, NULL);
    if (g.count == 0) out_info(o, T2(L"GPU info not available.", L"Info GPU tidak tersedia."));

    out_section(o, T2(L"BATTERY", L"BATERAI"));
    if (sys_battery(&pct, &plugged, &secs)) {
        bar_string(bar, 15, pct);
        out_line(o, pct > 60 ? OS_OK : (pct > 20 ? OS_WARN : OS_ERR), L"  [%ls] %d%% - %ls", bar, pct, plugged ? L"Charging" : L"Discharging");
    } else out_line(o, OS_NORMAL, L"  %ls", T2(L"Desktop (no battery)", L"Desktop (tidak ada baterai)"));

    out_section(o, L"NETWORK");
    GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, NULL, NULL, &sz);
    aa = (PIP_ADAPTER_ADDRESSES)malloc(sz ? sz : 1);
    if (aa && GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, NULL, aa, &sz) == NO_ERROR) {
        for (p = aa; p; p = p->Next) {
            PIP_ADAPTER_UNICAST_ADDRESS ua;
            if (p->OperStatus != IfOperStatusUp || p->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
            for (ua = p->FirstUnicastAddress; ua; ua = ua->Next) {
                wchar_t ip[64];
                struct sockaddr_in* sin = (struct sockaddr_in*)ua->Address.lpSockaddr;
                if (ua->Address.lpSockaddr->sa_family != AF_INET) continue;
                InetNtopW(AF_INET, &sin->sin_addr, ip, 64);
                out_kv(o, p->FriendlyName, L"%ls", ip);
            }
        }
    }
    free(aa);

    out_section(o, T2(L"DEVICE IDENTITY", L"IDENTITAS PERANGKAT"));
    reg_get_str(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"SystemManufacturer", t, 256);
    if (t[0]) out_kv(o, L"Manufacturer", L"%ls", t);
    reg_get_str(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"SystemProductName", t, 256);
    if (t[0]) out_kv(o, L"Model", L"%ls", t);
    reg_get_str(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"BIOSVersion", t, 256);
    if (t[0]) out_kv(o, L"BIOS", L"%ls", t);
    run_ps(L"Write-Output ('Serial Number='+(Get-CimInstance Win32_BIOS).SerialNumber)", 20000, ps_serial_line, o, o->cancel, NULL);
}

/* ---------- Disk health ---------- */
static void dh_line(const wchar_t* line, void* u)
{
    MtOut* o = (MtOut*)u;
    wchar_t buf[512], *f[6];
    int n = 0;
    wchar_t* p;
    if (!line[0]) return;
    wcopy(buf, 512, line);
    p = buf;
    while (n < 6) { f[n++] = p; p = wcschr(p, L'|'); if (!p) break; *p++ = 0; }
    if (n >= 6) {
        double sz = _wtof(f[5]);
        wchar_t s[64];
        int st = (_wcsicmp(f[3], L"Healthy") == 0) ? OS_OK : (_wcsicmp(f[3], L"Warning") == 0 ? OS_WARN : OS_ERR);
        fmt_size((ULONGLONG)sz, s, 64);
        out_nl(o);
        out_line(o, OS_ACCENT, L"  %ls", f[0]);
        out_kv(o, L"Media type", L"%ls", f[1]);
        out_kv(o, L"Bus", L"%ls", f[2]);
        out_kv_style(o, T2(L"Health", L"Kesehatan"), st, L"%ls %ls", U_BULLET, f[3]);
        out_kv(o, T2(L"Operational", L"Operasional"), L"%ls", f[4]);
        out_kv(o, T2(L"Size", L"Ukuran"), L"%ls", s);
    }
}

void f_diskhealth(MtOut* o, const FeatArgs* a)
{
    int rc;
    (void)a;
    out_title(o, T2(L"DISK HEALTH (S.M.A.R.T. STATUS)", L"KESEHATAN DISK (STATUS S.M.A.R.T.)"));
    out_info(o, T2(L"Querying physical disks...", L"Membaca disk fisik..."));
    rc = run_ps(L"Get-PhysicalDisk | ForEach-Object { ($_.FriendlyName+'|'+$_.MediaType+'|'+$_.BusType+'|'+$_.HealthStatus+'|'+$_.OperationalStatus+'|'+$_.Size) }",
                30000, dh_line, o, o->cancel, NULL);
    if (rc != 0) out_warn(o, T2(L"Could not read disk health (needs Windows 8+ Storage module).", L"Tidak dapat membaca kesehatan disk (perlu modul Storage Windows 8+)."));
    out_nl(o);
    out_info(o, T2(L"Tip: back up important data regularly; a 'Warning' status means the drive may fail soon.", L"Tips: backup data penting secara rutin; status 'Warning' berarti disk bisa segera rusak."));
}

/* ---------- Health report (also used by dashboard button) ---------- */
BOOL report_generate(wchar_t* path_out, size_t cch)
{
    wchar_t dir[MAX_PATH * 2], file[MAX_PATH * 2], stamp[32], host[256], os[256];
    SYSTEMTIME st;
    HANDLE h;
    MtOut o;
    ProcInfo* pr = NULL;
    int np, i, cpu_ok = 0;
    ULONGLONG tot = 0, avail = 0;
    int load = 0;
    DriveInfo d[26];
    int nd;
    wchar_t t1[64], t2[64], line[256];

    path_join(dir, MAX_PATH * 2, g_exe_dir, L"reports");
    if (!ensure_dir(dir)) {
        wchar_t docs[MAX_PATH];
        get_folder(CSIDL_PERSONAL, docs);
        path_join(dir, MAX_PATH * 2, docs, L"MultiTool Reports");
        ensure_dir(dir);
    }
    GetLocalTime(&st);
    wfmt(stamp, 32, L"%04d%02d%02d_%02d%02d%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    wfmt(line, 256, L"health_report_%ls.txt", stamp);
    path_join(file, MAX_PATH * 2, dir, line);
    h = CreateFileW(file, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    { DWORD w; WriteFile(h, "\xEF\xBB\xBF", 3, &w, NULL); }
    out_init_file(&o, h);

    sys_hostname(host, 256);
    sys_os_string(os, 256);
    out_line(&o, OS_NORMAL, L"MULTITOOL SYSTEM HEALTH REPORT");
    out_line(&o, OS_NORMAL, L"==========================================");
    out_line(&o, OS_NORMAL, L"Generated : %04d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    out_line(&o, OS_NORMAL, L"Hostname  : %ls", host);
    out_line(&o, OS_NORMAL, L"OS        : %ls", os);
    out_line(&o, OS_NORMAL, L"MultiTool : %ls (native C build)", MT_VERSION);
    out_line(&o, OS_NORMAL, L"");
    out_line(&o, OS_NORMAL, L"RESOURCE USAGE");
    out_line(&o, OS_NORMAL, L"------------------------------------------");
    sys_cpu_percent(); Sleep(500); cpu_ok = 1;
    out_line(&o, OS_NORMAL, L"CPU       : %.1f%%", cpu_ok ? sys_cpu_percent() : 0.0);
    sys_mem(&tot, &avail, &load);
    fmt_size(tot - avail, t1, 64); fmt_size(tot, t2, 64);
    out_line(&o, OS_NORMAL, L"RAM       : %d%% (%ls / %ls)", load, t1, t2);
    nd = sys_drives(d, 26);
    for (i = 0; i < nd; i++) {
        ULONGLONG used = d[i].total - d[i].free;
        fmt_size(used, t1, 64); fmt_size(d[i].total, t2, 64);
        out_line(&o, OS_NORMAL, L"DISK %-4ls : %.1f%% (%ls / %ls)", d[i].root, 100.0 * (double)used / (double)d[i].total, t1, t2);
    }
    fmt_duration(sys_uptime_secs(), t1, 64);
    out_line(&o, OS_NORMAL, L"Uptime    : %ls", t1);
    out_line(&o, OS_NORMAL, L"");
    out_line(&o, OS_NORMAL, L"TOP 5 PROCESSES BY RAM");
    out_line(&o, OS_NORMAL, L"------------------------------------------");
    proc_cache_refresh();
    np = procs_snapshot(&pr);
    for (i = 0; i < np; i++) {   /* simple selection of top 5 */
        int j, best = i;
        for (j = i + 1; j < np; j++) if (pr[j].ws > pr[best].ws) best = j;
        if (best != i) { ProcInfo tmp = pr[i]; pr[i] = pr[best]; pr[best] = tmp; }
        if (i >= 4) break;
    }
    for (i = 0; i < np && i < 5; i++) {
        out_line(&o, OS_NORMAL, L"PID %6lu | %-30ls | %8.1f MB", (unsigned long)pr[i].pid, pr[i].name, (double)pr[i].ws / (1024.0 * 1024.0));
    }
    free(pr);
    CloseHandle(h);
    wcopy(path_out, cch, file);
    return TRUE;
}

void f_healthreport(MtOut* o, const FeatArgs* a)
{
    wchar_t p[MAX_PATH * 2];
    (void)a;
    out_title(o, T2(L"SYSTEM HEALTH REPORT", L"LAPORAN KESEHATAN SISTEM"));
    if (report_generate(p, MT_ARRAY(p))) {
        out_ok(o, T2(L"Report created:", L"Laporan berhasil dibuat:"));
        out_line(o, OS_ACCENT, L"    %ls", p);
    } else out_err(o, T2(L"Failed to create report.", L"Gagal membuat report."));
}
