/*
    v3nus_fs.cpp. Multiplatform file system
    This file is part of the SunDog engine.
    Copyright (C) 2002 - 2009 Alex Zolotov <nightradio@gmail.com>
*/

#include "../../core/core.h"
#include "../../core/debug.h"
#include "../../memory/memory.h"
#include "../../utils/utils.h"
#include "../v3nus_fs.h"

#ifdef NONPALM
    #include <string.h>
#endif

#if defined(WIN) || defined(WINCE)
    #include <shlobj.h>
#endif

#ifdef UNIX
    #include <stdlib.h>
#endif

long disks = 0; //Number of local disks
UTF8_CHAR disk_names[ DISKNAME_SIZE * MAX_DISKS ]; //Disk names. Each name - 4 bytes (with NULL) (for example: "C:/", "H:/")
UTF8_CHAR current_filename[ MAX_DIR_LEN ];
UTF8_CHAR* get_disk_name( long number ) { return disk_names + ( DISKNAME_SIZE * number ); }

#ifdef WIN
void get_disks( void )
{
    UTF8_CHAR temp[ 512 ];
    int len, p, dp, tmp;
    disks = 0;
    len = GetLogicalDriveStrings( 512, temp );
    for( dp = 0, p = 0; p < len; p++ )
    {
	tmp = ( disks * DISKNAME_SIZE ) + dp;
	disk_names[ tmp ] = temp[ p ];
	if( disk_names[ tmp ] == 92 ) disk_names[ tmp ] = '/';
	if( temp[ p ] == 0 ) { disks++; dp = 0; continue; }
	dp++;
    }
}
long get_current_disk( void )
{
    UTF8_CHAR cur_dir[ MAX_DIR_LEN ];
    long a;
    UTF8_CHAR *disk_name;
    GetCurrentDirectory( MAX_DIR_LEN, cur_dir );
    if( cur_dir[ 0 ] > 0x60 && cur_dir[ 0 ] < 0x7B )
	cur_dir[ 0 ] -= 0x20; //Make it capital
    for( a = 0; a < disks; a++ )
    {
	disk_name = get_disk_name( a );
	if( disk_name[ 0 ] == cur_dir[ 0 ] ) 
	{
	    return a;
	}
    }
    return 0;
}
UTF8_CHAR g_current_path[ MAX_DIR_LEN ] = { 0 };
UTF8_CHAR* get_current_path( void )
{
    long a;
    UTF16_CHAR ts_utf16[ MAX_DIR_LEN ];
    GetCurrentDirectoryW( MAX_DIR_LEN, (wchar_t*)ts_utf16 );
    utf16_to_utf8( g_current_path, MAX_DIR_LEN, (const UTF16_CHAR*)ts_utf16 );
    for( a = 0; a < MAX_DIR_LEN; a++ ) //Make "/mydir" from "\mydir"
    {
        if( g_current_path[ a ] == 92 ) g_current_path[ a ] = '/';
    }
    a = mem_strlen( g_current_path );
    g_current_path[ a ] = '/';
    g_current_path[ a + 1 ] = 0;
    return g_current_path;
}
UTF8_CHAR g_user_path[ MAX_DIR_LEN ] = { 0 };
UTF8_CHAR* get_user_path( void )
{
    long a;
    UTF16_CHAR ts_utf16[ MAX_DIR_LEN ];
    SHGetFolderPathW( NULL, CSIDL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, (wchar_t*)ts_utf16 );
    utf16_to_utf8( g_user_path, MAX_DIR_LEN, (const UTF16_CHAR*)ts_utf16 );
    for( a = 0; a < MAX_DIR_LEN; a++ ) //Make "/mydir" from "\mydir"
    {
        if( g_user_path[ a ] == 92 ) g_user_path[ a ] = '/';
    }
    a = mem_strlen( g_user_path );
    g_user_path[ a ] = '/';
    g_user_path[ a + 1 ] = 0;
    return g_user_path;
}
UTF8_CHAR g_temp_path[ MAX_DIR_LEN ] = { 0 };
UTF8_CHAR* get_temp_path( void )
{
    long a;
    UTF16_CHAR ts_utf16[ MAX_DIR_LEN ];
    GetTempPathW( MAX_DIR_LEN, (wchar_t*)ts_utf16 );
    utf16_to_utf8( g_temp_path, MAX_DIR_LEN, (const UTF16_CHAR*)ts_utf16 );
    for( a = 0; a < MAX_DIR_LEN; a++ ) //Make "/mydir" from "\mydir"
    {
        if( g_temp_path[ a ] == 92 ) g_temp_path[ a ] = '/';
    }
    a = mem_strlen( g_temp_path );
    g_temp_path[ a ] = '/';
    g_temp_path[ a + 1 ] = 0;
    return g_temp_path;
}
#endif
#ifdef PALMOS
uint16 vfs_volumes = 0;
void get_disks( void )
{
    long d;
    UTF8_CHAR disk_char = 'B'; //VFS disk name
    disks = 1;
    disk_names[ 0 ] = 'A';
    disk_names[ 1 ] = ':';
    disk_names[ 2 ] = '/';
    disk_names[ 3 ] = 0;
    open_vfs(); //Check for VFS volumes
    for( d = 0; d < vfs_volumes; d++ ) 
    {
	disk_names[ 0 + ( DISKNAME_SIZE * d ) + DISKNAME_SIZE ] = disk_char++;
        disk_names[ 1 + ( DISKNAME_SIZE * d ) + DISKNAME_SIZE ] = ':';
	disk_names[ 2 + ( DISKNAME_SIZE * d ) + DISKNAME_SIZE ] = '/';
	disk_names[ 3 + ( DISKNAME_SIZE * d ) + DISKNAME_SIZE ] = 0;
	disks++;
    }
}
long get_current_disk( void )
{
    return 0;
}
UTF8_CHAR* get_current_path( void )
{
    return (UTF8_CHAR*)"A:/";
}
UTF8_CHAR* get_user_path( void )
{
    return (UTF8_CHAR*)"A:/";
}
UTF8_CHAR* get_temp_path( void )
{
    return (UTF8_CHAR*)"A:/";
}
#endif
#ifdef UNIX
void get_disks( void )
{
    disks = 1;
    disk_names[ 0 ] = '/';
    disk_names[ 1 ] = 0;
}
long get_current_disk( void )
{
    return 0;
}
UTF8_CHAR g_current_path[ MAX_DIR_LEN ] = { 0 };
UTF8_CHAR* get_current_path( void )
{
    long a;
    getcwd( g_current_path, MAX_DIR_LEN );
    a = mem_strlen( g_current_path );
    g_current_path[ a ] = '/';
    g_current_path[ a + 1 ] = 0;
    return g_current_path;
}
UTF8_CHAR g_user_path[ MAX_DIR_LEN ] = { 0 };
UTF8_CHAR* get_user_path( void )
{
    UTF8_CHAR* user_p = getenv( "HOME" );
    g_user_path[ 0 ] = 0;
    mem_strcat( g_user_path, (const UTF8_CHAR*)user_p );
    mem_strcat( g_user_path, "/" );
    return g_user_path;
}
UTF8_CHAR* get_temp_path( void )
{
    return (UTF8_CHAR*)"/tmp/";
}
#endif
#ifdef WINCE
void get_disks( void )
{
    disks = 1;
    disk_names[ 0 ] = '/';
    disk_names[ 1 ] = 0;
}
long get_current_disk( void )
{
    return 0;
}
UTF8_CHAR* get_current_path( void )
{
    return (UTF8_CHAR*)"";
}
UTF8_CHAR g_user_path[ MAX_DIR_LEN ] = { 0 };
UTF8_CHAR* get_user_path( void )
{
    long a;
    UTF16_CHAR ts_utf16[ MAX_DIR_LEN ];
    SHGetSpecialFolderPath( NULL, (wchar_t*)ts_utf16, CSIDL_APPDATA, 1 );
    utf16_to_utf8( g_user_path, MAX_DIR_LEN, (const UTF16_CHAR*)ts_utf16 );
    for( a = 0; a < MAX_DIR_LEN; a++ ) //Make "/mydir" from "\mydir"
    {
        if( g_user_path[ a ] == 92 ) g_user_path[ a ] = '/';
    }
    a = mem_strlen( g_user_path );
    g_user_path[ a ] = '/';
    g_user_path[ a + 1 ] = 0;
    return g_user_path;
}
UTF8_CHAR g_temp_path[ MAX_DIR_LEN ] = { 0 };
UTF8_CHAR* get_temp_path( void )
{
    long a;
    UTF16_CHAR ts_utf16[ MAX_DIR_LEN ];
    GetTempPathW( MAX_DIR_LEN, (wchar_t*)ts_utf16 );
    utf16_to_utf8( g_temp_path, MAX_DIR_LEN, (const UTF16_CHAR*)ts_utf16 );
    for( a = 0; a < MAX_DIR_LEN; a++ ) //Make "/mydir" from "\mydir"
    {
        if( g_temp_path[ a ] == 92 ) g_temp_path[ a ] = '/';
    }
    a = mem_strlen( g_temp_path );
    g_temp_path[ a ] = '/';
    g_temp_path[ a + 1 ] = 0;
    return g_temp_path;
}
#endif

