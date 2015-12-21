/*
    psynths_lfo.cpp.
    This file is part of the SunVox engine.
    Copyright (C) 2002 - 2008 Alex Zolotov <nightradio@gmail.com>
*/

#include "psynth.h"

//Unique names for objects in your synth:
#define SYNTH_DATA	psynth_lfo_data
#define SYNTH_HANDLER	psynth_lfo
//And unique parameters:
#define SYNTH_INPUTS	2
#define SYNTH_OUTPUTS	2

struct SYNTH_DATA
{
    //Controls: ############################################################
    CTYPE   ctl_volume;
    CTYPE   ctl_type;
    CTYPE   ctl_power;
    CTYPE   ctl_freq;
    CTYPE   ctl_shape;
    CTYPE   ctl_set_phase;
    //Synth data: ##########################################################
    uchar   *sin_tab;
    ulong   ptr;
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
	    retval = (int)"LFO";
	    break;

	case COMMAND_GET_SYNTH_INFO:
	    retval = (int)"LFO.\n(Low Frequency Oscillation)";
	    break;

	case COMMAND_GET_INPUTS_NUM: retval = SYNTH_INPUTS; break;
	case COMMAND_GET_OUTPUTS_NUM: retval = SYNTH_OUTPUTS; break;

	case COMMAND_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT; break;

	case COMMAND_INIT:
	    psynth_register_ctl( synth_id, "Volume", "", 0, 512, 256, 0, &data->ctl_volume, net );
	    psynth_register_ctl( synth_id, "Type", "amp/pan", 0, 1, 0, 1, &data->ctl_type, net );
	    psynth_register_ctl( synth_id, "Power", "", 0, 256, 256, 0, &data->ctl_power, net );
	    psynth_register_ctl( synth_id, "Frequency", "", 1, 2048, 256, 0, &data->ctl_freq, net );
	    psynth_register_ctl( synth_id, "Shape", "sin/square", 0, 1, 0, 1, &data->ctl_shape, net );
	    psynth_register_ctl( synth_id, "Set phase", "", 0, 256, 0, 0, &data->ctl_set_phase, net );
	    data->ptr = 0;
	    data->sin_tab = (uchar*)MEM_NEW( HEAP_DYNAMIC, 512 );
	    for( i = 0; i < 256; i++ )
	    {
		data->sin_tab[ ( i - 128 ) & 511 ] = ( g_vibrato_tab[ i ] / 2 ) + 127;
		data->sin_tab[ ( ( i + 256 ) - 128 ) & 511 ] = -( g_vibrato_tab[ i ] / 2 ) + 127;
	    }
	    retval = 1;
	    break;

	case COMMAND_CLEAN:
	    data->ptr = 0;
	    retval = 1;
	    break;

	case COMMAND_RENDER_REPLACE:
	    if( !inputs[ 0 ] || !outputs[ 0 ] ) break;
   
	    if( data->ctl_volume == 0 ) break;

	    {
		ulong delta = ( data->ctl_freq << 20 ) / pnet->sampling_freq;
		int ctl_power = data->ctl_power;
		int ctl_volume = data->ctl_volume;
		ulong ptr;
		for( ch = 0; ch < SYNTH_OUTPUTS; ch++ )
		{
		    STYPE *in = inputs[ ch ];
		    STYPE *out = outputs[ ch ];
		    ulong off = 0;
		    ptr = data->ptr;
		    if( data->ctl_type == 1 )
		    {
			//Paninig:
			if( ch == 1 )
			    off = 128;
		    }
		    if( data->ctl_shape == 0 )
		    {
			//SIN:
			uchar *sin_tab = data->sin_tab;
			for( i = 0; i < sample_frames; i++ )
			{
			    int amp = -sin_tab[ ( ( ptr >> 17 ) + off ) & 511 ];
			    amp *= ctl_power;
			    amp /= 256;
			    amp += 256;
			    amp *= ctl_volume;
			    amp /= 256;
			    STYPE_CALC out_val = in[ i ] * amp;
			    out[ i ] = out_val / 256;
			    ptr += delta;
			}
		    }
		    if( data->ctl_shape == 1 )
		    {
			//SQUARE:
			int v1 = ctl_volume;
			int v2 = ( ( 256 - ctl_power ) * ctl_volume ) / 256;
			for( i = 0; i < sample_frames; i++ )
			{
			    STYPE_CALC out_val = in[ i ];
			    int amp;
			    if( ( ( ( ptr >> 18 ) + off ) & 255 ) > 128 ) 
				amp = v1;
			    else 
				amp = v2;
			    out[ i ] = ( out_val * amp ) / 256;
			    ptr += delta;
			}
		    }
		}
		data->ptr = ptr;
	    }
	    retval = 1;
	    break;

	case COMMAND_SET_GLOBAL_CONTROLLER:
	    if( pnet->ctl_num == 5 )
	    {
		// "set phase" controller selected:
		data->ptr = ( pnet->ctl_val >> 6 ) << 17;
	    }
	    break;

	case COMMAND_CLOSE:
	    mem_free( data->sin_tab );
	    retval = 1;
	    break;
    }

    return retval;
}
