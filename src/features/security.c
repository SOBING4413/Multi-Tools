/* f_security.c - firewall, ports, antivirus, suspicious connections, accounts, privacy, overview */
#include "features/feat.h"
#include "core/util.h"
#include "modules/sys.h"
#include "modules/procs.h"
#include <iphlpapi.h>
#include <shlobj.h>
#include <lm.h>
#include <lmaccess.h>

/* =========================== connections =========================== */
typedef struct Conn {
    int      proto;           /* 6 = TCP, 17 = UDP */
    int      af;              /* AF_INET / AF_INET6 */
    wchar_t  laddr[64], raddr[64];
    int      lport, rport;
    DWORD    state, pid;
    BOOL     remote_private;
} Conn;

static BOOL v4_private(DWORD addr_net)
{
    const BYTE* b = (const BYTE*)&addr_net;
    if (addr_net == 0) return TRUE;
    if (b[0] == 10 || b[0] == 127) return TRUE;
    if (b[0] == 192 && b[1] == 168) return TRUE;
    if (b[0] == 172 && b[1] >= 16 && b[1] <= 31) return TRUE;
    if (b[0] == 169 && b[1] == 254) return TRUE;
    return FALSE;
}

static BOOL v6_private(const UCHAR* a)
{
    int i, zero = 1;
    for (i = 0; i < 16; i++) if (a[i]) { zero = 0; break; }
    if (zero) return TRUE;
    {   /* ::1 */
        int loop = 1;
        for (i = 0; i < 15; i++) if (a[i]) loop = 0;
        if (loop && a[15] == 1) return TRUE;
    }
    if (a[0] == 0xFE && (a[1] & 0xC0) == 0x80) return TRUE;   /* fe80::/10 */
    if ((a[0] & 0xFE) == 0xFC) return TRUE;                   /* fc00::/7 */
    return FALSE;
}

static void conn_push(Conn** arr, int* n, int* cap, const Conn* c)
{
    if (*n == *cap) {
        Conn* na = (Conn*)realloc(*arr, sizeof(Conn) * (size_t)(*cap ? *cap * 2 : 256));
        if (!na) return;
        *arr = na;
        *cap = *cap ? *cap * 2 : 256;
    }
    (*arr)[(*n)++] = *c;
}

static int conns_collect(Conn** out)
{
    Conn* arr = NULL;
    int n = 0, cap = 0, pass;
    DWORD sz;
    void* tbl;
    *out = NULL;
    proc_cache_refresh();
    for (pass = 0; pass < 4; pass++) {
        ULONG af = (pass == 0 || pass == 2) ? AF_INET : AF_INET6;
        BOOL tcp = pass < 2;
        DWORD r, tries = 0;
        sz = 0;
        tbl = NULL;
        do {
            free(tbl);
            tbl = sz ? malloc(sz) : NULL;
            r = tcp ? GetExtendedTcpTable(tbl, &sz, FALSE, af, TCP_TABLE_OWNER_PID_ALL, 0)
                    : GetExtendedUdpTable(tbl, &sz, FALSE, af, UDP_TABLE_OWNER_PID, 0);
        } while (r == ERROR_INSUFFICIENT_BUFFER && ++tries < 5);
        if (r == NO_ERROR && tbl) {
            DWORD i;
            if (tcp && af == AF_INET) {
                PMIB_TCPTABLE_OWNER_PID t = (PMIB_TCPTABLE_OWNER_PID)tbl;
                for (i = 0; i < t->dwNumEntries; i++) {
                    Conn c;
                    struct in_addr la, ra;
                    ZeroMemory(&c, sizeof(c));
                    c.proto = 6; c.af = AF_INET; c.state = t->table[i].dwState; c.pid = t->table[i].dwOwningPid;
                    la.S_un.S_addr = t->table[i].dwLocalAddr; ra.S_un.S_addr = t->table[i].dwRemoteAddr;
                    InetNtopW(AF_INET, &la, c.laddr, 64); InetNtopW(AF_INET, &ra, c.raddr, 64);
                    c.lport = ntohs((u_short)t->table[i].dwLocalPort); c.rport = ntohs((u_short)t->table[i].dwRemotePort);
                    c.remote_private = v4_private(t->table[i].dwRemoteAddr);
                    conn_push(&arr, &n, &cap, &c);
                }
            } else if (tcp) {
                PMIB_TCP6TABLE_OWNER_PID t = (PMIB_TCP6TABLE_OWNER_PID)tbl;
                for (i = 0; i < t->dwNumEntries; i++) {
                    Conn c;
                    ZeroMemory(&c, sizeof(c));
                    c.proto = 6; c.af = AF_INET6; c.state = t->table[i].dwState; c.pid = t->table[i].dwOwningPid;
                    InetNtopW(AF_INET6, t->table[i].ucLocalAddr, c.laddr, 64);
                    InetNtopW(AF_INET6, t->table[i].ucRemoteAddr, c.raddr, 64);
                    c.lport = ntohs((u_short)t->table[i].dwLocalPort); c.rport = ntohs((u_short)t->table[i].dwRemotePort);
                    c.remote_private = v6_private(t->table[i].ucRemoteAddr);
                    conn_push(&arr, &n, &cap, &c);
                }
            } else if (af == AF_INET) {
                PMIB_UDPTABLE_OWNER_PID t = (PMIB_UDPTABLE_OWNER_PID)tbl;
                for (i = 0; i < t->dwNumEntries; i++) {
                    Conn c;
                    struct in_addr la;
                    ZeroMemory(&c, sizeof(c));
                    c.proto = 17; c.af = AF_INET; c.pid = t->table[i].dwOwningPid; c.remote_private = TRUE;
                    la.S_un.S_addr = t->table[i].dwLocalAddr;
                    InetNtopW(AF_INET, &la, c.laddr, 64);
                    c.lport = ntohs((u_short)t->table[i].dwLocalPort);
                    conn_push(&arr, &n, &cap, &c);
                }
            } else {
                PMIB_UDP6TABLE_OWNER_PID t = (PMIB_UDP6TABLE_OWNER_PID)tbl;
                for (i = 0; i < t->dwNumEntries; i++) {
                    Conn c;
                    ZeroMemory(&c, sizeof(c));
                    c.proto = 17; c.af = AF_INET6; c.pid = t->table[i].dwOwningPid; c.remote_private = TRUE;
                    InetNtopW(AF_INET6, t->table[i].ucLocalAddr, c.laddr, 64);
                    c.lport = ntohs((u_short)t->table[i].dwLocalPort);
                    conn_push(&arr, &n, &cap, &c);
                }
            }
        }
        free(tbl);
    }
    *out = arr;
    return n;
}

