/*
    psynths_loop.cpp.
    This file is part of the SunVox engine.
    Copyright (C) 2002 - 2008 Alex Zolotov <nightradio@gmail.com>
*/

#include "psynth.h"

//Unique names for objects in your synth:
#define SYNTH_DATA	psynth_loop_data
#define SYNTH_HANDLER	psynth_loop
//And unique parameters:
#define SYNTH_INPUTS	2
#define SYNTH_OUTPUTS	2

struct SYNTH_DATA
{
    //Controls: ############################################################
    CTYPE   ctl_volume;
    CTYPE   ctl_delay;
    CTYPE   ctl_stereo;
    CTYPE   ctl_repeats;
    //Synth data: ##########################################################
    int	    rep_counter;
    int	    buf_size;
    STYPE   *buf[ SYNTH_OUTPUTS ];
    int	    buf_ptr;
};

int SYNTH_HANDLER( 
    PSYTEXX_SYNTH_PARAMETERS
    )
{
    psynth_net *pnet = (psynth_net*)net;
    SYNTH_DATA *data = (SYNTH_DATA*)data_ptr;
    int retval = 0;
    int i;
    int ch;

    switch( command )
    {
	case COMMAND_GET_DATA_SIZE:
	    retval = sizeof( SYNTH_DATA );
	    break;

	case COMMAND_GET_SYNTH_NAME:
	    retval = (int)"Loop";
	    break;

	case COMMAND_GET_SYNTH_INFO:
	    retval = (int)"Loop.\nMax delay is 2 lines";
	    break;

	case COMMAND_GET_INPUTS_NUM: retval = SYNTH_INPUTS; break;
	case COMMAND_GET_OUTPUTS_NUM: retval = SYNTH_OUTPUTS; break;

	case COMMAND_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT; break;

	case COMMAND_INIT:
	    psynth_register_ctl( synth_id, "Volume", "", 0, 256, 256, 0, &data->ctl_volume, net );
	    psynth_register_ctl( synth_id, "Delay", "", 0, 256, 256, 0, &data->ctl_delay, net );
	    psynth_register_ctl( synth_id, "Stereo", "off/on", 0, 1, 1, 1, &data->ctl_stereo, net );
	    psynth_register_ctl( synth_id, "Repeats", "", 0, 64, 0, 1, &data->ctl_repeats, net );
	    data->buf_size = pnet->sampling_freq / 3;
	    for( i = 0; i < SYNTH_OUTPUTS; i++ )
	    {
		data->buf[ i ] = (STYPE*)MEM_NEW( HEAP_DYNAMIC, data->buf_size * sizeof( STYPE ) );
	    }
	    data->buf_ptr = 0;
	    for( i = 0; i < SYNTH_OUTPUTS; i++ )
	    {
		if( data->buf[ i ] ) mem_set( data->buf[ i ], data->buf_size * sizeof( STYPE ), 0 );
	    }
	    data->rep_counter = 0;
	    retval = 1;
	    break;

	case COMMAND_CLEAN:
	    data->buf_ptr = 0;
	    for( i = 0; i < SYNTH_OUTPUTS; i++ )
	    {
		if( data->buf[ i ] ) 
		{
		    for( int s = 0; s < data->buf_size; s++ )
			data->buf[ i ][ s ] = 0;
		}
	    }
	    data->rep_counter = 0;
	    retval = 1;
	    break;

	case COMMAND_RENDER_REPLACE:
	    if( !inputs[ 0 ] || !outputs[ 0 ] ) break;
	    
	    {
		if( data->ctl_stereo )
		    psynth_set_number_of_outputs( SYNTH_OUTPUTS, synth_id, pnet );
		else
		    psynth_set_number_of_outputs( 1, synth_id, pnet );

		int line_size = ( pnet->tick_size * pnet->ticks_per_line ) / 256;
		int buf_size = ( data->ctl_delay * line_size ) / 128;
		if( buf_size > data->buf_size ) buf_size = data->buf_size;
		
		int rep_counter;
		int buf_ptr;

		int volume = data->ctl_volume;

		if( volume == 0 ) break;

		int anticlick_len = pnet->sampling_freq / 1000;

		int outputs_num = psynth_get_number_of_outputs( synth_id, pnet );
		for( ch = 0; ch < outputs_num; ch++ )
		{
		    STYPE *in = inputs[ ch ];
		    STYPE *out = outputs[ ch ];
		    STYPE *cbuf = data->buf[ ch ];
		    buf_ptr = data->buf_ptr;
		    rep_counter = data->rep_counter;
		    if( volume == 256 )
		    {
			for( i = 0; i < sample_frames; i++ )
			{
			    if( rep_counter == 0 )
			    {
				//Record:
				out[ i ] = in[ i ];
				cbuf[ buf_ptr ] = in[ i ];
			    }
			    else
			    {
				//Play:
				if( buf_ptr < anticlick_len )
				{
				    int c = ( buf_ptr << 8 ) / anticlick_len;
				    out[ i ] = ( cbuf[ buf_ptr ] * c ) / 256 + ( cbuf[ buf_size - 1 ] * (256-c) ) / 256;
				}
				else
				{
				    out[ i ] = cbuf[ buf_ptr ];
				}
			    }

			    buf_ptr++;
			    if( buf_ptr >= buf_size ) 
			    {
				rep_counter++;
				if( rep_counter > data->ctl_repeats )
				{
				    rep_counter = 0;
				}
				buf_ptr = 0;
			    }
			}
		    }
		    else
		    {
			for( i = 0; i < sample_frames; i++ )
			{
			    STYPE_CALC out_val;
			    if( rep_counter == 0 )
			    {
				//Record:
				cbuf[ buf_ptr ] = in[ i ];
				out_val = in[ i ];
			    }
			    else
			    {
				//Play:
				if( buf_ptr < anticlick_len )
				{
				    int c = ( buf_ptr << 8 ) / anticlick_len;
				    out_val = ( cbuf[ buf_ptr ] * c ) / 256 + ( cbuf[ buf_size - 1 ] * (256-c) ) / 256;
				}
				else
				{
				    out_val = cbuf[ buf_ptr ];
				}
			    }
			    out_val *= volume;
			    out_val /= 256;
			    out[ i ] = out_val;

			    buf_ptr++;
			    if( buf_ptr >= buf_size ) 
			    {
				rep_counter++;
				if( rep_counter > data->ctl_repeats )
				{
				    rep_counter = 0;
				}
				buf_ptr = 0;
			    }
			}
		    }
		}
		data->buf_ptr = buf_ptr;
		data->rep_counter = rep_counter;
	    }
	    retval = 1;
	    break;

	case COMMAND_SET_GLOBAL_CONTROLLER:
	    if( pnet->ctl_num == 3 )
	    {
		// "Repeats" controller selected:
		data->rep_counter = 0;
		data->buf_ptr = 0;
	    }
	    break;

	case COMMAND_CLOSE:
	    for( i = 0; i < SYNTH_OUTPUTS; i++ )
	    {
		if( data->buf[ i ] ) mem_free( data->buf[ i ] );
	    }
	    retval = 1;
	    break;
    }

    return retval;
}
