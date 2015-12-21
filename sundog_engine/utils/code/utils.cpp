/*
    utils.cpp. Various functions: string list; random generator; ...
    This file is part of the SunDog engine.
    Copyright (C) 2002 - 2009 Alex Zolotov <nightradio@gmail.com>
*/

#include "../../core/core.h"
#include "../../core/debug.h"
#include "../../memory/memory.h"
#include "../../filesystem/v3nus_fs.h"
#include "../utils.h"

//WORKING WITH A STRING LISTS:

void list_init( list_data *data )
{
    data->items = (UTF8_CHAR*)mem_new( HEAP_STORAGE, MAX_SIZE, "list items", 0 );
    data->items_ptr = (long*)mem_new( HEAP_STORAGE, MAX_ITEMS * 4, "list items ptrs", 0 );
    list_clear( data );
    list_reset_selection( data );
}

void list_close( list_data *data )
{
    if( data->items )
    {
	mem_free( data->items );
	data->items = 0;
    }
    if( data->items_ptr )
    {
	mem_free( data->items_ptr );
	data->items_ptr = 0;
    }
}

void list_clear( list_data *data )
{
    data->items_num = 0;
    data->start_item = 0;
}

void list_reset_selection( list_data *data )
{
    data->selected_item = -1;
}

void list_add_item( const UTF8_CHAR *item, char attr, list_data *data )
{
    long ptr, p;

    mem_off();
    
    if( data->items_num == 0 ) 
    {
	data->items_ptr[ 0 ] = 0;
	ptr = 0;
    }
    else 
    {
	ptr = data->items_ptr[ data->items_num ];
    }
    
    data->items[ ptr++ ] = attr | 128;
    for( p = 0; ; p++, ptr++ )
    {
	data->items[ ptr ] = item[ p ];
	if( item[ p ] == 0 ) break;
    }
    ptr++;
    data->items_num++;
    data->items_ptr[ data->items_num ] = ptr;
    
    mem_on();
}

void list_delete_item( long item_num, list_data *data )
{
    long ptr, p;

    if( item_num < data->items_num && item_num >= 0 )
    {
	mem_off();
	
	ptr = data->items_ptr[ item_num ]; //Offset of our item (in the "items")
	
	//Get item size (in chars):
	long size;
	for( size = 0; ; size++ )
	{
	    if( data->items[ ptr + size ] == 0 ) break;
	}
	
	//Delete it:
	for( p = ptr; p < MAX_SIZE - size - 1 ; p++ )
	{
	    data->items[ p ] = data->items[ p + size + 1 ];
	}
	for( p = 0; p < data->items_num; p++ )
	{
	    if( data->items_ptr[ p ] > ptr ) data->items_ptr[ p ] -= size + 1;
	}
	for( p = item_num; p < data->items_num; p++ )
	{
	    if( p + 1 < MAX_ITEMS ) 
		data->items_ptr[ p ] = data->items_ptr[ p + 1 ];
	    else
		data->items_ptr[ p ] = 0;
	}
	data->items_num--;
	if( data->items_num < 0 ) data->items_num = 0;

	if( data->selected_item >= data->items_num ) data->selected_item = data->items_num - 1;
	
	mem_on();
    }
}

void list_move_item_up( long item_num, list_data *data )
{
    if( item_num < data->items_num && item_num >= 0 )
    {
	mem_off();
	if( item_num != 0 )
	{
	    long temp = data->items_ptr[ item_num - 1 ];
	    data->items_ptr[ item_num - 1 ] = data->items_ptr[ item_num ];
	    data->items_ptr[ item_num ] = temp;
	    if( item_num == data->selected_item ) data->selected_item--;
	}
	mem_on();
    }
}	

void list_move_item_down( long item_num, list_data *data )
{
    if( item_num < data->items_num && item_num >= 0 )
    {
	mem_off();
	if( item_num != data->items_num - 1 )
	{
	    long temp = data->items_ptr[ item_num + 1 ];
	    data->items_ptr[ item_num + 1 ] = data->items_ptr[ item_num ];
	    data->items_ptr[ item_num ] = temp;
	    if( item_num == data->selected_item ) data->selected_item++;
	}
	mem_on();
    }
}

