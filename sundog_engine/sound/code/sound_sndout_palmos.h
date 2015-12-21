//################################
//## PALMOS:                    ##
//################################

#include "PalmOS.h"
SndStreamRef main_stream = 0;
MemHandle ARM_code_handler;
char *ARM_code;

int palmos_sound_output_callback( 
    void *user_data,
    long stream,
    void *buffer,
    long frames )
{
    sound_struct *ss = (sound_struct*)user_data;
    ss->out_buffer = buffer;
    ss->out_frames = frames;
    ss->out_time = 0;
    main_sound_output_callback( ss );
    return 0;
}

//################################
//## MAIN FUNCTIONS:            ##
//################################

int device_sound_stream_init( int mode, int freq, int channels )
{
    if( !( mode & SOUND_MODE_INT16 ) )
    {
	dprint( "Invalid sound mode\n" );
	return 1;
    }

    ulong processor; //Processor type
    ARM_code = (char*)palmos_sound_output_callback;
    FtrGet( sysFileCSystem, sysFtrNumProcessorID, &processor );
    if( sysFtrNumProcessorIsARM( processor ) )
    {
	SndStreamCreate( 
	    &main_stream,
    	    sndOutput,
	    freq,
	    sndInt16Little,
	    sndStereo,
	    (SndStreamBufferCallback) ARM_code,
	    &g_snd,
	    4096,
	    1 );
    }
    else 
    {
	main_stream = 0;
    }
    g_snd.channels = 2;

    return 0;
}

void device_sound_stream_play( void )
{
    if( main_stream )
    {
	if( g_paused ) 
	{
	    SndStreamPause( main_stream, 0 );
	    g_paused = 0;
	}
	if( g_first_play )
	{
	    SndStreamStart( main_stream );
	    g_first_play = 0;
	}
    }
}

void device_sound_stream_stop( void )
{
    if( main_stream && g_paused == 0 )
    {
	g_snd.stream_stoped = 0;
	g_snd.need_to_stop = 1;
	while( g_snd.stream_stoped == 0 ) {} //Waiting for stoping
	SndStreamPause( main_stream, 1 );
	g_paused = 1;
    }
}

void device_sound_stream_close( void )
{
    if( main_stream )
    {
	SndStreamDelete( main_stream );
    }
}
