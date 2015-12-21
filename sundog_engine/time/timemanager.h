#ifndef __TIMEMANAGER__
#define __TIMEMANAGER__

#include "../core/core.h"

typedef unsigned long ticks_t;

ulong time_hours( void );
ulong time_minutes( void );
ulong time_seconds( void );
ticks_t time_ticks_per_second( void );
ticks_t time_ticks( void );

#if defined(LINUX) || defined(WIN) || defined(WINCE)
#define HIRES_TIMER
ticks_t time_ticks_per_second_hires( void );
ticks_t time_ticks_hires( void );
#endif

#endif
