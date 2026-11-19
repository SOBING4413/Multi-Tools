#include "modules/driver_scan.h"
#include "core/util.h"
#include <setupapi.h>
#include <devpropdef.h>
#include <cfgmgr32.h>
#include <shlobj.h>

/* DEVPKEY_Device_Driver* (GUID a8b865dd-2e3d-4094-ad97-e593a70c75d6) */
#define DRV_GUID { 0xa8b865dd, 0x2e3d, 0x4094, { 0xad, 0x97, 0xe5, 0x93, 0xa7, 0x0c, 0x75, 0xd6 } }
static const DEVPROPKEY MK_DriverDate     = { DRV_GUID, 2 };
static const DEVPROPKEY MK_DriverVersion  = { DRV_GUID, 3 };
static const DEVPROPKEY MK_DriverInfPath  = { DRV_GUID, 5 };
static const DEVPROPKEY MK_DriverProvider = { DRV_GUID, 9 };

static BOOL prop_str(HDEVINFO h, SP_DEVINFO_DATA* d, const DEVPROPKEY* k, wchar_t* out, size_t cch)
{
    DEVPROPTYPE t = 0;
    DWORD req = 0;
    out[0] = 0;
    if (!SetupDiGetDevicePropertyW(h, d, k, &t, (PBYTE)out, (DWORD)((cch - 1) * sizeof(wchar_t)), &req, 0) || t != DEVPROP_TYPE_STRING) { out[0] = 0; return FALSE; }
    out[cch - 1] = 0;
    return TRUE;
}

static BOOL reg_str(HDEVINFO h, SP_DEVINFO_DATA* d, DWORD prop, wchar_t* out, size_t cch)
{
    DWORD t = 0, req = 0;
    out[0] = 0;
    if (!SetupDiGetDeviceRegistryPropertyW(h, d, prop, &t, (PBYTE)out, (DWORD)((cch - 1) * sizeof(wchar_t)), &req)) { out[0] = 0; return FALSE; }
    out[cch - 1] = 0;
    return TRUE;
}

static const wchar_t* problem_text(DWORD c)
{
    switch (c) {
    case 1:  return T2(L"Device is not configured correctly (Code 1).", L"Perangkat tidak dikonfigurasi dengan benar (Code 1).");
    case 3:  return T2(L"Driver may be corrupted or the system is low on memory (Code 3).", L"Driver mungkin rusak atau memori sistem rendah (Code 3).");
    case 10: return T2(L"Device cannot start (Code 10). Update or reinstall the driver.", L"Perangkat tidak dapat dijalankan (Code 10). Update atau install ulang driver.");
    case 12: return T2(L"Resource conflict with another device (Code 12).", L"Konflik resource dengan perangkat lain (Code 12).");
    case 14: return T2(L"Restart required to finish setting up (Code 14).", L"Perlu restart untuk menyelesaikan pemasangan (Code 14).");
    case 18: return T2(L"Reinstall the drivers for this device (Code 18).", L"Install ulang driver perangkat ini (Code 18).");
    case 19: return T2(L"Registry configuration is damaged (Code 19).", L"Konfigurasi registry rusak (Code 19).");
    case 21: return T2(L"Windows is removing this device (Code 21).", L"Windows sedang menghapus perangkat ini (Code 21).");
    case 24: return T2(L"Device is not present or not working properly (Code 24).", L"Perangkat tidak ada atau tidak berfungsi dengan benar (Code 24).");
    case 28: return T2(L"No driver installed. Install the vendor driver or use Windows Update (Code 28).", L"Driver belum terinstall. Install driver vendor atau gunakan Windows Update (Code 28).");
    case 31: return T2(L"Device is not working properly: Windows cannot load its driver (Code 31).", L"Perangkat tidak berfungsi: Windows tidak dapat memuat drivernya (Code 31).");
    case 32: return T2(L"The driver service is disabled (Code 32).", L"Layanan driver dinonaktifkan (Code 32).");
    case 37: return T2(L"Driver failed to initialize (Code 37). Update the driver.", L"Driver gagal diinisialisasi (Code 37). Update driver.");
    case 39: return T2(L"Driver is corrupted or missing (Code 39).", L"Driver rusak atau hilang (Code 39).");
    case 40: return T2(L"Registry service key is missing (Code 40).", L"Registry service key hilang (Code 40).");
    case 41: return T2(L"Driver loaded but the device was not found (Code 41).", L"Driver dimuat tetapi perangkat tidak ditemukan (Code 41).");
    case 43: return T2(L"Windows stopped this device because it reported problems (Code 43).", L"Windows menghentikan perangkat ini karena melaporkan masalah (Code 43).");
    case 45: return T2(L"Device is not connected (Code 45).", L"Perangkat tidak terhubung (Code 45).");
    case 48: return T2(L"The driver was blocked from starting (Code 48). Update the software.", L"Driver diblokir untuk berjalan (Code 48). Update software-nya.");
    case 52: return T2(L"Windows cannot verify the driver's digital signature (Code 52).", L"Windows tidak dapat memverifikasi tanda tangan digital driver (Code 52).");
    default: return NULL;
    }
}

