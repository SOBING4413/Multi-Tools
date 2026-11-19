#ifndef MT_POWER_H
#define MT_POWER_H
#include "core/common.h"

enum { PW_SHUTDOWN, PW_RESTART, PW_SLEEP, PW_HIBERNATE };

/* Schedule shutdown / restart through shutdown.exe (survives closing the app). */
BOOL power_schedule(int mode, int minutes, wchar_t* msg, size_t cch);
BOOL power_cancel(wchar_t* msg, size_t cch);
BOOL power_lock(void);
BOOL power_sleep_now(void);
BOOL power_hibernate_now(void);

#endif
