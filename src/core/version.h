/* version.h - single source of truth for the product version (used by C code and by the .rc file) */
#ifndef MT_VERSION_H
#define MT_VERSION_H

#define MT_VER_MAJOR 3
#define MT_VER_MINOR 0
#define MT_VER_PATCH 0
#define MT_VER_BUILD 0

#define MT_VER_STR_A   "3.0.0"
#define MT_VER_FILE_A  "3.0.0.0"
#define MT_VER_NUM     MT_VER_MAJOR,MT_VER_MINOR,MT_VER_PATCH,MT_VER_BUILD

#ifndef RC_INVOKED
#define MT_VERSION     L"3.0.0"
#endif

#endif
