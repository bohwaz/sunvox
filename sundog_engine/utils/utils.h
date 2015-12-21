#ifndef __UTILS_H__
#define __UTILS_H__

#ifdef UNIX
#include <pthread.h>
#endif

#if defined(WIN) || defined(WINCE)
#include <windows.h>
#endif

//LIST:

#define MAX_SIZE 16000
#define MAX_ITEMS (long)4000

struct list_data
{
    UTF8_CHAR *items; //Items data
    long *items_ptr; //Item pointers
    long items_num; //Number of items; 
    long selected_item;
    long start_item;
};

void list_init( list_data *data );
void list_close( list_data *data );
void list_clear( list_data *data );
void list_reset_selection( list_data *data );
void list_add_item( const UTF8_CHAR *item, char attr, list_data *data );
void list_delete_item( long item_num, list_data *data );
void list_move_item_up( long item_num, list_data *data );
void list_move_item_down( long item_num, list_data *data );
UTF8_CHAR* list_get_item( long item_num, list_data *data );
char list_get_attr( long item_num, list_data *data );
long list_get_selected_num( list_data *data );
void list_set_selected_num( long sel, list_data *data );
long list_compare_items( long item1, long item2, list_data *data );
void list_sort( list_data *data );

//RANDOM GENERATOR:

void set_seed( unsigned long seed );
unsigned long pseudo_random( void );

//MUTEXES:

struct sundog_mutex
{
#ifdef UNIX
    pthread_mutex_t mutex;
#endif
#if defined(WIN) || defined(WINCE)
    HANDLE mutex;
#endif
#ifdef PALMOS
    volatile int mutex_cnt;
    volatile int *main_sound_callback_working;
#endif
};

int sundog_mutex_init( sundog_mutex *mutex, int attr );
int sundog_mutex_destroy( sundog_mutex *mutex );
int sundog_mutex_lock( sundog_mutex *mutex );
int sundog_mutex_unlock( sundog_mutex *mutex );

//PROFILES:

#define KEY_SCREENX		"width"
#define KEY_SCREENY		"height"
#define KEY_SCREENFLIP		"flip"
#define KEY_FULLSCREEN		"fullscreen"
#define KEY_SOUNDBUFFER		"buffer"
#define KEY_AUDIODEVICE		"audiodevice"
#define KEY_FREQ		"frequency"
#define KEY_WINDOWNAME		"windowname"
#define KEY_NOBORDER		"noborder"
#define KEY_VIDEODRIVER		"videodriver"

struct profile_data
{
    UTF8_CHAR **keys;
    UTF8_CHAR **values;
    int num;
};

void profile_new( profile_data *p );
void profile_resize( int new_num, profile_data *p );
int profile_add_value( const UTF8_CHAR *key, const UTF8_CHAR *value, profile_data *p );
int profile_get_int_value( const UTF8_CHAR *key, profile_data *p );
UTF8_CHAR* profile_get_str_value( const UTF8_CHAR *key, profile_data *p );
void profile_close( profile_data *p );
void profile_load( const UTF8_CHAR *filename, profile_data *p );

//STRINGS:

void int_to_string( int value, UTF8_CHAR *str );
void hex_int_to_string( int value, UTF8_CHAR *str );
int string_to_int( const UTF8_CHAR *str );
int hex_string_to_int( const UTF8_CHAR *str );
char int_to_hchar( int value );

UTF16_CHAR *utf8_to_utf16( UTF16_CHAR *dest, int dest_chars, const UTF8_CHAR *s );
UTF32_CHAR *utf8_to_utf32( UTF32_CHAR *dest, int dest_chars, const UTF8_CHAR *s );
UTF8_CHAR *utf16_to_utf8( UTF8_CHAR *dst, int dest_chars, const UTF16_CHAR *src );
UTF8_CHAR *utf32_to_utf8( UTF8_CHAR *dst, int dest_chars, const UTF32_CHAR *src );

int utf8_to_utf32_char( const UTF8_CHAR *str, UTF32_CHAR *res );

void utf8_unix_slash_to_windows( UTF8_CHAR *str );
void utf16_unix_slash_to_windows( UTF16_CHAR *str );
void utf32_unix_slash_to_windows( UTF32_CHAR *str );

#endif