//***********************************************************
//PalmOS functions: *****************************************
//***********************************************************

#ifdef PALMOS

//PalmOS functions:
#define MAX_RECORDS 256
#define MAX_F_POINTERS 2
ulong r_size[ MAX_F_POINTERS ][ MAX_RECORDS ]; //size of each record in file
ulong f_size[ MAX_F_POINTERS ]; //size of each file (for each file pointer)
ulong cur_rec[ MAX_F_POINTERS ]; //current record number (for each file pointer)
long cur_off[ MAX_F_POINTERS ]; //current record offset (for each file pointer)
ulong cur_pnt[ MAX_F_POINTERS ]; //current file offset (for each file pointer)
ulong recs[ MAX_F_POINTERS ]; //number of records (for each file pointer)
DmOpenRef cur_db[ MAX_F_POINTERS ]; //current DB pointer
LocalID ID; //local ID for DB
uchar fp[ MAX_F_POINTERS ] = { 0, 0 }; //for each file pointer: 0-free 1-working
uchar *cur_rec_pnt[ MAX_F_POINTERS ];
MemHandle cur_rec_h[ MAX_F_POINTERS ];
FileRef vfs_file[ MAX_F_POINTERS ]; //VFS file or NULL
char vfs_first_read[ MAX_F_POINTERS ]; //1 - after first read
uchar write_flag[ MAX_F_POINTERS ]; //0 - read mode; 1 - write mode
uint16 vfs_volume_numbers[ 8 ];

long open_vfs( void )
{
    ulong vol_iterator = vfsIteratorStart;
    Err error;
    vfs_volumes = 0;
    while( vol_iterator != vfsIteratorStop ) 
    {
	mem_palm_normal_mode(); //#####
	error = VFSVolumeEnumerate( &vfs_volume_numbers[vfs_volumes], &vol_iterator );
	mem_palm_our_mode();    //#####
	if( error == errNone ) vfs_volumes++;
    }
    if( vfs_volumes ) return 1; else return 0;
}

int isBusy( DmOpenRef db, int index )  //Is record busy?
{
    unsigned short attributes;
    DmRecordInfo( db, index, &attributes, 0, 0 );
    if( attributes & dmRecAttrBusy ) return 1;
    return 0;
}

void recordsBusyReset( int f )
{
    int a;
    for( a = 0; a < recs[ f ]; a++ )
    {
	if( isBusy( cur_db[ f ], a ) ) DmReleaseRecord( cur_db[ f ], a, 0 );
    }
}

void get_records_info( DmOpenRef db, uint16 f_num )
{
    MemHandle cur_h;
    unsigned char *cur;
    uint16 a;
    recs[ f_num ] = DmNumRecords( db );
    f_size[ f_num ]=0;
    for( a = 0; a < recs[ f_num ]; a++ ) if( isBusy( db, a ) ) { DmReleaseRecord( db, a, 0 ); }
    for( a = 0; a < recs[ f_num ]; a++ ) 
    {
	cur_h = DmGetRecord( db, a );
        cur = (uchar*)MemHandleLock( cur_h );
        r_size[ f_num ][ a ]=MemHandleSize( cur_h );
	f_size[ f_num ] += r_size[ f_num ][ a ];
	MemHandleUnlock( cur_h );
    }
}

void create_new_record( DmOpenRef db )
{
    int records = DmNumRecords( db );
    UInt16 new_index = dmMaxRecordIndex;
    DmNewRecord( db, &new_index, 50000 );
}

