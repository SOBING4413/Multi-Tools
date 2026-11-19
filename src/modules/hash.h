#ifndef MT_HASH_H
#define MT_HASH_H
#include "core/common.h"

/* Hex outputs: sha256[65], sha1[41], md5[33]. Any pointer may be NULL. Returns FALSE on I/O error / cancel. */
BOOL hash_file(const wchar_t* path, wchar_t* sha256, wchar_t* sha1, wchar_t* md5,
               volatile LONG* cancel, void (*progress)(double pct, void* user), void* user);
BOOL hash_sha256_file(const wchar_t* path, wchar_t out[65]);
#endif
