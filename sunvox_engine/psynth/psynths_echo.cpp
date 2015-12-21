/*
    psynths_echo.cpp.
    This file is part of the SunVox engine.
    Copyright (C) 2002 - 2008 Alex Zolotov <nightradio@gmail.com>
*/

#include "psynth.h"

//Unique names for objects in your synth:
#define SYNTH_DATA	psynth_echo_data
#define SYNTH_HANDLER	psynth_echo
//And unique parameters:
#define SYNTH_INPUTS	2
#define SYNTH_OUTPUTS	2

struct SYNTH_DATA
{
    //Controls: ############################################################
    CTYPE   ctl_dry;
    CTYPE   ctl_wet;
    CTYPE   ctl_feedback;
    CTYPE   ctl_delay;
    CTYPE   ctl_stereo;
    CTYPE   ctl_pong;
    //Synth data: ##########################################################
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
	    retval = (int)"Echo";
	    break;

	case COMMAND_GET_SYNTH_INFO:
	    retval = (int)"Echo.\nMax delay is one second";
	    break;

	case COMMAND_GET_INPUTS_NUM: retval = SYNTH_INPUTS; break;
	case COMMAND_GET_OUTPUTS_NUM: retval = SYNTH_OUTPUTS; break;

	case COMMAND_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT; break;

	case COMMAND_INIT:
	    psynth_register_ctl( synth_id, "Dry", "", 0, 256, 256, 0, &data->ctl_dry, net );
	    psynth_register_ctl( synth_id, "Wet", "", 0, 256, 128, 0, &data->ctl_wet, net );
	    psynth_register_ctl( synth_id, "Feedback", "", 0, 256, 128, 0, &data->ctl_feedback, net );
	    psynth_register_ctl( synth_id, "Delay", "", 0, 256, 256, 0, &data->ctl_delay, net );
	    psynth_register_ctl( synth_id, "Stereo", "off/on", 0, 1, 1, 1, &data->ctl_stereo, net );
	    data->buf_size = pnet->sampling_freq;
	    for( i = 0; i < SYNTH_OUTPUTS; i++ )
	    {
		data->buf[ i ] = (STYPE*)MEM_NEW( HEAP_DYNAMIC, data->buf_size * sizeof( STYPE ) );
	    }
	    data->buf_ptr = 0;
	    for( i = 0; i < SYNTH_OUTPUTS; i++ )
	    {
		if( data->buf[ i ] ) mem_set( data->buf[ i ], data->buf_size * sizeof( STYPE ), 0 );
	    }
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
	    retval = 1;
	    break;

	case COMMAND_RENDER_REPLACE:
	    if( !inputs[ 0 ] || !outputs[ 0 ] ) break;
	    
	    {
		//int buf_size = ( pnet->tick_size * data->ctl_delay ) >> 8;
		int buf_size = ( data->buf_size * data->ctl_delay ) >> 8;
		if( buf_size > data->buf_size ) buf_size = data->buf_size;
		int buf_ptr;
		int ctl_stereo = data->ctl_stereo;
		int ctl_wet = data->ctl_wet;
		int ctl_dry = data->ctl_dry;
		int ctl_feedback = data->ctl_feedback;
		for( ch = 0; ch < SYNTH_OUTPUTS; ch++ )
		{
		    STYPE *in = inputs[ ch ];
		    STYPE *out = outputs[ ch ];
		    STYPE *cbuf = data->buf[ ch ];
		    buf_ptr = data->buf_ptr;
		    for( i = 0; i < sample_frames; i++ )
		    {
			STYPE_CALC out_val = cbuf[ buf_ptr ];
			out_val *= ctl_wet;
			out_val /= 256;
			out[ i ] = out_val;
    		    
			out_val = cbuf[ buf_ptr ];
			out_val *= ctl_feedback;
			out_val /= 256;
			denorm_add_white_noise( out_val );
			cbuf[ buf_ptr ] = out_val;

			if( ch && ctl_stereo )
			{
			    int ptr2 = buf_ptr + buf_size / 2;
			    if( ptr2 >= buf_size ) ptr2 -= buf_size;
			    cbuf[ ptr2 ] += in[ i ];
			}
			else
			{
			    cbuf[ buf_ptr ] += in[ i ];
			}

			buf_ptr++;
			if( buf_ptr >= buf_size ) buf_ptr = 0;
		    }
		    if( ctl_dry > 0 && ctl_dry < 256 )
		    {
			for( i = 0; i < sample_frames; i++ )
			{
			    STYPE_CALC out_val = in[ i ];
			    out_val *= ctl_dry;
			    out_val /= 256;
			    out[ i ] += out_val;
			}
		    }
		    if( ctl_dry == 256 )
		    {
			for( i = 0; i < sample_frames; i++ )
			{
			    out[ i ] += in[ i ];
			}
		    }
		}
		data->buf_ptr = buf_ptr;
	    }
	    retval = 1;
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