FILE* fopen( const char *filename, const char *filemode )
{
    ulong a;
    if( filemode[ 0 ] == 'r' ) 
    { //read file:
	if( ( filename[ 0 ] == 'A' && filename[ 1 ] == ':' ) || filename[ 1 ] != ':' )
	{ //It's PalmOS PDB:
	    if( filename[ 0 ] == 'A' && filename[ 1 ] == ':' )
		filename += 3; //Skip "A:/"
	    ID = DmFindDatabase( 0, (const char*)filename );
	    if( ID != 0 )
	    { //file exist:
		for( a = 0; a < MAX_F_POINTERS; a++ ) if( fp[ a ] == 0 ) break; //searching for a free file pointer
		if( a == MAX_F_POINTERS ) return 0; //not enought memory for new pointer
		fp[ a ] = 1;
		write_flag[ a ] = 0;
		vfs_file[ a ] = 0; //Not VFS 
		cur_db[ a ] = DmOpenDatabase( 0, ID, dmModeReadWrite ); //open
		get_records_info( cur_db[ a ], a );
		cur_rec[ a ] = 0;
		cur_off[ a ] = 0;
		cur_pnt[ a ] = 0;
		if( isBusy( cur_db[ a ], 0 ) ) DmReleaseRecord( cur_db[ a ], 0, 0 );
		cur_rec_h[ a ] = DmGetRecord( cur_db[ a ], 0 );
		cur_rec_pnt[ a ] = (uchar*)MemHandleLock( cur_rec_h[ a ] );
		return (FILE*)( a + 1 );
	    } else /*file not found*/ return 0;
	}
	else
	{ //It's VFS:
	    if( filename[ 0 ] - 65 >= disks ) return 0; //no such disk
	    int disk_num = filename[ 0 ] - 66;
	    filename += 2; //Skip "X:"
	    if( open_vfs() ) 
	    {
		for( a = 0; a < MAX_F_POINTERS; a++ ) if( fp[ a ] == 0 ) break; //searching for a free file pointer
		if( a == MAX_F_POINTERS ) return 0; //not enought memory for new pointer
		mem_palm_normal_mode(); //##### 
		Err err = VFSFileOpen( vfs_volume_numbers[ disk_num ], filename, vfsModeRead, &vfs_file[ a ] );
		mem_palm_our_mode();    //#####
		if( err != errNone ) { dprint( "error in FileOpen\n" ); dprint( "%d\n", err ); return 0; } //File not found
		//File found:
		fp[ a ] = 1;
		write_flag[ a ] = 0;
		vfs_first_read[ a ] = 0;
		
		//Create buffer:
		
		char temp_filename[ 3 ];
		temp_filename[ 0 ] = (char)a + 65;
		temp_filename[ 1 ] = 'F';
		temp_filename[ 2 ] = 0;
		ID = DmFindDatabase( 0, (const char*)temp_filename );
		if( ID != 0 ) DmDeleteDatabase( 0, ID ); //File exist: delete it
		//Create new database:
		DmCreateDatabase( 0, (const char*)temp_filename, 'ZULU', 'PSYX', 0 );
		ID = DmFindDatabase( 0, (const char*)temp_filename );
		cur_db[ a ] = DmOpenDatabase( 0, ID, dmModeWrite ); //open
		uint16 new_rec = 0;
		DmNewRecord( cur_db[ a ], &new_rec, 60000 );
		DmCloseDatabase( cur_db[ a ] );
		cur_db[ a ] = DmOpenDatabase( 0, ID, dmModeReadWrite ); //open it again
		get_records_info( cur_db[ a ], a );
		cur_rec[ a ] = 0;
		cur_off[ a ] = 0;
		cur_pnt[ a ] = 0;
		if( isBusy( cur_db[ a ], 0 ) ) DmReleaseRecord( cur_db[ a ], 0, 0 );
		cur_rec_h[ a ] = DmGetRecord( cur_db[ a ], 0 );
		cur_rec_pnt[ a ] = (uchar*)MemHandleLock( cur_rec_h[ a ] );
		VFSFileSize( vfs_file[ a ], &f_size[ a ] );
	    
		return (FILE*)( a + 1 );
	    }
	}
    }
    if( filemode[ 0 ] == 'w' ) 
    { //write file:
	if( ( filename[ 0 ] == 'A' && filename[ 1 ] == ':' ) || filename[ 1 ] != ':' )
	{ //It's PalmOS PDB:
	    if( filename[ 0 ] == 'A' && filename[ 1 ] == ':' )
		filename += 3; //Skip "A:/"
	    ID = DmFindDatabase( 0, (const char*)filename );
	    if( ID != 0 ) DmDeleteDatabase( 0, ID ); //File exist: delete it
	    //Create new database:
	    DmCreateDatabase( 0, (const char*)filename, 'ZULU', 'PSYX', 0 );
	    //Open new file:
	    ID = DmFindDatabase( 0, (const char*)filename );
	    if( ID != 0 )
	    { //file exist:
		for( a = 0; a < MAX_F_POINTERS; a++ ) if( fp[ a ] == 0 ) break; //searching for free file pointer
		if( a == MAX_F_POINTERS ) return 0; //not enought memory for new pointer
		fp[ a ] = 1;
		write_flag[ a ] = 1;
		vfs_file[ a ] = 0; //Not VFS 
		cur_db[ a ] = DmOpenDatabase( 0, ID, dmModeWrite ); //open
		create_new_record( cur_db[ a ] );
		get_records_info( cur_db[ a ], a );
		cur_rec[ a ] = 0;
		cur_off[ a ] = 0;
		cur_pnt[ a ] = 0;
		if( isBusy( cur_db[ a ], 0 ) ) DmReleaseRecord( cur_db[ a ], 0, 0 );
		cur_rec_h[ a ] = DmGetRecord( cur_db[ a ], 0 );
		cur_rec_pnt[ a ] = (uchar*)MemHandleLock( cur_rec_h[ a ] );
		return (FILE*)( a + 1 );
	    } else /*file not found*/ return 0;
	}
	else
	{ //It's VFS:
	    get_disks();
	    if( filename[ 0 ] - 65 >= disks ) return 0; //no such disk
	    int disk_num = filename[ 0 ] - 66;
	    filename += 2; //Skip "X:"
	    if( open_vfs() ) 
	    {
		for( a = 0; a < MAX_F_POINTERS; a++ ) if( fp[ a ] == 0 ) break; //searching for free file pointer
		if( a == MAX_F_POINTERS ) return 0; //not enought memory for new pointer
		mem_palm_normal_mode(); //#####
		Err err = VFSFileOpen( vfs_volume_numbers[ disk_num ], filename, vfsModeRead, &vfs_file[ a ] );
		VFSFileClose( vfs_file[ a ] ); 
		if( err == errNone )
		{ //File exist:
		    //Delete it:
		    VFSFileDelete( vfs_volume_numbers[ disk_num ], filename ); 
		}
		err = VFSFileOpen( vfs_volume_numbers[ disk_num ], filename, vfsModeCreate | vfsModeWrite, &vfs_file[ a ] );
		mem_palm_our_mode();    //#####
		if( err != errNone ) return 0; //File open error
		//File found:
		fp[ a ] = 1;
		write_flag[ a ] = 1;
		
		//Create buffer:
		
		char temp_filename[ 3 ];
		temp_filename[ 0 ] = (char)a + 65;
		temp_filename[ 1 ] = 'F';
		temp_filename[ 2 ] = 0;
		ID = DmFindDatabase( 0, (const char*)temp_filename );
		if( ID != 0 ) DmDeleteDatabase( 0, ID ); //File exist: delete it
		//Create new database:
		DmCreateDatabase( 0, (const char*)temp_filename, 'ZULU', 'PSYX', 0 );
		ID = DmFindDatabase( 0, (const char*)temp_filename );
		cur_db[ a ] = DmOpenDatabase( 0, ID, dmModeReadWrite ); //open it
		uint16 new_rec = 0;
		DmNewRecord( cur_db[ a ], &new_rec, 60000 );
		DmCloseDatabase( cur_db[ a ] );
		cur_db[ a ] = DmOpenDatabase( 0, ID, dmModeReadWrite ); //open it again
		get_records_info(cur_db[ a ],a);
		cur_rec[ a ] = 0;
		cur_off[ a ] = 0;
		cur_pnt[ a ] = 0;
		if( isBusy( cur_db[ a ], 0 ) ) DmReleaseRecord( cur_db[ a ], 0, 0 );
		cur_rec_h[ a ] = DmGetRecord( cur_db[ a ], 0 );
		cur_rec_pnt[ a ] = (uchar*)MemHandleLock( cur_rec_h[ a ] );
		
		return (FILE*)( a + 1 );
	    }
	}
    }
    return 0;
}

int fclose( FILE *fpp )
{
    ulong f = (ulong)fpp;
    if( f != 0 )
    {
	f--;
	if( fp[ f ] == 1 ) 
	{
	    if( vfs_file[ f ] == 0 )
	    { //PDB:
		fp[ f ] = 0; 
		if( cur_rec_h[ f ] != 0 ) MemHandleUnlock( cur_rec_h[ f ] ); //close previous record
		if( write_flag[ f ] )
		{
		    DmResizeRecord( cur_db[ f ], cur_rec[ f ], cur_off[ f ] );
		}
		recordsBusyReset( f );
		DmCloseDatabase( cur_db[ f ] ); return 0; //file close
	    }
	    else
	    { //VFS:
		if( cur_off[ f ] && write_flag[ f ] )
		{ //if there is some unsaved data in a buffer:
		    buffer2vfs( f ); //save it
		}
		fp[ f ] = 0;
		mem_palm_normal_mode(); //#####
		VFSFileClose( vfs_file[ f ] );
		mem_palm_our_mode();    //#####
		
		//Close buffer:
		if( cur_rec_h[ f ] != 0 ) MemHandleUnlock( cur_rec_h[ f ] ); //close previous record
		recordsBusyReset( f );
		DmCloseDatabase( cur_db[ f ] ); //file close

		//Delete buffer:
		char temp_filename[ 3 ];
		temp_filename[ 0 ] = (char)f + 65;
		temp_filename[ 1 ] = 'F';
		temp_filename[ 2 ] = 0;
		ID = DmFindDatabase( 0, (const char*)temp_filename );
		if( ID != 0 ) DmDeleteDatabase( 0, ID ); //File exist: delete it
		return 0;
	    }
	}
    }
    return 1; //error value
}