typedef struct PortName { int port; const wchar_t* name; } PortName;
static const PortName k_risky[] = {
    { 21, L"FTP (insecure)" }, { 23, L"Telnet (insecure)" }, { 135, L"RPC" }, { 139, L"NetBIOS" },
    { 445, L"SMB (often attacked)" }, { 1433, L"MSSQL" }, { 3306, L"MySQL" }, { 3389, L"Remote Desktop (RDP)" },
    { 5900, L"VNC" }
};
static const PortName k_susp[] = {
    { 4444, L"Metasploit default" }, { 5555, L"ADB / RAT" }, { 6666, L"IRC / Backdoor" }, { 6667, L"IRC (botnet C&C)" },
    { 1337, L"Hacker port" }, { 31337, L"Back Orifice" }, { 12345, L"NetBus" }, { 9001, L"Tor" }, { 9050, L"Tor SOCKS" },
    { 1080, L"SOCKS proxy" }, { 2222, L"Alt SSH / backdoor" }
};

static const wchar_t* port_lookup(const PortName* t, int n, int port)
{
    int i;
    for (i = 0; i < n; i++) if (t[i].port == port) return t[i].name;
    return NULL;
}

static int conn_port_cmp(const void* a, const void* b)
{
    const Conn* x = (const Conn*)a; const Conn* y = (const Conn*)b;
    if (x->lport != y->lport) return x->lport - y->lport;
    return (int)x->pid - (int)y->pid;
}

static int risky_listening_count(void)
{
    Conn* c = NULL;
    int n = conns_collect(&c), i, cnt = 0;
    for (i = 0; i < n; i++)
        if (c[i].proto == 6 && c[i].state == MIB_TCP_STATE_LISTEN && port_lookup(k_risky, MT_ARRAY(k_risky), c[i].lport)) cnt++;
    free(c);
    return cnt;
}

