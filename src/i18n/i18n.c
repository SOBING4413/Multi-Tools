/* i18n.c - language lookup */
#include "core/common.h"
#include "core/util.h"

int g_lang = 0;

const wchar_t* TX(I18nKey key)
{
    const wchar_t* s;
    if ((int)key < 0 || key >= K__COUNT) return L"?";
    s = g_i18n[key][g_lang];
    if (!s || !*s) s = g_i18n[key][0];
    return s;
}

const wchar_t* T2(const wchar_t* en, const wchar_t* id)
{
    /* Indonesian, Sundanese and Javanese users get the Indonesian text */
    if (g_lang == 1 || g_lang == 6 || g_lang == 7) return id;
    return en;
}

int lang_from_code(const wchar_t* code)
{
    int i;
    if (!code) return 0;
    for (i = 0; i < LANG_COUNT; i++)
        if (_wcsicmp(code, g_lang_code[i]) == 0) return i;
    return 0;
}

void lang_set(int idx)
{
    if (idx >= 0 && idx < LANG_COUNT) g_lang = idx;
}
