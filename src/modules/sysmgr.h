/* sysmgr.h - startup entries and installed applications */
#ifndef MT_SYSMGR_H
#define MT_SYSMGR_H
#include "core/common.h"

typedef struct StartupEntry {
    wchar_t name[256];
    wchar_t cmd[1024];
    wchar_t where[64];
    int     is_file;               /* 1 = startup folder shortcut, 0 = registry value */
    HKEY    root;
    REGSAM  wow;
    wchar_t key[256];
    wchar_t file[MAX_PATH];
} StartupEntry;

int  startup_list(StartupEntry** out);                                  /* malloc'd */
BOOL startup_remove(const StartupEntry* e, wchar_t* err, size_t cch);
void startup_target_path(const wchar_t* cmd, wchar_t* out, size_t cch); /* extract executable path */

typedef struct AppEntry {
    wchar_t   name[256], ver[64], pub[128], date[16];
    ULONGLONG size_kb;
    wchar_t   uninstall[1024];
    wchar_t   where[16];
} AppEntry;

int apps_list(AppEntry** out);                                          /* malloc'd */

#endif
