#ifndef __V3NUS_FS__
#define __V3NUS_FS__

//V3NUS file system. Device depended.
//Example of file names on a different platforms:
//PalmOS:
// A:/prog.txt		- prog.txt database on the internal RAM
// B:/prog.txt		- prog.txt file on the external FLASH card
//Linux:
// /prog.txt		- prog.txt file in the root directory 
//Windows:
// C:/prog.txt		- prog.txt on disk C
//Any OS:
// 0:/prog.txt		- prog.txt file on virtual disk 0. This virtual disk is a simple TAR file. 
//			  Example of using:
//			  g_virt_disk0 = v3_open( "arhive.tar", "rb" );
//			  v3_open( "0:/some_file.txt", "rb" );
//			  ...

#include "core/core.h"

#if defined(WIN) | defined(WINCE)
    #include "windows.h"
#endif

#ifdef UNIX
    #include "dirent.h" //for file find
    #include "unistd.h" //for current dir getting 
#endif

#define MAX_DISKS	    16
#define DISKNAME_SIZE	    4
#define MAX_DIR_LEN	    1024
extern long disks; //number of disks
void get_disks( void ); //get info about local disks
UTF8_CHAR* get_disk_name( long number ); //get name of local disk
long get_current_disk( void ); //get number of the current disk
UTF8_CHAR* get_current_path( void ); //get current path (example: "C:/mydir/")
UTF8_CHAR* get_user_path( void );
UTF8_CHAR* get_temp_path( void );

//***********************************************************
//PalmOS functions: *****************************************
//***********************************************************

#ifdef PALMOS

#include "VFSMgr.h"

typedef void FILE;

long open_vfs( void ); //Return 1 if successful. vfs_volume_number = number of first finded VFS volume (flash-card)
int isBusy( DmOpenRef db, int index ); //Is record busy?
void recordsBusyReset( int f );
void get_records_info( DmOpenRef db, uint16 f_num );
FILE* fopen( const char *filename, const char *filemode );
int fclose( FILE *fpp );
void vfs2buffer( ulong f );
void buffer2vfs( ulong f );
int next_record( ulong f );
void rewind( FILE *fpp );
int getc( FILE *fpp );
ulong get_record_rest( FILE *fpp );
int ftell ( FILE *fpp );
uint16 fseek( FILE *fpp, long offset, long access );
int feof( FILE *fpp );
ulong fread( void *ptr, ulong el_size, ulong elements, FILE *fpp );
ulong fwrite( void *ptr, ulong el_size, ulong elements, FILE *fpp );
void fputc( int val, FILE *fpp );
ulong remove( const char *filename );

#else

//***********************************************************
//***********************************************************
//***********************************************************

//For non-palm devices: =====================================
#include "stdio.h"
//===========================================================

#endif

//***********************************************************
//***********************************************************
//***********************************************************

//***********************************************************
//Main multiplatform functions:******************************
//***********************************************************

#define MAX_V3_DESCRIPTORS	    8

//V3_FILE types:
#define V3_FILE_STD		    0
#define V3_FILE_ON_VIRTUAL_DISK	    1
#define V3_FILE_IN_MEMORY	    2

struct V3_FILE_STRUCT
{
    UTF8_CHAR	    *filename;
    unsigned int    f;
    int		    type;
    char	    *virt_file_data;
    int		    virt_file_ptr;
    int		    virt_file_size;
};

#define V3_FILE unsigned long

extern V3_FILE g_virt_disk0;

V3_FILE v3_open_in_memory( void *data, int size );
V3_FILE v3_open( const UTF8_CHAR *filename, const UTF8_CHAR *filemode );
int v3_close( V3_FILE f );
void v3_rewind( V3_FILE f );
int v3_getc( V3_FILE f );
ulong v3_tell ( V3_FILE f );
int v3_seek( V3_FILE f, long offset, long access );
int v3_eof( V3_FILE f );
ulong v3_read( void *ptr, ulong el_size, ulong elements, V3_FILE f );
ulong v3_write( void *ptr, ulong el_size, ulong elements, V3_FILE f );
void v3_putc( int val, V3_FILE f );
ulong v3_remove( const UTF8_CHAR *filename );
ulong v3_get_file_size( const UTF8_CHAR *filename );

//***********************************************************
//***********************************************************
//***********************************************************

//FIND FILE FUNCTIONS:

//type in find_struct:
enum 
{
    TYPE_FILE = 0,
    TYPE_DIR
};

struct find_struct
{ //structure for file searching functions
    const UTF8_CHAR *start_dir; //Example: "c:/mydir/" "d:/"
    const UTF8_CHAR *mask; //Example: "xm/mod/it" (or NULL for all files)
    
    UTF8_CHAR name[ MAX_DIR_LEN ]; //Found file's name
    char type; //Found file's type: 0 - file; 1 - dir

#ifndef NONPALM
    uint16 card_id;
    LocalID db_id;
    DmSearchStateType search_info;
    FileRef dir_ref; //VFS: reference to the start_dir
    ulong dir_iterator; //VFS: dir iterator
    FileInfoType file_info; //VFS: file info
#endif
#if defined(WIN)
    WIN32_FIND_DATAW find_data;
#endif
#if defined(WINCE)
    WIN32_FIND_DATA find_data;
#endif
#if defined(WIN) | defined(WINCE)
    HANDLE find_handle;
    UTF8_CHAR win_mask[ MAX_DIR_LEN ]; //Example: "*.xm *.mod *.it"
    UTF8_CHAR win_start_dir[ MAX_DIR_LEN ]; //Example: "mydir\*.xm"
#endif
#ifdef UNIX
    DIR *dir;
    struct dirent *current_file;
    UTF8_CHAR new_start_dir[ MAX_DIR_LEN ];
#endif
};

int find_first( find_struct* ); //Return values: 0 - no files
int find_next( find_struct* ); //Return values: 0 - no files
void find_close( find_struct* );

#endif //__V3NUS_FS__