UTF8_CHAR* list_get_item( long item_num, list_data *data )
{
    if( item_num >= data->items_num ) return 0;
    if( item_num >= 0 )
	return data->items + data->items_ptr[ item_num ] + 1;
    else 
	return 0;
}

char list_get_attr( long item_num, list_data *data )
{
    if( item_num >= data->items_num ) return 0;
    if( item_num >= 0 )
	return data->items[ data->items_ptr[ item_num ] ] & 127;
    else
	return 0;
}

long list_get_selected_num( list_data *data )
{
    return data->selected_item;
}

void list_set_selected_num( long sel, list_data *data )
{
    data->selected_item = sel;
}

//Return values:
//1 - item1 > item2
//0 - item1 <= item2
long list_compare_items( long item1, long item2, list_data *data )
{
    UTF8_CHAR *i1 = data->items + data->items_ptr[ item1 ];
    UTF8_CHAR *i2 = data->items + data->items_ptr[ item2 ];
    UTF8_CHAR a1 = i1[ 0 ] & 127;
    UTF8_CHAR a2 = i2[ 0 ] & 127;
    i1++;
    i2++;
    
    long retval = 0;
    
    //Compare:
    if( a1 != a2 )
    {
	if( a1 == 1 ) retval = 0;
	if( a2 == 1 ) retval = 1;
    }
    else
    {
	for( int a = 0; ; a++ )
	{
	    if( i1[ a ] == 0 ) break;
	    if( i2[ a ] == 0 ) break;
	    if( i1[ a ] < i2[ a ] ) { break; }             //item1 < item2
	    if( i1[ a ] > i2[ a ] ) { retval = 1; break; } //item1 > item2
	}
    }
    
    return retval;
}

void list_sort( list_data *data )
{
    mem_off();
    for(;;)
    {
	int s = 0;
	for( long a = 0; a < data->items_num - 1; a++ )
	{
	    if( list_compare_items( a, a + 1, data ) )
	    {
		s = 1;
		long temp = data->items_ptr[ a + 1 ];
		data->items_ptr[ a + 1 ] = data->items_ptr[ a ];
		data->items_ptr[ a ] = temp;
	    }
	}
	if( s == 0 ) break;
    }
    mem_on();
}

//RANDOM GENERATOR:

unsigned long rand_next = 1;

void set_seed( unsigned long seed )
{
    rand_next = seed;
}

unsigned long pseudo_random( void )
{
    rand_next = rand_next * 1103515245 + 12345;
    return ( (unsigned int)( rand_next / 65536 ) % 32768 );
}

//MUTEXES:

#ifdef PALMOS
#include "../../sound/sound.h"
#endif

int sundog_mutex_init( sundog_mutex *mutex, int attr )
{
    int retval = 0;
#ifdef UNIX
    pthread_mutexattr_t mutexattr;
    pthread_mutexattr_settype( &mutexattr, PTHREAD_MUTEX_NORMAL );
    retval = pthread_mutex_init( &mutex->mutex, &mutexattr );
    pthread_mutexattr_destroy( &mutexattr );
#endif
#if defined(WIN) || defined(WINCE)
    mutex->mutex = CreateMutex( 0, 0, 0 );
#endif
#ifdef PALMOS
    mutex->mutex_cnt = 0;
    mutex->main_sound_callback_working = &g_snd.main_sound_callback_working;
#endif
    return retval;
}

int sundog_mutex_destroy( sundog_mutex *mutex )
{
    int retval = 0;
#ifdef UNIX
    retval = pthread_mutex_destroy( &mutex->mutex );
#endif
#if defined(WIN) || defined(WINCE)
    CloseHandle( mutex->mutex );
#endif
#ifdef PALMOS
    mutex->mutex_cnt = 0;
#endif
    return retval;
}