/* =========================== Open ports =========================== */
void f_ports(MtOut* o, const FeatArgs* a)
{
    Conn* c = NULL, *l;
    int n = conns_collect(&c), i, nl = 0, risky = 0, estab = 0;
    wchar_t remotes[256][64];
    int nr = 0;
    (void)a;
    out_title(o, T2(L"OPEN PORTS & NETWORK CONNECTIONS", L"PORT TERBUKA & KONEKSI JARINGAN"));
    l = (Conn*)malloc(sizeof(Conn) * (size_t)(n > 0 ? n : 1));
    if (!l) { free(c); return; }
    for (i = 0; i < n; i++) {
        if (c[i].proto == 6 && c[i].state == MIB_TCP_STATE_LISTEN) l[nl++] = c[i];
        else if (c[i].proto == 6 && c[i].state == MIB_TCP_STATE_ESTAB) {
            int j, dup = 0;
            estab++;
            if (!c[i].remote_private) {
                for (j = 0; j < nr; j++) if (wcscmp(remotes[j], c[i].raddr) == 0) { dup = 1; break; }
                if (!dup && nr < 256) wcopy(remotes[nr++], 64, c[i].raddr);
            }
        }
    }
    qsort(l, (size_t)nl, sizeof(Conn), conn_port_cmp);
    out_section(o, T2(L"LISTENING PORTS (TCP)", L"PORT LISTENING (TCP)"));
    out_line(o, OS_ACCENT, L"  %-6ls %-26ls %-7ls %-24ls %ls", L"Proto", T2(L"Address", L"Alamat"), L"Port", T2(L"Process", L"Proses"), T2(L"Note", L"Keterangan"));
    out_sep(o);
    for (i = 0; i < nl; i++) {
        wchar_t pn[64];
        const wchar_t* r = port_lookup(k_risky, MT_ARRAY(k_risky), l[i].lport);
        if (i > 0 && l[i].lport == l[i - 1].lport && l[i].pid == l[i - 1].pid && l[i].af != l[i - 1].af) { /* v4+v6 duplicate */ }
        proc_name_of(l[i].pid, pn, 64);
        if (r) risky++;
        out_line(o, r ? OS_WARN : OS_NORMAL, L"  %-6ls %-26ls %-7d %-24ls %ls", l[i].af == AF_INET ? L"TCP" : L"TCP6", l[i].laddr, l[i].lport, pn, r ? r : L"");
    }
    if (nl == 0) out_info(o, T2(L"No listening TCP ports found.", L"Tidak ada port TCP listening."));
    out_section(o, T2(L"SUMMARY", L"RINGKASAN"));
    out_kv(o, T2(L"Listening ports", L"Port listening"), L"%d", nl);
    out_kv_style(o, T2(L"Risky ports", L"Port berisiko"), risky ? OS_WARN : OS_OK, L"%d", risky);
    out_kv(o, T2(L"Active connections", L"Koneksi aktif"), L"%d", estab);
    out_kv(o, T2(L"Unique remote IPs", L"IP remote unik"), L"%d", nr);
    if (risky) { out_nl(o); out_warn(o, T2(L"Risky ports are open. Close services you do not need.", L"Ada port berisiko terbuka. Tutup layanan yang tidak dibutuhkan.")); }
    else { out_nl(o); out_ok(o, T2(L"No commonly attacked ports are listening.", L"Tidak ada port yang sering diserang sedang listening.")); }
    free(l);
    free(c);
}

/* =========================== Suspicious connections =========================== */
static const wchar_t* const k_susp_procs[] = {
    L"cmd.exe", L"powershell.exe", L"pwsh.exe", L"wscript.exe", L"cscript.exe", L"mshta.exe",
    L"regsvr32.exe", L"certutil.exe", L"bitsadmin.exe"
};

void f_suspicious(MtOut* o, const FeatArgs* a)
{
    Conn* c = NULL;
    int n = conns_collect(&c), i, j, found = 0, checked = 0;
    (void)a;
    out_title(o, T2(L"SUSPICIOUS CONNECTION DETECTION", L"DETEKSI KONEKSI MENCURIGAKAN"));
    for (i = 0; i < n; i++) {
        wchar_t pn[64];
        const wchar_t* why = NULL;
        if (c[i].proto != 6 || c[i].state != MIB_TCP_STATE_ESTAB || c[i].remote_private) continue;
        checked++;
        proc_name_of(c[i].pid, pn, 64);
        why = port_lookup(k_susp, MT_ARRAY(k_susp), c[i].rport);
        if (!why) for (j = 0; j < MT_ARRAY(k_susp_procs); j++)
            if (_wcsicmp(pn, k_susp_procs[j]) == 0) { why = T2(L"Script/LOLBin process with outbound connection", L"Proses script/LOLBin dengan koneksi keluar"); break; }
        if (why) {
            found++;
            out_line(o, OS_WARN, L"  %ls %ls (PID %lu)", U_WARNSIGN, pn, (unsigned long)c[i].pid);
            out_kv(o, T2(L"Remote", L"Remote"), L"%ls:%d", c[i].raddr, c[i].rport);
            out_kv(o, T2(L"Local", L"Lokal"), L"%ls:%d", c[i].laddr, c[i].lport);
            out_kv_style(o, T2(L"Reason", L"Alasan"), OS_WARN, L"%ls", why);
            out_nl(o);
        }
    }
    out_kv(o, T2(L"External connections checked", L"Koneksi eksternal diperiksa"), L"%d", checked);
    out_nl(o);
    if (!found) out_ok(o, T2(L"No suspicious connections detected.", L"Tidak ada koneksi mencurigakan."));
    else out_warn(o, T2(L"%d suspicious connection(s). Verify each process; use Task Manager to end unknown ones.", L"%d koneksi mencurigakan. Verifikasi setiap proses; akhiri yang tidak dikenal."), found);
    free(c);
}