const wchar_t* driver_state_text(int s)
{
    switch (s) {
    case DS_OK:       return T2(L"OK", L"OK");
    case DS_OLD:      return T2(L"Old", L"Lama");
    case DS_OUTDATED: return T2(L"Outdated", L"Usang");
    case DS_GENERIC:  return T2(L"Generic", L"Generik");
    case DS_UPDATE:   return T2(L"Update available", L"Ada update");
    case DS_DISABLED: return T2(L"Disabled", L"Nonaktif");
    case DS_UNSIGNED: return T2(L"Unsigned", L"Tanpa tanda tangan");
    case DS_PROBLEM:  return T2(L"Problem", L"Bermasalah");
    case DS_MISSING:  return T2(L"Missing", L"Tidak ada");
    default: return L"?";
    }
}

int driver_state_severity(int s)
{
    switch (s) {
    case DS_OK: return 0;
    case DS_DISABLED: case DS_GENERIC: return 1;
    case DS_OLD: case DS_OUTDATED: case DS_UPDATE: return 2;
    default: return 3;
    }
}

void driver_recount(DriverScan* ds)
{
    int i;
    for (i = 0; i < DS__COUNT; i++) ds->cnt[i] = 0;
    for (i = 0; i < ds->n; i++) ds->cnt[ds->rows[i].state]++;
}

