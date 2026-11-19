/* f_tools.c - launchers, screenshot, quick actions (volume via Core Audio), hash calculator */
#include "features/feat.h"
#include "core/util.h"
#include "modules/hash.h"
#include <shlobj.h>
#include <shellapi.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>

/* ---------- Core Audio (C vtable) ---------- */
static const GUID MT_CLSID_MMDeviceEnumerator = { 0xBCDE0395, 0xE52F, 0x467C, { 0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E } };
static const GUID MT_IID_IMMDeviceEnumerator  = { 0xA95664D2, 0x9614, 0x4F35, { 0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6 } };
static const GUID MT_IID_IAudioEndpointVolume = { 0x5CDF2C82, 0x841E, 0x4546, { 0x97, 0x22, 0x0C, 0xF7, 0x40, 0x78, 0x22, 0x9A } };

/* mode 0: toggle mute (returns new mute state in *res), mode 1: set volume 0..100 */
static BOOL audio_op(int mode, int level, BOOL* res)
{
    IMMDeviceEnumerator* en = NULL;
    IMMDevice* dev = NULL;
    IAudioEndpointVolume* vol = NULL;
    BOOL ok = FALSE;
    if (FAILED(CoCreateInstance(&MT_CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &MT_IID_IMMDeviceEnumerator, (void**)&en)) || !en) return FALSE;
    if (SUCCEEDED(en->lpVtbl->GetDefaultAudioEndpoint(en, eRender, eConsole, &dev)) && dev) {
        if (SUCCEEDED(dev->lpVtbl->Activate(dev, &MT_IID_IAudioEndpointVolume, CLSCTX_ALL, NULL, (void**)&vol)) && vol) {
            if (mode == 0) {
                BOOL m = FALSE;
                if (SUCCEEDED(vol->lpVtbl->GetMute(vol, &m)) && SUCCEEDED(vol->lpVtbl->SetMute(vol, !m, NULL))) { ok = TRUE; if (res) *res = !m; }
            } else {
                if (level < 0) level = 0;
                if (level > 100) level = 100;
                if (SUCCEEDED(vol->lpVtbl->SetMasterVolumeLevelScalar(vol, (float)level / 100.0f, NULL))) {
                    vol->lpVtbl->SetMute(vol, FALSE, NULL);
                    ok = TRUE;
                }
            }
            vol->lpVtbl->Release(vol);
        }
        dev->lpVtbl->Release(dev);
    }
    en->lpVtbl->Release(en);
    return ok;
}

