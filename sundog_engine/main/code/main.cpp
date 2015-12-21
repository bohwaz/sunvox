/*
    main.cpp. SunDog Engine main()
    This file is part of the SunDog engine.
    Copyright (C) 2002 - 2009 Alex Zolotov <nightradio@gmail.com>
*/

#ifndef PALMOS
    #include <stdio.h>
#endif

#include "../user_code.h"

//################################
//## DEVICE VARIABLES:          ##
//################################

//PALMOS
#ifdef PALMOS
    #include <PalmOS.h>
    #define arm_startup __attribute__ ((section ("arm_startup")))
    #include "palm_functions.h"
#endif

//################################
//################################
//################################

//################################
//## APPLICATION MAIN:          ##
//################################

extern const UTF8_CHAR *user_window_name;
extern const UTF8_CHAR *user_profile_name;
extern const UTF8_CHAR *user_debug_log_file_name;
extern int user_window_xsize;
extern int user_window_ysize;
extern int user_window_flags;

window_manager wm;
int g_argc = 0;
UTF8_CHAR *g_argv[ 64 ];

#if defined(WIN) || defined(WINCE)
void make_arguments( UTF8_CHAR *cmd_line )
{
    //Make standart argc and argv[] from windows lpszCmdLine:
    g_argv[ 0 ] = (UTF8_CHAR*)"prog";
    g_argc = 1;
    if( cmd_line && cmd_line[ 0 ] != 0 )
    {
        int str_ptr = 0;
        int space = 1;
        while( cmd_line[ str_ptr ] != 0 )
        {
    	    if( cmd_line[ str_ptr ] != ' ' )
    	    {
	        if( space == 1 )
	        {
	    	    g_argv[ g_argc ] = &cmd_line[ str_ptr ];
		    g_argc++;
		}
		space = 0;
	    }
	    else
	    {
	        space = 1;
	    }
	    str_ptr++;
	}
    }
}
#endif