/* =========================== Firewall =========================== */
static int fw_state(int profile)    /* 0 Domain, 1 Private, 2 Public -> 1 on / 0 off / -1 unknown */
{
    static const wchar_t* pol[] = { L"DomainProfile", L"PrivateProfile", L"PublicProfile" };
    static const wchar_t* loc[] = { L"DomainProfile", L"StandardProfile", L"PublicProfile" };
    wchar_t key[256];
    DWORD v = 0;
    wfmt(key, 256, L"SOFTWARE\\Policies\\Microsoft\\WindowsFirewall\\%ls", pol[profile]);
    if (reg_get_dword(HKEY_LOCAL_MACHINE, key, L"EnableFirewall", &v)) return v ? 1 : 0;
    wfmt(key, 256, L"SYSTEM\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy\\%ls", loc[profile]);
    if (reg_get_dword(HKEY_LOCAL_MACHINE, key, L"EnableFirewall", &v)) return v ? 1 : 0;
    return -1;
}

void f_firewall(MtOut* o, const FeatArgs* a)
{
    static const wchar_t* names[] = { L"Domain", L"Private", L"Public" };
    int i, on = 0;
    (void)a;
    out_title(o, T2(L"WINDOWS FIREWALL STATUS", L"STATUS WINDOWS FIREWALL"));
    out_nl(o);
    for (i = 0; i < 3; i++) {
        int s = fw_state(i);
        out_text(o, OS_KEY, L"  ");
        out_text(o, OS_NORMAL, L"");
        if (s == 1) { on++; out_line(o, OS_OK, L"%ls %-8ls %ls", U_BULLET, names[i], T2(L"ACTIVE", L"AKTIF")); }
        else if (s == 0) out_line(o, OS_ERR, L"%ls %-8ls %ls", U_BULLET, names[i], T2(L"OFF", L"NONAKTIF"));
        else out_line(o, OS_WARN, L"%ls %-8ls %ls", U_BULLET, names[i], T2(L"UNKNOWN", L"TIDAK DIKETAHUI"));
    }
    out_nl(o);
    if (on == 3) out_ok(o, T2(L"All firewall profiles are active.", L"Semua profil firewall aktif."));
    else if (on > 0) out_warn(o, T2(L"Some firewall profiles are not active.", L"Beberapa profil firewall tidak aktif."));
    else out_err(o, T2(L"Firewall is not active! Your PC is at risk.", L"Firewall tidak aktif! PC kamu berisiko."));
    out_nl(o);
    out_info(o, T2(L"Open Windows Security > Firewall & network protection to change settings.", L"Buka Windows Security > Firewall & network protection untuk mengubah pengaturan."));
}

/* =========================== Antivirus =========================== */
typedef struct AvProd { wchar_t name[128]; unsigned long state; } AvProd;
typedef struct AvInfo {
    AvProd prod[8]; int nprod;
    int def_enabled, def_rt;              /* -1 unknown */
    wchar_t sigver[64], sigdate[64];
    int sigage;                           /* days, -1 unknown */
} AvInfo;

static void av_line(const wchar_t* line, void* u)
{
    AvInfo* av = (AvInfo*)u;
    wchar_t* eq = wcschr((wchar_t*)line, L'=');
    wchar_t key[32], val[256];
    if (!eq) return;
    wcopy(key, 32, line);
    key[eq - line] = 0;
    wcopy(val, 256, eq + 1);
    wtrim(val);
    if (wcscmp(key, L"AV") == 0 && av->nprod < 8) {
        wchar_t* bar = wcsrchr(val, L'|');
        if (bar) {
            *bar = 0;
            wcopy(av->prod[av->nprod].name, 128, val);
            av->prod[av->nprod].state = wcstoul(bar + 1, NULL, 10);
            av->nprod++;
        }
    } else if (wcscmp(key, L"DEF_ENABLED") == 0) av->def_enabled = _wcsicmp(val, L"True") == 0;
    else if (wcscmp(key, L"DEF_RT") == 0) av->def_rt = _wcsicmp(val, L"True") == 0;
    else if (wcscmp(key, L"DEF_SIGVER") == 0) wcopy(av->sigver, 64, val);
    else if (wcscmp(key, L"DEF_SIGDATE") == 0) wcopy(av->sigdate, 64, val);
    else if (wcscmp(key, L"DEF_SIGAGE") == 0 && val[0]) av->sigage = _wtoi(val);
}

