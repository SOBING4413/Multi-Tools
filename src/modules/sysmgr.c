#include "modules/sysmgr.h"
#include "core/util.h"
#include <shlobj.h>

static void se_push(StartupEntry** a, int* n, int* cap, const StartupEntry* e)
{
    if (*n == *cap) {
        StartupEntry* na = (StartupEntry*)realloc(*a, sizeof(StartupEntry) * (size_t)(*cap ? *cap * 2 : 32));
        if (!na) return;
        *a = na;
        *cap = *cap ? *cap * 2 : 32;
    }
    (*a)[(*n)++] = *e;
}

static void scan_run_key(StartupEntry** a, int* n, int* cap, HKEY root, REGSAM wow, const wchar_t* sub, const wchar_t* label)
{
    HKEY k;
    DWORD i;
    if (RegOpenKeyExW(root, sub, 0, KEY_READ | wow, &k) != ERROR_SUCCESS) return;
    for (i = 0;; i++) {
        wchar_t name[256];
        BYTE data[2048];
        DWORD nl = 256, dl = sizeof(data) - 2, type = 0;
        LONG r = RegEnumValueW(k, i, name, &nl, NULL, &type, data, &dl);
        StartupEntry e;
        if (r == ERROR_NO_MORE_ITEMS) break;
        if (r != ERROR_SUCCESS) continue;
        if (type != REG_SZ && type != REG_EXPAND_SZ) continue;
        ZeroMemory(&e, sizeof(e));
        data[dl] = 0; data[dl + 1] = 0;
        wcopy(e.name, 256, name);
        if (type == REG_EXPAND_SZ) expand_env((wchar_t*)data, e.cmd, 1024); else wcopy(e.cmd, 1024, (wchar_t*)data);
        wcopy(e.where, 64, label);
        e.root = root; e.wow = wow; wcopy(e.key, 256, sub);
        se_push(a, n, cap, &e);
    }
    RegCloseKey(k);
}

