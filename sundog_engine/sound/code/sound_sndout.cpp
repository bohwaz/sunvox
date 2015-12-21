/*
    SunDog: sound_sndout.cpp. Multiplatform functions for the sound output
    This file is part of the SunDog engine.
    Copyright (C) 2002 - 2009 Alex Zolotov <nightradio@gmail.com>
*/

#define DEFAULT_BUFFER_SIZE	4096	/*in samples*/
//for PalmOS devices it's 1024 samples

//#define OUTPUT_TO_FILE		1	/*output file: audio.raw*/

#include "../sound.h"
#include "core/debug.h"
#include "filesystem/v3nus_fs.h"
#include "utils/utils.h"
#include "memory/memory.h"

#ifdef OUTPUT_TO_FILE
FILE *g_audio_output = 0;
#endif

sound_struct g_snd; //Main sound structure;

#ifndef NOSOUND

int g_paused = 0;
int g_first_play = 1;

#ifdef LINUX
    #include "sound_sndout_linux.h"
#endif

#ifdef PALMOS
    #include "sound_sndout_palmos.h"
#endif

#ifdef OSX
    #include "sound_sndout_osx.h"
#endif

#if defined(WIN) || defined(WINCE)
    #include "sound_sndout_win.h"
#endif

int sound_stream_init( int mode, int freq, int channels )
{
#ifdef OUTPUT_TO_FILE
    g_audio_output = fopen( "audio.raw", "wb" );
#endif

    mem_set( &g_snd, sizeof( g_snd ), 0 );
    g_snd.user_data = 0;
    g_snd.mode = mode;
    g_snd.freq = freq;
    g_snd.channels = channels;
    g_snd.stop_counter = 0;
    g_snd.main_sound_callback_working = 0;

    int res = device_sound_stream_init( mode, freq, channels );
    if( res )
	return res;

    g_first_play = 1;
    dprint( "Sound stream initialized\n" );
    return 0;
}

void sound_stream_play( void )
{
    if( g_snd.stop_counter > 0 ) g_snd.stop_counter--;
    if( g_snd.stop_counter > 0 ) return;

    g_snd.need_to_stop = 0;
    
    device_sound_stream_play();
}

void sound_stream_stop( void )
{
    device_sound_stream_stop();

    g_snd.stop_counter++;
}

void sound_stream_close( void )
{
    dprint( "SOUND: sound_stream_close()\n" );
    sound_stream_stop();

    device_sound_stream_close();

#ifdef OUTPUT_TO_FILE
    if( g_audio_output ) fclose( g_audio_output );
#endif
}

#else //NOSOUND

//Sound disabled:

int sound_stream_init( int freq, int channels )
{
    return 0;
}

void sound_stream_play( void )
{
}

void sound_stream_stop( void )
{
}

void sound_stream_close( void )
{
}

#endif