int sundog_mutex_lock( sundog_mutex *mutex )
{
    int retval = 0;
#ifdef UNIX
    retval = pthread_mutex_lock( &mutex->mutex );
#endif
#if defined(WIN) || defined(WINCE)
    WaitForSingleObject( mutex->mutex, INFINITE );
#endif
#ifdef PALMOS
    if( *mutex->main_sound_callback_working )
    {
	//We are in the main sound callback.
	if( mutex->mutex_cnt == 0 )
	{
	    mutex->mutex_cnt = 1;
	}
	else
	{
	    //Mutex is locked. But we can't wait for unlocking here,
	    //because all another "threads" are frozen (PalmOS is not multitask).
	    retval = 1;
	}
    }
    else
    {
	while( mutex->mutex_cnt != 0 ) {}
	mutex->mutex_cnt = 1;
    }
#endif
    return retval;
}

int sundog_mutex_unlock( sundog_mutex *mutex )
{
    int retval = 0;
#ifdef UNIX
    retval = pthread_mutex_unlock( &mutex->mutex );
#endif
#if defined(WIN) || defined(WINCE)
    ReleaseMutex( mutex->mutex );
#endif
#ifdef PALMOS
    mutex->mutex_cnt = 0;
#endif
    return retval;
}

//PROFILES:

profile_data g_profile;

void profile_new( profile_data *p )
{
    if( p == 0 ) p = &g_profile;

    p->num = 4;
    p->keys = (UTF8_CHAR**)MEM_NEW( HEAP_DYNAMIC, sizeof( UTF8_CHAR* ) * p->num );
    p->values = (UTF8_CHAR**)MEM_NEW( HEAP_DYNAMIC, sizeof( UTF8_CHAR* ) * p->num );
    mem_set( p->keys, sizeof( UTF8_CHAR* ) * p->num, 0 );
    mem_set( p->values, sizeof( UTF8_CHAR* ) * p->num, 0 );
}

void profile_resize( int new_num, profile_data *p )
{
    if( p == 0 ) p = &g_profile;

    if( new_num > p->num )
    {
	p->keys = (UTF8_CHAR**)mem_resize( p->keys, sizeof( UTF8_CHAR* ) * new_num );
	p->values = (UTF8_CHAR**)mem_resize( p->values, sizeof( UTF8_CHAR* ) * new_num );
	char *ptr = (char*)p->keys;
	mem_set( ptr + sizeof( UTF8_CHAR* ) * p->num, sizeof( UTF8_CHAR* ) * ( new_num - p->num ), 0 );
	ptr = (char*)p->values;
	mem_set( ptr + sizeof( UTF8_CHAR* ) * p->num, sizeof( UTF8_CHAR* ) * ( new_num - p->num ), 0 );
	p->num = new_num;
    }
}

int profile_add_value( const UTF8_CHAR *key, const UTF8_CHAR *value, profile_data *p )
{
    int rv = -1;

    if( p == 0 ) p = &g_profile;

    //dprint( "NEW VALUE: %s => %s\n", key, value );

    if( key )
    {
	for( rv = 0; rv < p->num; rv++ )
	{
	    if( p->keys[ rv ] == 0 ) break;
	}
	if( rv == p->num )
	{
	    //Free item not found.
	    profile_resize( p->num + 4, p );
	}
	if( value )
	{
	    p->values[ rv ] = (UTF8_CHAR*)MEM_NEW( HEAP_DYNAMIC, mem_strlen( value ) + 1 );
	    p->values[ rv ][ 0 ] = 0;
	    mem_strcat( p->values[ rv ], value );
	}
	p->keys[ rv ] = (UTF8_CHAR*)MEM_NEW( HEAP_DYNAMIC, mem_strlen( key ) + 1 );
	p->keys[ rv ][ 0 ] = 0;
	mem_strcat( p->keys[ rv ], key );
    }

    return rv;
}

int profile_get_int_value( const UTF8_CHAR *key, profile_data *p )
{
    int rv = -1;

    if( p == 0 ) p = &g_profile;

    if( key )
    {
	int i;
	for( i = 0; i < p->num; i++ )
	{
	    if( p->keys[ i ] )
		if( mem_strcmp( p->keys[ i ], key ) == 0 ) 
		    break;
	}
	if( i < p->num && p->values[ i ] )
	{
	    rv = string_to_int( p->values[ i ] );
	}
    }
    return rv;
}

