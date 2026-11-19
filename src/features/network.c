/* f_network.c - IP, ping, traceroute, DNS, WiFi, speed test (native Win32 APIs) */
#include "features/feat.h"
#include "core/util.h"
#include "modules/sys.h"
#include <iphlpapi.h>
#include <netioapi.h>
#include <icmpapi.h>
#include <winhttp.h>
#include <wlanapi.h>

#ifndef WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
#define WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY 4
#endif
#define MT_WLAN_PROFILE_GET_PLAINTEXT_KEY 4

/* ===================== HTTP (WinHTTP) ===================== */
typedef struct HttpState {
    ULONGLONG total;
    double    t_first, t_end;
    int       status;
} HttpState;

static HINTERNET http_open(void)
{
    HINTERNET h = WinHttpOpen(L"MultiTool/3.0 (+native)", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!h) h = WinHttpOpen(L"MultiTool/3.0 (+native)", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    return h;
}

/* Generic request. body may be NULL. `out` (optional) collects the response; otherwise data is discarded.
   Returns TRUE if a response was received (check st->status). */
static BOOL http_fetch(const wchar_t* url, const wchar_t* method, const void* body, DWORD blen, Buf* out,
                       HttpState* st, volatile LONG* cancel, DWORD recv_timeout_ms, MtOut* prog, double prog_expect)
{
    URL_COMPONENTS uc;
    wchar_t host[256], path[2048], extra[2048], full[4200];
    HINTERNET hs = NULL, hc = NULL, hr = NULL;
    BOOL ok = FALSE;
    DWORD sz, status = 0;
    char* chunk = NULL;

    memset(st, 0, sizeof(*st));
    ZeroMemory(&uc, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host;  uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;   uc.dwUrlPathLength = 2048;
    uc.lpszExtraInfo = extra; uc.dwExtraInfoLength = 2048;
    if (!WinHttpCrackUrl(url, 0, 0, &uc)) return FALSE;
    wfmt(full, MT_ARRAY(full), L"%ls%ls", path, extra);

    hs = http_open();
    if (!hs) return FALSE;
    WinHttpSetTimeouts(hs, 8000, 8000, 15000, (int)recv_timeout_ms);
    hc = WinHttpConnect(hs, host, uc.nPort, 0);
    if (!hc) goto done;
    hr = WinHttpOpenRequest(hc, method, full, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                            uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    if (!hr) goto done;
    if (!WinHttpSendRequest(hr, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)body, blen, blen, 0)) goto done;
    if (!WinHttpReceiveResponse(hr, NULL)) goto done;
    sz = sizeof(status);
    WinHttpQueryHeaders(hr, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);
    st->status = (int)status;
    chunk = (char*)malloc(65536);
    if (!chunk) goto done;
    st->t_first = qpc_seconds();
    for (;;) {
        DWORD got = 0;
        if (cancel && *cancel) break;
        if (!WinHttpReadData(hr, chunk, 65536, &got)) break;
        if (got == 0) { ok = TRUE; break; }
        st->total += got;
        if (out) buf_add(out, chunk, got);
        if (prog && prog_expect > 0) out_progress(prog, MT_MIN(100.0, 100.0 * (double)st->total / prog_expect));
    }
    st->t_end = qpc_seconds();
    if (!ok && st->status) ok = (st->total > 0);   /* partial counts for timing */
    if (st->status == 0) ok = FALSE;
done:
    free(chunk);
    if (hr) WinHttpCloseHandle(hr);
    if (hc) WinHttpCloseHandle(hc);
    if (hs) WinHttpCloseHandle(hs);
    return ok || st->status != 0;
}

static BOOL http_get_text(const wchar_t* url, Buf* out, volatile LONG* cancel, int* status)
{
    HttpState st;
    BOOL r = http_fetch(url, L"GET", NULL, 0, out, &st, cancel, 10000, NULL, 0);
    if (status) *status = st.status;
    return r && st.status == 200;
}

/* ===================== adapters ===================== */
static PIP_ADAPTER_ADDRESSES get_adapters(void)
{
    ULONG sz = 16384, r, tries = 0;
    PIP_ADAPTER_ADDRESSES aa = NULL;
    do {
        free(aa);
        aa = (PIP_ADAPTER_ADDRESSES)malloc(sz);
        if (!aa) return NULL;
        r = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST, NULL, aa, &sz);
    } while (r == ERROR_BUFFER_OVERFLOW && ++tries < 4);
    if (r != NO_ERROR) { free(aa); return NULL; }
    return aa;
}

static void prefix_to_mask(ULONG prefix, wchar_t* out)
{
    unsigned long m = prefix >= 32 ? 0xFFFFFFFFUL : (prefix == 0 ? 0 : (0xFFFFFFFFUL << (32 - prefix)));
    wfmt(out, 32, L"%lu.%lu.%lu.%lu", (m >> 24) & 255, (m >> 16) & 255, (m >> 8) & 255, m & 255);
}

static void mac_to_str(const PIP_ADAPTER_ADDRESSES p, wchar_t* out, size_t cch)
{
    ULONG i;
    out[0] = 0;
    if (p->PhysicalAddressLength == 0) return;
    for (i = 0; i < p->PhysicalAddressLength; i++) {
        wchar_t b[8];
        wfmt(b, 8, L"%02X%ls", p->PhysicalAddress[i], i + 1 < p->PhysicalAddressLength ? L":" : L"");
        wcsncat(out, b, cch - wcslen(out) - 1);
    }
}

static BOOL first_ipv4(PIP_ADAPTER_ADDRESSES p, wchar_t* ip, ULONG* prefix)
{
    PIP_ADAPTER_UNICAST_ADDRESS ua;
    for (ua = p->FirstUnicastAddress; ua; ua = ua->Next) {
        if (ua->Address.lpSockaddr && ua->Address.lpSockaddr->sa_family == AF_INET) {
            struct sockaddr_in* s = (struct sockaddr_in*)ua->Address.lpSockaddr;
            InetNtopW(AF_INET, &s->sin_addr, ip, 64);
            if (prefix) *prefix = ua->OnLinkPrefixLength;
            return TRUE;
        }
    }
    return FALSE;
}

/* ===================== Check IP ===================== */
void f_checkip(MtOut* o, const FeatArgs* a)
{
    PIP_ADAPTER_ADDRESSES aa, p;
    wchar_t host[256], ip[64], mask[32];
    Buf b;
    int status = 0;
    (void)a;
    out_title(o, T2(L"CHECK IP ADDRESS", L"CEK IP ADDRESS"));
    out_section(o, T2(L"LOCAL IP ADDRESS", L"IP ADDRESS LOKAL"));
    sys_hostname(host, 256);
    out_kv(o, L"Hostname", L"%ls", host);
    aa = get_adapters();
    if (aa) {
        BOOL shown = FALSE;
        for (p = aa; p; p = p->Next) {
            ULONG pre = 0;
            if (p->OperStatus != IfOperStatusUp || p->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
            if (p->FirstGatewayAddress && first_ipv4(p, ip, &pre)) {
                out_kv_style(o, T2(L"Local IP", L"IP Lokal"), OS_OK, L"%ls", ip);
                shown = TRUE;
                break;
            }
        }
        if (!shown) out_warn(o, T2(L"No active network connection with a gateway.", L"Tidak ada koneksi jaringan aktif."));
        out_section(o, T2(L"ALL NETWORK INTERFACES", L"SEMUA NETWORK INTERFACE"));
        for (p = aa; p; p = p->Next) {
            ULONG pre = 0;
            if (!first_ipv4(p, ip, &pre)) continue;
            prefix_to_mask(pre, mask);
            out_kv(o, p->FriendlyName, L"%ls (Netmask: %ls)", ip, mask);
        }
        free(aa);
    }
    out_section(o, T2(L"PUBLIC IP ADDRESS", L"IP ADDRESS PUBLIK"));
    out_info(o, T2(L"Fetching public IP...", L"Mengambil IP publik..."));
    buf_init(&b);
    if (http_get_text(L"https://api.ipify.org?format=json", &b, o->cancel, &status) && b.data) {
        char ipc[64];
        if (json_get_str(b.data, "ip", ipc, sizeof(ipc))) {
            wchar_t* w = utf8_to_w(ipc, -1);
            out_kv_style(o, T2(L"Public IP", L"IP Publik"), OS_OK, L"%ls", w);
            free(w);
        } else out_warn(o, T2(L"Unexpected response from IP service.", L"Respons layanan IP tidak sesuai."));
    } else if (status) out_warn(o, T2(L"IP service returned HTTP %d.", L"Layanan IP mengembalikan HTTP %d."), status);
    else out_warn(o, T2(L"No internet connection (or request timed out).", L"Tidak ada koneksi internet (atau timeout)."));
    buf_free(&b);
}

/* ===================== IP detail ===================== */
static void jkv(MtOut* o, const wchar_t* label, const char* json, const char* key)
{
    char v[256];
    wchar_t* w;
    if (!json_get_str(json, key, v, sizeof(v))) v[0] = 0;
    w = utf8_to_w(v[0] ? v : "N/A", -1);
    out_kv(o, label, L"%ls", w);
    free(w);
}

void f_ipdetail(MtOut* o, const FeatArgs* a)
{
    Buf b;
    int status = 0;
    PIP_ADAPTER_ADDRESSES aa, p;
    (void)a;
    out_title(o, T2(L"IP REAL - FULL DETAIL", L"IP REAL - DETAIL LENGKAP"));
    out_info(o, T2(L"Fetching IP information...", L"Mengambil informasi IP..."));
    buf_init(&b);
    if (http_get_text(L"http://ip-api.com/json/?fields=status,message,continent,country,countryCode,region,regionName,city,district,zip,lat,lon,timezone,offset,currency,isp,org,as,asname,mobile,proxy,hosting,query", &b, o->cancel, &status) && b.data) {
        char st[32] = "";
        json_get_str(b.data, "status", st, sizeof(st));
        if (strcmp(st, "success") == 0) {
            double lat = 0, lon = 0, off = 0;
            BOOL mobile = FALSE, proxy = FALSE, host = FALSE;
            char cc[16] = "", rc[16] = "", dist[128] = "";
            wchar_t* w;
            out_section(o, T2(L"PUBLIC IP INFORMATION", L"INFORMASI IP PUBLIK"));
            jkv(o, L"IP Address", b.data, "query");
            out_section(o, T2(L"LOCATION", L"LOKASI"));
            jkv(o, T2(L"Continent", L"Benua"), b.data, "continent");
            json_get_str(b.data, "countryCode", cc, sizeof(cc));
            { char c[128] = ""; wchar_t* wc, *wcc; json_get_str(b.data, "country", c, sizeof(c)); wc = utf8_to_w(c, -1); wcc = utf8_to_w(cc, -1);
              out_kv(o, T2(L"Country", L"Negara"), L"%ls (%ls)", wc, wcc); free(wc); free(wcc); }
            json_get_str(b.data, "region", rc, sizeof(rc));
            { char c[128] = ""; wchar_t* wc, *wr; json_get_str(b.data, "regionName", c, sizeof(c)); wc = utf8_to_w(c, -1); wr = utf8_to_w(rc, -1);
              out_kv(o, T2(L"Region", L"Provinsi"), L"%ls (%ls)", wc, wr); free(wc); free(wr); }
            jkv(o, T2(L"City", L"Kota"), b.data, "city");
            json_get_str(b.data, "district", dist, sizeof(dist));
            if (dist[0]) { w = utf8_to_w(dist, -1); out_kv(o, T2(L"District", L"Kecamatan"), L"%ls", w); free(w); }
            jkv(o, T2(L"Postal code", L"Kode pos"), b.data, "zip");
            json_get_num(b.data, "lat", &lat); json_get_num(b.data, "lon", &lon);
            out_kv(o, T2(L"Coordinates", L"Koordinat"), L"%.4f, %.4f", lat, lon);
            out_kv(o, L"Google Maps", L"https://www.google.com/maps?q=%.4f,%.4f", lat, lon);
            out_section(o, T2(L"PROVIDER & NETWORK", L"PROVIDER & JARINGAN"));
            jkv(o, L"ISP", b.data, "isp");
            jkv(o, T2(L"Organization", L"Organisasi"), b.data, "org");
            jkv(o, L"AS Number", b.data, "as");
            jkv(o, L"AS Name", b.data, "asname");
            out_section(o, T2(L"ADDITIONAL DETAILS", L"DETAIL TAMBAHAN"));
            jkv(o, L"Timezone", b.data, "timezone");
            json_get_num(b.data, "offset", &off);
            out_kv(o, L"UTC Offset", L"UTC%ls%.1f", off >= 0 ? L"+" : L"", off / 3600.0);
            jkv(o, T2(L"Currency", L"Mata uang"), b.data, "currency");
            json_get_bool(b.data, "mobile", &mobile);
            out_kv(o, L"Mobile", L"%ls", mobile ? T2(L"Yes", L"Ya") : T2(L"No", L"Tidak"));
            json_get_bool(b.data, "proxy", &proxy);
            out_kv_style(o, L"Proxy/VPN", proxy ? OS_WARN : OS_OK, L"%ls", proxy ? T2(L"Yes (!)", L"Ya (!)") : T2(L"No", L"Tidak"));
            json_get_bool(b.data, "hosting", &host);
            out_kv(o, L"Hosting/DC", L"%ls", host ? T2(L"Yes", L"Ya") : T2(L"No", L"Tidak"));
        } else {
            char m[128] = "unknown";
            wchar_t* w;
            json_get_str(b.data, "message", m, sizeof(m));
            w = utf8_to_w(m, -1);
            out_err(o, T2(L"Failed: %ls", L"Gagal: %ls"), w);
            free(w);
        }
    } else if (status) out_err(o, T2(L"API returned HTTP %d.", L"API mengembalikan HTTP %d."), status);
    else out_err(o, T2(L"No internet connection or timeout contacting API.", L"Tidak ada koneksi internet atau timeout saat menghubungi API."));
    buf_free(&b);

    out_section(o, T2(L"MAC ADDRESS (LOCAL)", L"MAC ADDRESS (LOKAL)"));
    aa = get_adapters();
    if (aa) {
        BOOL any = FALSE;
        for (p = aa; p; p = p->Next) {
            wchar_t mac[64];
            mac_to_str(p, mac, 64);
            if (!mac[0] || p->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
            out_kv(o, p->FriendlyName, L"%ls", mac);
            any = TRUE;
        }
        if (!any) out_info(o, T2(L"Could not detect MAC address.", L"Tidak dapat mendeteksi MAC address."));
        free(aa);
    }
}

/* ===================== WLAN (dynamic) ===================== */
typedef DWORD (WINAPI *PFN_WlanOpenHandle)(DWORD, PVOID, PDWORD, PHANDLE);
typedef DWORD (WINAPI *PFN_WlanCloseHandle)(HANDLE, PVOID);
typedef DWORD (WINAPI *PFN_WlanEnumInterfaces)(HANDLE, PVOID, PWLAN_INTERFACE_INFO_LIST*);
typedef DWORD (WINAPI *PFN_WlanGetProfileList)(HANDLE, const GUID*, PVOID, PWLAN_PROFILE_INFO_LIST*);
typedef DWORD (WINAPI *PFN_WlanGetProfile)(HANDLE, const GUID*, LPCWSTR, PVOID, LPWSTR*, DWORD*, DWORD*);
typedef DWORD (WINAPI *PFN_WlanQueryInterface)(HANDLE, const GUID*, WLAN_INTF_OPCODE, PVOID, PDWORD, PVOID*, PWLAN_OPCODE_VALUE_TYPE);
typedef VOID  (WINAPI *PFN_WlanFreeMemory)(PVOID);

typedef struct WlanApi {
    HMODULE m;
    PFN_WlanOpenHandle OpenHandle;
    PFN_WlanCloseHandle CloseHandle_;
    PFN_WlanEnumInterfaces EnumInterfaces;
    PFN_WlanGetProfileList GetProfileList;
    PFN_WlanGetProfile GetProfile;
    PFN_WlanQueryInterface QueryInterface;
    PFN_WlanFreeMemory FreeMemory;
    HANDLE h;
} WlanApi;

#define WLAN_FN(type, name) (type)(void*)GetProcAddress(w->m, name)

static BOOL wlan_open(WlanApi* w)
{
    DWORD ver = 0;
    ZeroMemory(w, sizeof(*w));
    w->m = LoadLibraryW(L"wlanapi.dll");
    if (!w->m) return FALSE;
    w->OpenHandle = WLAN_FN(PFN_WlanOpenHandle, "WlanOpenHandle");
    w->CloseHandle_ = WLAN_FN(PFN_WlanCloseHandle, "WlanCloseHandle");
    w->EnumInterfaces = WLAN_FN(PFN_WlanEnumInterfaces, "WlanEnumInterfaces");
    w->GetProfileList = WLAN_FN(PFN_WlanGetProfileList, "WlanGetProfileList");
    w->GetProfile = WLAN_FN(PFN_WlanGetProfile, "WlanGetProfile");
    w->QueryInterface = WLAN_FN(PFN_WlanQueryInterface, "WlanQueryInterface");
    w->FreeMemory = WLAN_FN(PFN_WlanFreeMemory, "WlanFreeMemory");
    if (!w->OpenHandle || !w->EnumInterfaces || !w->GetProfileList || !w->GetProfile || !w->FreeMemory ||
        w->OpenHandle(2, NULL, &ver, &w->h) != ERROR_SUCCESS) {
        FreeLibrary(w->m);
        ZeroMemory(w, sizeof(*w));
        return FALSE;
    }
    return TRUE;
}

static void wlan_close(WlanApi* w)
{
    if (w->m) {
        if (w->h && w->CloseHandle_) w->CloseHandle_(w->h, NULL);
        FreeLibrary(w->m);
    }
    ZeroMemory(w, sizeof(*w));
}

static void xml_tag(const wchar_t* xml, const wchar_t* tag, wchar_t* out, size_t cch)
{
    wchar_t open[64], close[64];
    const wchar_t* s, *e;
    out[0] = 0;
    wfmt(open, 64, L"<%ls>", tag);
    wfmt(close, 64, L"</%ls>", tag);
    s = wcsstr(xml, open);
    if (!s) return;
    s += wcslen(open);
    e = wcsstr(s, close);
    if (!e) return;
    if ((size_t)(e - s) >= cch) e = s + cch - 1;
    wcsncpy(out, s, (size_t)(e - s));
    out[e - s] = 0;
}

static const wchar_t* phy_name(int t)
{
    switch (t) {
    case dot11_phy_type_ofdm: return L"802.11a";
    case dot11_phy_type_hrdsss: return L"802.11b";
    case dot11_phy_type_erp: return L"802.11g";
    case 7: return L"802.11n";
    case 8: return L"802.11ac";
    case 10: return L"802.11ax (Wi-Fi 6)";
    default: return L"802.11";
    }
}

static void wlan_list_profiles(MtOut* o, WlanApi* w, BOOL passwords)
{
    PWLAN_INTERFACE_INFO_LIST il = NULL;
    DWORD i;
    int total = 0;
    if (w->EnumInterfaces(w->h, NULL, &il) != ERROR_SUCCESS || !il) {
        out_warn(o, T2(L"No WiFi adapter available.", L"WiFi adapter tidak tersedia."));
        return;
    }
    if (passwords) {
        out_line(o, OS_ACCENT, L"  %3ls  %-30ls  %-22ls  %ls", L"No", L"SSID", T2(L"Security", L"Keamanan"), L"Password");
        out_sep(o);
    }
    for (i = 0; i < il->dwNumberOfItems; i++) {
        PWLAN_PROFILE_INFO_LIST pl = NULL;
        DWORD j;
        if (w->GetProfileList(w->h, &il->InterfaceInfo[i].InterfaceGuid, NULL, &pl) != ERROR_SUCCESS || !pl) continue;
        for (j = 0; j < pl->dwNumberOfItems; j++) {
            const wchar_t* name = pl->ProfileInfo[j].strProfileName;
            total++;
            if (!passwords) {
                out_line(o, OS_NORMAL, L"  %ls %ls", U_BULLET, name);
            } else {
                LPWSTR xml = NULL;
                DWORD flags = MT_WLAN_PROFILE_GET_PLAINTEXT_KEY, access = 0;
                wchar_t key[256] = L"", auth[64] = L"";
                if (w->GetProfile(w->h, &il->InterfaceInfo[i].InterfaceGuid, name, NULL, &xml, &flags, &access) == ERROR_SUCCESS && xml) {
                    xml_tag(xml, L"keyMaterial", key, 256);
                    xml_tag(xml, L"authentication", auth, 64);
                    w->FreeMemory(xml);
                }
                out_line(o, key[0] ? OS_NORMAL : OS_DIM, L"  %3d  %-30ls  %-22ls  %ls", total, name, auth[0] ? auth : L"-",
                         key[0] ? key : T2(L"(none / hidden / needs Administrator)", L"(tidak ada / hidden / perlu Administrator)"));
            }
        }
        w->FreeMemory(pl);
    }
    w->FreeMemory(il);
    if (total == 0) out_info(o, T2(L"No saved WiFi profiles.", L"Tidak ada WiFi tersimpan."));
    else { out_nl(o); out_info(o, T2(L"%d profile(s) found.", L"Ditemukan %d profil WiFi."), total); }
}

static void wlan_current(MtOut* o, WlanApi* w)
{
    PWLAN_INTERFACE_INFO_LIST il = NULL;
    DWORD i;
    BOOL any = FALSE;
    if (!w->QueryInterface || w->EnumInterfaces(w->h, NULL, &il) != ERROR_SUCCESS || !il) return;
    for (i = 0; i < il->dwNumberOfItems; i++) {
        PWLAN_CONNECTION_ATTRIBUTES ca = NULL;
        DWORD sz = 0;
        if (il->InterfaceInfo[i].isState != wlan_interface_state_connected) continue;
        if (w->QueryInterface(w->h, &il->InterfaceInfo[i].InterfaceGuid, wlan_intf_opcode_current_connection, NULL, &sz, (PVOID*)&ca, NULL) == ERROR_SUCCESS && ca) {
            char ssid[40];
            wchar_t* ws;
            memcpy(ssid, ca->wlanAssociationAttributes.dot11Ssid.ucSSID, ca->wlanAssociationAttributes.dot11Ssid.uSSIDLength);
            ssid[ca->wlanAssociationAttributes.dot11Ssid.uSSIDLength] = 0;
            ws = utf8_to_w(ssid, -1);
            out_kv(o, L"SSID", L"%ls", ws);
            free(ws);
            out_kv(o, T2(L"Adapter", L"Adapter"), L"%ls", il->InterfaceInfo[i].strInterfaceDescription);
            out_kv(o, T2(L"Signal", L"Sinyal"), L"%lu%%", (unsigned long)ca->wlanAssociationAttributes.wlanSignalQuality);
            out_kv(o, L"Radio type", L"%ls", phy_name((int)ca->wlanAssociationAttributes.dot11PhyType));
            out_kv(o, T2(L"Speed (Rx/Tx)", L"Kecepatan (Rx/Tx)"), L"%lu / %lu Mbps", (unsigned long)(ca->wlanAssociationAttributes.ulRxRate / 1000), (unsigned long)(ca->wlanAssociationAttributes.ulTxRate / 1000));
            any = TRUE;
            w->FreeMemory(ca);
        }
    }
    w->FreeMemory(il);
    if (!any) out_info(o, T2(L"Not connected to WiFi or no WiFi adapter.", L"Tidak terhubung ke WiFi atau WiFi adapter tidak tersedia."));
}

/* ===================== Network info ===================== */
void f_netinfo(MtOut* o, const FeatArgs* a)
{
    PIP_ADAPTER_ADDRESSES aa, p;
    WlanApi w;
    (void)a;
    out_title(o, T2(L"FULL NETWORK INFORMATION", L"INFO JARINGAN LENGKAP"));
    out_section(o, L"NETWORK INTERFACES");
    aa = get_adapters();
    if (aa) {
        for (p = aa; p; p = p->Next) {
            wchar_t ip[64], mask[32], mac[64], s1[64], s2[64];
            ULONG pre = 0;
            MIB_IF_ROW2 row;
            if (p->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
            out_nl(o);
            out_line(o, OS_ACCENT, L"  %ls %ls", U_ARROW, p->FriendlyName);
            out_kv(o, T2(L"Description", L"Deskripsi"), L"%ls", p->Description);
            out_kv_style(o, L"Status", p->OperStatus == IfOperStatusUp ? OS_OK : OS_DIM, L"%ls", p->OperStatus == IfOperStatusUp ? L"Up" : L"Down");
            if (first_ipv4(p, ip, &pre)) {
                prefix_to_mask(pre, mask);
                out_kv(o, L"IPv4", L"%ls  (Netmask: %ls)", ip, mask);
            }
            if (p->FirstGatewayAddress && p->FirstGatewayAddress->Address.lpSockaddr && p->FirstGatewayAddress->Address.lpSockaddr->sa_family == AF_INET) {
                struct sockaddr_in* g = (struct sockaddr_in*)p->FirstGatewayAddress->Address.lpSockaddr;
                InetNtopW(AF_INET, &g->sin_addr, ip, 64);
                out_kv(o, L"Gateway", L"%ls", ip);
            }
            {
                PIP_ADAPTER_DNS_SERVER_ADDRESS d;
                for (d = p->FirstDnsServerAddress; d; d = d->Next) {
                    if (d->Address.lpSockaddr && d->Address.lpSockaddr->sa_family == AF_INET) {
                        struct sockaddr_in* sd = (struct sockaddr_in*)d->Address.lpSockaddr;
                        InetNtopW(AF_INET, &sd->sin_addr, ip, 64);
                        out_kv(o, L"DNS", L"%ls", ip);
                    }
                }
            }
            mac_to_str(p, mac, 64);
            if (mac[0]) out_kv(o, L"MAC", L"%ls", mac);
            if (p->TransmitLinkSpeed && p->TransmitLinkSpeed != (ULONG64)-1)
                out_kv(o, T2(L"Link speed", L"Kecepatan link"), L"%I64u Mbps", (unsigned long long)(p->TransmitLinkSpeed / 1000000ULL));
            ZeroMemory(&row, sizeof(row));
            row.InterfaceLuid = p->Luid;
            if (GetIfEntry2(&row) == NO_ERROR) {
                fmt_size(row.OutOctets, s1, 64);
                fmt_size(row.InOctets, s2, 64);
                out_kv(o, L"Traffic", L"%ls %ls   %ls %ls", U_UP, s1, U_DOWN, s2);
            }
        }
        free(aa);
    }
    if (wlan_open(&w)) {
        out_section(o, T2(L"SAVED WIFI", L"WIFI TERSIMPAN"));
        wlan_list_profiles(o, &w, FALSE);
        out_section(o, T2(L"CURRENTLY CONNECTED WIFI", L"WIFI TERHUBUNG SAAT INI"));
        wlan_current(o, &w);
        wlan_close(&w);
    }
}

/* ===================== Ping ===================== */
static BOOL resolve_v4(const wchar_t* host, IPAddr* addr, wchar_t* ipstr)
{
    ADDRINFOW hints, *res = NULL;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (GetAddrInfoW(host, NULL, &hints, &res) != 0 || !res) return FALSE;
    {
        struct sockaddr_in* s = (struct sockaddr_in*)res->ai_addr;
        *addr = s->sin_addr.S_un.S_addr;
        if (ipstr) InetNtopW(AF_INET, &s->sin_addr, ipstr, 64);
    }
    FreeAddrInfoW(res);
    return TRUE;
}

void f_ping(MtOut* o, const FeatArgs* a)
{
    wchar_t host[300], ipstr[64];
    IPAddr addr;
    HANDLE ic;
    int i, sent = 0, recv = 0;
    double mn = 1e9, mx = 0, sum = 0;
    char data[32];
    union { ICMP_ECHO_REPLY r; char pad[sizeof(ICMP_ECHO_REPLY) + 64]; } rb;
    out_title(o, T2(L"CONNECTION SPEED TEST (PING)", L"TES KECEPATAN KONEKSI (PING)"));
    if (a->opt == 0) wcopy(host, 300, L"8.8.8.8");
    else if (a->opt == 1) wcopy(host, 300, L"1.1.1.1");
    else { wcopy(host, 300, a->text); wtrim(host); }
    if (!host_is_safe(host)) {
        out_err(o, T2(L"Invalid input! Only letters, digits, dots, colons and dashes are allowed.", L"Input tidak valid! Hanya huruf, angka, titik, titik dua, dan strip yang diizinkan."));
        return;
    }
    if (!resolve_v4(host, &addr, ipstr)) {
        out_err(o, T2(L"Cannot resolve %ls. Check the name or your internet connection.", L"Domain %ls tidak dapat di-resolve. Periksa input atau koneksi internet."), host);
        return;
    }
    ic = IcmpCreateFile();
    if (ic == INVALID_HANDLE_VALUE) { out_err(o, L"IcmpCreateFile failed (%lu)", (unsigned long)GetLastError()); return; }
    memset(data, 'a', sizeof(data));
    out_info(o, T2(L"Pinging %ls [%ls] with 32 bytes of data (10 packets)...", L"Ping ke %ls [%ls] dengan 32 byte data (10 paket)..."), host, ipstr);
    out_sep(o);
    for (i = 0; i < 10 && !OUT_CANCELLED(o); i++) {
        DWORD n;
        sent++;
        ZeroMemory(&rb, sizeof(rb));
        n = IcmpSendEcho(ic, addr, data, sizeof(data), NULL, &rb, sizeof(rb), 3000);
        if (n > 0 && rb.r.Status == IP_SUCCESS) {
            double ms = (double)rb.r.RoundTripTime;
            recv++;
            sum += ms;
            if (ms < mn) mn = ms;
            if (ms > mx) mx = ms;
            out_line(o, ms < 100 ? OS_OK : (ms < 250 ? OS_WARN : OS_ERR), T2(L"  Reply from %ls: bytes=%lu time=%.0fms TTL=%u", L"  Balasan dari %ls: byte=%lu waktu=%.0fms TTL=%u"),
                     ipstr, (unsigned long)rb.r.DataSize, ms, (unsigned)rb.r.Options.Ttl);
        } else if (n > 0) {
            out_line(o, OS_WARN, T2(L"  Reply from %ls: status %lu", L"  Balasan dari %ls: status %lu"), ipstr, (unsigned long)rb.r.Status);
        } else {
            out_line(o, OS_ERR, T2(L"  Request timed out.", L"  Request timed out."));
        }
        if (i < 9) { int k; for (k = 0; k < 10 && !OUT_CANCELLED(o); k++) Sleep(100); }
    }
    IcmpCloseHandle(ic);
    out_section(o, T2(L"SUMMARY", L"RINGKASAN"));
    out_kv(o, T2(L"Packets", L"Paket"), T2(L"sent %d, received %d, lost %d (%d%% loss)", L"terkirim %d, diterima %d, hilang %d (%d%% loss)"),
           sent, recv, sent - recv, sent ? (100 * (sent - recv) / sent) : 0);
    if (recv) {
        out_kv(o, T2(L"Round trip", L"Waktu pulang-pergi"), L"min %.0f ms / avg %.1f ms / max %.0f ms", mn, sum / recv, mx);
        if (recv == sent) out_ok(o, T2(L"Connection is stable.", L"Koneksi stabil."));
        else out_warn(o, T2(L"Packet loss detected.", L"Terdeteksi packet loss."));
    } else out_err(o, T2(L"No replies. Host may be down or blocking ICMP.", L"Tidak ada balasan. Host mungkin mati atau memblokir ICMP."));
}

/* ===================== Traceroute ===================== */
void f_traceroute(MtOut* o, const FeatArgs* a)
{
    wchar_t host[300], ipstr[64];
    IPAddr addr;
    HANDLE ic;
    int ttl, reached = 0;
    char data[32];
    union { ICMP_ECHO_REPLY r; char pad[sizeof(ICMP_ECHO_REPLY) + 64]; } rb;
    out_title(o, L"TRACEROUTE");
    wcopy(host, 300, a->text);
    wtrim(host);
    if (!host_is_safe(host)) {
        out_err(o, T2(L"Invalid input! Only letters, digits, dots, colons and dashes are allowed.", L"Input tidak valid! Hanya huruf, angka, titik, titik dua, dan strip yang diizinkan."));
        return;
    }
    out_info(o, T2(L"Validating target %ls...", L"Validasi target %ls..."), host);
    if (!resolve_v4(host, &addr, ipstr)) {
        out_err(o, T2(L"Cannot resolve %ls.", L"Domain %ls tidak dapat di-resolve."), host);
        return;
    }
    ic = IcmpCreateFile();
    if (ic == INVALID_HANDLE_VALUE) { out_err(o, L"IcmpCreateFile failed"); return; }
    memset(data, 'a', sizeof(data));
    out_info(o, T2(L"Tracing route to %ls [%ls], max 30 hops", L"Traceroute ke %ls [%ls], maks 30 hop"), host, ipstr);
    out_sep(o);
    for (ttl = 1; ttl <= 30 && !reached && !OUT_CANCELLED(o); ttl++) {
        wchar_t line[256], hop[64] = L"*", t[32];
        int probe;
        IP_OPTION_INFORMATION opt;
        line[0] = 0;
        for (probe = 0; probe < 3 && !OUT_CANCELLED(o); probe++) {
            DWORD n;
            ZeroMemory(&opt, sizeof(opt));
            opt.Ttl = (UCHAR)ttl;
            ZeroMemory(&rb, sizeof(rb));
            n = IcmpSendEcho(ic, addr, data, sizeof(data), &opt, &rb, sizeof(rb), 2500);
            if (n > 0 && (rb.r.Status == IP_SUCCESS || rb.r.Status == IP_TTL_EXPIRED_TRANSIT)) {
                struct in_addr ia;
                ia.S_un.S_addr = rb.r.Address;
                InetNtopW(AF_INET, &ia, hop, 64);
                wfmt(t, 32, L"%4lu ms  ", (unsigned long)rb.r.RoundTripTime);
                if (rb.r.Status == IP_SUCCESS) reached = 1;
            } else if (n > 0) {
                struct in_addr ia;
                ia.S_un.S_addr = rb.r.Address;
                InetNtopW(AF_INET, &ia, hop, 64);
                wfmt(t, 32, L"  !%lu    ", (unsigned long)rb.r.Status);
                reached = 1;
            } else wcopy(t, 32, L"   *      ");
            wcsncat(line, t, 255 - wcslen(line));
        }
        out_line(o, reached ? OS_OK : OS_NORMAL, L"  %2d  %ls %ls", ttl, line, hop);
        out_progress(o, ttl * 100.0 / 30.0);
    }
    IcmpCloseHandle(ic);
    out_nl(o);
    if (reached) out_ok(o, T2(L"Trace complete.", L"Trace selesai."));
    else if (OUT_CANCELLED(o)) out_warn(o, T2(L"Traceroute cancelled.", L"Traceroute dibatalkan."));
    else out_warn(o, T2(L"Destination not reached within 30 hops.", L"Tujuan tidak tercapai dalam 30 hop."));
}

/* ===================== DNS lookup ===================== */
static void dns_print(MtOut* o, const wchar_t* domain, int family)
{
    ADDRINFOW hints, *res = NULL, *p;
    wchar_t seen[16][64];
    int ns = 0, i;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = family;
    hints.ai_socktype = SOCK_STREAM;
    if (GetAddrInfoW(domain, NULL, &hints, &res) != 0 || !res) {
        out_warn(o, T2(L"Not found or invalid domain.", L"Tidak ditemukan atau domain tidak valid."));
        return;
    }
    for (p = res; p && ns < 16; p = p->ai_next) {
        wchar_t ip[64];
        BOOL dup = FALSE;
        if (family == AF_INET) InetNtopW(AF_INET, &((struct sockaddr_in*)p->ai_addr)->sin_addr, ip, 64);
        else InetNtopW(AF_INET6, &((struct sockaddr_in6*)p->ai_addr)->sin6_addr, ip, 64);
        for (i = 0; i < ns; i++) if (wcscmp(seen[i], ip) == 0) dup = TRUE;
        if (dup) continue;
        wcopy(seen[ns++], 64, ip);
        out_line(o, OS_OK, L"  %ls %ls", U_ARROW, ip);
    }
    FreeAddrInfoW(res);
}

static void ns_line(const wchar_t* line, void* u)
{
    if (line[0]) out_line((MtOut*)u, OS_NORMAL, L"  %ls", line);
}

void f_dnslookup(MtOut* o, const FeatArgs* a)
{
    wchar_t domain[300], cmd[400];
    int rc;
    out_title(o, L"DNS LOOKUP");
    wcopy(domain, 300, a->text);
    wtrim(domain);
    if (!host_is_safe(domain)) {
        out_err(o, T2(L"Invalid input! Only letters, digits, dots, colons and dashes are allowed.", L"Input tidak valid! Hanya huruf, angka, titik, titik dua, dan strip yang diizinkan."));
        return;
    }
    out_section(o, L"A Record (IPv4)");
    dns_print(o, domain, AF_INET);
    out_section(o, L"AAAA Record (IPv6)");
    dns_print(o, domain, AF_INET6);
    out_section(o, T2(L"NSLOOKUP DETAIL", L"DETAIL NSLOOKUP"));
    wfmt(cmd, MT_ARRAY(cmd), L"nslookup.exe %ls", domain);
    rc = run_capture(cmd, 15000, CP_OEMCP, ns_line, o, o->cancel, NULL);
    if (rc == RC_FAILED_START) out_warn(o, T2(L"NSLookup not available on this system.", L"NSLookup tidak tersedia di sistem ini."));
    else if (rc == RC_TIMEOUT) out_warn(o, T2(L"NSLookup timed out.", L"NSLookup timeout."));
}

/* ===================== Flush DNS ===================== */
static void plain_line(const wchar_t* line, void* u)
{
    if (line[0]) out_line((MtOut*)u, OS_NORMAL, L"  %ls", line);
}

void f_flushdns(MtOut* o, const FeatArgs* a)
{
    int rc;
    (void)a;
    out_title(o, T2(L"FLUSH DNS CACHE", L"FLUSH DNS CACHE"));
    out_info(o, T2(L"Clearing DNS cache...", L"Membersihkan DNS Cache..."));
    rc = run_capture(L"ipconfig.exe /flushdns", 20000, CP_OEMCP, plain_line, o, o->cancel, NULL);
    out_nl(o);
    if (rc == 0) {
        out_ok(o, T2(L"DNS cache cleared successfully!", L"DNS Cache berhasil dibersihkan!"));
        out_info(o, T2(L"Useful when websites fail to load or resolve.", L"Berguna jika kamu mengalami masalah akses website."));
    } else out_warn(o, T2(L"DNS cache may already be empty, or Administrator rights are required.", L"DNS Cache mungkin sudah dibersihkan, atau memerlukan hak admin."));
}

/* ===================== WiFi toggle ===================== */
static BOOL find_wifi_adapter(wchar_t* name, size_t cch)
{
    PIP_ADAPTER_ADDRESSES aa = get_adapters(), p;
    BOOL found = FALSE;
    if (!aa) return FALSE;
    for (p = aa; p; p = p->Next)
        if (p->IfType == IF_TYPE_IEEE80211) { wcopy(name, cch, p->FriendlyName); found = TRUE; break; }
    free(aa);
    return found;
}

void f_wifitoggle(MtOut* o, const FeatArgs* a)
{
    wchar_t name[128], cmd[400];
    int rc;
    out_title(o, T2(L"TURN WIFI ON / OFF", L"MATIKAN / NYALAKAN WIFI"));
    if (a->opt == 2) {
        out_section(o, T2(L"NETWORK INTERFACE STATUS", L"STATUS NETWORK INTERFACE"));
        run_capture(L"netsh.exe interface show interface", 15000, CP_OEMCP, plain_line, o, o->cancel, NULL);
        return;
    }
    if (!find_wifi_adapter(name, 128)) {
        out_err(o, T2(L"Cannot detect a WiFi adapter.", L"Tidak dapat mendeteksi WiFi adapter."));
        out_info(o, T2(L"Make sure a WiFi adapter is installed and its driver is active.", L"Pastikan WiFi adapter terinstall dan driver sudah aktif."));
        return;
    }
    { wchar_t* q; while ((q = wcschr(name, L'"')) != NULL) *q = L' '; }
    out_info(o, T2(L"WiFi adapter detected: %ls", L"WiFi adapter terdeteksi: %ls"), name);
    wfmt(cmd, MT_ARRAY(cmd), L"netsh.exe interface set interface name=\"%ls\" admin=%ls", name, a->opt == 0 ? L"disabled" : L"enabled");
    rc = run_capture(cmd, 20000, CP_OEMCP, plain_line, o, o->cancel, NULL);
    out_nl(o);
    if (rc == 0) out_ok(o, a->opt == 0 ? T2(L"WiFi turned off.", L"WiFi berhasil dimatikan.") : T2(L"WiFi turned on.", L"WiFi berhasil dinyalakan."));
    else {
        out_warn(o, a->opt == 0 ? T2(L"Failed to turn WiFi off. Administrator rights may be required.", L"Gagal mematikan WiFi. Mungkin memerlukan hak admin.")
                                : T2(L"Failed to turn WiFi on. Administrator rights may be required.", L"Gagal menyalakan WiFi. Mungkin memerlukan hak admin."));
        out_info(o, T2(L"Try running MultiTool as Administrator.", L"Coba jalankan program sebagai Administrator."));
    }
}

/* ===================== WiFi passwords ===================== */
void f_wifipass(MtOut* o, const FeatArgs* a)
{
    WlanApi w;
    (void)a;
    out_title(o, T2(L"SAVED WIFI PASSWORDS", L"PASSWORD WIFI TERSIMPAN"));
    if (!wlan_open(&w)) {
        out_warn(o, T2(L"WLAN service is not available on this PC.", L"Layanan WLAN tidak tersedia di PC ini."));
        return;
    }
    out_nl(o);
    wlan_list_profiles(o, &w, TRUE);
    wlan_close(&w);
    if (!g_admin) {
        out_nl(o);
        out_info(o, T2(L"Tip: run as Administrator to reveal all passwords.", L"Tips: jalankan sebagai Administrator untuk menampilkan semua password."));
    }
}

/* ===================== Speed test ===================== */
static const wchar_t* const k_dl_urls[] = {
    L"https://speed.cloudflare.com/__down?bytes=25000000",
    L"https://speed.hetzner.de/10MB.bin",
    L"https://proof.ovh.net/files/10Mb.dat",
    L"http://speedtest.tele2.net/10MB.zip",
    L"http://ipv4.download.thinkbroadband.com/10MB.zip",
    L"https://bouygues.testdebit.info/10M.iso"
};
static const wchar_t* const k_ul_urls[] = {
    L"https://speed.cloudflare.com/__up",
    L"http://speedtest.tele2.net/upload.php",
    L"https://httpbin.org/post"
};

static void url_host(const wchar_t* url, wchar_t* host, size_t cch)
{
    const wchar_t* s = wcsstr(url, L"://");
    const wchar_t* e;
    s = s ? s + 3 : url;
    e = s;
    while (*e && *e != L'/' && *e != L'?') e++;
    if ((size_t)(e - s) >= cch) e = s + cch - 1;
    wcsncpy(host, s, (size_t)(e - s));
    host[e - s] = 0;
}

void f_speedtest(MtOut* o, const FeatArgs* a)
{
    int i;
    double dl_mbps = 0, ul_mbps = 0;
    BOOL dl_ok = FALSE, ul_ok = FALSE;
    (void)a;
    out_title(o, T2(L"INTERNET SPEED TEST", L"TES KECEPATAN INTERNET"));
    out_section(o, T2(L"DOWNLOAD TEST", L"TES KECEPATAN DOWNLOAD"));
    out_info(o, T2(L"Downloading a test file to measure speed...", L"Mengunduh file test untuk mengukur kecepatan..."));
    for (i = 0; i < MT_ARRAY(k_dl_urls) && !dl_ok && !OUT_CANCELLED(o); i++) {
        HttpState st;
        wchar_t host[128];
        BOOL r;
        url_host(k_dl_urls[i], host, 128);
        out_nl(o);
        out_line(o, OS_DIM, T2(L"  Trying server: %ls...", L"  Mencoba server: %ls..."), host);
        r = http_fetch(k_dl_urls[i], L"GET", NULL, 0, NULL, &st, o->cancel, 30000, o, 10.0 * 1024 * 1024);
        if (OUT_CANCELLED(o)) break;
        if (!r || st.status != 200) {
            if (st.status) out_warn(o, T2(L"Server returned status %d, skipping.", L"Server mengembalikan status %d, skip."), st.status);
            else out_warn(o, T2(L"Server %ls failed.", L"Server %ls gagal."), host);
            continue;
        }
        if (st.total < 1000000ULL) { out_warn(o, T2(L"Data too small for a valid measurement, skipping.", L"Data terlalu kecil untuk pengukuran valid, skip.")); continue; }
        {
            double el = st.t_end - st.t_first, mb = (double)st.total / (1024.0 * 1024.0);
            if (el < 0.001) el = 0.001;
            dl_mbps = ((double)st.total * 8.0) / (el * 1000000.0);
            out_ok(o, T2(L"Download finished from %ls", L"Download selesai dari %ls"), host);
            out_kv(o, T2(L"Size", L"Ukuran"), L"%.2f MB", mb);
            out_kv(o, T2(L"Time", L"Waktu"), T2(L"%.2f seconds", L"%.2f detik"), el);
            out_kv_style(o, T2(L"Speed", L"Kecepatan"), OS_OK, L"%.2f Mbps (%.2f MB/s)", dl_mbps, dl_mbps / 8.0);
            dl_ok = TRUE;
        }
    }
    out_progress(o, -1);
    if (!dl_ok) {
        if (OUT_CANCELLED(o)) out_warn(o, T2(L"Test cancelled.", L"Test dibatalkan."));
        else out_err(o, T2(L"All download servers failed. Check your internet connection.", L"Semua server download test gagal. Periksa koneksi internet."));
        return;
    }

    out_section(o, T2(L"UPLOAD TEST", L"TES KECEPATAN UPLOAD"));
    out_info(o, T2(L"Uploading test data to measure speed...", L"Mengupload data test untuk mengukur kecepatan..."));
    {
        DWORD len = 2 * 1024 * 1024;
        char* data = (char*)malloc(len);
        if (data) memset(data, '0', len);
        for (i = 0; data && i < MT_ARRAY(k_ul_urls) && !ul_ok && !OUT_CANCELLED(o); i++) {
            HttpState st;
            wchar_t host[128];
            double t0, el;
            BOOL r;
            url_host(k_ul_urls[i], host, 128);
            out_nl(o);
            out_line(o, OS_DIM, T2(L"  Trying server: %ls...", L"  Mencoba server: %ls..."), host);
            t0 = qpc_seconds();
            r = http_fetch(k_ul_urls[i], L"POST", data, len, NULL, &st, o->cancel, 30000, NULL, 0);
            el = qpc_seconds() - t0;
            if (!r || (st.status != 200 && st.status != 201 && st.status != 202)) {
                if (st.status) out_warn(o, T2(L"Server returned status %d, skipping.", L"Server mengembalikan status %d, skip."), st.status);
                else out_warn(o, T2(L"Server %ls failed.", L"Server %ls gagal."), host);
                continue;
            }
            if (el < 0.001) el = 0.001;
            ul_mbps = ((double)len * 8.0) / (el * 1000000.0);
            out_ok(o, T2(L"Upload finished.", L"Upload selesai."));
            out_kv(o, T2(L"Data size", L"Ukuran data"), L"%.2f MB", (double)len / (1024.0 * 1024.0));
            out_kv(o, T2(L"Time", L"Waktu"), T2(L"%.2f seconds", L"%.2f detik"), el);
            out_kv_style(o, T2(L"Speed", L"Kecepatan"), OS_OK, L"%.2f Mbps (%.2f MB/s)", ul_mbps, ul_mbps / 8.0);
            ul_ok = TRUE;
        }
        free(data);
    }
    if (!ul_ok && !OUT_CANCELLED(o)) out_warn(o, T2(L"Upload test failed, but download succeeded.", L"Upload test gagal, tapi download berhasil."));

    out_section(o, T2(L"SUMMARY", L"RINGKASAN"));
    out_kv(o, L"Download", L"%.2f Mbps", dl_mbps);
    if (ul_ok) out_kv(o, L"Upload", L"%.2f Mbps", ul_mbps);
    out_nl(o);
    if (dl_mbps > 100) out_ok(o, T2(L"Your internet is VERY FAST!", L"Kecepatan internet kamu SANGAT CEPAT!"));
    else if (dl_mbps > 50) out_ok(o, T2(L"Your internet is FAST!", L"Kecepatan internet kamu CEPAT!"));
    else if (dl_mbps > 10) out_info(o, T2(L"Your internet speed is NORMAL.", L"Kecepatan internet kamu NORMAL."));
    else if (dl_mbps > 1) out_warn(o, T2(L"Your internet is SLOW.", L"Kecepatan internet kamu LAMBAT."));
    else out_err(o, T2(L"Your internet is VERY SLOW!", L"Kecepatan internet kamu SANGAT LAMBAT!"));
    out_nl(o);
    out_line(o, OS_DIM, T2(L"  Note: results may differ from speedtest.net (single-connection test).", L"  Catatan: hasil bisa berbeda dari speedtest.net (tes koneksi tunggal)."));
}