void buffer2vfs( ulong f )
{
    dprint( "saving block to VFS\n" );
    ulong bytes_written;
    if( cur_off[ f ] == 0 ) return;
    mem_palm_normal_mode(); //#####
    Err err = VFSFileWrite( vfs_file[ f ], cur_off[ f ], cur_rec_pnt[ f ], &bytes_written );
    mem_palm_our_mode();    //#####
    if( err != errNone ) { dprint( "error in buffer2vfs %d\n", err ); }
    cur_off[ f ] = 0;
}

void vfs2buffer( ulong f )
{
    dprint( "geting block from VFS\n" );
    ulong bytes_read;
    long size = f_size[ f ] - cur_pnt[ f ];
    if( size > r_size[ f ][ 0 ] ) size = r_size[ f ][ 0 ];
    if( size <= 0 ) return;
    mem_palm_normal_mode(); //#####
    Err err = VFSFileReadData( vfs_file[ f ], size, cur_rec_pnt[ f ], 0, &bytes_read );
    r_size[ f ][ 0 ] = bytes_read;
    mem_palm_our_mode();    //#####
    if( err != errNone ) { dprint( "error in vfs2buffer %d\n", err ); }
    vfs_first_read[ f ] = 1;
}

int next_record( ulong f )
{ //go to the next record 
    if( vfs_file[ f ] )
    { //VFS file (working with a buffer):
	cur_off[ f ] = 0;
	if( !write_flag[ f ] )
	{ //Read next block from VFS to our buffer:
	    vfs2buffer( f );
	}
    }
    else
    { //PDB file:
	cur_off[ f ] = 0;
	cur_rec[ f ] ++;
	if( cur_rec_h[ f ] != 0 ) 
	    { MemHandleUnlock( cur_rec_h[ f ] ); cur_rec_h[ f ] = 0; } //close previous
	if( cur_rec[ f ] < recs[ f ] )
	{
    	    if( isBusy( cur_db[ f ], cur_rec[ f ] ) ) 
		DmReleaseRecord( cur_db[ f ], cur_rec[ f ], 0 );
    	    cur_rec_h[ f ] = DmGetRecord( cur_db[ f ], cur_rec[ f ] );
	    cur_rec_pnt[ f ] = (uchar*)MemHandleLock( cur_rec_h[ f ] );
	}
	else
	{
	    if( write_flag[ f ] )
	    { //create new record:
		create_new_record( cur_db[ f ] );
		recs[ f ]++;
		r_size[ f ][ recs[ f ] - 1 ] = 50000;
    		if( isBusy( cur_db[ f ],cur_rec[ f ] ) ) 
		    DmReleaseRecord( cur_db[ f ], cur_rec[ f ], 0 );
    		cur_rec_h[ f ] = DmGetRecord( cur_db[ f ], cur_rec[ f ] );
		cur_rec_pnt[ f ] = (uchar*)MemHandleLock( cur_rec_h[ f ] );
	    }
	}
    }
    return 0;
}

void rewind( FILE *fpp )
{
    ulong f = (ulong)fpp;
    if( f != 0 )
    {
    	f--;
	cur_off[ f ] = 0;
	cur_rec[ f ] = 0;
	cur_pnt[ f ] = 0;
	if( vfs_file[ f ] )
	{
	    Err err = VFSFileSeek( vfs_file[ f ], vfsOriginBeginning, 0 );
	}
	else
	{
	    if( cur_rec_h[ f ] != 0 ) 
		{ MemHandleUnlock( cur_rec_h[ f ] ); cur_rec_h[ f ] = 0; } //close previous
	    //Open new record:
    	    if( isBusy( cur_db[ f ], cur_rec[ f ] ) ) 
		DmReleaseRecord( cur_db[ f ], cur_rec[ f ], 0 );
    	    cur_rec_h[ f ] = DmGetRecord( cur_db[ f ], cur_rec[ f ] );
	    cur_rec_pnt[ f ] = (uchar*)MemHandleLock( cur_rec_h[ f ] );
	}
    }
}

int getc( FILE *fpp )
{
    MemHandle cur_h;
    uchar *cur;
    uchar res;
    ulong bytes_read;
    ulong f = (ulong)fpp;
    if( f != 0 )
    {
    	f--;
	if( vfs_file[ f ] )
	{
	    if( vfs_first_read[ f ] == 0 ) vfs2buffer( f ); //Fill buffer for first time
	}
	if( cur_pnt[ f ] < f_size[ f ] )
	{
	    cur = cur_rec_pnt[ f ];
	    res = cur[cur_off[ f ]];
	    //inc position:
	    cur_pnt[ f ]++;
	    cur_off[ f ]++; 
	    if( cur_off[ f ] >= r_size[ f ][cur_rec[ f ] ] ) 
	    {
		//if need for next record:
	        next_record(f);
	    }
	    return res;
	} else /*out of file space*/ return -1;
    }
    return -1;
}

ulong get_record_rest( FILE *fpp )
{ //get rest space (from current position to the end of current record)
    ulong f = (ulong)fpp;
    if( f != 0 )
    {
	f--;
	if( cur_pnt[ f ] < f_size[ f ] || write_flag[ f ] )
	{
	    return r_size[ f ][ cur_rec[ f ] ] - cur_off[ f ];
	}
    }
    return 0; //out of file space
}

int ftell ( FILE *fpp )
{
    ulong f = (ulong)fpp;
    if( f != 0 )
    {
	f--;
	return cur_pnt[ f ];
    }
    return -1;
}

