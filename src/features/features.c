/* features.c - declarative feature tables for the category pages */
#include "features/feat.h"
#include "core/util.h"

/* ---------------- App launcher grid (29 apps) ---------------- */
static const GridItem k_apps[] = {
    { L"CAMERA & MEDIA", L"KAMERA & MEDIA", NULL, NULL },
    { L"Camera", L"Kamera (Camera)", L"microsoft.windows.camera:", NULL },
    { L"Voice Recorder", L"Voice Recorder", L"microsoft.windows.soundrecorder:", NULL },
    { L"Media Player", L"Media Player", L"mswindowsmusic:", NULL },
    { L"Photos", L"Photos", L"ms-photos:", NULL },
    { L"Movies & TV", L"Movies & TV", L"mswindowsvideo:", NULL },
    { L"UTILITIES", L"UTILITAS", NULL, NULL },
    { L"Calculator", L"Calculator", L"calc.exe", NULL },
    { L"Notepad", L"Notepad", L"notepad.exe", NULL },
    { L"Paint", L"Paint", L"mspaint.exe", NULL },
    { L"Snipping Tool", L"Snipping Tool", L"snippingtool.exe", L"ms-screenclip:" },
    { L"Magnifier", L"Magnifier", L"magnify.exe", NULL },
    { L"On-Screen Keyboard", L"On-Screen Keyboard", L"osk.exe", NULL },
    { L"Character Map", L"Character Map", L"charmap.exe", NULL },
    { L"Sticky Notes", L"Sticky Notes", L"ms-stickynotes:", NULL },
    { L"SYSTEM TOOLS", L"SYSTEM TOOLS", NULL, NULL },
    { L"Control Panel", L"Control Panel", L"control.exe", NULL },
    { L"Settings", L"Settings", L"ms-settings:", NULL },
    { L"Task Manager", L"Task Manager", L"taskmgr.exe", NULL },
    { L"Device Manager", L"Device Manager", L"devmgmt.msc", NULL },
    { L"Disk Management", L"Disk Management", L"diskmgmt.msc", NULL },
    { L"System Information", L"System Information", L"msinfo32.exe", NULL },
    { L"Registry Editor", L"Registry Editor", L"regedit.exe", NULL },
    { L"Resource Monitor", L"Resource Monitor", L"resmon.exe", NULL },
    { L"Performance Monitor", L"Performance Monitor", L"perfmon.exe", NULL },
    { L"Event Viewer", L"Event Viewer", L"eventvwr.msc", NULL },
    { L"INTERNET & OTHERS", L"INTERNET & LAINNYA", NULL, NULL },
    { L"Microsoft Edge", L"Microsoft Edge", L"msedge.exe", L"microsoft-edge:" },
    { L"Windows Explorer", L"Windows Explorer", L"explorer.exe", NULL },
    { L"Command Prompt", L"Command Prompt", L"cmd.exe", NULL },
    { L"PowerShell", L"PowerShell", L"powershell.exe", NULL },
    { L"Remote Desktop", L"Remote Desktop", L"mstsc.exe", NULL },
    { L"WordPad", L"WordPad", L"write.exe", NULL },
};

/* ---------------- Windows settings grid (20) ---------------- */
static const GridItem k_settings[] = {
    { L"WiFi & Internet", L"WiFi dan Internet", L"ms-settings:network-wifi", NULL },
    { L"Bluetooth", L"Bluetooth", L"ms-settings:bluetooth", NULL },
    { L"Sound", L"Sound (Suara)", L"ms-settings:sound", NULL },
    { L"Display", L"Display (Layar)", L"ms-settings:display", NULL },
    { L"Battery", L"Battery (Baterai)", L"ms-settings:batterysaver", NULL },
    { L"Storage", L"Storage (Penyimpanan)", L"ms-settings:storagesense", NULL },
    { L"Privacy", L"Privacy", L"ms-settings:privacy", NULL },
    { L"Windows Update", L"Windows Update", L"ms-settings:windowsupdate", NULL },
    { L"Personalization", L"Personalization", L"ms-settings:personalization", NULL },
    { L"Keyboard", L"Keyboard", L"ms-settings:easeofaccess-keyboard", NULL },
    { L"Mouse", L"Mouse", L"ms-settings:mousetouchpad", NULL },
    { L"Phone (link your phone)", L"Phone (Hubungkan HP)", L"ms-settings:mobile-devices", NULL },
    { L"Date & Time", L"Date dan Time", L"ms-settings:dateandtime", NULL },
    { L"Language & Region", L"Language dan Region", L"ms-settings:regionlanguage", NULL },
    { L"Email & Accounts", L"Email dan Accounts", L"ms-settings:emailandaccounts", NULL },
    { L"Sign-in Options", L"Sign-in Options", L"ms-settings:signinoptions", NULL },
    { L"Gaming", L"Gaming", L"ms-settings:gaming-gamebar", NULL },
    { L"Accessibility", L"Accessibility", L"ms-settings:easeofaccess", NULL },
    { L"Apps & Features", L"Apps dan Features", L"ms-settings:appsfeatures", NULL },
    { L"Notifications", L"Notifications", L"ms-settings:notifications", NULL },
};

