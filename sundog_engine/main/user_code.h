#ifndef __SUNUSER__
#define __SUNUSER__

#include "core/core.h"
#include "core/debug.h"
#include "sound/sound.h"
#include "window_manager/wmanager.h"
#include "time/timemanager.h"
#include "utils/utils.h"

void user_init( window_manager *wm );
int user_event_handler( sundog_event *evt, window_manager *wm );
void user_close( window_manager *wm );

#endif
