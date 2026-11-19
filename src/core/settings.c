#include "core/settings.h"
#include "core/util.h"

Settings g_set = { 0, 1, 1, -1 };

static void ini_path(wchar_t* out)
{
    path_join(out, MAX_PATH, g_data_dir, L"settings.ini");
}

void settings_load(void)
{
    wchar_t p[MAX_PATH], code[16];
    ini_path(p);
    g_set.theme        = GetPrivateProfileIntW(L"ui", L"theme", 0, p);
    g_set.animations   = GetPrivateProfileIntW(L"ui", L"animations", 1, p);
    g_set.auto_refresh = GetPrivateProfileIntW(L"ui", L"auto_refresh", 1, p);
    if (g_set.theme < 0 || g_set.theme > 5) g_set.theme = 0;
    GetPrivateProfileStringW(L"ui", L"lang", L"", code, 16, p);
    if (code[0]) {
        g_set.lang = lang_from_code(code);
    } else {
        /* first run: follow the Windows UI language when we support it */
        wchar_t loc[LOCALE_NAME_MAX_LENGTH];
        g_set.lang = 0;
        if (GetUserDefaultLocaleName(loc, LOCALE_NAME_MAX_LENGTH) > 0) {
            wchar_t two[4];
            two[0] = loc[0]; two[1] = loc[1]; two[2] = 0;
            if (loc[0] && loc[1]) {
                int i;
                for (i = 0; i < LANG_COUNT; i++)
                    if (_wcsicmp(two, g_lang_code[i]) == 0) { g_set.lang = i; break; }
            }
        }
    }
    lang_set(g_set.lang);
}

void settings_save(void)
{
    wchar_t p[MAX_PATH], v[16];
    ensure_dir(g_data_dir);
    ini_path(p);
    wfmt(v, 16, L"%d", g_set.theme);        WritePrivateProfileStringW(L"ui", L"theme", v, p);
    wfmt(v, 16, L"%d", g_set.animations);   WritePrivateProfileStringW(L"ui", L"animations", v, p);
    wfmt(v, 16, L"%d", g_set.auto_refresh); WritePrivateProfileStringW(L"ui", L"auto_refresh", v, p);
    WritePrivateProfileStringW(L"ui", L"lang", g_lang_code[g_lang], p);
}