/* ---------------- tables ---------------- */
static const FeatureDef k_system[] = {
    { .key = K_menu_sysinfo, .desc_en = L"OS, CPU, RAM and storage overview", .desc_id = L"Ringkasan OS, CPU, RAM, dan penyimpanan", .run = f_sysinfo },
    { .key = K_menu_hardware, .desc_en = L"Complete hardware summary with identity and network", .desc_id = L"Ringkasan hardware lengkap beserta identitas dan jaringan", .run = f_hardware },
    { .key = K_menu_gpu, .desc_en = L"Graphics adapters, driver version and NVIDIA details", .desc_id = L"Adapter grafis, versi driver, dan detail NVIDIA", .run = f_gpu },
    { .key = K_menu_disk, .desc_en = L"Used / free space on every drive", .desc_id = L"Ruang terpakai / sisa di setiap drive", .run = f_disk },
    { .key = -1, .en = L"Disk Health", .idn = L"Kesehatan Disk", .desc_en = L"S.M.A.R.T. health status of physical disks", .desc_id = L"Status kesehatan S.M.A.R.T. disk fisik", .run = f_diskhealth },
    { .key = K_menu_battery, .desc_en = L"Battery level, status and detailed battery report", .desc_id = L"Level baterai, status, dan laporan baterai detail", .run = f_battery },
    { .key = K_menu_uptime, .desc_en = L"How long the PC has been running", .desc_id = L"Sudah berapa lama PC menyala", .run = f_uptime },
    { .key = K_menu_userinfo, .desc_en = L"User folders, sizes and all local accounts", .desc_id = L"Folder user, ukuran, dan semua akun lokal", .run = f_userinfo, .flags = FF_STOPPABLE },
    { .key = -1, .en = L"Health Report", .idn = L"Laporan Kesehatan", .desc_en = L"Save a system health report (.txt) next to the program", .desc_id = L"Simpan laporan kesehatan sistem (.txt) di samping program", .run = f_healthreport },
};