static void classify(DriverRow* r)
{
    BOOL ms = wcontains_i(r->provider, L"Microsoft") || (!r->provider[0] && wcontains_i(r->mfg, L"Microsoft")) ||
              wcontains_i(r->mfg, L"(Standard system devices)") || wcontains_i(r->mfg, L"(Standard disk drives)");
    r->state = DS_OK;
    wcopy(r->note, 320, T2(L"Driver looks fine.", L"Driver terlihat baik."));
    if (r->problem == 22) { r->state = DS_DISABLED; wcopy(r->note, 320, T2(L"Device is disabled (Code 22).", L"Perangkat dinonaktifkan (Code 22).")); return; }
    if (r->problem == 28) { r->state = DS_MISSING; wcopy(r->note, 320, problem_text(28)); return; }
    if (r->problem == 52) { r->state = DS_UNSIGNED; wcopy(r->note, 320, problem_text(52)); return; }
    if (r->problem != 0) {
        const wchar_t* t = problem_text(r->problem);
        r->state = DS_PROBLEM;
        if (t) wcopy(r->note, 320, t); else wfmt(r->note, 320, T2(L"Device reports problem code %lu.", L"Perangkat melaporkan kode masalah %lu."), (unsigned long)r->problem);
        return;
    }
    if (ms) {
        if (wcscmp(r->cls, L"Display") == 0 && wcontains_i(r->name, L"Basic")) {
            r->state = DS_GENERIC;
            wcopy(r->note, 320, T2(L"Generic Windows display driver. Install the GPU vendor driver (NVIDIA / AMD / Intel) for full performance.", L"Driver display generik Windows. Install driver vendor GPU (NVIDIA / AMD / Intel) untuk performa penuh."));
        } else if ((wcscmp(r->cls, L"MEDIA") == 0 || wcscmp(r->cls, L"Media") == 0) && wcontains_i(r->name, L"High Definition Audio Device")) {
            r->state = DS_GENERIC;
            wcopy(r->note, 320, T2(L"Generic Windows audio driver. The vendor driver (Realtek, etc.) may add features.", L"Driver audio generik Windows. Driver vendor (Realtek, dll.) mungkin menambah fitur."));
        } else if (wcscmp(r->cls, L"Bluetooth") == 0 && wcontains_i(r->name, L"Generic")) {
            r->state = DS_GENERIC;
            wcopy(r->note, 320, T2(L"Generic Bluetooth adapter driver. Install the vendor driver for stable pairing.", L"Driver Bluetooth generik. Install driver vendor untuk pairing yang stabil."));
        } else wcopy(r->note, 320, T2(L"Windows built-in driver (maintained through Windows Update).", L"Driver bawaan Windows (dirawat lewat Windows Update)."));
        return;
    }
    if (r->age_days >= 0) {
        if (r->age_days > 1825) {
            r->state = DS_OUTDATED;
            wfmt(r->note, 320, T2(L"Driver is %d years old. Updating is recommended.", L"Driver berumur %d tahun. Disarankan untuk update."), r->age_days / 365);
        } else if (r->age_days > 1095) {
            r->state = DS_OLD;
            wfmt(r->note, 320, T2(L"Driver is %d years old. Check for a newer version.", L"Driver berumur %d tahun. Cek versi yang lebih baru."), r->age_days / 365);
        }
    }
}