UTF8_CHAR* profile_get_str_value( const UTF8_CHAR *key, profile_data *p )
{
    UTF8_CHAR* rv = 0;

    if( p == 0 ) p = &g_profile;

    if( key )
    {
	int i;
	for( i = 0; i < p->num; i++ )
	{
	    if( p->keys[ i ] )
		if( mem_strcmp( p->keys[ i ], key ) == 0 ) 
		    break;
	}
	if( i < p->num && p->values[ i ] )
	{
	    rv = p->values[ i ];
	}
    }
    return rv;
}

void profile_close( profile_data *p )
{
    if( p == 0 ) p = &g_profile;

    if( p->num )
    {
	for( int i = 0; i < p->num; i++ )
	{
	    if( p->keys[ i ] ) mem_free( p->keys[ i ] );
	    if( p->values[ i ] ) mem_free( p->values[ i ] );
	}
	mem_free( p->keys );
	mem_free( p->values );
	p->keys = 0;
	p->values = 0;
	p->num = 0;
    }
}

#define PROFILE_KEY_CHAR( cc ) ( !( cc < 0x21 || ptr >= size ) )

void profile_load( const UTF8_CHAR *filename, profile_data *p )
{
    UTF8_CHAR str1[ 129 ];
    UTF8_CHAR str2[ 129 ];
    str1[ 128 ] = 0;
    str2[ 128 ] = 0;
    int i;

    if( p == 0 ) p = &g_profile;

    profile_close( p );
    profile_new( p );

    int size = v3_get_file_size( filename );
    if( size == 0 ) return;
    uchar *f = (uchar*)MEM_NEW( HEAP_DYNAMIC, size + 1 );
    V3_FILE fp = v3_open( filename, "rb" );
    if( fp )
    {
	v3_read( f, 1, size, fp );
	v3_close( fp );
    }
    
    int ptr = 0;
    int c;
    char comment_mode = 0;
    char key_mode = 0;
    while( ptr < size )
    {
        c = f[ ptr ];
        if( c == 0xD || c == 0xA )
        {
    	    comment_mode = 0; //Reset comment mode at the end of line
	    if( key_mode > 0 )
	    {
		profile_add_value( str1, str2, p );
	    }
	    key_mode = 0;
	}
	if( comment_mode == 0 )
	{
	    if( f[ ptr ] == '/' && f[ ptr + 1 ] == '/' )
	    {
	        comment_mode = 1; //Comments
		ptr += 2;
	        continue;
	    }
	    if( PROFILE_KEY_CHAR( c ) )
	    {
	        if( key_mode == 0 )
	        {
	    	    //Get key name:
		    str2[ 0 ] = 0;
		    for( i = 0; i < 128; i++ )
		    {
			if( !PROFILE_KEY_CHAR( f[ ptr ] ) ) 
			{ 
			    str1[ i ] = 0;
			    ptr--;
			    break; 
			}
		        str1[ i ] = f[ ptr ];
			ptr++;
		    }
		    key_mode = 1;
		}
		else if( key_mode == 1 )
		{
		    //Get value:
		    str2[ 0 ] = 0;
		    if( f[ ptr ] == '"' )
		    {
			ptr++;
			for( i = 0; i < 128; i++ )
			{
			    if( f[ ptr ] == '"' || ptr >= size ) 
			    { 
			        str2[ i ] = 0;
			        break; 
			    }
			    str2[ i ] = f[ ptr ];
			    ptr++;
			}
		    }
		    else
		    {
			for( i = 0; i < 128; i++ )
			{
			    if( f[ ptr ] < 0x21 || ptr >= size ) 
			    { 
			        str2[ i ] = 0;
				ptr--;
			        break; 
			    }
			    str2[ i ] = f[ ptr ];
			    ptr++;
			}
		    }
		    key_mode = 2;
		}
	    }
	}
	ptr++;
    }
    if( key_mode > 0 )
    {
	profile_add_value( str1, str2, p );
    }
    
    if( f )
    {
	mem_free( f );
    }
}

//WORKING WITH A STRINGS:

