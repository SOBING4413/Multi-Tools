#ifndef MT_SETTINGS_H
#define MT_SETTINGS_H
#include "core/common.h"

typedef struct Settings {
    int theme;            /* 0..5 */
    int animations;       /* bool */
    int auto_refresh;     /* dashboard auto refresh */
    int lang;             /* language index, -1 = auto */
} Settings;

extern Settings g_set;
void settings_load(void);
void settings_save(void);

#endif