static const FeatureDef k_network[] = {
    { .key = K_menu_checkip, .desc_en = L"Local and public IP address", .desc_id = L"IP address lokal dan publik", .run = f_checkip },
    { .key = K_menu_ipdetail, .desc_en = L"Location, ISP, ASN and proxy details of your public IP", .desc_id = L"Lokasi, ISP, ASN, dan detail proxy dari IP publik", .run = f_ipdetail },
    { .key = K_menu_netinfo, .desc_en = L"All adapters, gateway, DNS, MAC, traffic and saved WiFi", .desc_id = L"Semua adapter, gateway, DNS, MAC, traffic, dan WiFi tersimpan", .run = f_netinfo },
    { .key = K_menu_ping, .desc_en = L"Latency and packet loss (10 packets)", .desc_id = L"Latensi dan packet loss (10 paket)", .run = f_ping, .flags = FF_STOPPABLE,
      .in_kind = IN_TEXT, .in_label_en = L"Custom host / IP", .in_label_id = L"Host / IP kustom", .in_default = L"google.com",
      .nopt = 3, .opt_en = { L"Google DNS (8.8.8.8)", L"Cloudflare (1.1.1.1)", L"Custom host" }, .opt_id = { L"Google DNS (8.8.8.8)", L"Cloudflare (1.1.1.1)", L"Host kustom" } },
    { .key = K_menu_traceroute, .desc_en = L"Show every hop to a destination", .desc_id = L"Tampilkan setiap hop menuju tujuan", .run = f_traceroute, .flags = FF_STOPPABLE,
      .in_kind = IN_TEXT, .in_label_en = L"Target", .in_label_id = L"Target", .in_default = L"google.com" },
    { .key = K_menu_dnslookup, .desc_en = L"Resolve a domain (A, AAAA, nslookup)", .desc_id = L"Resolve domain (A, AAAA, nslookup)", .run = f_dnslookup,
      .in_kind = IN_TEXT, .in_label_en = L"Domain", .in_label_id = L"Domain", .in_default = L"google.com" },
    { .key = K_menu_flushdns, .desc_en = L"Clear the DNS resolver cache", .desc_id = L"Bersihkan DNS resolver cache", .run = f_flushdns },
    { .key = K_menu_wifi_toggle, .desc_en = L"Disable / enable the WiFi adapter (needs Administrator)", .desc_id = L"Matikan / nyalakan WiFi adapter (butuh Administrator)", .run = f_wifitoggle,
      .nopt = 3, .opt_en = { L"Turn OFF", L"Turn ON", L"Show status" }, .opt_id = { L"Matikan", L"Nyalakan", L"Lihat status" } },
    { .key = K_menu_wifi_pass, .desc_en = L"Saved WiFi networks with passwords (run as Administrator)", .desc_id = L"WiFi tersimpan beserta password (jalankan sebagai Administrator)", .run = f_wifipass },
    { .key = K_menu_speedtest, .desc_en = L"Download and upload speed", .desc_id = L"Kecepatan download dan upload", .run = f_speedtest, .flags = FF_STOPPABLE },
};

static const FeatureDef k_cleanup[] = {
    { .key = K_menu_cleantemp, .desc_en = L"Delete temp files, Windows\\Temp and Prefetch", .desc_id = L"Hapus file temp, Windows\\Temp, dan Prefetch", .run = f_cleantemp, .flags = FF_CONFIRM | FF_STOPPABLE,
      .confirm_en = L"Delete temporary files now?\nFiles in use are skipped.", .confirm_id = L"Hapus file sementara sekarang?\nFile yang sedang dipakai akan dilewati." },
    { .key = K_menu_cleanbin, .desc_en = L"Permanently empty the Recycle Bin", .desc_id = L"Kosongkan Recycle Bin secara permanen", .run = f_cleanbin, .flags = FF_CONFIRM,
      .confirm_en = L"Permanently empty the Recycle Bin?", .confirm_id = L"Kosongkan Recycle Bin secara permanen?" },
    { .key = K_menu_benchmark, .desc_en = L"Sequential write/read test with a 100 MB file", .desc_id = L"Tes tulis/baca sekuensial dengan file 100 MB", .run = f_benchmark, .flags = FF_STOPPABLE },
    { .key = -1, .en = L"Large Files Finder", .idn = L"Pencari File Besar", .desc_en = L"Find the 50 biggest files inside a folder", .desc_id = L"Cari 50 file terbesar di dalam sebuah folder", .run = f_largefiles, .flags = FF_STOPPABLE,
      .in_kind = IN_FOLDER, .in_label_en = L"Folder", .in_label_id = L"Folder", .in_default = L"%USERPROFILE%" },
};

static const FeatureDef k_security[] = {
    { .key = K_menu_secoverview, .desc_en = L"Security score from 7 checks", .desc_id = L"Skor keamanan dari 7 pengecekan", .run = f_secoverview },
    { .key = K_menu_firewall, .desc_en = L"Domain / Private / Public firewall profiles", .desc_id = L"Profil firewall Domain / Private / Public", .run = f_firewall },
    { .key = K_menu_ports, .desc_en = L"Listening ports and active connections with owning process", .desc_id = L"Port listening dan koneksi aktif beserta proses pemiliknya", .run = f_ports },
    { .key = K_menu_antivirus, .desc_en = L"Antivirus and Microsoft Defender status", .desc_id = L"Status antivirus dan Microsoft Defender", .run = f_antivirus },
    { .key = K_menu_suspicious, .desc_en = L"Outbound connections to suspicious ports / processes", .desc_id = L"Koneksi keluar ke port / proses mencurigakan", .run = f_suspicious },
    { .key = K_menu_useraccount, .desc_en = L"Local accounts, password policy and administrators", .desc_id = L"Akun lokal, kebijakan password, dan administrator", .run = f_useraccounts },
    { .key = K_menu_privacy, .desc_en = L"Clear traces: recent files, thumbnails, DNS, temp", .desc_id = L"Hapus jejak: recent files, thumbnail, DNS, temp", .run = f_privacy, .flags = FF_CONFIRM | FF_STOPPABLE,
      .confirm_en = L"Clean the selected privacy traces?", .confirm_id = L"Bersihkan jejak privasi yang dipilih?",
      .nopt = 5, .opt_en = { L"Recent files", L"Thumbnail cache", L"DNS cache", L"Temp + Prefetch", L"ALL" }, .opt_id = { L"File recent", L"Thumbnail cache", L"DNS cache", L"Temp + Prefetch", L"SEMUA" } },
};