void int_to_string( int value, UTF8_CHAR *str )
{
    int n;
    UTF8_CHAR ts[ 10 ];
    UTF8_CHAR ts_ptr = 0;
    
    if( value < 0 )
    {
    	*str = '-';
	*str++;
	value = -value;
    }

    n = value % 10; ts[ ts_ptr++ ] = (UTF8_CHAR) n + 48; value /= 10; if( !value ) goto int_finish;
    n = value % 10; ts[ ts_ptr++ ] = (UTF8_CHAR) n + 48; value /= 10; if( !value ) goto int_finish;
    n = value % 10; ts[ ts_ptr++ ] = (UTF8_CHAR) n + 48; value /= 10; if( !value ) goto int_finish;
    n = value % 10; ts[ ts_ptr++ ] = (UTF8_CHAR) n + 48; value /= 10; if( !value ) goto int_finish;
    n = value % 10; ts[ ts_ptr++ ] = (UTF8_CHAR) n + 48; value /= 10; if( !value ) goto int_finish;
    n = value % 10; ts[ ts_ptr++ ] = (UTF8_CHAR) n + 48; value /= 10; if( !value ) goto int_finish;
    n = value % 10; ts[ ts_ptr++ ] = (UTF8_CHAR) n + 48; value /= 10; if( !value ) goto int_finish;
    n = value % 10; ts[ ts_ptr++ ] = (UTF8_CHAR) n + 48; value /= 10; if( !value ) goto int_finish;
    n = value % 10; ts[ ts_ptr++ ] = (UTF8_CHAR) n + 48; value /= 10; if( !value ) goto int_finish;
    n = value; ts[ ts_ptr++ ] = (UTF8_CHAR) n + 48;
int_finish:

    while( ts_ptr )
    {
	ts_ptr--;
	*str = ts[ ts_ptr ];
	*str++;
    }
    
    *str = 0;
}

void hex_int_to_string( int value, UTF8_CHAR *str )
{
    int n;
    UTF8_CHAR ts[ 8 ];
    UTF8_CHAR ts_ptr = 0;

    n = value & 3; ts[ ts_ptr++ ] = (UTF8_CHAR) n + 48; if( n > 9 ) ts[ ts_ptr ] += 7; value /= 16; if( !value ) goto hex_int_finish;
    n = value & 3; ts[ ts_ptr++ ] = (UTF8_CHAR) n + 48; if( n > 9 ) ts[ ts_ptr ] += 7; value /= 16; if( !value ) goto hex_int_finish;
    n = value & 3; ts[ ts_ptr++ ] = (UTF8_CHAR) n + 48; if( n > 9 ) ts[ ts_ptr ] += 7; value /= 16; if( !value ) goto hex_int_finish;
    n = value & 3; ts[ ts_ptr++ ] = (UTF8_CHAR) n + 48; if( n > 9 ) ts[ ts_ptr ] += 7; value /= 16; if( !value ) goto hex_int_finish;
    n = value & 3; ts[ ts_ptr++ ] = (UTF8_CHAR) n + 48; if( n > 9 ) ts[ ts_ptr ] += 7; value /= 16; if( !value ) goto hex_int_finish;
    n = value & 3; ts[ ts_ptr++ ] = (UTF8_CHAR) n + 48; if( n > 9 ) ts[ ts_ptr ] += 7; value /= 16; if( !value ) goto hex_int_finish;
    n = value & 3; ts[ ts_ptr++ ] = (UTF8_CHAR) n + 48; if( n > 9 ) ts[ ts_ptr ] += 7; value /= 16; if( !value ) goto hex_int_finish;
    n = value; ts[ ts_ptr++ ] = (UTF8_CHAR) n + 48; if( n > 9 ) ts[ ts_ptr ] += 7;
hex_int_finish:

    while( ts_ptr )
    {
	ts_ptr--;
	*str = ts[ ts_ptr ];
	*str++;
    }
    
    *str = 0;
}

