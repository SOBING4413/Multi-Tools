#include "modules/power_control.h"
#include "core/util.h"

static void cap_line(const wchar_t* l, void* u)
{
    wchar_t* dst = (wchar_t*)u;
    if (l[0] && !dst[0]) wcopy(dst, 256, l);
}

BOOL power_schedule(int mode, int minutes, wchar_t* msg, size_t cch)
{
    wchar_t cmd[256], out[256] = L"";
    int rc;
    if (minutes < 1 || minutes > 10000) { wcopy(msg, cch, T2(L"Minutes must be between 1 and 10000.", L"Menit harus antara 1 dan 10000.")); return FALSE; }
    wfmt(cmd, 256, L"shutdown.exe %ls /t %d /c \"MultiTool: %ls in %d minute(s)\"", mode == PW_RESTART ? L"/r" : L"/s", minutes * 60,
         mode == PW_RESTART ? L"restart" : L"shutdown", minutes);
    rc = run_capture(cmd, 15000, CP_OEMCP, cap_line, out, NULL, NULL);
    if (rc == 0) {
        wfmt(msg, cch, mode == PW_RESTART ? T2(L"Restart scheduled in %d minute(s).", L"Restart dijadwalkan %d menit lagi.") : T2(L"Shutdown scheduled in %d minute(s).", L"Shutdown dijadwalkan %d menit lagi."), minutes);
        return TRUE;
    }
    wfmt(msg, cch, T2(L"Failed to schedule: %ls", L"Gagal menjadwalkan: %ls"), out[0] ? out : L"shutdown.exe error");
    return FALSE;
}

BOOL power_cancel(wchar_t* msg, size_t cch)
{
    wchar_t out[256] = L"";
    int rc = run_capture(L"shutdown.exe /a", 10000, CP_OEMCP, cap_line, out, NULL, NULL);
    if (rc == 0) { wcopy(msg, cch, T2(L"Scheduled shutdown/restart cancelled.", L"Jadwal shutdown/restart dibatalkan.")); return TRUE; }
    wcopy(msg, cch, T2(L"No scheduled shutdown/restart to cancel.", L"Tidak ada jadwal shutdown/restart untuk dibatalkan."));
    return FALSE;
}

BOOL power_lock(void) { return LockWorkStation() != 0; }

typedef BOOLEAN (WINAPI *PFN_SetSuspendState)(BOOLEAN, BOOLEAN, BOOLEAN);

BOOL power_sleep_now(void)
{
    HMODULE m = LoadLibraryW(L"powrprof.dll");
    PFN_SetSuspendState fn = m ? (PFN_SetSuspendState)(void*)GetProcAddress(m, "SetSuspendState") : NULL;
    BOOL ok = FALSE;
    if (fn) ok = fn(FALSE, FALSE, FALSE) != 0;
    if (m) FreeLibrary(m);
    return ok;
}

BOOL power_hibernate_now(void)
{
    return run_hidden_nowait(L"shutdown.exe /h");
}