DriverScan* driver_scan_local(volatile LONG* cancel)
{
    DriverScan* ds = (DriverScan*)calloc(1, sizeof(DriverScan));
    HDEVINFO h;
    SP_DEVINFO_DATA d;
    DWORD i;
    int cap = 0;
    FILETIME now;
    ULARGE_INTEGER un;
    if (!ds) return NULL;
    h = SetupDiGetClassDevsW(NULL, NULL, NULL, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (h == INVALID_HANDLE_VALUE) return ds;
    GetSystemTimeAsFileTime(&now);
    un.LowPart = now.dwLowDateTime; un.HighPart = now.dwHighDateTime;
    ZeroMemory(&d, sizeof(d));
    d.cbSize = sizeof(d);
    for (i = 0; SetupDiEnumDeviceInfo(h, i, &d); i++) {
        DriverRow* r;
        ULONG status = 0, problem = 0;
        DEVPROPTYPE t = 0;
        DWORD req = 0;
        FILETIME ft;
        wchar_t hw[1024];
        if (cancel && *cancel) break;
        if (ds->n == cap) {
            DriverRow* nr = (DriverRow*)realloc(ds->rows, sizeof(DriverRow) * (size_t)(cap ? cap * 2 : 128));
            if (!nr) break;
            ds->rows = nr;
            cap = cap ? cap * 2 : 128;
        }
        r = &ds->rows[ds->n];
        ZeroMemory(r, sizeof(*r));
        r->age_days = -1;
        if (!reg_str(h, &d, SPDRP_FRIENDLYNAME, r->name, 256) || !r->name[0]) reg_str(h, &d, SPDRP_DEVICEDESC, r->name, 256);
        reg_str(h, &d, SPDRP_CLASS, r->cls, 64);
        reg_str(h, &d, SPDRP_MFG, r->mfg, 128);
        prop_str(h, &d, &MK_DriverProvider, r->provider, 128);
        prop_str(h, &d, &MK_DriverVersion, r->version, 64);
        prop_str(h, &d, &MK_DriverInfPath, r->inf, 64);
        SetupDiGetDeviceInstanceIdW(h, &d, r->instance, 256, NULL);
        ZeroMemory(hw, sizeof(hw));
        if (SetupDiGetDeviceRegistryPropertyW(h, &d, SPDRP_HARDWAREID, NULL, (PBYTE)hw, sizeof(hw) - 4, &req)) {
            const wchar_t* p = hw;
            int cnt = 0;
            while (*p && cnt < 3) {
                if (cnt) wcsncat(r->hwid, L" | ", 511 - wcslen(r->hwid));
                wcsncat(r->hwid, p, 511 - wcslen(r->hwid));
                p += wcslen(p) + 1;
                cnt++;
            }
        }
        if (!r->name[0]) wcopy(r->name, 256, r->hwid[0] ? r->hwid : T2(L"Unknown device", L"Perangkat tidak dikenal"));
        ZeroMemory(&ft, sizeof(ft));
        if (SetupDiGetDevicePropertyW(h, &d, &MK_DriverDate, &t, (PBYTE)&ft, sizeof(ft), &req, 0) && t == DEVPROP_TYPE_FILETIME && (ft.dwLowDateTime || ft.dwHighDateTime)) {
            ULARGE_INTEGER ud;
            ud.LowPart = ft.dwLowDateTime; ud.HighPart = ft.dwHighDateTime;
            fmt_filetime(&ft, r->date, 16, FALSE);
            if (un.QuadPart > ud.QuadPart) r->age_days = (int)((un.QuadPart - ud.QuadPart) / (864000000000ULL));
            else r->age_days = 0;
        }
        if (CM_Get_DevNode_Status(&status, &problem, d.DevInst, 0) == CR_SUCCESS) r->problem = (status & DN_HAS_PROBLEM) ? problem : 0;
        if (!r->version[0] && r->problem == 0 && !r->provider[0]) r->age_days = -1;
        classify(r);
        ds->n++;
    }
    SetupDiDestroyDeviceInfoList(h);
    driver_recount(ds);
    return ds;
}

void driver_scan_free(DriverScan* ds)
{
    if (!ds) return;
    free(ds->rows);
    free(ds);
}

/* ---------------- Windows Update lookup ---------------- */
typedef struct WuCtx { DriverScan* ds; int cap; int done; } WuCtx;

static void wu_line(const wchar_t* line, void* u)
{
    WuCtx* c = (WuCtx*)u;
    wchar_t buf[2048], *f[8];
    int n = 0, i;
    wchar_t* p;
    DriverScan* ds = c->ds;
    if (wcscmp(line, L"WU_DONE=1") == 0) { c->done = 1; return; }
    if (wcsncmp(line, L"WU=", 3) != 0) return;
    wcopy(buf, 2048, line + 3);
    p = buf;
    while (n < 8) { f[n++] = p; p = wcschr(p, L'|'); if (!p) break; *p++ = 0; }
    if (n < 6) return;
    /* f: 0 title, 1 model, 2 mfg, 3 class, 4 date, 5 hwid, 6 size */
    {
        wchar_t ver[64] = L"";
        wchar_t* dash = wcsrchr(f[0], L'-');
        int matched = 0;
        if (dash) { wchar_t* v = dash + 1; while (*v == L' ') v++; if (*v >= L'0' && *v <= L'9') wcopy(ver, 64, v); }
        ds->wu_found++;
        if (f[5][0]) {
            for (i = 0; i < ds->n; i++) {
                if (!ds->rows[i].from_wu_only && ds->rows[i].hwid[0] && wcontains_i(ds->rows[i].hwid, f[5])) {
                    DriverRow* r = &ds->rows[i];
                    wcopy(r->wu_title, 256, f[0]);
                    wcopy(r->wu_ver, 64, ver);
                    wcopy(r->wu_date, 16, f[4]);
                    if (r->state == DS_OK || r->state == DS_OLD || r->state == DS_OUTDATED || r->state == DS_GENERIC) {
                        r->state = DS_UPDATE;
                        wfmt(r->note, 320, T2(L"Windows Update offers: %ls (%ls).", L"Windows Update menawarkan: %ls (%ls)."), f[0], f[4]);
                    }
                    matched = 1;
                    break;
                }
            }
        }
        if (!matched) {
            DriverRow* r;
            if (ds->n >= c->cap) {
                int nc = c->cap ? c->cap * 2 : 128;
                DriverRow* nr = (DriverRow*)realloc(ds->rows, sizeof(DriverRow) * (size_t)nc);
                if (!nr) return;
                ds->rows = nr;
                c->cap = nc;
            }
            r = &ds->rows[ds->n++];
            ZeroMemory(r, sizeof(*r));
            wcopy(r->name, 256, f[1][0] ? f[1] : f[0]);
            wcopy(r->cls, 64, f[3]);
            wcopy(r->mfg, 128, f[2]);
            wcopy(r->provider, 128, f[2]);
            wcopy(r->version, 64, ver);
            wcopy(r->date, 16, f[4]);
            wcopy(r->hwid, 512, f[5]);
            wcopy(r->wu_title, 256, f[0]);
            wcopy(r->wu_ver, 64, ver);
            wcopy(r->wu_date, 16, f[4]);
            r->age_days = -1;
            r->from_wu_only = 1;
            r->state = DS_UPDATE;
            wfmt(r->note, 320, T2(L"Available from Windows Update: %ls", L"Tersedia dari Windows Update: %ls"), f[0]);
        }
    }
}

void driver_scan_wu(DriverScan* ds, volatile LONG* cancel)
{
    WuCtx c;
    int rc;
    c.ds = ds;
    c.cap = ds->n > 0 ? ds->n : 0;       /* realloc capacity is unknown; force growth path to be safe */
    c.cap = ds->n;
    c.done = 0;
    ds->wu_checked = 1;
    ds->wu_found = 0;
    /* ensure rows buffer has room to grow: copy into an exact-size block first */
    {
        DriverRow* nr = (DriverRow*)realloc(ds->rows, sizeof(DriverRow) * (size_t)(ds->n + 1));
        if (nr) ds->rows = nr;
        c.cap = ds->n + 1;
    }
    rc = run_ps(
        L"$s=New-Object -ComObject Microsoft.Update.Session;$se=$s.CreateUpdateSearcher();"
        L"$r=$se.Search(\"IsInstalled=0 and Type='Driver' and IsHidden=0\");"
        L"foreach($u in $r.Updates){Write-Output ('WU='+$u.Title+'|'+$u.DriverModel+'|'+$u.DriverManufacturer+'|'+$u.DriverClass+'|'+$u.DriverVerDate.ToString('yyyy-MM-dd')+'|'+$u.DriverHardwareID+'|'+[math]::Round($u.MaxDownloadSize/1MB,1))};"
        L"Write-Output 'WU_DONE=1'",
        240000, wu_line, &c, cancel, NULL);
    ds->wu_ok = c.done && rc == 0;
    driver_recount(ds);
}

/* ---------------- export / jobs ---------------- */
static void csv_field(HANDLE h, const wchar_t* s, BOOL last)
{
    wchar_t buf[1200];
    size_t i, k = 0;
    char* u;
    DWORD w;
    buf[k++] = L'"';
    for (i = 0; s[i] && k < 1190; i++) { if (s[i] == L'"') buf[k++] = L'"'; buf[k++] = s[i]; }
    buf[k++] = L'"';
    buf[k++] = last ? L'\n' : L',';
    buf[k] = 0;
    u = w_to_utf8(buf);
    if (u) { WriteFile(h, u, (DWORD)strlen(u), &w, NULL); free(u); }
}

BOOL driver_export_csv(const DriverScan* ds, const wchar_t* path)
{
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    int i;
    DWORD w;
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    WriteFile(h, "\xEF\xBB\xBF", 3, &w, NULL);
    csv_field(h, L"Device", FALSE); csv_field(h, L"Class", FALSE); csv_field(h, L"Manufacturer", FALSE); csv_field(h, L"Provider", FALSE);
    csv_field(h, L"Version", FALSE); csv_field(h, L"Date", FALSE); csv_field(h, L"Status", FALSE); csv_field(h, L"Windows Update", FALSE);
    csv_field(h, L"Hardware ID", FALSE); csv_field(h, L"Notes", TRUE);
    for (i = 0; i < ds->n; i++) {
        const DriverRow* r = &ds->rows[i];
        csv_field(h, r->name, FALSE); csv_field(h, r->cls, FALSE); csv_field(h, r->mfg, FALSE); csv_field(h, r->provider, FALSE);
        csv_field(h, r->version, FALSE); csv_field(h, r->date, FALSE); csv_field(h, driver_state_text(r->state), FALSE);
        csv_field(h, r->wu_title, FALSE); csv_field(h, r->hwid, FALSE); csv_field(h, r->note, TRUE);
    }
    CloseHandle(h);
    return TRUE;
}

static void log_line_cb(const wchar_t* line, void* u)
{
    if (line[0]) out_line((MtOut*)u, OS_NORMAL, L"  %ls", line);
}

void driver_backup_job(MtOut* o)
{
    wchar_t docs[MAX_PATH], base[MAX_PATH * 2], dir[MAX_PATH * 2], cmd[MAX_PATH * 3], stamp[32];
    SYSTEMTIME st;
    int rc;
    out_line(o, OS_TITLE, T2(L"DRIVER BACKUP", L"BACKUP DRIVER"));
    if (!g_admin) { out_warn(o, T2(L"Driver export needs Administrator rights. Restart MultiTool as Administrator.", L"Export driver butuh hak Administrator. Jalankan ulang MultiTool sebagai Administrator.")); return; }
    get_folder(CSIDL_PERSONAL, docs);
    path_join(base, MAX_PATH * 2, docs, L"MultiTool Driver Backup");
    GetLocalTime(&st);
    wfmt(stamp, 32, L"%04d%02d%02d_%02d%02d%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    path_join(dir, MAX_PATH * 2, base, stamp);
    ensure_dir(dir);
    out_info(o, T2(L"Exporting third-party drivers to: %ls", L"Mengekspor driver pihak ketiga ke: %ls"), dir);
    wfmt(cmd, MT_ARRAY(cmd), L"pnputil.exe /export-driver * \"%ls\"", dir);
    rc = run_capture(cmd, 600000, CP_OEMCP, log_line_cb, o, o->cancel, NULL);
    if (rc == 0) out_ok(o, T2(L"Backup finished: %ls", L"Backup selesai: %ls"), dir);
    else out_err(o, T2(L"pnputil failed (code %d).", L"pnputil gagal (kode %d)."), rc);
}

void driver_rescan_job(MtOut* o)
{
    int rc;
    out_line(o, OS_TITLE, T2(L"RESCAN HARDWARE", L"PINDAI ULANG HARDWARE"));
    rc = run_capture(L"pnputil.exe /scan-devices", 60000, CP_OEMCP, log_line_cb, o, o->cancel, NULL);
    if (rc == 0) out_ok(o, T2(L"Hardware rescan requested.", L"Pindai ulang hardware diminta."));
    else out_warn(o, T2(L"Rescan could not run (needs Administrator).", L"Pindai ulang tidak bisa dijalankan (butuh Administrator)."));
}