int string_to_int( const UTF8_CHAR *str )
{
    int res = 0;
    int d = 1;
    int minus = 1;
    for( int a = mem_strlen( str ) - 1; a >= 0; a-- )
    {
	int v = str[ a ];
	if( v >= '0' && v <= '9' )
	{
	    v -= '0';
	    res += v * d;
	    d *= 10;
	}
	else
	if( v == '-' ) minus = -1;
    }
    return res * minus;
}

int hex_string_to_int( const UTF8_CHAR *str )
{
    int res = 0;
    int d = 1;
    int minus = 1;
    for( int a = mem_strlen( str ) - 1; a >= 0; a-- )
    {
	int v = str[ a ];
	if( ( v >= '0' && v <= '9' ) || ( v >= 'A' && v <= 'F' ) || ( v >= 'a' && v <= 'f' ) )
	{
	    if( v >= '0' && v <= '9' ) v -= '0';
	    else
	    if( v >= 'A' && v <= 'F' ) { v -= 'A'; v += 10; }
	    else
	    if( v >= 'a' && v <= 'f' ) { v -= 'a'; v += 10; }
	    res += v * d;
	    d *= 16;
	}
	else
	if( v == '-' ) minus = -1;
    }
    return res * minus;
}

char int_to_hchar( int value )
{
    if( value < 10 ) return value + '0';
	else return ( value - 10 ) + 'A';
}

#define TEMP_STR_SIZE_DWORDS 256

int g_temp_str[ 256 * 4 ];
int g_str_num = 0;

UTF16_CHAR *utf8_to_utf16( UTF16_CHAR *dest, int dest_chars, const UTF8_CHAR *s )
{
    unsigned char *src = (unsigned char*)s;
    if( dest == 0 )
    {
	char *t = (char*)&g_temp_str[ TEMP_STR_SIZE_DWORDS * g_str_num ];
	dest = (UTF16_CHAR*)t;
	dest_chars = TEMP_STR_SIZE_DWORDS * 2;
    }
    UTF16_CHAR *dest_begin = dest;
    UTF16_CHAR *dest_end = dest + dest_chars;
    while( *src != 0 )
    {
	if( *src < 128 ) 
	{
	    *dest = (UTF16_CHAR)(*src);
	    src++;
	    dest++;
	}
	else
	{
	    if( *src & 64 )
	    {
		int res;
		if( ( *src & 32 ) == 0 )
		{
		    //Two bytes:
		    res = ( *src & 31 ) << 6;
		    src++;
		    res |= ( *src & 63 );
		    src++;
		    *dest = (UTF16_CHAR)res;
		    dest++;
		}
		else if( ( *src & 16 ) == 0 )
		{
		    //Three bytes:
		    res = ( *src & 15 ) << 12;
		    src++;
		    res |= ( *src & 63 ) << 6;
		    src++;
		    res |= ( *src & 63 );
		    src++;
		    *dest = (UTF16_CHAR)res;
		    dest++;
		}
		else if( ( *src & 8 ) == 0 )
		{
		    //Four bytes:
		    res = ( *src & 7 ) << 18;
		    src++;
		    res |= ( *src & 63 ) << 12;
		    src++;
		    res |= ( *src & 63 ) << 6;
		    src++;
		    res |= ( *src & 63 );
		    src++;
		    *dest = 0xD800 | (UTF16_CHAR)( res & 1023 );
		    dest++;
		    if( dest >= dest_end )
		    {
			dest--;
			break;
		    }
		    *dest = 0xDC00 | (UTF16_CHAR)( ( res >> 10 ) & 1023 );
		    dest++;
		}
		else
		{
		    //Unknown byte:
		    src++;
		    continue;
		}
	    }
	    else
	    {
		//Unknown byte:
		src++;
		continue;
	    }
	}
	if( dest >= dest_end )
	{
	    dest--;
	    break;
	}
    }
    *dest = 0;
    g_str_num++; g_str_num &= 3;
    return dest_begin;
}

