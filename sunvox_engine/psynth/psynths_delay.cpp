/*
    psynths_delay.cpp.
    This file is part of the SunVox engine.
    Copyright (C) 2002 - 2008 Alex Zolotov <nightradio@gmail.com>
*/

#include "psynth.h"

//Unique names for objects in your synth:
#define SYNTH_DATA	psynth_delay_data
#define SYNTH_HANDLER	psynth_delay
//And unique parameters:
#define SYNTH_INPUTS	2
#define SYNTH_OUTPUTS	2

struct SYNTH_DATA
{
    //Controls: ############################################################
    CTYPE   ctl_dry;
    CTYPE   ctl_wet;
    CTYPE   ctl_delay_l;
    CTYPE   ctl_delay_r;
    CTYPE   ctl_volume_l;
    CTYPE   ctl_volume_r;
    CTYPE   ctl_mono;
    CTYPE   ctl_inverse;
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
	    retval = (int)"Delay";
	    break;

	case COMMAND_GET_SYNTH_INFO:
	    retval = (int)"Delay.\nMax delay is 1/64 second";
	    break;

	case COMMAND_GET_INPUTS_NUM: retval = SYNTH_INPUTS; break;
	case COMMAND_GET_OUTPUTS_NUM: retval = SYNTH_OUTPUTS; break;

	case COMMAND_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT; break;

	case COMMAND_INIT:
	    psynth_register_ctl( synth_id, "Dry", "", 0, 512, 256, 0, &data->ctl_dry, net );
	    psynth_register_ctl( synth_id, "Wet", "", 0, 512, 256, 0, &data->ctl_wet, net );
	    psynth_register_ctl( synth_id, "Delay L", "", 0, 256, 128, 0, &data->ctl_delay_l, net );
	    psynth_register_ctl( synth_id, "Delay R", "", 0, 256, 160, 0, &data->ctl_delay_r, net );
	    psynth_register_ctl( synth_id, "Volume L", "", 0, 256, 256, 0, &data->ctl_volume_l, net );
	    psynth_register_ctl( synth_id, "Volume R", "", 0, 256, 256, 0, &data->ctl_volume_r, net );
	    psynth_register_ctl( synth_id, "Mono", "off/on", 0, 1, 0, 1, &data->ctl_mono, net );
	    psynth_register_ctl( synth_id, "Inverse amplitude", "off/on", 0, 1, 0, 1, &data->ctl_inverse, net );
	    data->buf_size = pnet->sampling_freq / 64;
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
	    
	    if( data->ctl_mono )
	        psynth_set_number_of_outputs( 1, synth_id, pnet );
	    else
	        psynth_set_number_of_outputs( SYNTH_OUTPUTS, synth_id, pnet );

	    {
		int buf_size = data->buf_size;
		int delay_l = ( data->buf_size * data->ctl_delay_l ) >> 8;
		int delay_r = ( data->buf_size * data->ctl_delay_r ) >> 8;
		if( delay_l >= data->buf_size ) delay_l = data->buf_size - 1;
		if( delay_r >= data->buf_size ) delay_r = data->buf_size - 1;
		int buf_ptr;
		int ctl_dry = data->ctl_dry;
		int outputs_num = psynth_get_number_of_outputs( synth_id, pnet );
		for( ch = 0; ch < outputs_num; ch++ )
		{
		    STYPE *in = inputs[ ch ];
		    STYPE *out = outputs[ ch ];
		    STYPE *cbuf = data->buf[ ch ];
		    buf_ptr = data->buf_ptr;
		    int ctl_wet = data->ctl_wet;
		    if( ch == 0 ) ctl_wet = ( ctl_wet * data->ctl_volume_l ) / 256;
		    if( ch == 1 ) ctl_wet = ( ctl_wet * data->ctl_volume_r ) / 256;
		    if( ctl_wet == 0 )
		    {
			for( i = 0; i < sample_frames; i++ )
			{
			    cbuf[ buf_ptr ] = in[ i ];
			    out[ i ] = 0;
			    buf_ptr++;
			    if( buf_ptr >= buf_size ) buf_ptr = 0;
			}
		    }
		    else
		    {
			for( i = 0; i < sample_frames; i++ )
			{
			    cbuf[ buf_ptr ] = in[ i ];
        		    
			    STYPE_CALC out_val;
			    int ptr;
			    if( ch == 0 )
				ptr = buf_ptr - delay_l;
			    else
				ptr = buf_ptr - delay_r;
			    if( ptr < 0 ) ptr += buf_size;
			    out_val = cbuf[ ptr ];
			    out_val *= ctl_wet;
			    out_val /= 256;
			    out[ i ] = out_val;

			    buf_ptr++;
			    if( buf_ptr >= buf_size ) buf_ptr = 0;
			}
			if( data->ctl_inverse )
			{
			    for( i = 0; i < sample_frames; i++ )
				out[ i ] = -out[ i ];
			}
		    }
		    if( ctl_dry > 0 && ctl_dry != 256 )
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
			    out[ i ] += in[ i ];
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
