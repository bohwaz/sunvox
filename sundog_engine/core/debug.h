#ifndef __DEBUG__
#define __DEBUG__

#include "core.h"

void hide_debug( void );
void show_debug( void );

void debug_set_output_file( const UTF8_CHAR *filename );
void debug_reset( void );
void sprint( UTF8_CHAR *dest_str, const UTF8_CHAR *str, ... );
void dprint( const UTF8_CHAR *str, ... );
void debug_close( void );

#endif