UTF32_CHAR *utf8_to_utf32( UTF32_CHAR *dest, int dest_chars, const UTF8_CHAR *s )
{
    unsigned char *src = (unsigned char*)s;
    if( dest == 0 )
    {
	char *t = (char*)&g_temp_str[ TEMP_STR_SIZE_DWORDS * g_str_num ];
	dest = (UTF32_CHAR*)t;
	dest_chars = TEMP_STR_SIZE_DWORDS;
    }
    UTF32_CHAR *dest_begin = dest;
    UTF32_CHAR *dest_end = dest + dest_chars;
    while( *src != 0 )
    {
	if( *src < 128 ) 
	{
	    *dest = *src;
	    src++;
	    dest++;
	}
	else
	{
	    if( *src & 64 )
	    {
		int res;
		if( ( *src & 32 ) == 0 )
		{
		    //Two bytes:
		    res = ( *src & 31 ) << 6;
		    src++;
		    res |= ( *src & 63 );
		    src++;
		    *dest = (UTF32_CHAR)res;
		    dest++;
		}
		else if( ( *src & 16 ) == 0 )
		{
		    //Three bytes:
		    res = ( *src & 15 ) << 12;
		    src++;
		    res |= ( *src & 63 ) << 6;
		    src++;
		    res |= ( *src & 63 );
		    src++;
		    *dest = (UTF32_CHAR)res;
		    dest++;
		}
		else if( ( *src & 8 ) == 0 )
		{
		    //Four bytes:
		    res = ( *src & 7 ) << 18;
		    src++;
		    res |= ( *src & 63 ) << 12;
		    src++;
		    res |= ( *src & 63 ) << 6;
		    src++;
		    res |= ( *src & 63 );
		    src++;
		    *dest = (UTF32_CHAR)res;
		    dest++;
		}
		else
		{
		    //Unknown byte:
		    src++;
		    continue;
		}
	    }
	    else
	    {
		//Unknown byte:
		src++;
		continue;
	    }
	}
	if( dest >= dest_end )
	{
	    dest--;
	    break;
	}
    }
    *dest = 0;
    g_str_num++; g_str_num &= 3;
    return dest_begin;
}

UTF8_CHAR *utf16_to_utf8( UTF8_CHAR *dst, int dest_chars, const UTF16_CHAR *src )
{
    unsigned char *dest = (unsigned char*)dst;
    if( dest == 0 )
    {
	dest = (unsigned char*)&g_temp_str[ TEMP_STR_SIZE_DWORDS * g_str_num ];
	dest_chars = TEMP_STR_SIZE_DWORDS * 4;
    }
    unsigned char *dest_begin = dest;
    unsigned char *dest_end = dest + dest_chars;
    while( *src != 0 )
    {
	int res;
	if( ( *src & ~1023 ) != 0xD800 )
	{
	    res = *src;
	    src++;
	}
	else
	{
	    res = *src & 1023;
	    src++;
	    res |= ( *src & 1023 ) << 10;
	    src++;
	}
	if( res < 128 )
	{
	    *dest = (unsigned char)res;
	    dest++;
	}
	else if( res < 0x800 )
	{
	    if( dest >= dest_end - 2 ) break;
	    *dest = 0xC0 | ( ( res >> 6 ) & 31 );
	    dest++;
	    *dest = 0x80 | ( ( res >> 0 ) & 63 );
	    dest++;
	}
	else if( res < 0x10000 )
	{
	    if( dest >= dest_end - 3 ) break;
	    *dest = 0xC0 | ( ( res >> 12 ) & 15 );
	    dest++;
	    *dest = 0x80 | ( ( res >> 6 ) & 63 );
	    dest++;
	    *dest = 0x80 | ( ( res >> 0 ) & 63 );
	    dest++;
	}
	else
	{
	    if( dest >= dest_end - 4 ) break;
	    *dest = 0xC0 | ( ( res >> 18 ) & 7 );
	    dest++;
	    *dest = 0x80 | ( ( res >> 12 ) & 63 );
	    dest++;
	    *dest = 0x80 | ( ( res >> 6 ) & 63 );
	    dest++;
	    *dest = 0x80 | ( ( res >> 0 ) & 63 );
	    dest++;
	}
	if( dest >= dest_end )
	{
	    dest--;
	    break;
	}
    }
    *dest = 0;
    g_str_num++; g_str_num &= 3;
    return (UTF8_CHAR*)dest_begin;
}

