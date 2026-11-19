#include "modules/hash.h"
#include "core/util.h"
#include <bcrypt.h>

#ifndef NT_SUCCESS
#define NT_SUCCESS(s) (((NTSTATUS)(s)) >= 0)
#endif

typedef struct HAlg { BCRYPT_ALG_HANDLE alg; BCRYPT_HASH_HANDLE h; PUCHAR obj; DWORD len; } HAlg;

static BOOL halg_open(HAlg* a, LPCWSTR id)
{
    DWORD cb = 0, objlen = 0;
    ZeroMemory(a, sizeof(*a));
    if (!NT_SUCCESS(BCryptOpenAlgorithmProvider(&a->alg, id, NULL, 0))) return FALSE;
    BCryptGetProperty(a->alg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objlen, sizeof(objlen), &cb, 0);
    BCryptGetProperty(a->alg, BCRYPT_HASH_LENGTH, (PUCHAR)&a->len, sizeof(a->len), &cb, 0);
    a->obj = (PUCHAR)malloc(objlen ? objlen : 1);
    if (!a->obj || !NT_SUCCESS(BCryptCreateHash(a->alg, &a->h, a->obj, objlen, NULL, 0, 0))) {
        free(a->obj);
        BCryptCloseAlgorithmProvider(a->alg, 0);
        ZeroMemory(a, sizeof(*a));
        return FALSE;
    }
    return TRUE;
}

static void halg_close(HAlg* a)
{
    if (a->h) BCryptDestroyHash(a->h);
    if (a->alg) BCryptCloseAlgorithmProvider(a->alg, 0);
    free(a->obj);
    ZeroMemory(a, sizeof(*a));
}

static void hex_out(const BYTE* d, DWORD n, wchar_t* out)
{
    static const wchar_t* H = L"0123456789abcdef";
    DWORD i;
    for (i = 0; i < n; i++) { out[i * 2] = H[d[i] >> 4]; out[i * 2 + 1] = H[d[i] & 15]; }
    out[n * 2] = 0;
}

BOOL hash_file(const wchar_t* path, wchar_t* sha256, wchar_t* sha1, wchar_t* md5,
               volatile LONG* cancel, void (*progress)(double, void*), void* user)
{
    HAlg a256, a1, am;
    BOOL o256 = FALSE, o1 = FALSE, om = FALSE, ok = TRUE;
    HANDLE f;
    BYTE* buf;
    LARGE_INTEGER sz;
    ULONGLONG done = 0;
    DWORD got;
    BYTE dig[64];

    f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (f == INVALID_HANDLE_VALUE) return FALSE;
    if (!GetFileSizeEx(f, &sz)) sz.QuadPart = 0;
    buf = (BYTE*)malloc(1 << 20);
    if (!buf) { CloseHandle(f); return FALSE; }
    if (sha256) o256 = halg_open(&a256, BCRYPT_SHA256_ALGORITHM);
    if (sha1)   o1 = halg_open(&a1, BCRYPT_SHA1_ALGORITHM);
    if (md5)    om = halg_open(&am, BCRYPT_MD5_ALGORITHM);
    while (ReadFile(f, buf, 1 << 20, &got, NULL) && got > 0) {
        if (cancel && *cancel) { ok = FALSE; break; }
        if (o256) BCryptHashData(a256.h, buf, got, 0);
        if (o1) BCryptHashData(a1.h, buf, got, 0);
        if (om) BCryptHashData(am.h, buf, got, 0);
        done += got;
        if (progress && sz.QuadPart > 0) progress(100.0 * (double)done / (double)sz.QuadPart, user);
    }
    if (ok) {
        if (o256 && NT_SUCCESS(BCryptFinishHash(a256.h, dig, a256.len, 0))) hex_out(dig, a256.len, sha256); else if (sha256) sha256[0] = 0;
        if (o1 && NT_SUCCESS(BCryptFinishHash(a1.h, dig, a1.len, 0))) hex_out(dig, a1.len, sha1); else if (sha1) sha1[0] = 0;
        if (om && NT_SUCCESS(BCryptFinishHash(am.h, dig, am.len, 0))) hex_out(dig, am.len, md5); else if (md5) md5[0] = 0;
    }
    if (o256) halg_close(&a256);
    if (o1) halg_close(&a1);
    if (om) halg_close(&am);
    free(buf);
    CloseHandle(f);
    return ok;
}

BOOL hash_sha256_file(const wchar_t* path, wchar_t out[65])
{
    return hash_file(path, out, NULL, NULL, NULL, NULL, NULL);
}