static void av_query(AvInfo* av, volatile LONG* cancel)
{
    ZeroMemory(av, sizeof(*av));
    av->def_enabled = av->def_rt = -1;
    av->sigage = -1;
    run_ps(
        L"Get-CimInstance -Namespace root/SecurityCenter2 -ClassName AntiVirusProduct | ForEach-Object { Write-Output ('AV='+$_.displayName+'|'+$_.productState) };"
        L"$d=Get-MpComputerStatus; if($d){ Write-Output ('DEF_ENABLED='+$d.AntivirusEnabled); Write-Output ('DEF_RT='+$d.RealTimeProtectionEnabled);"
        L"Write-Output ('DEF_SIGVER='+$d.AntivirusSignatureVersion); Write-Output ('DEF_SIGAGE='+$d.AntivirusSignatureAge);"
        L"if($d.AntivirusSignatureLastUpdated){Write-Output ('DEF_SIGDATE='+$d.AntivirusSignatureLastUpdated.ToString('yyyy-MM-dd HH:mm'))} }",
        45000, av_line, av, cancel, NULL);
}

static BOOL av_good(const AvInfo* av)
{
    int i;
    for (i = 0; i < av->nprod; i++) {
        unsigned long s = av->prod[i].state;
        unsigned scanner = (unsigned)((s >> 12) & 0xF), defs = (unsigned)((s >> 4) & 0xF);
        if ((scanner == 1 || scanner == 3) && defs == 0) return TRUE;
    }
    return av->def_enabled == 1 && av->def_rt == 1 && (av->sigage < 0 || av->sigage <= 7);
}

void f_antivirus(MtOut* o, const FeatArgs* a)
{
    AvInfo av;
    int i;
    (void)a;
    out_title(o, T2(L"ANTIVIRUS STATUS", L"STATUS ANTIVIRUS"));
    out_info(o, T2(L"Querying Windows Security Center...", L"Memeriksa Windows Security Center..."));
    av_query(&av, o->cancel);
    out_nl(o);
    if (av.nprod == 0 && av.def_enabled < 0) {
        out_warn(o, T2(L"Could not read antivirus status (Security Center unavailable).", L"Tidak dapat membaca status antivirus (Security Center tidak tersedia)."));
        return;
    }
    for (i = 0; i < av.nprod; i++) {
        unsigned long s = av.prod[i].state;
        unsigned scanner = (unsigned)((s >> 12) & 0xF), defs = (unsigned)((s >> 4) & 0xF);
        BOOL en = (scanner == 1 || scanner == 3), upd = (defs == 0);
        out_line(o, OS_ACCENT, L"  %ls %ls", U_ARROW, av.prod[i].name);
        out_kv_style(o, T2(L"Protection", L"Perlindungan"), en ? OS_OK : OS_ERR, L"%ls %ls", U_BULLET, en ? T2(L"ACTIVE", L"AKTIF") : T2(L"INACTIVE", L"NONAKTIF"));
        out_kv_style(o, T2(L"Definitions", L"Definisi virus"), upd ? OS_OK : OS_WARN, L"%ls %ls", U_BULLET, upd ? T2(L"Up to date", L"Terbaru") : T2(L"Outdated", L"Kedaluwarsa"));
        out_nl(o);
    }
    if (av.def_enabled >= 0) {
        out_section(o, L"MICROSOFT DEFENDER");
        out_kv_style(o, T2(L"Antivirus", L"Antivirus"), av.def_enabled ? OS_OK : OS_WARN, L"%ls", av.def_enabled ? T2(L"Enabled", L"Aktif") : T2(L"Disabled", L"Nonaktif"));
        out_kv_style(o, T2(L"Real-time protection", L"Perlindungan real-time"), av.def_rt ? OS_OK : OS_WARN, L"%ls", av.def_rt ? T2(L"Enabled", L"Aktif") : T2(L"Disabled", L"Nonaktif"));
        if (av.sigver[0]) out_kv(o, T2(L"Signature version", L"Versi signature"), L"%ls", av.sigver);
        if (av.sigdate[0]) out_kv(o, T2(L"Signatures updated", L"Signature diperbarui"), L"%ls", av.sigdate);
        if (av.sigage >= 0) out_kv_style(o, T2(L"Signature age", L"Umur signature"), av.sigage > 7 ? OS_WARN : OS_OK, T2(L"%d day(s)", L"%d hari"), av.sigage);
    }
    out_nl(o);
    if (av_good(&av)) out_ok(o, T2(L"Your PC is protected by an active antivirus.", L"PC kamu dilindungi antivirus yang aktif."));
    else out_warn(o, T2(L"Antivirus is inactive or outdated. Update definitions in the Malware Scan page.", L"Antivirus nonaktif atau kedaluwarsa. Perbarui definisi di halaman Malware Scan."));
}

