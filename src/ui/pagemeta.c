/* pagemeta.c - titles, sub-titles and icons of the application pages */
#include "ui/ui.h"
#include "core/util.h"

static const wchar_t* nice_case(const wchar_t* in)
{
    /* "POWER dan SYSTEM" -> "Power dan System": lower-case ASCII capitals that follow another capital */
    static wchar_t buf[48][96];
    static int idx = 0;
    wchar_t* o = buf[idx++ % 48];
    size_t i, n = wcslen(in);
    if (n >= 95) return in;
    for (i = 0; i < n; i++) {
        wchar_t c = in[i];
        if (c >= L'A' && c <= L'Z' && i > 0 && in[i - 1] >= L'A' && in[i - 1] <= L'Z') c = (wchar_t)(c + 32);
        o[i] = c;
    }
    o[n] = 0;
    return o;
}

static const wchar_t* page_title_raw(int p);

const wchar_t* page_title(int p)
{
    const wchar_t* raw = page_title_raw(p);
    if (p == PG_SETTINGS) {
        static wchar_t sb[4][96];
        static int si = 0;
        wchar_t* o = sb[si++ & 3];
        wchar_t* cut;
        wcopy(o, 96, raw);
        cut = wcsstr(o, L" (");
        if (!cut) cut = wcschr(o, 0xFF08);
        if (cut) *cut = 0;
        wtrim(o);
        return o;
    }
    return nice_case(raw);
}

static const wchar_t* page_title_raw(int p)
{
    switch (p) {
    case PG_DASH: return T2(L"Dashboard", L"Dashboard");
    case PG_MONITOR: return T2(L"Live Monitor", L"Monitor Langsung");
    case PG_POWER: return TX(K_cat_power);
    case PG_SYSTEM: return TX(K_cat_info);
    case PG_PROCS: return TX(K_cat_process);
    case PG_NETWORK: return TX(K_cat_network);
    case PG_SECURITY: return TX(K_cat_security);
    case PG_MALWARE: return T2(L"Malware Scan", L"Pemindai Malware");
    case PG_DRIVERS: return T2(L"Driver Scanner", L"Pemindai Driver");
    case PG_CLEANUP: return TX(K_cat_cleanup);
    case PG_TOOLS: return TX(K_cat_shortcuts);
    case PG_SETTINGS: return TX(K_menu_settings);
    default: return T2(L"About", L"Tentang");
    }
}

const wchar_t* page_subtitle(int p)
{
    switch (p) {
    case PG_DASH: return T2(L"Live overview of your computer", L"Ringkasan langsung komputer kamu");
    case PG_MONITOR: return T2(L"Real-time CPU, memory and network graphs", L"Grafik CPU, memori, dan jaringan real-time");
    case PG_POWER: return T2(L"Shutdown, restart, sleep and lock", L"Shutdown, restart, sleep, dan lock");
    case PG_SYSTEM: return T2(L"Hardware, storage, battery and user information", L"Info hardware, penyimpanan, baterai, dan user");
    case PG_PROCS: return T2(L"Running programs, startup entries and installed apps", L"Program berjalan, entri startup, dan aplikasi terpasang");
    case PG_NETWORK: return T2(L"IP, ping, traceroute, DNS, WiFi and speed test", L"IP, ping, traceroute, DNS, WiFi, dan tes kecepatan");
    case PG_SECURITY: return T2(L"Firewall, ports, antivirus, accounts and privacy", L"Firewall, port, antivirus, akun, dan privasi");
    case PG_MALWARE: return T2(L"Heuristic scanner with Microsoft Defender integration", L"Pemindai heuristik dengan integrasi Microsoft Defender");
    case PG_DRIVERS: return T2(L"Find outdated, missing and problematic drivers", L"Temukan driver usang, hilang, dan bermasalah");
    case PG_CLEANUP: return T2(L"Free disk space and speed up your PC", L"Bebaskan ruang disk dan percepat PC kamu");
    case PG_TOOLS: return T2(L"Launchers, screenshots, quick actions and hashing", L"Peluncur, screenshot, aksi cepat, dan hash");
    case PG_SETTINGS: return T2(L"Theme, language and behaviour", L"Tema, bahasa, dan perilaku");
    default: return T2(L"Version, credits and license", L"Versi, kredit, dan lisensi");
    }
}

int page_icon(int p)
{
    static const int ic[PG_COUNT] = { ICO_DASH, ICO_MONITOR, ICO_POWER, ICO_INFO, ICO_PROC, ICO_NET, ICO_SHIELD, ICO_BUG, ICO_CHIP, ICO_CLEAN, ICO_TOOLS, ICO_GEAR, ICO_INFO };
    return (p >= 0 && p < PG_COUNT) ? ic[p] : ICO_INFO;
}