/* ---------- Screenshot ---------- */
void f_screenshot(MtOut* o, const FeatArgs* a)
{
    out_title(o, T2(L"TAKE SCREENSHOT", L"AMBIL SCREENSHOT"));
    if (a->opt == 0) {
        out_info(o, T2(L"Opening Snipping Tool...", L"Membuka Snipping Tool..."));
        if ((INT_PTR)ShellExecuteW(NULL, L"open", L"snippingtool.exe", NULL, NULL, SW_SHOWNORMAL) <= 32)
            ShellExecuteW(NULL, L"open", L"ms-screenclip:", NULL, NULL, SW_SHOWNORMAL);
        out_ok(o, T2(L"Snipping Tool opened!", L"Snipping Tool dibuka!"));
    } else if (a->opt == 2) {
        out_info(o, T2(L"Opening Snip & Sketch...", L"Membuka Snip & Sketch..."));
        ShellExecuteW(NULL, L"open", L"ms-screenclip:", NULL, NULL, SW_SHOWNORMAL);
        out_ok(o, T2(L"Snip & Sketch opened (also Win+Shift+S).", L"Snip & Sketch dibuka (juga Win+Shift+S)."));
    } else {
        wchar_t desk[MAX_PATH], file[MAX_PATH * 2], script[2048], esc[MAX_PATH * 2];
        SYSTEMTIME st;
        size_t i, k = 0;
        int rc;
        out_info(o, T2(L"Capturing the full screen...", L"Mengambil screenshot seluruh layar..."));
        get_folder(CSIDL_DESKTOPDIRECTORY, desk);
        GetLocalTime(&st);
        wfmt(script, 64, L"screenshot_%04d%02d%02d_%02d%02d%02d.png", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        path_join(file, MAX_PATH * 2, desk, script);
        for (i = 0; file[i] && k < MAX_PATH * 2 - 2; i++) { if (file[i] == L'\'') esc[k++] = L'\''; esc[k++] = file[i]; }
        esc[k] = 0;
        wfmt(script, MT_ARRAY(script),
             L"Add-Type -AssemblyName System.Windows.Forms; Add-Type -AssemblyName System.Drawing;"
             L"$b=[System.Windows.Forms.SystemInformation]::VirtualScreen;"
             L"$bmp=New-Object System.Drawing.Bitmap($b.Width,$b.Height);"
             L"$g=[System.Drawing.Graphics]::FromImage($bmp);"
             L"$g.CopyFromScreen($b.Location,[System.Drawing.Point]::Empty,$b.Size);"
             L"$bmp.Save('%ls',[System.Drawing.Imaging.ImageFormat]::Png);$g.Dispose();$bmp.Dispose()", esc);
        run_ps_text(script, 30000, o->cancel, &rc);
        if (file_exists(file)) {
            out_ok(o, T2(L"Screenshot saved:", L"Screenshot disimpan:"));
            out_line(o, OS_ACCENT, L"    %ls", file);
        } else out_err(o, T2(L"Failed to capture the screenshot.", L"Gagal mengambil screenshot."));
    }
}

/* ---------- Quick system actions ---------- */
static void ln_plain(const wchar_t* l, void* u) { if (l[0]) out_line((MtOut*)u, OS_NORMAL, L"  %ls", l); }

void f_sysactions(MtOut* o, const FeatArgs* a)
{
    out_title(o, T2(L"QUICK SYSTEM ACTIONS", L"AKSI SISTEM CEPAT"));
    switch (a->opt) {
    case 0: {
        BOOL muted = FALSE;
        if (audio_op(0, 0, &muted)) out_ok(o, muted ? T2(L"Volume muted.", L"Volume di-mute.") : T2(L"Volume unmuted.", L"Volume di-unmute."));
        else { keybd_event(VK_VOLUME_MUTE, 0, 0, 0); keybd_event(VK_VOLUME_MUTE, 0, KEYEVENTF_KEYUP, 0); out_ok(o, T2(L"Mute key sent.", L"Tombol mute dikirim.")); }
        break; }
    case 1: {
        wchar_t t[32];
        wchar_t* e;
        long lv;
        wcopy(t, 32, a->text);
        wtrim(t);
        lv = wcstol(t, &e, 10);
        if (!t[0] || *e || lv < 0 || lv > 100) { out_err(o, T2(L"Enter a volume level between 0 and 100.", L"Masukkan level volume antara 0-100!")); break; }
        if (audio_op(1, (int)lv, NULL)) out_ok(o, T2(L"Volume set to %ld%%.", L"Volume diatur ke %ld%%."), lv);
        else out_err(o, T2(L"Could not access the audio device.", L"Tidak dapat mengakses perangkat audio."));
        break; }
    case 2: ShellExecuteW(NULL, L"open", L"ms-settings:nightlight", NULL, NULL, SW_SHOWNORMAL); out_ok(o, T2(L"Night Light settings opened!", L"Pengaturan Night Light dibuka!")); break;
    case 3: ShellExecuteW(NULL, L"open", L"ms-settings:network-airplanemode", NULL, NULL, SW_SHOWNORMAL); out_ok(o, T2(L"Airplane Mode settings opened!", L"Pengaturan Airplane Mode dibuka!")); break;
    case 4: ShellExecuteW(NULL, L"open", L"ms-settings:display", NULL, NULL, SW_SHOWNORMAL); out_ok(o, T2(L"Display settings opened!", L"Pengaturan Display dibuka!")); break;
    case 5: out_info(o, T2(L"Tip: press Win+V to open Clipboard History.", L"Tips: tekan Win+V untuk membuka Clipboard History."));
            ShellExecuteW(NULL, L"open", L"ms-settings:clipboard", NULL, NULL, SW_SHOWNORMAL); out_ok(o, T2(L"Clipboard settings opened!", L"Pengaturan Clipboard dibuka!")); break;
    case 6:
        if (OpenClipboard(g_hwnd)) { EmptyClipboard(); CloseClipboard(); out_ok(o, T2(L"Clipboard cleared!", L"Clipboard berhasil dibersihkan!")); }
        else out_err(o, T2(L"Clipboard is busy.", L"Clipboard sedang dipakai."));
        break;
    case 7: {
        wchar_t t[256];
        out_section(o, T2(L"SERIAL NUMBER (BIOS)", L"SERIAL NUMBER (BIOS)"));
        run_ps(L"Write-Output ((Get-CimInstance Win32_BIOS).SerialNumber)", 20000, ln_plain, o, o->cancel, NULL);
        out_section(o, T2(L"WINDOWS PRODUCT KEY", L"PRODUCT KEY WINDOWS"));
        { wchar_t* r = run_ps_text(L"Write-Output ((Get-CimInstance SoftwareLicensingService).OA3xOriginalProductKey)", 20000, o->cancel, NULL);
          wtrim(r);
          if (r[0]) out_kv(o, L"Product Key", L"%ls", r); else out_info(o, T2(L"Product key not available (probably a digital license).", L"Product Key tidak tersedia (mungkin digital license)."));
          free(r); }
        out_section(o, T2(L"COMPUTER MODEL", L"MODEL KOMPUTER"));
        reg_get_str(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"SystemManufacturer", t, 256);
        if (t[0]) out_kv(o, L"Manufacturer", L"%ls", t);
        reg_get_str(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"SystemProductName", t, 256);
        if (t[0]) out_kv(o, L"Model", L"%ls", t);
        break; }
    case 8: ShellExecuteW(NULL, L"open", L"dxdiag.exe", NULL, NULL, SW_SHOWNORMAL); out_ok(o, T2(L"DxDiag opened!", L"DxDiag dibuka!")); break;
    case 9:
        out_info(o, T2(L"Starting System File Checker in a console window...", L"Menjalankan System File Checker di jendela konsol..."));
        if (!g_admin) out_warn(o, T2(L"SFC needs Administrator rights; restart MultiTool as Administrator.", L"SFC memerlukan hak Administrator; jalankan ulang MultiTool sebagai Administrator."));
        ShellExecuteW(NULL, L"open", L"cmd.exe", L"/k sfc /scannow", NULL, SW_SHOWNORMAL);
        out_ok(o, T2(L"SFC console opened. Wait for it to finish.", L"Konsol SFC dibuka. Tunggu hingga selesai."));
        break;
    case 10: {
        wchar_t desk[MAX_PATH], dir[MAX_PATH * 2];
        get_folder(CSIDL_DESKTOPDIRECTORY, desk);
        path_join(dir, MAX_PATH * 2, desk, L"GodMode.{ED7BA470-8E54-465E-825C-99712043E01C}");
        if (!dir_exists(dir)) CreateDirectoryW(dir, NULL);
        if (dir_exists(dir)) {
            out_ok(o, T2(L"God Mode folder is ready on your Desktop.", L"Folder God Mode siap di Desktop."));
            ShellExecuteW(NULL, L"open", L"explorer.exe", dir, NULL, SW_SHOWNORMAL);
        } else out_err(o, T2(L"Failed to create the God Mode folder.", L"Gagal membuat folder God Mode."));
        break; }
    default: break;
    }
}

/* ---------- Hash calculator ---------- */
static void hash_prog(double pct, void* u) { out_progress((MtOut*)u, pct); }

void f_hashcalc(MtOut* o, const FeatArgs* a)
{
    wchar_t path[MAX_PATH * 2], s256[65], s1[41], md5[33], sz[64];
    WIN32_FILE_ATTRIBUTE_DATA fa;
    out_title(o, T2(L"FILE HASH CALCULATOR", L"KALKULATOR HASH FILE"));
    wcopy(path, MAX_PATH * 2, a->text);
    wtrim(path);
    if (path[0] == L'"') { size_t n = wcslen(path); memmove(path, path + 1, n * sizeof(wchar_t)); n = wcslen(path); if (n && path[n - 1] == L'"') path[n - 1] = 0; }
    if (!file_exists(path)) { out_err(o, T2(L"File not found. Pick a file with Browse.", L"File tidak ditemukan. Pilih file dengan Browse.")); return; }
    out_info(o, T2(L"Computing hashes...", L"Menghitung hash..."));
    if (!hash_file(path, s256, s1, md5, o->cancel, hash_prog, o)) {
        out_progress(o, -1);
        out_err(o, OUT_CANCELLED(o) ? T2(L"Cancelled.", L"Dibatalkan.") : T2(L"Could not read the file (in use or access denied).", L"Tidak dapat membaca file (sedang dipakai atau akses ditolak)."));
        return;
    }
    out_progress(o, -1);
    out_nl(o);
    out_kv(o, T2(L"File", L"File"), L"%ls", path);
    if (GetFileAttributesExW(path, GetFileExInfoStandard, &fa)) {
        fmt_size(((ULONGLONG)fa.nFileSizeHigh << 32) | fa.nFileSizeLow, sz, 64);
        out_kv(o, T2(L"Size", L"Ukuran"), L"%ls", sz);
    }
    out_nl(o);
    out_kv_style(o, L"SHA-256", OS_ACCENT, L"%ls", s256);
    out_kv(o, L"SHA-1", L"%ls", s1);
    out_kv(o, L"MD5", L"%ls", md5);
    out_nl(o);
    out_info(o, T2(L"Paste the SHA-256 on virustotal.com to check a file reputation.", L"Tempel SHA-256 di virustotal.com untuk mengecek reputasi file."));
}
