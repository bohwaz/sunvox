/*
    sound_player.cpp. Main sound playing function
    This file is part of the SunDog engine.
    Copyright (C) 2002 - 2009 Alex Zolotov <nightradio@gmail.com>
*/

#ifndef NOSOUND

#include "../sound.h"

void main_sound_output_callback( sound_struct *ss )
{
    ss->main_sound_callback_working = 1;
    
    int clear = 0;
    
    //for stream stop: ===========
    if( ss->need_to_stop ) 
    {
	clear = 1;
	ss->stream_stoped = 1; 
	goto sound_end;
    }
    //============================
    
    //render piece of sound: =====
    if( render_piece_of_sound( ss ) == 0 )
    {
	clear = 1;
    }
    //============================

sound_end:

    if( clear )
    {
	if( ss->mode & SOUND_MODE_INT16 )
	{
	    short *ptr = (short*)ss->out_buffer;
	    for( int i = 0; i < ss->out_frames * ss->channels; i++ ) ptr[ i ] = 0;
	}
	if( ss->mode & SOUND_MODE_FLOAT32 )
	{
	    float *ptr = (float*)ss->out_buffer;
	    for( int i = 0; i < ss->out_frames * ss->channels; i++ ) ptr[ i ] = 0;
	}
    }
    
    ss->main_sound_callback_working = 0;
}

#endif
