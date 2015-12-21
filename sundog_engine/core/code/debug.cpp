/*
    debug.cpp. Debug functions
    This file is part of the SunDog engine.
    Copyright (C) 2002 - 2009 Alex Zolotov <nightradio@gmail.com>
*/

#include "../core.h"
#include "../../memory/memory.h"
#include "../../utils/utils.h"
#include "../../filesystem/v3nus_fs.h"
#include "../debug.h"
#include <stdarg.h>

#ifndef PALMOS
    #include <stdio.h>
#else
    #include "palm_functions.h"
#endif

#define Y_LIMIT 50

int hide_debug_counter = 0;

void hide_debug( void )
{
    hide_debug_counter++;
}

void show_debug( void )
{
    if( hide_debug_counter > 0 )
	hide_debug_counter--;
}

long debug_count = 0;
UTF8_CHAR temp_debug_buf[ 256 ];
UTF8_CHAR *debug_buf = 0;
int debug_buf_size = 0;
int y = 10;

const UTF8_CHAR *debug_output_file = "log.txt";

void debug_set_output_file( const UTF8_CHAR *filename )
{
    debug_output_file = filename;
}

void debug_reset( void )
{
#ifdef NODEBUG
    return;
#endif

    if( debug_output_file == 0 ) return;

#ifndef PALMOS
    v3_remove( debug_output_file );
#endif
}

void sprint( UTF8_CHAR *dest_str, const UTF8_CHAR *str, ... )
{
    va_list p;
    va_start( p, str );

    int ptr = 0;
    int ptr2 = 0;
    UTF8_CHAR num_str[ 32 ];
    int len;

    //Make a string:
    for(;;)
    {
	if( str[ ptr ] == 0 ) break;
	if( str[ ptr ] == '%' )
	{
	    if( str[ ptr + 1 ] == 'd' || str[ ptr + 1 ] == 'x' )
	    {
		//Integer value:
		int arg = va_arg( p, int );
		if( str[ ptr + 1 ] == 'd' )
		    int_to_string( arg, num_str );
		else
		    hex_int_to_string( arg, num_str );
		len = mem_strlen( num_str );
		mem_copy( dest_str + ptr2, num_str, len );
		ptr2 += len;
		ptr++;
	    }
	    else
	    if( str[ ptr + 1 ] == 's' )
	    {
		//ASCII string:
		UTF8_CHAR *arg2 = va_arg( p, UTF8_CHAR* );
		if( arg2 )
		{
		    len = mem_strlen( arg2 );
		    if( len )
		    {
			mem_copy( dest_str + ptr2, arg2, len );
			ptr2 += len;
		    }
		}
		ptr++;
	    }
	}
	else
	{
	    dest_str[ ptr2 ] = str[ ptr ];
	    ptr2++;
	}
	ptr++;
    }
    dest_str[ ptr2 ] = 0;
    va_end( p );
}

void dprint( const UTF8_CHAR *str, ... )
{
#ifdef NODEBUG
    return;
#endif

    if( hide_debug_counter ) return;

    if( debug_output_file == 0 ) return;

    va_list p;
    va_start( p, str );
    if( debug_buf_size == 0 )
    {
	debug_buf = temp_debug_buf;
	debug_buf_size = 256;
	debug_buf = (UTF8_CHAR*)MEM_NEW( HEAP_DYNAMIC, 256 );
    }
    int ptr = 0;
    int ptr2 = 0;
    UTF8_CHAR num_str[ 32 ];
    int len;

    //Make a number:
#ifdef PALMOS
    int_to_string( debug_count, num_str );
    len = mem_strlen( num_str );
    mem_copy( debug_buf, num_str, len );
    debug_buf[ len ] = ':';
    debug_buf[ len + 1 ] = ' ';
    ptr2 += len + 2;
#endif
    debug_count++;

    //Make a string:
    for(;;)
    {
	if( str[ ptr ] == 0 ) break;
	if( str[ ptr ] == '%' )
	{
	    if( str[ ptr + 1 ] == 'd' || str[ ptr + 1 ] == 'x' )
	    {
		int arg = va_arg( p, int );
		if( str[ ptr + 1 ] == 'd' )
		    int_to_string( arg, num_str );
		else
		    hex_int_to_string( arg, num_str );
		len = mem_strlen( num_str );
		if( ptr2 + len >= debug_buf_size )
		{
		    //Resize debug buffer:
		    debug_buf_size += 256;
		    debug_buf = (UTF8_CHAR*)mem_resize( debug_buf, debug_buf_size );
		}
		mem_copy( debug_buf + ptr2, num_str, len );
		ptr2 += len;
		ptr++;
	    }
	    else
	    if( str[ ptr + 1 ] == 's' )
	    {
		//ASCII string:
		UTF8_CHAR *arg2 = va_arg( p, UTF8_CHAR* );
		if( arg2 )
		{
		    len = mem_strlen( arg2 );
		    if( len )
		    {
			if( ptr2 + len >= debug_buf_size )
			{
			    //Resize debug buffer:
			    debug_buf_size += 256;
			    debug_buf = (UTF8_CHAR*)mem_resize( debug_buf, debug_buf_size );
			}
			mem_copy( debug_buf + ptr2, arg2, len );
			ptr2 += len;
		    }
		}
		ptr++;
	    }
	}
	else
	{
	    debug_buf[ ptr2 ] = str[ ptr ];
	    ptr2++;
	    if( ptr2 >= debug_buf_size )
	    {
		//Resize debug buffer:
		debug_buf_size += 256;
		debug_buf = (UTF8_CHAR*)mem_resize( debug_buf, debug_buf_size );
	    }
	}
	ptr++;
    }
    debug_buf[ ptr2 ] = 0;
    va_end( p );

    //Save result:
#ifdef NONPALM
    V3_FILE f = v3_open( debug_output_file, "ab" );
    if( f )
    {
	v3_write( debug_buf, 1, mem_strlen( debug_buf ), f );
	v3_close( f );
    }
    printf( "%s", debug_buf );
#else
    //PalmOS:
    int a;
    for( a = 0; a < 128; a++ )
    {
	if( debug_buf[ a ] == 0 ) break;
	if( debug_buf[ a ] == 0xA ) break;
	if( debug_buf[ a ] == 0xD ) break;
    }
    WinDrawChars( "                                                                                ", 80, 0, y );
    WinDrawChars( debug_buf, a, 0, y );
    y += 10;
    if( y >= Y_LIMIT ) y = 0;
#endif
}

void debug_close( void )
{
#ifdef NODEBUG
    return;
#endif

    if( debug_buf && debug_buf != temp_debug_buf ) 
    {
	mem_free( debug_buf );
	debug_buf = temp_debug_buf;
	debug_buf_size = 256;
    }
}