/* =========================== User accounts =========================== */
void f_useraccounts(MtOut* o, const FeatArgs* a)
{
    LPUSER_INFO_2 ui = NULL;
    DWORD er = 0, tot = 0, resume = 0, i;
    NET_API_STATUS st;
    int admins_n = 0, nopass = 0;
    BYTE sid[SECURITY_MAX_SID_SIZE];
    DWORD sidlen = sizeof(sid);
    wchar_t grp[128] = L"Administrators", dom[128];
    DWORD gl = 128, dl = 128;
    SID_NAME_USE use;
    LPLOCALGROUP_MEMBERS_INFO_1 gm = NULL;
    DWORD gcnt = 0, gtot = 0;
    (void)a;
    out_title(o, T2(L"USER ACCOUNT ANALYSIS", L"ANALISIS AKUN USER"));
    if (CreateWellKnownSid(WinBuiltinAdministratorsSid, NULL, sid, &sidlen))
        LookupAccountSidW(NULL, sid, grp, &gl, dom, &dl, &use);
    st = NetUserEnum(NULL, 2, FILTER_NORMAL_ACCOUNT, (LPBYTE*)&ui, MAX_PREFERRED_LENGTH, &er, &tot, &resume);
    if ((st == NERR_Success || st == ERROR_MORE_DATA) && ui) {
        out_section(o, T2(L"LOCAL ACCOUNTS", L"AKUN LOKAL"));
        out_line(o, OS_ACCENT, L"  %-22ls %-9ls %-10ls %-18ls %ls", L"Name", L"Enabled", L"Password", T2(L"Last logon", L"Login terakhir"), T2(L"Pwd age", L"Umur pwd"));
        out_sep(o);
        for (i = 0; i < er; i++) {
            BOOL dis = (ui[i].usri2_flags & UF_ACCOUNTDISABLE) != 0, np = (ui[i].usri2_flags & UF_PASSWD_NOTREQD) != 0;
            wchar_t when[64];
            if (ui[i].usri2_last_logon) {
                FILETIME ft; ULARGE_INTEGER u;
                u.QuadPart = 116444736000000000ULL + (ULONGLONG)ui[i].usri2_last_logon * 10000000ULL;
                ft.dwLowDateTime = u.LowPart; ft.dwHighDateTime = u.HighPart;
                fmt_filetime(&ft, when, 64, TRUE);
            } else wcopy(when, 64, T2(L"never", L"belum pernah"));
            if (!dis && np) nopass++;
            out_line(o, dis ? OS_DIM : (np ? OS_WARN : OS_NORMAL), L"  %-22ls %-9ls %-10ls %-18ls %lu d", ui[i].usri2_name,
                     dis ? L"No" : L"Yes", np ? T2(L"NOT REQ.", L"TDK WAJIB") : T2(L"required", L"wajib"), when, (unsigned long)(ui[i].usri2_password_age / 86400));
            if (!dis && _wcsicmp(ui[i].usri2_name, L"Guest") == 0)
                out_line(o, OS_WARN, L"    %ls %ls", U_WARNSIGN, T2(L"Guest account is enabled - recommended to disable.", L"Akun Guest aktif - disarankan dinonaktifkan."));
        }
        NetApiBufferFree(ui);
    } else out_warn(o, T2(L"Cannot read account list (error %lu).", L"Tidak dapat membaca daftar akun (error %lu)."), (unsigned long)st);

    { wchar_t hd[200]; wfmt(hd, 200, L"%ls%ls", T2(L"MEMBERS OF GROUP: ", L"ANGGOTA GRUP: "), grp); out_section(o, hd); }
    if (NetLocalGroupGetMembers(NULL, grp, 1, (LPBYTE*)&gm, MAX_PREFERRED_LENGTH, &gcnt, &gtot, NULL) == NERR_Success && gm) {
        for (i = 0; i < gcnt; i++) { out_line(o, OS_NORMAL, L"  %ls %ls", U_BULLET, gm[i].lgrmi1_name); admins_n++; }
        NetApiBufferFree(gm);
    }
    out_nl(o);
    if (nopass) out_warn(o, T2(L"%d enabled account(s) do not require a password.", L"%d akun aktif tidak mewajibkan password."), nopass);
    if (admins_n > 2) out_warn(o, T2(L"Many administrators (%d). Limit admin accounts when possible.", L"Banyak administrator (%d). Batasi akun admin jika memungkinkan."), admins_n);
    if (!nopass && admins_n <= 2) out_ok(o, T2(L"Account configuration looks reasonable.", L"Konfigurasi akun terlihat wajar."));
}