//********************************
//WIN32 MAIN *********************
#ifdef WIN
WCHAR g_cmd_line[ 2048 ];
UTF8_CHAR g_cmd_line_utf8[ 2048 ];
int APIENTRY WinMain( HINSTANCE hCurrentInst, HINSTANCE hPreviousInst, LPSTR lpszCmdLine, int nCmdShow )
{
    {
	wm.hCurrentInst = hCurrentInst;
	wm.hPreviousInst = hPreviousInst; 
	wm.lpszCmdLine = lpszCmdLine;
	wm.nCmdShow = nCmdShow;
	mbstowcs( g_cmd_line, lpszCmdLine, 2047 );
	utf16_to_utf8( g_cmd_line_utf8, 2048, (const UTF16_CHAR*)g_cmd_line );
	make_arguments( g_cmd_line_utf8 );
	int argc = g_argc;
	UTF8_CHAR **argv = g_argv;
#endif
//********************************
//********************************

//********************************
//WINCE MAIN *********************
#ifdef WINCE
UTF8_CHAR g_cmd_line_utf8[ 2048 ];
WCHAR g_window_name[ 256 ];
extern WCHAR *className; //defined in window manager (wm_wince.h)
int WINAPI WinMain( HINSTANCE hCurrentInst, HINSTANCE hPreviousInst, LPWSTR lpszCmdLine, int nCmdShow )
{
    {
	wm.hCurrentInst = hCurrentInst;
	wm.hPreviousInst = hPreviousInst;
	wm.lpszCmdLine = lpszCmdLine;
	wm.nCmdShow = nCmdShow;
	utf16_to_utf8( g_cmd_line_utf8, 2048, (UTF16_CHAR*)lpszCmdLine );
	make_arguments( g_cmd_line_utf8 );
	utf8_to_utf16( (UTF16_CHAR*)g_window_name, 256, user_window_name );
	HWND wnd = FindWindow( className, g_window_name );
	if( wnd )
	{
	    //Already opened:
	    SetForegroundWindow( wnd ); //Make it foreground
	    return 0;
	}
	int argc = g_argc;
	UTF8_CHAR **argv = g_argv;
#endif
//********************************
//********************************

//********************************
//PALMOS MAIN ********************
#ifdef PALMOS
long ARM_PalmOS_main( const void *emulStateP, void *userData, Call68KFuncType *call68KFuncP ) arm_startup;
long ARM_PalmOS_main( const void *emulStateP, void *userData, Call68KFuncType *call68KFuncP )
{
    {
	volatile void *oldGOT;
	volatile register void *gGOT asm ("r10");
	volatile ARM_INFO *arm_info = (ARM_INFO *)userData;
	oldGOT = (void*)gGOT;
	//gGOT = (void *)arm_info->GOT;
	volatile unsigned long newgot = (unsigned long)arm_info->GOT;
	__asm__ ( "mov r10, %0" : : "r" (newgot) );
	ownID = (unsigned short)arm_info->ID;
	g_form_handler = arm_info->FORMHANDLER; //g_form_handler defined in palm_functions.cpp
	g_new_screen_size = arm_info->new_screen_size;
	CALL_INIT
	dprint( "MAIN: ARM started\n" );
	int autooff_time = SysSetAutoOffTime( 0 );
	g_argc = 1;
	g_argv[ 0 ] = (UTF8_CHAR*)"prog";
	int argc = g_argc;
	UTF8_CHAR **argv = g_argv;
#endif //PALMOS
//********************************
//********************************

//********************************
//UNIX MAIN **********************
#ifdef UNIX
int main( int argc, char *argv[] )
{
    {
#endif
//********************************
//********************************

	int err_count = 0;

	debug_set_output_file( user_debug_log_file_name );
	debug_reset();

	dprint( "\n" );
	dprint( "\n" );
	dprint( "%s\n", SUNDOG_VERSION );
	dprint( "%s\n", SUNDOG_DATE );
	dprint( "\n" );
	dprint( "STARTING...\n" );
	dprint( "\n" );
	dprint( "\n" );

	get_disks();
	profile_new( 0 );
	profile_load( user_profile_name, 0 );
	const UTF8_CHAR *window_name = user_window_name;
	if( profile_get_str_value( KEY_WINDOWNAME, 0 ) ) 
	    window_name = (const UTF8_CHAR*)profile_get_str_value( KEY_WINDOWNAME, 0 );
	int flags = 0;
	if( profile_get_int_value( KEY_NOBORDER, 0 ) != -1 ) 
	    flags = WIN_INIT_FLAG_NOBORDER;
	if( profile_get_int_value( KEY_SCREENFLIP, 0 ) != -1 ) 
	    flags = WIN_INIT_FLAG_SCREENFLIP;
	if( win_init( window_name, user_window_xsize, user_window_ysize, user_window_flags | flags, argc, argv, &wm ) == 0 )
	{
	    int sound_stream_error = 0;
#if defined(PALMOS) || defined(WINCE)
	    int sound_mode = SOUND_MODE_INT16;
#else
	    int sound_mode = SOUND_MODE_FLOAT32;
#endif
#ifndef NOSOUND
	    if( profile_get_int_value( KEY_FREQ, 0 ) == -1 )
	    {
		sound_stream_error = sound_stream_init( sound_mode, 44100, 2 );
	    }
	    else
	    {
		int freq = profile_get_int_value( KEY_FREQ, 0 );
		if( freq < 44100 )
		{
		    dprint( "ERROR. Sampling frequency must be >= 44100\n" );
		    sound_stream_error = 1;
		}
#ifdef ONLY44100
		if( freq != 44100 ) 
		{
		    dprint( "ERROR. Sampling frequency must be 44100 for this device\n" );
		    sound_stream_error = 1;
		}
#endif
		sound_stream_error = sound_stream_init( sound_mode, freq, 2 );
	    }
	    if( !sound_stream_error )
		sound_stream_play();
	    else
		err_count++;
#endif
	    if( !sound_stream_error )
	    {
		user_init( &wm );
		while( 1 )
		{
		    sundog_event evt;
		    EVENT_LOOP_BEGIN( &evt, &wm );
		    if( EVENT_LOOP_END( &wm ) ) break;
		}
		user_close( &wm );
#ifndef NOSOUND
		sound_stream_close(); //Close sound stream
#endif
	    }
	    win_close( &wm ); //Close window manager
	}
	else
	{
	    err_count++;
	}
	profile_close( 0 );
	debug_close();
	if( mem_free_all() ) err_count++; //Fix memory leaks
	
	dprint( "\n" );
	dprint( "\n" );
	dprint( "BYE !\n" );
	dprint( "\n" );
	dprint( "\n" );

	if( err_count == 0 )
	    debug_reset(); //Remove log if no errors

#ifdef PALMOS
	SysSetAutoOffTime( autooff_time );
	//gGOT = (void*)oldGOT;
	newgot = (unsigned long)oldGOT;
	__asm__ ( "mov r10, %0" : : "r" (newgot) );
	return 1;
#endif
    }

    return wm.exit_code;
}

//################################
//################################
//################################