static void scan_folder(StartupEntry** a, int* n, int* cap, int csidl, const wchar_t* label)
{
    wchar_t dir[MAX_PATH], pat[MAX_PATH * 2];
    WIN32_FIND_DATAW fd;
    HANDLE h;
    if (!get_folder(csidl, dir)) return;
    wfmt(pat, MT_ARRAY(pat), L"%ls\\*", dir);
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        StartupEntry e;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (_wcsicmp(fd.cFileName, L"desktop.ini") == 0) continue;
        ZeroMemory(&e, sizeof(e));
        wcopy(e.name, 256, fd.cFileName);
        wfmt(e.file, MAX_PATH, L"%ls\\%ls", dir, fd.cFileName);
        wcopy(e.cmd, 1024, e.file);
        wcopy(e.where, 64, label);
        e.is_file = 1;
        se_push(a, n, cap, &e);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

int startup_list(StartupEntry** out)
{
    StartupEntry* a = NULL;
    int n = 0, cap = 0;
    const wchar_t* run = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    const wchar_t* once = L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce";
    scan_run_key(&a, &n, &cap, HKEY_CURRENT_USER, 0, run, L"HKCU Run");
    scan_run_key(&a, &n, &cap, HKEY_CURRENT_USER, 0, once, L"HKCU RunOnce");
    scan_run_key(&a, &n, &cap, HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY, run, L"HKLM Run");
    scan_run_key(&a, &n, &cap, HKEY_LOCAL_MACHINE, KEY_WOW64_32KEY, run, L"HKLM Run (32-bit)");
    scan_run_key(&a, &n, &cap, HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY, once, L"HKLM RunOnce");
    scan_folder(&a, &n, &cap, CSIDL_STARTUP, L"Startup folder (user)");
    scan_folder(&a, &n, &cap, CSIDL_COMMON_STARTUP, L"Startup folder (all users)");
    *out = a;
    return n;
}

BOOL startup_remove(const StartupEntry* e, wchar_t* err, size_t cch)
{
    if (e->is_file) {
        SetFileAttributesW(e->file, FILE_ATTRIBUTE_NORMAL);
        if (DeleteFileW(e->file)) return TRUE;
        wfmt(err, cch, L"Cannot delete file (error %lu)", (unsigned long)GetLastError());
        return FALSE;
    } else {
        HKEY k;
        LONG r = RegOpenKeyExW(e->root, e->key, 0, KEY_SET_VALUE | e->wow, &k);
        if (r != ERROR_SUCCESS) {
            wcopy(err, cch, r == ERROR_ACCESS_DENIED ? T2(L"Access denied - run as Administrator", L"Akses ditolak - jalankan sebagai Administrator") : T2(L"Cannot open registry key", L"Tidak dapat membuka registry key"));
            return FALSE;
        }
        r = RegDeleteValueW(k, e->name);
        RegCloseKey(k);
        if (r == ERROR_SUCCESS) return TRUE;
        wfmt(err, cch, L"Registry error %ld", (long)r);
        return FALSE;
    }
}

void startup_target_path(const wchar_t* cmd, wchar_t* out, size_t cch)
{
    wchar_t tmp[1024];
    wchar_t* p;
    out[0] = 0;
    wcopy(tmp, 1024, cmd);
    wtrim(tmp);
    if (tmp[0] == L'"') {
        p = wcschr(tmp + 1, L'"');
        if (p) *p = 0;
        wcopy(out, cch, tmp + 1);
    } else {
        /* take up to the first ".exe" (case-insensitive) if present; otherwise first space */
        wchar_t low[1024];
        size_t i;
        for (i = 0; tmp[i] && i < 1023; i++) low[i] = (wchar_t)towlower(tmp[i]);
        low[i] = 0;
        p = wcsstr(low, L".exe");
        if (p) { size_t len = (size_t)(p - low) + 4; tmp[len] = 0; }
        else { wchar_t* sp = wcschr(tmp, L' '); if (sp) *sp = 0; }
        wcopy(out, cch, tmp);
    }
    {
        wchar_t exp[MAX_PATH];
        expand_env(out, exp, MAX_PATH);
        wcopy(out, cch, exp);
    }
}

/* ------------------------- installed apps ------------------------- */
static void app_push(AppEntry** a, int* n, int* cap, const AppEntry* e)
{
    int i;
    for (i = 0; i < *n; i++)
        if (_wcsicmp((*a)[i].name, e->name) == 0 && _wcsicmp((*a)[i].ver, e->ver) == 0) return;   /* duplicate (32/64) */
    if (*n == *cap) {
        AppEntry* na = (AppEntry*)realloc(*a, sizeof(AppEntry) * (size_t)(*cap ? *cap * 2 : 128));
        if (!na) return;
        *a = na;
        *cap = *cap ? *cap * 2 : 128;
    }
    (*a)[(*n)++] = *e;
}

static void scan_uninstall(AppEntry** a, int* n, int* cap, HKEY root, REGSAM wow, const wchar_t* label)
{
    HKEY k;
    DWORD i;
    if (RegOpenKeyExW(root, L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall", 0, KEY_READ | wow, &k) != ERROR_SUCCESS) return;
    for (i = 0;; i++) {
        wchar_t sub[256];
        DWORD sl = 256;
        HKEY sk;
        LONG r = RegEnumKeyExW(k, i, sub, &sl, NULL, NULL, NULL, NULL);
        AppEntry e;
        DWORD v = 0, sysc = 0;
        wchar_t tmp[64];
        if (r == ERROR_NO_MORE_ITEMS) break;
        if (r != ERROR_SUCCESS) continue;
        if (RegOpenKeyExW(k, sub, 0, KEY_READ | wow, &sk) != ERROR_SUCCESS) continue;
        ZeroMemory(&e, sizeof(e));
        {
            DWORD t = 0, sz = sizeof(e.name) - sizeof(wchar_t);
            if (RegQueryValueExW(sk, L"DisplayName", NULL, &t, (LPBYTE)e.name, &sz) != ERROR_SUCCESS || (t != REG_SZ && t != REG_EXPAND_SZ)) e.name[0] = 0;
        }
        if (e.name[0]) {
            DWORD t, sz;
            DWORD dsz = sizeof(DWORD);
            sz = sizeof(e.ver) - 2; RegQueryValueExW(sk, L"DisplayVersion", NULL, &t, (LPBYTE)e.ver, &sz);
            sz = sizeof(e.pub) - 2; RegQueryValueExW(sk, L"Publisher", NULL, &t, (LPBYTE)e.pub, &sz);
            sz = sizeof(tmp) - 2;
            if (RegQueryValueExW(sk, L"InstallDate", NULL, &t, (LPBYTE)tmp, &sz) == ERROR_SUCCESS && t == REG_SZ && wcslen(tmp) == 8)
                wfmt(e.date, 16, L"%.4ls-%.2ls-%.2ls", tmp, tmp + 4, tmp + 6);
            sz = sizeof(e.uninstall) - 2; RegQueryValueExW(sk, L"UninstallString", NULL, &t, (LPBYTE)e.uninstall, &sz);
            v = 0; dsz = sizeof(DWORD);
            if (RegQueryValueExW(sk, L"EstimatedSize", NULL, &t, (LPBYTE)&v, &dsz) == ERROR_SUCCESS && t == REG_DWORD) e.size_kb = v;
            dsz = sizeof(DWORD);
            if (RegQueryValueExW(sk, L"SystemComponent", NULL, &t, (LPBYTE)&sysc, &dsz) == ERROR_SUCCESS && sysc == 1) e.name[0] = 0;
            sz = sizeof(tmp) - 2;
            if (e.name[0] && RegQueryValueExW(sk, L"ParentKeyName", NULL, &t, (LPBYTE)tmp, &sz) == ERROR_SUCCESS) e.name[0] = 0;
            wcopy(e.where, 16, label);
            if (e.name[0]) app_push(a, n, cap, &e);
        }
        RegCloseKey(sk);
    }
    RegCloseKey(k);
}

int apps_list(AppEntry** out)
{
    AppEntry* a = NULL;
    int n = 0, cap = 0;
    scan_uninstall(&a, &n, &cap, HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY, L"64-bit");
    scan_uninstall(&a, &n, &cap, HKEY_LOCAL_MACHINE, KEY_WOW64_32KEY, L"32-bit");
    scan_uninstall(&a, &n, &cap, HKEY_CURRENT_USER, 0, L"User");
    *out = a;
    return n;
}