uint16 fseek( FILE *fpp, long offset, long access )
{ //seek forward (file position += offset)
    ulong f = (ulong)fpp;
    ulong rest, off = 0;
    if( f != 0 )
    {
	f--;
	if( vfs_file[ f ] == 0 )
	{ //PDB:
	    if( access == 1 )
	    if( cur_pnt[ f ] < f_size[ f ] || write_flag[ f ] )
	    {
		if( offset < 0 )
		{ //Seek back:
		    off = cur_pnt[ f ] + offset;
		    if( off >= 0 )
		    {
			rewind( fpp );
			fseek( fpp, off, 1 );
		    }
		}
		else //Seek forward:	    
		if( offset <= get_record_rest( fpp ) ) 
		{ //offset <= rest record space
		    cur_pnt[ f ] += offset;
		    cur_off[ f ] += offset;
		    if( cur_off[ f ] >= r_size[ f ][ cur_rec[ f ] ] ) next_record( f );
		    return offset;
		} 
		else 
		{ //offset is large (few records)
		    for(;;)
		    {
			rest = get_record_rest( fpp );
			if( offset >= rest ) offset -= rest; else { rest = offset; offset = 0; }
			cur_pnt[ f ] += rest; off += rest; 
			if( cur_pnt[ f ] >= f_size[ f ] && !write_flag[ f ] ) 
			    return off; //out of file space
		        if( offset == 0 ) 
			    { cur_off[ f ] = rest; return off; } //seekf is ok!
		        next_record( f );
		    }
		}
	    }
	}
	else
	{ //VFS:
	    long old_cur_pnt = cur_pnt[ f ];
	    FileOrigin origin;
	    if( access == 0 ) { origin = vfsOriginBeginning; cur_pnt[ f ] = offset; }
	    if( access == 1 ) { origin = vfsOriginCurrent; cur_pnt[ f ] += offset; }
	    if( access == 2 ) { origin = vfsOriginEnd; cur_pnt[ f ] = f_size[ f ] + offset; }
	    long delta = cur_pnt[ f ] - old_cur_pnt;
	    cur_off[ f ] += delta;
	    if( cur_off[ f ] >= r_size[ f ][ 0 ] || cur_off[ f ] < 0 )
	    { //We needs new block from VFS:
		mem_palm_normal_mode(); //#####
		Err err = VFSFileSeek( vfs_file[ f ], vfsOriginBeginning, cur_pnt[ f ] );
		mem_palm_our_mode();    //#####
		if( err == errNone )
		{
		    cur_off[ f ] = 0;
		    vfs2buffer( f );		
		    return 1;
		}
	    }
	    else return 1;
	}
    }
    return 0;
}

int feof( FILE *fpp )
{
    ulong f = (ulong)fpp;
    if( f != 0 )
    {
	f--;
	if( cur_pnt[ f ] >= f_size[ f ] ) return 1;
    }
    return 0;
}

ulong fread( void *ptr, ulong el_size, ulong elements, FILE *fpp )
{
    ulong real_size = el_size * elements;
    ulong size; //current block size
    uchar *cur;
    uchar res;
    ulong f = (ulong)fpp;
    ulong rest, off = 0;
    if( f != 0 )
    {
	f--;
	if( vfs_file[ f ] )
	{
	    if( vfs_first_read[ f ] == 0 ) vfs2buffer( f ); //Fill buffer for first time
	}
	if( write_flag[ f ] ) return 0;
	if( get_record_rest( fpp ) >= real_size )
	{ //we need for small block only:
	    MemMove( ptr, cur_rec_pnt[ f ] + cur_off[ f ], real_size );
	    fseek( fpp, real_size, 1 );
	    return real_size; //all is ok
	} 
	else
	{ //we need for large block (more than one record)
	    for(;;)
	    {
	        rest = get_record_rest( fpp );
	        if( real_size >= rest ) real_size -= rest; else { rest = real_size; real_size = 0; }
		if( rest != 0 ) MemMove( (uchar*)ptr + off, cur_rec_pnt[ f ] + cur_off[ f ], rest );
		off += rest;
		fseek( fpp, rest, 1 );
		if( cur_pnt[ f ] >= f_size[ f ] ) return off; //out of file space
		if( real_size == 0 ) return off; //fread is ok!
	    }
	}
    }
    return 0;
}

ulong fwrite( void *ptr, ulong el_size, ulong elements, FILE *fpp )
{
    ulong real_size = el_size * elements;
    ulong size; //current block size
    uchar *cur;
    uchar res;
    ulong f = (ulong)fpp;
    ulong rest, off = 0;
    if( f != 0 )
    {
	f--;	
	if( write_flag[ f ] == 0 ) return 0;
	if( get_record_rest( fpp ) >= real_size ) 
	{//we need to write small block only:
	    DmWrite( cur_rec_pnt[ f ], cur_off[ f ], ptr, real_size );
	    if( vfs_file[ f ] )
	    {
		cur_off[ f ] += real_size; cur_pnt[ f ] += real_size;
		if( cur_off[ f ] >= r_size[ f ][ 0 ] ) //if we must to save current buffer
		    buffer2vfs( f );
	    }
	    else fseek( fpp, real_size, 1 );
	    return real_size; //all is ok
	} 
	else
	{//we need to write large block (more than one record)
	    for(;;)
	    {
	        rest = get_record_rest(fpp);
	        if( real_size >= rest ) 
		    real_size -= rest; 
		else 
		{ 
		    rest = real_size; 
		    real_size = 0; 
		}
		if( rest != 0 ) DmWrite( cur_rec_pnt[ f ], cur_off[ f ], (uchar*)ptr + off, rest );
		off += rest;
		if( vfs_file[ f ] )
		{
		    cur_off[ f ] += rest; cur_pnt[ f ] += rest;
		    if( cur_off[ f ] >= r_size[ f ][ 0 ] ) //if we must to save current buffer
		        buffer2vfs( f );
		}
		else fseek( fpp, rest, 1 );
		if( real_size == 0 ) return off; //fwrite is ok!
	    }
	}
    }
    return 0;
}

void fputc( int val, FILE *fpp )
{
    uchar v = (uchar)val;
    fwrite( &v, 1, 1, fpp );
}

ulong remove( const char *filename )
{
    if( ( filename[ 0 ] == 'A' && filename[ 1 ] == ':' ) || filename[ 1 ] != ':' )
    { //It's PalmOS PDB:
        if( filename[ 0 ] == 'A' && filename[ 1 ] == ':' )
	    filename += 3; //Skip "A:/"
	ID = DmFindDatabase( 0, (const char*)filename );
	if( ID != 0 ) DmDeleteDatabase( 0, ID ); //File exist: delete it
	return 0;
    }
    else
    { //It's VFS:
        get_disks();
        if( filename[ 0 ] - 65 >= disks ) return 1; //no such disk
        int disk_num = filename[ 0 ] - 66;
        filename += 2; //Skip "X:"
        if( open_vfs() ) 
	{
	    //Delete file:
	    mem_palm_normal_mode(); //#####
	    VFSFileDelete( vfs_volume_numbers[ disk_num ], filename );
	    mem_palm_our_mode();    //#####
	    return 0;
	}
    }
    return 1;
}

#endif

//***********************************************************
//Main multiplatform functions:******************************
//***********************************************************

V3_FILE g_virt_disk0 = 0; //Virtual disk "0:" - local TAR file

V3_FILE_STRUCT *v3_fd[ MAX_V3_DESCRIPTORS ] = { 0, 0, 0, 0, 0, 0, 0, 0 };

V3_FILE v3_open_in_memory( void *data, int size )
{
    int a;
    for( a = 0; a < MAX_V3_DESCRIPTORS; a++ ) if( v3_fd[ a ] == 0 ) break;
    if( a == MAX_V3_DESCRIPTORS ) return 0; //No free descriptors

    //Create new descriptor:
    v3_fd[ a ] = (V3_FILE_STRUCT*)MEM_NEW( HEAP_DYNAMIC, sizeof( V3_FILE_STRUCT ) );
    mem_set( v3_fd[ a ], sizeof( V3_FILE_STRUCT ), 0 );

    v3_fd[ a ]->type = V3_FILE_IN_MEMORY;
    v3_fd[ a ]->virt_file_data = (char*)data;
    v3_fd[ a ]->virt_file_size = size;

    return a + 1;
}