UTF8_CHAR *utf32_to_utf8( UTF8_CHAR *dst, int dest_chars, const UTF32_CHAR *src )
{
    unsigned char *dest = (unsigned char*)dst;
    if( dest == 0 )
    {
	dest = (unsigned char*)&g_temp_str[ TEMP_STR_SIZE_DWORDS * g_str_num ];
	dest_chars = TEMP_STR_SIZE_DWORDS * 4;
    }
    unsigned char *dest_begin = dest;
    unsigned char *dest_end = dest + dest_chars;
    while( *src != 0 )
    {
	int res = (int)*src;
	src++;
	if( res < 128 )
	{
	    *dest = (unsigned char)res;
	    dest++;
	}
	else if( res < 0x800 )
	{
	    if( dest >= dest_end - 2 ) break;
	    *dest = 0xC0 | ( ( res >> 6 ) & 31 );
	    dest++;
	    *dest = 0x80 | ( ( res >> 0 ) & 63 );
	    dest++;
	}
	else if( res < 0x10000 )
	{
	    if( dest >= dest_end - 3 ) break;
	    *dest = 0xC0 | ( ( res >> 12 ) & 15 );
	    dest++;
	    *dest = 0x80 | ( ( res >> 6 ) & 63 );
	    dest++;
	    *dest = 0x80 | ( ( res >> 0 ) & 63 );
	    dest++;
	}
	else
	{
	    if( dest >= dest_end - 4 ) break;
	    *dest = 0xC0 | ( ( res >> 18 ) & 7 );
	    dest++;
	    *dest = 0x80 | ( ( res >> 12 ) & 63 );
	    dest++;
	    *dest = 0x80 | ( ( res >> 6 ) & 63 );
	    dest++;
	    *dest = 0x80 | ( ( res >> 0 ) & 63 );
	    dest++;
	}
	if( dest >= dest_end )
	{
	    dest--;
	    break;
	}
    }
    *dest = 0;
    g_str_num++; g_str_num &= 3;
    return (UTF8_CHAR*)dest_begin;
}

int utf8_to_utf32_char( const UTF8_CHAR *str, UTF32_CHAR *res )
{
    *res = 0;
    unsigned char *src = (unsigned char*)str;
    while( *src != 0 )
    {
	if( *src < 128 ) 
	{
	    *res = (UTF32_CHAR)*src;
	    return 1;
	}
	else
	{
	    if( *src & 64 )
	    {
		if( ( *src & 32 ) == 0 )
		{
		    //Two bytes:
		    *res = ( *src & 31 ) << 6;
		    src++;
		    *res |= ( *src & 63 );
		    return 2;
		}
		else if( ( *src & 16 ) == 0 )
		{
		    //Three bytes:
		    *res = ( *src & 15 ) << 12;
		    src++;
		    *res |= ( *src & 63 ) << 6;
		    src++;
		    *res |= ( *src & 63 );
		    return 3;
		}
		else if( ( *src & 8 ) == 0 )
		{
		    //Four bytes:
		    *res = ( *src & 7 ) << 18;
		    src++;
		    *res |= ( *src & 63 ) << 12;
		    src++;
		    *res |= ( *src & 63 ) << 6;
		    src++;
		    *res |= ( *src & 63 );
		    return 4;
		}
		else
		{
		    //Unknown byte:
		    src++;
		    continue;
		}
	    }
	    else
	    {
		//Unknown byte:
		src++;
		continue;
	    }
	}
    }
    return 1;
}

void utf8_unix_slash_to_windows( UTF8_CHAR *str )
{
    while( *str != 0 )
    {
	if( *str == 0x2F ) *str = 0x5C;	    
	str++;
    }
}

void utf16_unix_slash_to_windows( UTF16_CHAR *str )
{
    while( *str != 0 )
    {
	if( *str == 0x2F ) *str = 0x5C;	    
	str++;
    }
}

void utf32_unix_slash_to_windows( UTF32_CHAR *str )
{
    while( *str != 0 )
    {
	if( *str == 0x2F ) *str = 0x5C;	    
	str++;
    }
}