/* =========================== Privacy cleaner =========================== */
static void clean_pattern(MtOut* o, const wchar_t* dir, const wchar_t* pattern, ULONGLONG* files, ULONGLONG* bytes)
{
    wchar_t pat[MAX_PATH * 2], full[MAX_PATH * 2];
    WIN32_FIND_DATAW fd;
    HANDLE h;
    wfmt(pat, MT_ARRAY(pat), L"%ls\\%ls", dir, pattern);
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        wfmt(full, MT_ARRAY(full), L"%ls\\%ls", dir, fd.cFileName);
        SetFileAttributesW(full, FILE_ATTRIBUTE_NORMAL);
        if (DeleteFileW(full)) { (*files)++; *bytes += ((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow; }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    (void)o;
}

static void plain_ln(const wchar_t* l, void* u) { (void)l; (void)u; }

void f_privacy(MtOut* o, const FeatArgs* a)
{
    ULONGLONG files = 0, bytes = 0;
    wchar_t p[MAX_PATH * 2], win[MAX_PATH], sz[64];
    int opt = a->opt;
    out_title(o, T2(L"PRIVACY CLEANER", L"PEMBERSIH PRIVASI"));
    if (opt == 0 || opt == 4) {
        out_info(o, T2(L"Cleaning recent files...", L"Membersihkan recent files..."));
        if (get_folder(CSIDL_RECENT, p)) { ULONGLONG f0 = files; clean_pattern(o, p, L"*", &files, &bytes); out_ok(o, T2(L"%I64u recent item(s) removed", L"%I64u item recent dihapus"), files - f0); }
    }
    if (opt == 1 || opt == 4) {
        wchar_t la[MAX_PATH];
        ULONGLONG f0 = files;
        out_info(o, T2(L"Cleaning thumbnail cache...", L"Membersihkan thumbnail cache..."));
        if (get_folder(CSIDL_LOCAL_APPDATA, la)) {
            path_join(p, MAX_PATH * 2, la, L"Microsoft\\Windows\\Explorer");
            clean_pattern(o, p, L"thumbcache_*.db", &files, &bytes);
            clean_pattern(o, p, L"iconcache_*.db", &files, &bytes);
        }
        out_ok(o, T2(L"%I64u cache file(s) removed (locked files are skipped)", L"%I64u file cache dihapus (file terkunci dilewati)"), files - f0);
    }
    if (opt == 2 || opt == 4) {
        out_info(o, T2(L"Flushing DNS cache...", L"Membersihkan DNS cache..."));
        if (run_capture(L"ipconfig.exe /flushdns", 20000, CP_OEMCP, plain_ln, NULL, o->cancel, NULL) == 0) out_ok(o, T2(L"DNS cache cleared", L"DNS cache dibersihkan"));
        else out_warn(o, T2(L"DNS cache could not be flushed.", L"DNS cache tidak dapat dibersihkan."));
    }
    if (opt == 3 || opt == 4) {
        DelStats st;
        wchar_t t[MAX_PATH];
        int i;
        wchar_t dirs[3][MAX_PATH];
        out_info(o, T2(L"Cleaning temp files...", L"Membersihkan file temp..."));
        GetTempPathW(MAX_PATH, t);
        { size_t n = wcslen(t); if (n > 3 && t[n - 1] == L'\\') t[n - 1] = 0; }
        wcopy(dirs[0], MAX_PATH, t);
        GetWindowsDirectoryW(win, MAX_PATH);
        path_join(dirs[1], MAX_PATH, win, L"Temp");
        path_join(dirs[2], MAX_PATH, win, L"Prefetch");
        ZeroMemory(&st, sizeof(st));
        for (i = 0; i < 3 && !OUT_CANCELLED(o); i++) del_contents(dirs[i], &st, o->cancel);
        files += st.files; bytes += st.bytes;
        out_ok(o, T2(L"%I64u temp file(s) removed", L"%I64u file temp dihapus"), st.files);
    }
    fmt_size(bytes, sz, 64);
    out_section(o, T2(L"RESULT", L"HASIL"));
    out_kv_style(o, T2(L"Total files removed", L"Total file dihapus"), OS_OK, L"%I64u", files);
    out_kv_style(o, T2(L"Space freed", L"Ruang dibebaskan"), OS_OK, L"%ls", sz);
}

/* =========================== Security overview =========================== */
typedef struct Check { const wchar_t* name; int ok; wchar_t detail[160]; } Check;

void f_secoverview(MtOut* o, const FeatArgs* a)
{
    Check ck[7];
    int n = 0, i, okc = 0, fwon = 0;
    AvInfo av;
    DWORD v = 0;
    double score;
    wchar_t bar[64];
    USER_MODALS_INFO_0* um = NULL;
    (void)a;
    out_title(o, T2(L"SECURITY CHECK RESULTS", L"HASIL PENGECEKAN KEAMANAN"));
    out_info(o, T2(L"Checking system security (about 5 seconds)...", L"Memeriksa keamanan sistem (sekitar 5 detik)..."));

    for (i = 0; i < 3; i++) if (fw_state(i) == 1) fwon++;
    ck[n].name = L"Windows Firewall"; ck[n].ok = fwon == 3 ? 1 : (fwon == 0 ? 0 : 0);
    wfmt(ck[n].detail, 160, fwon == 3 ? T2(L"All profiles active", L"Semua profil aktif") : T2(L"%d of 3 profiles active", L"%d dari 3 profil aktif"), fwon);
    n++;

    av_query(&av, o->cancel);
    ck[n].name = L"Antivirus"; ck[n].ok = av_good(&av);
    wcopy(ck[n].detail, 160, ck[n].ok ? T2(L"Active protection", L"Perlindungan aktif") : T2(L"Inactive or outdated", L"Nonaktif atau kedaluwarsa"));
    n++;

    { int r = risky_listening_count();
      ck[n].name = T2(L"Risky open ports", L"Port berisiko terbuka"); ck[n].ok = (r == 0);
      wfmt(ck[n].detail, 160, r == 0 ? T2(L"None found", L"Tidak ada") : T2(L"%d risky port(s) listening", L"%d port berisiko listening"), r); n++; }

    ck[n].name = T2(L"Password policy", L"Kebijakan password"); ck[n].ok = 0;
    if (NetUserModalsGet(NULL, 0, (LPBYTE*)&um) == NERR_Success && um) {
        ck[n].ok = um->usrmod0_min_passwd_len >= 8;
        wfmt(ck[n].detail, 160, T2(L"Minimum length: %lu", L"Panjang minimum: %lu"), (unsigned long)um->usrmod0_min_passwd_len);
        NetApiBufferFree(um);
    } else wcopy(ck[n].detail, 160, T2(L"Unknown", L"Tidak diketahui"));
    n++;

    ck[n].name = L"UAC"; ck[n].ok = reg_get_dword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", L"EnableLUA", &v) && v == 1;
    wcopy(ck[n].detail, 160, ck[n].ok ? T2(L"Enabled", L"Aktif") : T2(L"Disabled or unknown", L"Nonaktif atau tidak diketahui")); n++;

    ck[n].name = L"Secure Boot"; ck[n].ok = reg_get_dword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\SecureBoot\\State", L"UEFISecureBootEnabled", &v) && v == 1;
    wcopy(ck[n].detail, 160, ck[n].ok ? T2(L"Enabled", L"Aktif") : T2(L"Disabled or not supported (legacy BIOS)", L"Nonaktif atau tidak didukung (BIOS legacy)")); n++;

    ck[n].name = T2(L"Remote Desktop", L"Remote Desktop");
    ck[n].ok = reg_get_dword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Terminal Server", L"fDenyTSConnections", &v) && v == 1;
    wcopy(ck[n].detail, 160, ck[n].ok ? T2(L"Disabled (good)", L"Nonaktif (bagus)") : T2(L"Enabled - make sure it is needed", L"Aktif - pastikan memang dibutuhkan")); n++;

    out_nl(o);
    for (i = 0; i < n; i++) {
        if (ck[i].ok) okc++;
        out_text(o, ck[i].ok ? OS_OK : OS_WARN, L"  ");
        out_text(o, ck[i].ok ? OS_OK : OS_WARN, U_BULLET L" ");
        { wchar_t l[80]; wfmt(l, 80, L"%-24ls", ck[i].name); out_text(o, OS_NORMAL, l); }
        out_text(o, ck[i].ok ? OS_OK : OS_WARN, ck[i].ok ? (T2(L"SAFE      ", L"AMAN      ")) : (T2(L"ATTENTION ", L"PERHATIAN ")));
        out_line(o, OS_DIM, L"%ls", ck[i].detail);
    }
    score = 100.0 * okc / n;
    bar_string(bar, 30, score);
    out_section(o, T2(L"SECURITY SCORE", L"SKOR KEAMANAN"));
    out_text(o, score >= 70 ? OS_OK : (score >= 50 ? OS_WARN : OS_ERR), L"  ");
    out_line(o, score >= 70 ? OS_OK : (score >= 50 ? OS_WARN : OS_ERR), L"[%ls] %.0f%%  (%d/%d)", bar, score, okc, n);
    out_nl(o);
    if (score >= 90) out_ok(o, T2(L"EXCELLENT - your system is well protected.", L"SANGAT BAIK - sistem kamu terlindungi dengan baik."));
    else if (score >= 70) out_ok(o, T2(L"GOOD - a few improvements possible.", L"BAIK - masih ada beberapa hal yang bisa diperbaiki."));
    else if (score >= 50) out_warn(o, T2(L"FAIR - fix the items marked ATTENTION.", L"CUKUP - perbaiki item yang bertanda PERHATIAN."));
    else out_err(o, T2(L"POOR - your system needs security attention now!", L"KURANG - sistem kamu perlu perhatian keamanan segera!"));
}
