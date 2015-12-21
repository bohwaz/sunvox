#ifndef __MEMORY__
#define __MEMORY__

enum 
{
    HEAP_DYNAMIC = 0,
    HEAP_STORAGE
};
#define HEAP_MASK		3

#define MEM_FLAG_NOCACHE	4

#ifdef PALMOS
#else
    #define USE_NAMES
#endif

#ifdef USE_NAMES
    #define MEM_BLOCK_INFO   32
    #define MEM_NAME        -16
    #define MEM_HEAP        -20
    #define MEM_SIZE        -24
    #define MEM_NEXT        -28
    #define MEM_PREV        -32
#else
    #define MEM_BLOCK_INFO   16
    #define MEM_HEAP        -4
    #define MEM_SIZE        -8
    #define MEM_NEXT        -12
    #define MEM_PREV        -16
#endif

#define MEM_NEW( heap, size ) mem_new( heap, size, (UTF8_CHAR*)__FUNCTION__, 0 )

//mem_new():
//heap - heap number:
//HEAP_DYNAMIC = dynamic heap for small blocks
//HEAP_STORAGE = storage heap for large static blocks
//size - block size
int mem_free_all();
void* mem_new( unsigned long heap, unsigned long size, const UTF8_CHAR *name, unsigned long id ); //Each memory block has own name and ID
void simple_mem_free( void *ptr );
void mem_free( void *ptr );
void mem_set( void *ptr, unsigned long size, unsigned char value );
void* mem_resize( void *ptr, int size );
void mem_copy( void *dest, const void *src, unsigned long size );
int mem_cmp( const char *p1, const char *p2, unsigned long size );
void mem_strcat( char *dest, const char *src );
long mem_strcmp( const char *s1, const char *s2 );
int mem_strlen( const char *s );
int mem_strlen_utf32( const UTF32_CHAR *s );
char *mem_strdup( const char *s1 );

//Get info about memory block:
long mem_get_heap( void *ptr );
long mem_get_flags( void *ptr );
long mem_get_size( void *ptr );
char *mem_get_name( void *ptr );

//Palm specific:
void mem_on( void ); //Storage protection ON
void mem_off( void ); //Storage protection OFF
void mem_palm_normal_mode( void ); //Switch to normal mode (Storage protection ON)
void mem_palm_our_mode( void ); //Switch back to our mode (Storage protection is ON or OFF)

//Posix compatibility for PalmOS devices:
#ifdef PALMOS
#include <PalmOS.h>
extern SysAppInfoPtr ai1, ai2, appInfo;
extern unsigned short ownID;
void *malloc ( int size );
void *realloc ( void * ptr, int size );
void free ( void * ptr );
void *memcpy ( void * destination, const void * source, int num );
int strcmp ( const char * str1, const char * str2 );
int memcmp ( const char * p1, const char * p2, int size );
int strlen ( const char * str1 );
#endif

#endif