V3_FILE v3_open( const UTF8_CHAR *filename, const UTF8_CHAR *filemode )
{
    int a;
    for( a = 0; a < MAX_V3_DESCRIPTORS; a++ ) if( v3_fd[ a ] == 0 ) break;
    if( a == MAX_V3_DESCRIPTORS ) return 0; //No free descriptors
    
    //Create new descriptor:
    v3_fd[ a ] = (V3_FILE_STRUCT*)MEM_NEW( HEAP_DYNAMIC, sizeof( V3_FILE_STRUCT ) );
    mem_set( v3_fd[ a ], sizeof( V3_FILE_STRUCT ), 0 );

    if( filename && mem_strlen( filename ) > 3 && filename[ 0 ] == '0' && filename[ 1 ] == ':' && filename[ 2 ] == '/' )
    {
	//Virtual disk selected:
	v3_fd[ a ]->type = V3_FILE_ON_VIRTUAL_DISK;
	v3_fd[ a ]->filename = (UTF8_CHAR*)MEM_NEW( HEAP_DYNAMIC, ( mem_strlen( filename ) + 1 ) - 3 );
	v3_fd[ a ]->filename[ 0 ] = 0;
	mem_strcat( v3_fd[ a ]->filename, filename + 3 );
	int not_found = 0;
	if( g_virt_disk0 == 0 ) not_found = 1;
	if( g_virt_disk0 )
	{
	    v3_rewind( g_virt_disk0 );
	    UTF8_CHAR next_file[ 100 ];
	    UTF8_CHAR temp[ 8 * 3 ];
	    not_found = 1;
	    while( 1 )
	    {
		if( v3_read( next_file, 1, 100, g_virt_disk0 ) != 100 ) break;
		v3_read( temp, 1, 8 * 3, g_virt_disk0 );
		v3_read( temp, 1, 12, g_virt_disk0 );
		temp[ 12 ] = 0;
		int filelen = 0;
		for( int i = 0; i < 12; i++ )
		{
		    //Convert from OCT string to integer:
		    if( temp[ i ] >= '0' && temp[ i ] <= '9' ) 
		    {
			filelen *= 8;
			filelen += temp[ i ] - '0';
		    }
		}
		v3_seek( g_virt_disk0, 376, 1 );
		if( mem_strcmp( next_file, v3_fd[ a ]->filename ) == 0 )
		{
		    //File found:
		    v3_fd[ a ]->virt_file_data = (char*)MEM_NEW( HEAP_DYNAMIC, filelen );
		    v3_read( v3_fd[ a ]->virt_file_data, 1, filelen, g_virt_disk0 );
		    v3_fd[ a ]->virt_file_size = filelen;
		    not_found = 0;
		    break;
		}
		else
		{
		    if( filelen & 511 )
			filelen += 512 - ( filelen & 511 );
		    v3_seek( g_virt_disk0, filelen, 1 );
		}
	    }
	}
	if( not_found )
	{
	    //Virtual disk 0 not found:
	    v3_close( a + 1 );
	    a = -1;
	}
    }
    else
    {
	//Standart file:
	v3_fd[ a ]->filename = (UTF8_CHAR*)MEM_NEW( HEAP_DYNAMIC, mem_strlen( filename ) + 1 );
	v3_fd[ a ]->filename[ 0 ] = 0;
	mem_strcat( v3_fd[ a ]->filename, filename );
	//Open it:
#if defined(PALMOS) || defined(UNIX)
	v3_fd[ a ]->f = (unsigned long)fopen( filename, filemode );
#endif
#if defined(WIN) || defined(WINCE)
	UTF16_CHAR *ts = utf8_to_utf16( 0, 0, filename );
	utf16_unix_slash_to_windows( ts );	
	v3_fd[ a ]->f = (unsigned long)_wfopen( (const wchar_t*)ts, (const wchar_t*)utf8_to_utf16( 0, 0, filemode ) );
#endif
	if( v3_fd[ a ]->f == 0 )
	{
	    //File not found:
	    v3_close( a + 1 );
	    a = -1;
	}
    }

    return a + 1;
}

int v3_close( V3_FILE f )
{
    int retval = 0;
    f--;
    if( f >= 0 && f < MAX_V3_DESCRIPTORS && v3_fd[ f ] )
    {
	if( v3_fd[ f ]->filename ) mem_free( v3_fd[ f ]->filename );
	if( v3_fd[ f ]->f )
	{
	    //Close standart file:
	    retval = (int)fclose( (FILE*)v3_fd[ f ]->f );
	}
	if( v3_fd[ f ]->virt_file_data && v3_fd[ f ]->type == V3_FILE_ON_VIRTUAL_DISK ) 
	    mem_free( v3_fd[ f ]->virt_file_data );
	mem_free( v3_fd[ f ] );
	v3_fd[ f ] = 0;
    }
    return retval;
}

void v3_rewind( V3_FILE f )
{
    f--;
    if( f >= 0 && f < MAX_V3_DESCRIPTORS && v3_fd[ f ] )
    {
	if( v3_fd[ f ]->f && v3_fd[ f ]->type == V3_FILE_STD )
	{
	    //Standart file:
	    fseek( (FILE*)v3_fd[ f ]->f, 0, 0 );
	}
	else
	if( v3_fd[ f ]->type >= V3_FILE_ON_VIRTUAL_DISK )
	{
	    //Virtual disk 0:
	    v3_fd[ f ]->virt_file_ptr = 0;
	}
    }
}

int v3_getc( V3_FILE f )
{
    int retval = 0;
    f--;
    if( f >= 0 && f < MAX_V3_DESCRIPTORS && v3_fd[ f ] )
    {
	if( v3_fd[ f ]->f && v3_fd[ f ]->type == V3_FILE_STD )
	{
	    //Standart file:
	    retval = getc( (FILE*)v3_fd[ f ]->f );
	}
	else
	if( v3_fd[ f ]->type >= V3_FILE_ON_VIRTUAL_DISK )
	{
	    //Virtual disk 0:
	    if( v3_fd[ f ]->virt_file_ptr < v3_fd[ f ]->virt_file_size )
	    {
		retval = (int)( (uchar)v3_fd[ f ]->virt_file_data[ v3_fd[ f ]->virt_file_ptr ] );
		v3_fd[ f ]->virt_file_ptr++;
	    }
	    else
	    {
		retval = -1;
	    }
	}
    }
    return retval;
}

ulong v3_tell ( V3_FILE f )
{
    ulong retval = 0;
    f--;
    if( f >= 0 && f < MAX_V3_DESCRIPTORS && v3_fd[ f ] )
    {
	if( v3_fd[ f ]->f && v3_fd[ f ]->type == V3_FILE_STD )
	{
	    //Standart file:
	    retval = ftell( (FILE*)v3_fd[ f ]->f );
	}
	else
	if( v3_fd[ f ]->type >= V3_FILE_ON_VIRTUAL_DISK )
	{
	    //Virtual disk 0:
	    retval = v3_fd[ f ]->virt_file_ptr;
	}
    }
    return retval;
}

int v3_seek( V3_FILE f, long offset, long access )
{
    int retval = 0;
    f--;
    if( f >= 0 && f < MAX_V3_DESCRIPTORS && v3_fd[ f ] )
    {
	if( v3_fd[ f ]->f && v3_fd[ f ]->type == V3_FILE_STD )
	{
	    //Standart file:
	    retval = fseek( (FILE*)v3_fd[ f ]->f, offset, access );
	}
	else
	if( v3_fd[ f ]->type >= V3_FILE_ON_VIRTUAL_DISK )
	{
	    //Virtual disk 0:
	    if( access == 0 ) v3_fd[ f ]->virt_file_ptr = offset;
	    if( access == 1 ) v3_fd[ f ]->virt_file_ptr += offset;
	    if( access == 2 ) v3_fd[ f ]->virt_file_ptr = v3_fd[ f ]->virt_file_size + offset;
	}
    }
    return retval;
}

