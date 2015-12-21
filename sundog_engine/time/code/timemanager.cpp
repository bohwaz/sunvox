/*
    timemanager.cpp. Time functions
    This file is part of the SunDog engine.
    Copyright (C) 2002 - 2009 Alex Zolotov <nightradio@gmail.com>
*/

#include "../timemanager.h"

#ifdef UNIX
    #include <time.h>
#endif
#ifdef OSX
    #include <mach/mach.h>	    //mach_absolute_time() ...
    #include <mach/mach_time.h>	    //mach_absolute_time() ...
#endif
#ifdef FREEBSD
     #include <sys/time.h>
#endif
#ifndef NONPALM
    #include <PalmOS.h>
#endif
#if defined(WIN) || defined(WINCE)
    #include <windows.h>
    #include <time.h>
#endif

ulong time_hours( void )
{
#ifdef UNIX
    //UNIX:
    time_t t;
    time( &t );
    return localtime( &t )->tm_hour;
#endif
#ifndef NONPALM
    //PALM:
    DateTimeType t;
    TimSecondsToDateTime( TimGetSeconds(), &t );
    return t.hour;
#endif
#ifdef WIN
    //WINDOWS:
    time_t t;
    time( &t );
    return localtime( &t )->tm_hour;
#endif
#ifdef WINCE
    //WINDOWS CE:
    SYSTEMTIME st;
    GetLocalTime( &st );
    return st.wHour;
#endif
}

ulong time_minutes( void )
{
#ifdef UNIX
    //UNIX:
    time_t t;
    time( &t );
    return localtime( &t )->tm_min;
#endif
#ifndef NONPALM
    //PALM:
    DateTimeType t;
    TimSecondsToDateTime( TimGetSeconds(), &t );
    return t.minute;
#endif
#ifdef WIN
    //WINDOWS:
    time_t t;
    time( &t );
    return localtime( &t )->tm_min;
#endif
#ifdef WINCE
    //WINDOWS CE:
    SYSTEMTIME st;
    GetLocalTime( &st );
    return st.wMinute;
#endif
}

ulong time_seconds( void )
{
#ifdef UNIX
    //UNIX:
    time_t t;
    time( &t );
    return localtime( &t )->tm_sec;
#endif
#ifndef NONPALM
    //PALM:
    DateTimeType t;
    TimSecondsToDateTime( TimGetSeconds(), &t );
    return t.second;
#endif
#ifdef WIN
    //WINDOWS:
    time_t t;
    time( &t );
    return localtime( &t )->tm_sec;
#endif
#ifdef WINCE
    //WINDOWS CE:
    SYSTEMTIME st;
    GetLocalTime( &st );
    return st.wSecond;
#endif
}

ticks_t time_ticks_per_second( void )
{
#ifdef UNIX
    //UNIX:
    return (ticks_t)1000;
#endif
#ifdef PALMOS
    //PALM:
    return (ticks_t)SysTicksPerSecond();
#endif
#if defined(WIN) || defined(WINCE)
    //WINDOWS:
    return (ticks_t)1000;
#endif
}

ticks_t time_ticks( void )
{
#ifdef UNIX
    #ifdef OSX
	//OSX:
	mach_timebase_info_data_t sTimebaseInfo;
	if( sTimebaseInfo.denom == 0 ) 
	{
	    mach_timebase_info( &sTimebaseInfo );
	}
	uint64_t t = mach_absolute_time();
	return (ticks_t)( ( t * sTimebaseInfo.numer / sTimebaseInfo.denom ) / 1000000 );
    #else
	//UNIX:
	timespec t;
	clock_gettime( CLOCK_REALTIME, &t );
	return (ticks_t)( t.tv_nsec / 1000000 ) + t.tv_sec * 1000;
    #endif
#endif
#ifdef PALMOS
    //PALM:
    return (ticks_t)TimGetTicks();
#endif
#if defined(WIN) || defined(WINCE)
    //WINDOWS:
    return (ticks_t)GetTickCount();
#endif
}

#ifdef HIRES_TIMER
#include <stdio.h>
ticks_t time_ticks_per_second_hires( void )
{
    return (ticks_t)50000;
}
ticks_t __attribute__ ((force_align_arg_pointer)) time_ticks_hires( void )
{
#ifdef LINUX
    timespec t;
    clock_gettime( CLOCK_PROCESS_CPUTIME_ID, &t );
    return (ticks_t)( t.tv_nsec / ( 1000000 / 50 ) ) + t.tv_sec * 50000;
#endif
#if defined(WIN) || defined(WINCE)
    unsigned long long ticks_per_second;
    unsigned long long tick;
    QueryPerformanceFrequency( (LARGE_INTEGER*)&ticks_per_second );
    QueryPerformanceCounter( (LARGE_INTEGER*)&tick );
    return (ticks_t)( tick / ( ticks_per_second / 50000 ) );
#endif
}
#endif
