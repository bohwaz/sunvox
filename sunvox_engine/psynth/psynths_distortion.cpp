/*
    psynths_distortion.cpp.
    This file is part of the SunVox engine.
    Copyright (C) 2002 - 2008 Alex Zolotov <nightradio@gmail.com>
*/

#include "psynth.h"

//Unique names for objects in your synth:
#define SYNTH_DATA	psynth_distortion_data
#define SYNTH_HANDLER	psynth_distortion
//And unique parameters:
#define SYNTH_INPUTS	2
#define SYNTH_OUTPUTS	2

struct SYNTH_DATA
{
    //Controls: ############################################################
    CTYPE   ctl_volume;
    CTYPE   ctl_type;
    CTYPE   ctl_power;
    CTYPE   ctl_bitrate;
    CTYPE   ctl_freq;
    //Synth data: ##########################################################
    int	    cnt;
    STYPE   hold_smp[ SYNTH_OUTPUTS ];
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
    int cnt;

    switch( command )
    {
	case COMMAND_GET_DATA_SIZE:
	    retval = sizeof( SYNTH_DATA );
	    break;

	case COMMAND_GET_SYNTH_NAME:
	    retval = (int)"Distortion";
	    break;

	case COMMAND_GET_SYNTH_INFO:
	    retval = (int)"Distortion and Amplifier";
	    break;

	case COMMAND_GET_INPUTS_NUM: retval = SYNTH_INPUTS; break;
	case COMMAND_GET_OUTPUTS_NUM: retval = SYNTH_OUTPUTS; break;

	case COMMAND_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT; break;

	case COMMAND_INIT:
	    psynth_register_ctl( synth_id, "Volume", "", 0, 256, 128, 0, &data->ctl_volume, net );
	    psynth_register_ctl( synth_id, "Type", "lim/sat", 0, 1, 0, 1, &data->ctl_type, net );
	    psynth_register_ctl( synth_id, "Power", "", 0, 256, 0, 0, &data->ctl_power, net );
	    psynth_register_ctl( synth_id, "Bitrate", "bits", 1, 16, 16, 1, &data->ctl_bitrate, net );
	    psynth_register_ctl( synth_id, "Freq", "Hz", 0, 44100, 44100, 0, &data->ctl_freq, net );
	    data->cnt = 0;
	    for( int a = 0; a < SYNTH_OUTPUTS; a++ ) data->hold_smp[ a ] = 0;
	    retval = 1;
	    break;

	case COMMAND_RENDER_REPLACE:
	    if( !inputs[ 0 ] || !outputs[ 0 ] ) break;
	    
	    cnt = data->cnt;
	    for( ch = 0; ch < SYNTH_OUTPUTS; ch++ )
	    {
		STYPE *in = inputs[ ch ];
		STYPE *out = outputs[ ch ];
		int volume = data->ctl_volume;
		int type = data->ctl_type;
		int power = data->ctl_power;
		int bitrate = data->ctl_bitrate;
		int freq = data->ctl_freq; 
		if( freq <= 0 ) freq = 1;
		int smp_len = ( pnet->sampling_freq << 8 ) / freq;
		cnt = data->cnt;
		STYPE *hold_smp = &data->hold_smp[ ch ];
		if( power > 255 ) power = 255;
		STYPE limit;
		STYPE max_val;
		INT16_TO_STYPE( limit, ( ( 256 - power ) << 7 ) );
		INT16_TO_STYPE( max_val, 32767 );
		STYPE_CALC coef = ( max_val * 256 ) / limit;
		if( bitrate < 16 || freq < 44100 )
		{
		    for( i = 0; i < sample_frames; i++ )
		    {
			STYPE_CALC out_val = in[ i ];
			if( bitrate < 16 )
			{
			    int res16;
			    STYPE_TO_INT16( res16, out_val );
			    res16 >>= 16 - bitrate;
			    res16 <<= 16 - bitrate;
			    INT16_TO_STYPE( out_val, res16 );
			}
			if( freq < 44100 )
			{
			    cnt += 256;
			    if( cnt > smp_len ) 
			    {
				*hold_smp = out_val;
				cnt -= smp_len;
			    }
			    out_val = *hold_smp;
			}
			if( power )
			{
			    if( type == 1 )
			    {
				if( out_val > limit ) out_val = limit - ( out_val - limit );
				if( -out_val > limit ) out_val = -limit + ( -out_val - limit );
			    }
			    if( out_val > limit ) out_val = limit;
			    if( -out_val > limit ) out_val = -limit;
			    out_val *= coef;
			    out_val /= 256;
			}
			out_val *= volume;
			out_val /= 128;
			out[ i ] = out_val;
		    }
		}
		else
		{
		    for( i = 0; i < sample_frames; i++ )
		    {
			STYPE_CALC out_val = in[ i ];
			if( power )
			{
			    if( type == 1 )
			    {
				if( out_val > limit ) out_val = limit - ( out_val - limit );
				if( -out_val > limit ) out_val = -limit + ( -out_val - limit );
			    }
			    if( out_val > limit ) out_val = limit;
			    if( -out_val > limit ) out_val = -limit;
			    out_val *= coef;
			    out_val /= 256;
			}
			out_val *= volume;
			out_val /= 128;
			out[ i ] = out_val;
		    }
		}
	    }
	    data->cnt = cnt;
	    retval = 1;
	    break;

	case COMMAND_CLOSE:
	    retval = 1;
	    break;
    }

    return retval;
}