int v3_eof( V3_FILE f )
{
    int retval = 0;
    f--;
    if( f >= 0 && f < MAX_V3_DESCRIPTORS && v3_fd[ f ] )
    {
	if( v3_fd[ f ]->f && v3_fd[ f ]->type == V3_FILE_STD )
	{
	    //Standart file:
	    retval = feof( (FILE*)v3_fd[ f ]->f );
	}
	else
	if( v3_fd[ f ]->type >= V3_FILE_ON_VIRTUAL_DISK )
	{
	    //Virtual disk 0:
	    if( v3_fd[ f ]->virt_file_ptr >= v3_fd[ f ]->virt_file_size ) retval = 1;
	}
    }
    return retval;
}

ulong v3_read( void *ptr, ulong el_size, ulong elements, V3_FILE f )
{
    ulong retval = 0;
    f--;
    if( f >= 0 && f < MAX_V3_DESCRIPTORS && v3_fd[ f ] )
    {
	if( v3_fd[ f ]->f && v3_fd[ f ]->type == V3_FILE_STD )
	{
	    //Standart file:
	    retval = fread( ptr, el_size, elements, (FILE*)v3_fd[ f ]->f );
	}
	else
	if( v3_fd[ f ]->type >= V3_FILE_ON_VIRTUAL_DISK )
	{
	    //Virtual disk 0:
	    int size = el_size * elements;
	    if( v3_fd[ f ]->virt_file_ptr + size > v3_fd[ f ]->virt_file_size )
		size = v3_fd[ f ]->virt_file_size - v3_fd[ f ]->virt_file_ptr;
	    if( size < 0 ) size = 0;
	    if( size > 0 )
		mem_copy( ptr, v3_fd[ f ]->virt_file_data + v3_fd[ f ]->virt_file_ptr, size );
	    v3_fd[ f ]->virt_file_ptr += size;
	    retval = size;
	}
    }
    return retval;
}

ulong v3_write( void *ptr, ulong el_size, ulong elements, V3_FILE f )
{
    ulong retval = 0;
    f--;
    if( f >= 0 && f < MAX_V3_DESCRIPTORS && v3_fd[ f ] )
    {
	if( v3_fd[ f ]->f && v3_fd[ f ]->type == V3_FILE_STD )
	{
	    //Standart file:
	    retval = fwrite( ptr, el_size, elements, (FILE*)v3_fd[ f ]->f );
	}
	else
	if( v3_fd[ f ]->type >= V3_FILE_ON_VIRTUAL_DISK )
	{
	    //Virtual disk 0:
	    int size = el_size * elements;
	    if( v3_fd[ f ]->virt_file_ptr + size > v3_fd[ f ]->virt_file_size )
		size = v3_fd[ f ]->virt_file_size - v3_fd[ f ]->virt_file_ptr;
	    if( size < 0 ) size = 0;
	    if( size > 0 )
		mem_copy( v3_fd[ f ]->virt_file_data + v3_fd[ f ]->virt_file_ptr, ptr, size );
	    v3_fd[ f ]->virt_file_ptr += size;
	    retval = size;
	}
    }
    return retval;
}

void v3_putc( int val, V3_FILE f )
{
    f--;
    if( f >= 0 && f < MAX_V3_DESCRIPTORS && v3_fd[ f ] )
    {
	if( v3_fd[ f ]->f && v3_fd[ f ]->type == V3_FILE_STD )
	{
	    //Standart file:
	    fputc( val, (FILE*)v3_fd[ f ]->f );
	}
	else
	if( v3_fd[ f ]->type >= V3_FILE_ON_VIRTUAL_DISK )
	{
	    //Virtual disk 0:
	    if( v3_fd[ f ]->virt_file_ptr < v3_fd[ f ]->virt_file_size )
	    {
		v3_fd[ f ]->virt_file_data[ v3_fd[ f ]->virt_file_ptr ] = (char)val;
		v3_fd[ f ]->virt_file_ptr++;
	    }
	}
    }
}

ulong v3_remove( const UTF8_CHAR *filename )
{
    ulong retval = 0;
#if defined(PALMOS) || defined(UNIX)
    retval = remove( filename );
#endif
#if defined(WIN) || defined(WINCE)
    UTF16_CHAR *ts = utf8_to_utf16( 0, 0, filename );
    utf16_unix_slash_to_windows( ts );
    DeleteFileW( (const WCHAR*)ts );
#endif
    return retval;
}

ulong v3_get_file_size( const UTF8_CHAR *filename )
{
    ulong retval = 0;
    V3_FILE f = v3_open( filename, "rb" );
    if( f )
    {
	v3_seek( f, 0, 2 );
	retval = v3_tell( f );
	v3_close( f );
    }
    return retval;
}

//***********************************************************
//***********************************************************
//***********************************************************

//Functions for files find:

#ifdef PALMOS
    #define strlen(str) StrLen(str)
#endif

int check_file( UTF8_CHAR *our_file, find_struct *fs )
{
    int p;
    int mp; //mask pointer
    int equal = 0;
    int str_len;
    
    if( fs->mask ) mp = strlen( fs->mask ) - 1; else return 1;
    str_len = strlen( our_file );
    p = str_len - 1;
    
    for( ; p >= 0 ; p--, mp-- ) 
    {
	if( our_file[ p ] == '.' ) 
	{
	    if( equal ) return 1; //It is our file!
	    else 
	    {
		for(;;) 
		{ //Is there other file types (in mask[]) ?:
		    if( fs->mask[ mp ] == '/' ) break; //There is other type
		    mp--;
		    if( mp < 0 ) return 0; //no... it was the last type in mask[]
		}
	    }
	}
	if( mp < 0 ) return 0;
	if( fs->mask[ mp ] == '/' ) { mp--; p = str_len - 1; }
	UTF8_CHAR c = our_file[ p ];
	if( c >= 65 && c <= 90 ) c += 0x20; //Make small leters
	if( c != fs->mask[ mp ] ) 
	{
	    for(;;) 
	    { //Is there other file types (in mask[]) ?:
	        if( fs->mask[ mp ] == '/' ) { p = str_len; mp++; break; } //There is other type
	        mp--;
	        if( mp < 0 ) return 0; //no... it was the last type in mask[]
	    }
	}
	else equal = 1;
    }
    
    return 0;
}

#ifdef PALMOS
//PalmOS:

int find_first( find_struct *fs )
{
    Err error;
    if( fs->start_dir[ 0 ] >= 'B' ) 
    { //Flash-card search:
	if( open_vfs() ) 
	{
	    mem_palm_normal_mode(); //#####
	    error = VFSFileOpen( vfs_volume_numbers[ fs->start_dir[ 0 ] - 66 ], fs->start_dir + 2, vfsModeRead, &fs->dir_ref );
	    mem_palm_our_mode();    //#####
	    if( error != errNone ) 
	    {
		return 0; //Dir not found
	    }
	    fs->dir_iterator = vfsIteratorStart;
	    //File_info init:
	    fs->file_info.nameP = fs->name; 
	    fs->file_info.nameBufLen = MAX_DIR_LEN;
	    mem_palm_normal_mode(); //#####
	    error = VFSDirEntryEnumerate( fs->dir_ref, &fs->dir_iterator, &fs->file_info );
	    mem_palm_our_mode();    //#####
	    if( error != errNone ) 
	    {
		mem_palm_normal_mode(); //#####
		VFSFileClose( fs->dir_ref );
		mem_palm_our_mode();    //#####
		return 0;
	    }
	    if( fs->file_info.attributes & vfsFileAttrDirectory ) fs->type = TYPE_DIR;
	    else { //If it is a file:
		fs->type = TYPE_FILE;
		if( !check_file( fs->name, fs ) ) return find_next(fs);
	    }
	} else return 0;
    } else { //PDB  search:
	if( DmGetNextDatabaseByTypeCreator( 1, &fs->search_info, 'PSYX', 'ZULU', 0, &fs->card_id, &fs->db_id ) == dmErrCantFind ) 
	    return 0; //no files
	DmDatabaseInfo( fs->card_id, fs->db_id, fs->name, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ); //Get database name
	fs->type = TYPE_FILE;
	if( !check_file( fs->name, fs ) ) return find_next(fs);
    }
    return 1;
}

int find_next( find_struct *fs )
{
    Err error;
    if( fs->start_dir[ 0 ] >= 'B' ) 
    { //Flash-card search:
	for(;;) 
	{
	    mem_palm_normal_mode(); //#####
	    error = VFSDirEntryEnumerate( fs->dir_ref, &fs->dir_iterator, &fs->file_info );
	    mem_palm_our_mode();    //#####
	    if( error != errNone ) 
	    {
		mem_palm_normal_mode(); //#####
		VFSFileClose( fs->dir_ref );
		mem_palm_our_mode();    //#####
		return 0; //no more files
	    }
	    if( fs->file_info.attributes & vfsFileAttrDirectory ) { fs->type = TYPE_DIR; return 1; } //Dir founded
	    else { //If it is a file:
		fs->type = TYPE_FILE;
		if( check_file( fs->name, fs ) ) return 1;
	    }
	}
    } else { //PDB  search:
	for(;;) 
	{
	    if( DmGetNextDatabaseByTypeCreator( 0, &fs->search_info, 'PSYX', 'ZULU', 0, &fs->card_id, &fs->db_id ) == dmErrCantFind ) 
		return 0; //no more files
	    DmDatabaseInfo( fs->card_id, fs->db_id, fs->name, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ); //Get database name
	    fs->type = TYPE_FILE;
	    if( check_file( fs->name, fs ) ) return 1;
	}
    }
    return 1;
}

void find_close( find_struct *fs )
{
}

#endif

#if defined(WIN) || defined(WINCE)
//Windows:

int find_first( find_struct *fs )
{
    int wp = 0, p = 0;
    fs->win_mask[ 0 ] = 0;
    fs->win_start_dir[ 0 ] = 0;
    
    //convert start dir from "dir/dir/" to "dir\dir\*.*"
    strcat( fs->win_start_dir, fs->start_dir );
    strcat( fs->win_start_dir, "*.*" );
    for( wp = 0; ; wp++ ) 
    {
	if( fs->win_start_dir[ wp ] == 0 ) break;
	if( fs->win_start_dir[ wp ] == '/' ) fs->win_start_dir[ wp ] = 92;
    }
    
    //do it:
#if WINCE
    fs->find_handle = FindFirstFile( (const WCHAR*)utf8_to_utf16( 0, 0, fs->win_start_dir ), &fs->find_data );
#endif
#ifdef WIN
    fs->find_handle = FindFirstFileW( (const WCHAR*)utf8_to_utf16( 0, 0, fs->win_start_dir ), &fs->find_data );
#endif
    if( fs->find_handle == INVALID_HANDLE_VALUE ) return 0; //no files found :(
    
    //save filename:
    fs->name[ 0 ] = 0;
    strcat( fs->name, utf16_to_utf8( 0, 0, (const UTF16_CHAR*)fs->find_data.cFileName ) );
    if( fs->find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) 
    {
	//Dir:
	fs->type = TYPE_DIR; 
    }
    else 
    { 
	//File:
	fs->type = TYPE_FILE;
	if( !check_file( fs->name, fs ) ) return find_next( fs );
    }
    
    return 1;
}

int find_next( find_struct *fs )
{
    for(;;) 
    {
	if( !FindNextFileW( fs->find_handle, &fs->find_data ) ) return 0; //files not found

	//save filename:
        fs->name[ 0 ] = 0;
	strcat( fs->name, utf16_to_utf8( 0, 0, (const UTF16_CHAR*)fs->find_data.cFileName ) );
	if( fs->find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) 
	{ 
	    //Dir:
	    fs->type = TYPE_DIR;
	    return 1; 
	} 
	else 
	{ 
	    //File found. 
	    fs->type = TYPE_FILE;
	    //Is it our file?
	    if( check_file( fs->name, fs ) ) 
		return 1;
	}
    }

    return 1;
}

void find_close( find_struct *fs )
{
    FindClose( fs->find_handle );
}

#endif

#ifdef UNIX
//UNIX:

int find_first( find_struct *fs )
{
    DIR *test;
    UTF8_CHAR test_dir[ MAX_DIR_LEN ];
    
    //convert start dir to unix standart:
    fs->new_start_dir[ 0 ] = 0;
    if( fs->start_dir[ 0 ] == 0 ) 
	strcat( fs->new_start_dir, "./" ); 
    else 
	strcat( fs->new_start_dir, fs->start_dir );
    
    //open dir and read first entry:
    fs->dir = opendir( fs->new_start_dir );
    if( fs->dir == 0 ) return 0; //no such dir :(
    fs->current_file = readdir( fs->dir );
    if( !fs->current_file ) return 0; //no files
    
    //copy file name:
    fs->name[ 0 ] = 0;
    strcpy( fs->name, fs->current_file->d_name );
    
    //is it a dir?
    test_dir[ 0 ] = 0;
    strcat( test_dir, fs->new_start_dir );
    strcat( test_dir, fs->current_file->d_name );
    test = opendir( test_dir );
    if( test ) fs->type = TYPE_DIR; else fs->type = TYPE_FILE;
    closedir( test );
    if( fs->current_file->d_name[ 0 ] == '.' ) fs->type = TYPE_DIR;
    
    if( fs->type == TYPE_FILE )
	if( !check_file( fs->name, fs ) ) return find_next( fs );
	
    return 1;
}

int find_next( find_struct *fs )
{
    DIR *test;
    UTF8_CHAR test_dir[ MAX_DIR_LEN ];
    
    for(;;) 
    {
	//read next entry:
	if( fs->dir == 0 ) return 0; //directory not exist
	fs->current_file = readdir( fs->dir );
	if( !fs->current_file ) return 0; //no files
    
	//copy file name:
	fs->name[ 0 ] = 0;
	strcpy( fs->name, fs->current_file->d_name );
    
	//is it dir?
	test_dir[ 0 ] = 0;
	strcat( test_dir, fs->new_start_dir );
	strcat( test_dir, fs->current_file->d_name );
	test = opendir( test_dir );
	if( test ) fs->type = TYPE_DIR; else fs->type = TYPE_FILE;
	closedir( test );
	if( fs->current_file->d_name[ 0 ] == '.' ) fs->type = TYPE_DIR;
	
	if( fs->type == TYPE_FILE )
	{
	    if( check_file( fs->name, fs ) ) return 1;
	}
	else return 1; //Dir found
    }
}

void find_close( find_struct *fs )
{
    closedir( fs->dir );
}

#endif