static const FeatureDef k_tools[] = {
    { .key = K_menu_applauncher, .desc_en = L"Click an app to open it", .desc_id = L"Klik aplikasi untuk membukanya", .grid = k_apps, .ngrid = MT_ARRAY(k_apps) },
    { .key = K_menu_quicksettings, .desc_en = L"Jump straight to a Windows settings page", .desc_id = L"Langsung ke halaman pengaturan Windows", .grid = k_settings, .ngrid = MT_ARRAY(k_settings) },
    { .key = K_menu_screenshot, .desc_en = L"Capture the screen or open a snipping tool", .desc_id = L"Ambil screenshot atau buka alat snipping", .run = f_screenshot,
      .nopt = 3, .opt_en = { L"Snipping Tool", L"Full screen to Desktop", L"Snip & Sketch" }, .opt_id = { L"Snipping Tool", L"Seluruh layar ke Desktop", L"Snip & Sketch" } },
    { .key = K_menu_sysshortcuts, .desc_en = L"Volume, clipboard, product key, DxDiag, SFC, God Mode...", .desc_id = L"Volume, clipboard, product key, DxDiag, SFC, God Mode...", .run = f_sysactions,
      .in_kind = IN_TEXT, .in_label_en = L"Volume 0-100 (only for 'Set Volume')", .in_label_id = L"Volume 0-100 (hanya untuk 'Set Volume')", .in_default = L"50",
      .nopt = 11, .opt_en = { L"Mute / Unmute", L"Set Volume", L"Night Light", L"Airplane Mode", L"Display", L"Clipboard History", L"Clear Clipboard", L"Serial / Product Key", L"DxDiag", L"System File Checker", L"God Mode" },
      .opt_id = { L"Mute / Unmute", L"Set Volume", L"Night Light", L"Airplane Mode", L"Display", L"Clipboard History", L"Bersihkan Clipboard", L"Serial / Product Key", L"DxDiag", L"System File Checker", L"God Mode" },
      .flags = FF_STOPPABLE },
    { .key = -1, .en = L"Hash Calculator", .idn = L"Kalkulator Hash", .desc_en = L"SHA-256 / SHA-1 / MD5 of any file", .desc_id = L"SHA-256 / SHA-1 / MD5 dari file apa pun", .run = f_hashcalc, .flags = FF_STOPPABLE,
      .in_kind = IN_FILE, .in_label_en = L"File", .in_label_id = L"File", .in_default = L"" },
};

const FeatureDef* features_for(int fp, int* count)
{
    switch (fp) {
    case FP_SYSTEM:   *count = MT_ARRAY(k_system);   return k_system;
    case FP_NETWORK:  *count = MT_ARRAY(k_network);  return k_network;
    case FP_CLEANUP:  *count = MT_ARRAY(k_cleanup);  return k_cleanup;
    case FP_SECURITY: *count = MT_ARRAY(k_security); return k_security;
    case FP_TOOLS:    *count = MT_ARRAY(k_tools);    return k_tools;
    default: *count = 0; return NULL;
    }
}

const wchar_t* feat_name(const FeatureDef* f)
{
    if (f->key >= 0) {
        /* original menu keys carry a leading hint like "1. " in some languages? keep as-is */
        return TX((I18nKey)f->key);
    }
    return T2(f->en, f->idn);
}

const wchar_t* feat_desc(const FeatureDef* f)
{
    return T2(f->desc_en, f->desc_id);
}
