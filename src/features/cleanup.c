/* f_cleanup.c - temp cleaner, recycle bin, disk benchmark, large files finder */
#include "features/feat.h"
#include "core/util.h"
#include "modules/sys.h"
#include <shlobj.h>
#include <shellapi.h>

/* ---------- recursive delete of directory contents ---------- */
void del_contents(const wchar_t* dir, DelStats* st, volatile LONG* cancel)
{
    wchar_t pat[MAX_PATH * 2], full[MAX_PATH * 2];
    WIN32_FIND_DATAW fd;
    HANDLE h;
    wfmt(pat, MT_ARRAY(pat), L"%ls\\*", dir);
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (cancel && *cancel) break;
        if (fd.cFileName[0] == L'.' && (fd.cFileName[1] == 0 || (fd.cFileName[1] == L'.' && fd.cFileName[2] == 0))) continue;
        wfmt(full, MT_ARRAY(full), L"%ls\\%ls", dir, fd.cFileName);
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && !(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
            del_contents(full, st, cancel);
            if (RemoveDirectoryW(full)) st->dirs++;
        } else if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (RemoveDirectoryW(full)) st->dirs++; else st->failed++;     /* junction / symlink: remove link only */
        } else {
            ULONGLONG sz = ((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_READONLY) SetFileAttributesW(full, FILE_ATTRIBUTE_NORMAL);
            if (DeleteFileW(full)) { st->files++; st->bytes += sz; } else st->failed++;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

void f_cleantemp(MtOut* o, const FeatArgs* a)
{
    wchar_t dirs[3][MAX_PATH], win[MAX_PATH], t[MAX_PATH];
    int i;
    ULONGLONG tf = 0, tb = 0, tfail = 0;
    wchar_t sz[64];
    (void)a;
    out_title(o, T2(L"CLEAN TEMPORARY FILES", L"BERSIHKAN FILE SEMENTARA"));
    GetTempPathW(MAX_PATH, t);
    { size_t n = wcslen(t); if (n > 3 && t[n - 1] == L'\\') t[n - 1] = 0; }
    wcopy(dirs[0], MAX_PATH, t);
    GetWindowsDirectoryW(win, MAX_PATH);
    path_join(dirs[1], MAX_PATH, win, L"Temp");
    path_join(dirs[2], MAX_PATH, win, L"Prefetch");
    for (i = 0; i < 3 && !OUT_CANCELLED(o); i++) {
        DelStats st;
        ZeroMemory(&st, sizeof(st));
        out_nl(o);
        out_info(o, T2(L"Cleaning: %ls", L"Membersihkan: %ls"), dirs[i]);
        if (!dir_exists(dirs[i])) { out_line(o, OS_DIM, T2(L"    (folder not found)", L"    (folder tidak ditemukan)")); continue; }
        del_contents(dirs[i], &st, o->cancel);
        fmt_size(st.bytes, sz, 64);
        out_line(o, OS_OK, T2(L"    Deleted %I64u files, %I64u folders (%ls)", L"    Dihapus %I64u file, %I64u folder (%ls)"), st.files, st.dirs, sz);
        if (st.failed) out_line(o, OS_WARN, T2(L"    %I64u item(s) skipped (in use / protected)", L"    %I64u item dilewati (sedang dipakai / dilindungi)"), st.failed);
        tf += st.files; tb += st.bytes; tfail += st.failed;
        out_progress(o, (i + 1) * 100.0 / 3.0);
    }
    fmt_size(tb, sz, 64);
    out_section(o, T2(L"RESULT", L"HASIL"));
    out_kv_style(o, T2(L"Files deleted", L"File dihapus"), OS_OK, L"%I64u", tf);
    out_kv_style(o, T2(L"Space freed", L"Ruang dibebaskan"), OS_OK, L"%ls", sz);
    out_kv(o, T2(L"Skipped", L"Dilewati"), L"%I64u", tfail);
    if (!g_admin) out_info(o, T2(L"Tip: run as Administrator to also clean Windows\\Temp and Prefetch.", L"Tips: jalankan sebagai Administrator untuk membersihkan Windows\\Temp dan Prefetch."));
}

/* ---------- recycle bin ---------- */
void f_cleanbin(MtOut* o, const FeatArgs* a)
{
    SHQUERYRBINFO qi;
    HRESULT hr;
    wchar_t sz[64];
    (void)a;
    out_title(o, T2(L"EMPTY RECYCLE BIN", L"KOSONGKAN RECYCLE BIN"));
    ZeroMemory(&qi, sizeof(qi));
    qi.cbSize = sizeof(qi);
    if (SUCCEEDED(SHQueryRecycleBinW(NULL, &qi))) {
        fmt_size((ULONGLONG)qi.i64Size, sz, 64);
        out_kv(o, T2(L"Items", L"Jumlah item"), L"%I64d", (long long)qi.i64NumItems);
        out_kv(o, T2(L"Size", L"Ukuran"), L"%ls", sz);
        if (qi.i64NumItems == 0) { out_nl(o); out_ok(o, T2(L"Recycle Bin is already empty.", L"Recycle Bin sudah kosong.")); return; }
    }
    out_nl(o);
    hr = SHEmptyRecycleBinW(NULL, NULL, SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);
    if (SUCCEEDED(hr)) out_ok(o, T2(L"Recycle Bin emptied successfully!", L"Recycle Bin berhasil dikosongkan!"));
    else out_err(o, T2(L"Failed to empty Recycle Bin (0x%08lX).", L"Gagal mengosongkan Recycle Bin (0x%08lX)."), (unsigned long)hr);
}

/* ---------- disk benchmark ---------- */
static const wchar_t* bench_rating(double mbs)
{
    if (mbs > 500) return T2(L"VERY FAST (NVMe SSD)", L"SANGAT CEPAT (NVMe SSD)");
    if (mbs > 150) return T2(L"FAST (SSD)", L"CEPAT (SSD)");
    if (mbs > 80)  return T2(L"NORMAL (HDD)", L"NORMAL (HDD)");
    return T2(L"SLOW", L"LAMBAT");
}

void f_benchmark(MtOut* o, const FeatArgs* a)
{
    enum { CH = 4 * 1024 * 1024, N = 25 };           /* 25 x 4 MB = 100 MB */
    wchar_t tmp[MAX_PATH], path[MAX_PATH * 2];
    BYTE* buf;
    HANDLE h;
    double t0, wr_mbs = 0, rd_mbs = 0;
    int i;
    uint32_t x = 2463534242u;
    DWORD io, k;
    ULARGE_INTEGER fa, tot, fr;
    (void)a;
    out_title(o, T2(L"DISK SPEED TEST (BENCHMARK)", L"TES KECEPATAN DISK (BENCHMARK)"));
    GetTempPathW(MAX_PATH, tmp);
    path_join(path, MAX_PATH * 2, tmp, L"multitool_bench.tmp");
    if (GetDiskFreeSpaceExW(tmp, &fa, &tot, &fr) && fa.QuadPart < 300ULL * 1024 * 1024) {
        out_err(o, T2(L"Not enough free space on the temp drive (need 300 MB).", L"Ruang kosong di drive temp tidak cukup (butuh 300 MB)."));
        return;
    }
    buf = (BYTE*)VirtualAlloc(NULL, CH, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!buf) { out_err(o, T2(L"Out of memory.", L"Memori tidak cukup.")); return; }
    for (k = 0; k < CH / 4; k++) { x ^= x << 13; x ^= x >> 17; x ^= x << 5; ((uint32_t*)buf)[k] = x; }

    out_info(o, T2(L"Test file: 100 MB in %ls", L"File test: 100 MB di %ls"), tmp);
    out_section(o, T2(L"WRITE TEST", L"TES TULIS (WRITE)"));
    h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);
    if (h == INVALID_HANDLE_VALUE) { out_err(o, T2(L"Cannot create the test file.", L"Tidak dapat membuat file test.")); VirtualFree(buf, 0, MEM_RELEASE); return; }
    t0 = qpc_seconds();
    for (i = 0; i < N && !OUT_CANCELLED(o); i++) {
        if (!WriteFile(h, buf, CH, &io, NULL) || io != CH) { out_err(o, T2(L"Write failed.", L"Penulisan gagal.")); break; }
        out_progress(o, i * 50.0 / N);
    }
    FlushFileBuffers(h);
    t0 = qpc_seconds() - t0;
    CloseHandle(h);
    if (t0 < 0.001) t0 = 0.001;
    wr_mbs = 100.0 / t0;
    out_kv(o, T2(L"Time", L"Waktu"), T2(L"%.2f seconds", L"%.2f detik"), t0);
    out_kv_style(o, T2(L"Write speed", L"Kecepatan tulis"), OS_OK, L"%.1f MB/s", wr_mbs);

    out_section(o, T2(L"READ TEST (uncached)", L"TES BACA (tanpa cache)"));
    h = CreateFileW(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (h == INVALID_HANDLE_VALUE)
        h = CreateFileW(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        t0 = qpc_seconds();
        for (i = 0; i < N && !OUT_CANCELLED(o); i++) {
            if (!ReadFile(h, buf, CH, &io, NULL) || io == 0) break;
            out_progress(o, 50.0 + i * 50.0 / N);
        }
        t0 = qpc_seconds() - t0;
        CloseHandle(h);
        if (t0 < 0.001) t0 = 0.001;
        rd_mbs = 100.0 / t0;
        out_kv(o, T2(L"Time", L"Waktu"), T2(L"%.2f seconds", L"%.2f detik"), t0);
        out_kv_style(o, T2(L"Read speed", L"Kecepatan baca"), OS_OK, L"%.1f MB/s", rd_mbs);
    } else out_err(o, T2(L"Cannot open the test file for reading.", L"Tidak dapat membuka file test untuk dibaca."));
    DeleteFileW(path);
    VirtualFree(buf, 0, MEM_RELEASE);
    out_progress(o, -1);

    out_section(o, T2(L"RESULT", L"HASIL"));
    out_kv(o, T2(L"Write", L"Tulis"), L"%.1f MB/s  - %ls", wr_mbs, bench_rating(wr_mbs));
    out_kv(o, T2(L"Read", L"Baca"), L"%.1f MB/s  - %ls", rd_mbs, bench_rating(rd_mbs));
    out_nl(o);
    out_ok(o, T2(L"Test file deleted.", L"File test sudah dihapus."));
}

/* ---------- large files finder ---------- */
typedef struct LargeFile { ULONGLONG size; FILETIME mt; wchar_t path[MAX_PATH * 2]; } LargeFile;
typedef struct LfCtx { LargeFile top[50]; int n; ULONGLONG scanned, bytes; volatile LONG* cancel; } LfCtx;

static void lf_add(LfCtx* c, const wchar_t* path, ULONGLONG size, const FILETIME* mt)
{
    int pos, i;
    if (c->n == 50 && size <= c->top[49].size) return;
    pos = c->n < 50 ? c->n : 49;
    while (pos > 0 && c->top[pos - 1].size < size) { c->top[pos] = c->top[pos - 1]; pos--; }
    c->top[pos].size = size;
    c->top[pos].mt = *mt;
    wcopy(c->top[pos].path, MAX_PATH * 2, path);
    if (c->n < 50) c->n++;
    (void)i;
}

static void lf_scan(LfCtx* c, const wchar_t* dir, int depth)
{
    wchar_t pat[MAX_PATH * 2], full[MAX_PATH * 2];
    WIN32_FIND_DATAW fd;
    HANDLE h;
    if (depth > 40) return;
    wfmt(pat, MT_ARRAY(pat), L"%ls\\*", dir);
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (c->cancel && *c->cancel) break;
        if (fd.cFileName[0] == L'.' && (fd.cFileName[1] == 0 || (fd.cFileName[1] == L'.' && fd.cFileName[2] == 0))) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
        wfmt(full, MT_ARRAY(full), L"%ls\\%ls", dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) lf_scan(c, full, depth + 1);
        else {
            ULONGLONG sz = ((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
            c->scanned++;
            c->bytes += sz;
            lf_add(c, full, sz, &fd.ftLastWriteTime);
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

void f_largefiles(MtOut* o, const FeatArgs* a)
{
    LfCtx* c;
    wchar_t root[MAX_PATH * 2], s[64], when[32];
    int i;
    out_title(o, T2(L"LARGE FILES FINDER (TOP 50)", L"PENCARI FILE BESAR (TOP 50)"));
    wcopy(root, MAX_PATH * 2, a->text);
    wtrim(root);
    { size_t n = wcslen(root); while (n > 3 && root[n - 1] == L'\\') root[--n] = 0; }
    if (!dir_exists(root)) { out_err(o, T2(L"Folder not found: %ls", L"Folder tidak ditemukan: %ls"), root); return; }
    c = (LfCtx*)calloc(1, sizeof(LfCtx));
    if (!c) return;
    c->cancel = o->cancel;
    out_info(o, T2(L"Scanning %ls (press Stop to cancel)...", L"Memindai %ls (tekan Stop untuk membatalkan)..."), root);
    lf_scan(c, root, 0);
    fmt_size(c->bytes, s, 64);
    out_nl(o);
    out_kv(o, T2(L"Files scanned", L"File dipindai"), L"%I64u", c->scanned);
    out_kv(o, T2(L"Total size", L"Ukuran total"), L"%ls", s);
    if (OUT_CANCELLED(o)) out_warn(o, T2(L"Scan cancelled - partial results.", L"Pemindaian dibatalkan - hasil sebagian."));
    out_nl(o);
    out_line(o, OS_ACCENT, L"  %3ls  %12ls  %-11ls  %ls", L"#", T2(L"Size", L"Ukuran"), T2(L"Modified", L"Diubah"), L"Path");
    out_sep(o);
    for (i = 0; i < c->n; i++) {
        fmt_size(c->top[i].size, s, 64);
        fmt_filetime(&c->top[i].mt, when, 32, FALSE);
        out_line(o, c->top[i].size > 1024ULL * 1024 * 1024 ? OS_WARN : OS_NORMAL, L"  %3d  %12ls  %-11ls  %ls", i + 1, s, when, c->top[i].path);
    }
    if (c->n == 0) out_info(o, T2(L"No files found.", L"Tidak ada file ditemukan."));
    free(c);
}
