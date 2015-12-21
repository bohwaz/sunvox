/*
    psynths_kicker.cpp.
    This file is part of the SunVox engine.
    Copyright (C) 2002 - 2008 Alex Zolotov <nightradio@gmail.com>
*/

#include "psynth.h"

//Unique names for objects in your synth:
#define SYNTH_DATA	psynth_kicker_data
#define SYNTH_HANDLER	psynth_kicker
//And unique parameters:
#define SYNTH_INPUTS	0
#define SYNTH_OUTPUTS	2

#define MAX_CHANNELS	4

#define GET_FREQ(res,per)  \
{ \
    if( per >= 0 ) \
	res = ( data->linear_tab[ per % 768 ] >> ( per / 768 ) ); \
    else \
	res = ( data->linear_tab[ (7680*4+per) % 768 ] << -( ( (7680*4+per) / 768 ) - (7680*4)/768 ) ); /*if period is negative value*/ \
}
#define GET_DELTA(f,resh,resl)   \
{ \
    resh = f / pnet->sampling_freq; \
    resl = ( ( ( f % pnet->sampling_freq ) << 14 ) / pnet->sampling_freq ) << 2; /*Max sampling freq = 256 kHz*/ \
}

struct gen_channel
{
    int	    playing;
    ulong   id;
    int	    vel;
    ulong   ptr;
    ulong   delta;
    ulong   env_vol;
    int	    sustain;

    int	    accel;

    //Local controllers:
    CTYPE   local_type;
};

struct SYNTH_DATA
{
    //Controls: ############################################################
    CTYPE   ctl_volume;
    CTYPE   ctl_type;
    CTYPE   ctl_pan;
    CTYPE   ctl_attack;
    CTYPE   ctl_release;
    CTYPE   ctl_vol_add;
    CTYPE   ctl_env_accel;
    CTYPE   ctl_channels;
    CTYPE   ctl_anticlick;
    //Synth data: ##########################################################
    gen_channel   channels[ MAX_CHANNELS ];
    int	    search_ptr;
    ulong   *linear_tab;
};

#define GET_VAL_TRIANGLE \
    STYPE_CALC val; \
    if( ( ( ptr >> 16 ) & 15 ) < 8 ) \
    { \
	INT16_TO_STYPE( val, (int)( -32767 + ( ( ptr & 0xFFFF ) / 8 + ( ( ( ptr >> 16 ) & 7 ) * 8192 ) ) ) ); \
    } \
    else \
    { \
	INT16_TO_STYPE( val, (int)( 32767 - ( ( ptr & 0xFFFF ) / 8 + ( ( ( ptr >> 16 ) & 7 ) * 8192 ) ) ) ); \
    }
#define GET_VAL_RECTANGLE \
    STYPE_CALC val = 0; \
    if( ( ptr >> 16 ) & 16 ) \
	val = max_val; \
    else \
	val = min_val;
#define APPLY_VOLUME \
    val *= vol; \
    val /= 256;
#define APPLY_ENV_VOLUME \
    ulong new_env_vol = ( env_vol >> 20 ) + vol_add; \
    if( new_env_vol > 1024 ) new_env_vol = 1024; \
    val *= new_env_vol; \
    val /= 1024;
#define LAST_PART_REPLACE \
    out[ i ] = val; \
    ptr += ( delta * ( env_vol >> 20 ) ) / 1024; \
    if( sustain ) \
    { \
	env_vol += attack_delta; \
	if( env_vol >= ( 1 << 30 ) ) { env_vol = ( 1 << 30 ); sustain = 0; } \
    } \
    else \
    { \
	env_vol -= release_delta; \
	if( ( env_vol >> 20 ) > env_accel ) \
	    env_vol -= release_delta; \
	else \
	    if( accel ) { accel = 0; env_vol = env_accel << 20; } \
	if( env_vol > ( 1 << 30 ) ) { env_vol = 0; playing = 0; while( i < sample_frames ) { out[ i ] = 0; i++; } break; } \
    }
#define LAST_PART_ADD \
    out[ i ] += val; \
    ptr += ( delta * ( env_vol >> 20 ) ) / 1024; \
    if( sustain ) \
    { \
	env_vol += attack_delta; \
	if( env_vol >= ( 1 << 30 ) ) { env_vol = ( 1 << 30 ); sustain = 0; } \
    } \
    else \
    { \
	env_vol -= release_delta; \
	if( ( env_vol >> 20 ) > env_accel ) \
	    env_vol -= release_delta; \
	else \
	    if( accel ) { accel = 0; env_vol = env_accel << 20; } \
	if( env_vol > ( 1 << 30 ) ) { env_vol = 0; playing = 0; break; } \
    }

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
	    retval = (int)"Kicker";
	    break;

	case COMMAND_GET_SYNTH_INFO:
	    retval = (int)"Kicker.\n\nAvailable local controllers:\n * Type";
	    break;

	case COMMAND_GET_INPUTS_NUM: retval = SYNTH_INPUTS; break;
	case COMMAND_GET_OUTPUTS_NUM: retval = SYNTH_OUTPUTS; break;

	case COMMAND_GET_FLAGS: retval = PSYNTH_FLAG_GENERATOR; break;

	case COMMAND_INIT:
	    psynth_register_ctl( synth_id, "Volume", "", 0, 256, 256, 0, &data->ctl_volume, net );
	    psynth_register_ctl( synth_id, "Type", "tri/rect", 0, 1, 0, 1, &data->ctl_type, net );
	    psynth_register_ctl( synth_id, "Panning", "", 0, 256, 128, 0, &data->ctl_pan, net );
	    psynth_register_ctl( synth_id, "Attack", "", 0, 512, 0, 0, &data->ctl_attack, net );
	    psynth_register_ctl( synth_id, "Release", "", 0, 512, 32, 0, &data->ctl_release, net );
	    psynth_register_ctl( synth_id, "Vol. Add", "", 0, 1024, 0, 0, &data->ctl_vol_add, net );
	    psynth_register_ctl( synth_id, "Env. Accel.", "", 0, 1024, 256, 0, &data->ctl_env_accel, net );
	    psynth_register_ctl( synth_id, "Polyphony", "ch.", 1, MAX_CHANNELS, 1, 1, &data->ctl_channels, net );
	    psynth_register_ctl( synth_id, "Anticlick", "off/on", 0, 1, 0, 1, &data->ctl_anticlick, net );
	    for( int c = 0; c < MAX_CHANNELS; c++ )
	    {
		data->channels[ c ].playing = 0;
		data->channels[ c ].ptr = 0;
		data->channels[ c ].env_vol = 0;
		data->channels[ c ].sustain = 0;
		data->channels[ c ].accel = 0;
	    }
	    data->search_ptr = 0;
	    data->linear_tab = g_linear_tab;
	    retval = 1;
	    break;

	case COMMAND_CLEAN:
	    retval = 1;
	    break;

	case COMMAND_RENDER_REPLACE:
	    if( !outputs[ 0 ] ) break;
	    for( int c = 0; c < data->ctl_channels; c++ )
	    {
		if( data->channels[ c ].playing )
		{
		    int attack_len = ( pnet->sampling_freq * data->ctl_attack ) / 256;
		    int release_len = ( pnet->sampling_freq * data->ctl_release ) / 256;
		    
		    attack_len /= 1024;
		    int attack_delta = 1 << 30;
		    if( attack_len != 0 ) attack_delta = ( 1 << 20 ) / attack_len;

		    release_len /= 1024;
		    int release_delta = 1 << 30;
		    if( release_len != 0 ) release_delta = ( 1 << 20 ) / release_len;
		    
		    gen_channel *chan = &data->channels[ c ];
		    ulong ptr = 0;
		    ulong env_vol = 0;
		    int sustain = 0;
		    int accel = 0;
		    int playing = 0;
		    ulong delta = chan->delta;
		    int vol_add = data->ctl_vol_add;
		    ulong env_accel = 1024 - data->ctl_env_accel;
		    for( ch = 0; ch < SYNTH_OUTPUTS; ch++ )
		    {
			STYPE *in = inputs[ ch ];
			STYPE *out = outputs[ ch ];
        		ptr = chan->ptr;
			env_vol = chan->env_vol;
			sustain = chan->sustain;
			accel = chan->accel;
			playing = chan->playing;
			int vol = ( data->ctl_volume * chan->vel ) >> 8;
			if( data->ctl_pan < 128 )
			{
			    if( ch == 1 ) { vol *= data->ctl_pan; vol >>= 7; }
			}
			else
			{
			    if( ch == 0 ) { vol *= 128 - ( data->ctl_pan - 128 ); vol >>= 7; }
			}
			STYPE_CALC max_val;
			STYPE_CALC min_val;
			INT16_TO_STYPE( max_val, 32767 );
			INT16_TO_STYPE( min_val, -32767 );
			max_val *= vol; max_val /= 256;
			min_val *= vol; min_val /= 256;
			int add = ch * 32768 / 2;
			if( retval == 0 ) 
			{
			    switch( chan->local_type )
			    {
				case 0:
				//Triangle:
				if( vol == 256 )
				    for( i = 0; i < sample_frames; i++ ) {
					GET_VAL_TRIANGLE;
					APPLY_ENV_VOLUME;
					LAST_PART_REPLACE; }
				else
				    for( i = 0; i < sample_frames; i++ ) {
					GET_VAL_TRIANGLE;
					APPLY_VOLUME;
					APPLY_ENV_VOLUME;
					LAST_PART_REPLACE; }
				break;

				case 1:
				//Rectangle:
				for( i = 0; i < sample_frames; i++ )
				{
				    GET_VAL_RECTANGLE;
				    APPLY_ENV_VOLUME;
				    LAST_PART_REPLACE;
				}
				break;
			    }
			}
			else
			{
			    switch( chan->local_type )
			    {
				case 0:
				//Triangle:
				if( vol == 256 )
				    for( i = 0; i < sample_frames; i++ ) {
					GET_VAL_TRIANGLE;
					APPLY_ENV_VOLUME;
					LAST_PART_ADD; }
				else
				    for( i = 0; i < sample_frames; i++ ) {
					GET_VAL_TRIANGLE;
					APPLY_VOLUME;
					APPLY_ENV_VOLUME;
					LAST_PART_ADD; }
				break;

				case 1:
				//Rectangle:
				for( i = 0; i < sample_frames; i++ )
				{
				    GET_VAL_RECTANGLE;
				    APPLY_ENV_VOLUME;
				    LAST_PART_ADD;
				}
				break;
			    }
			}
		    }
		    chan->ptr = ptr;
		    chan->delta = delta;
		    chan->env_vol = env_vol;
		    chan->sustain = sustain;
		    chan->accel = accel;
		    chan->playing = playing;
		    retval = 1;
		}
	    }
	    break;

	case COMMAND_NOTE_ON:
	    {
		int c;
		for( c = 0; c < data->ctl_channels; c++ )
		{
		    if( data->channels[ data->search_ptr ].playing == 0 ) break;
		    data->search_ptr++;
		    if( data->search_ptr >= data->ctl_channels ) data->search_ptr = 0;
		}
		if( c == data->ctl_channels )
		{
		    //Channel not found:
		    data->search_ptr++;
		    if( data->search_ptr >= data->ctl_channels ) data->search_ptr = 0;
		}

		ulong delta_h, delta_l;
		int freq;
		GET_FREQ( freq, pnet->period_ptr / 4 );
		GET_DELTA( freq, delta_h, delta_l );

		c = data->search_ptr;
		data->channels[ c ].playing = 1;
		data->channels[ c ].vel = pnet->velocity;
		data->channels[ c ].delta = delta_l | ( delta_h << 16 );
		if( data->ctl_anticlick )
		    data->channels[ c ].ptr = 0 | ( 4 << 16 );
		else
		    data->channels[ c ].ptr = 0;
		data->channels[ c ].id = pnet->channel_id;
		data->channels[ c ].env_vol = 0;
		data->channels[ c ].sustain = 1;
		data->channels[ c ].accel = 1;
		
		data->channels[ c ].local_type = data->ctl_type;
		retval = c;
	    }
	    break;

    	case COMMAND_SET_FREQ:
	    if( pnet->synth_channel >= 0 && pnet->synth_channel < MAX_CHANNELS )
	    {
		int c = pnet->synth_channel;
		if( data->channels[ c ].id == pnet->channel_id )
		{
		    ulong delta_h, delta_l;
		    int freq;
		    GET_FREQ( freq, pnet->period_ptr / 4 );
		    GET_DELTA( freq, delta_h, delta_l );

		    data->channels[ c ].delta = delta_l | ( delta_h << 16 );
		}
	    }
	    retval = 1;
	    break;

    	case COMMAND_SET_VELOCITY:
	    if( pnet->synth_channel >= 0 && pnet->synth_channel < MAX_CHANNELS )
	    {
		int c = pnet->synth_channel;
		if( data->channels[ c ].id == pnet->channel_id )
		{
		    data->channels[ c ].vel = pnet->velocity;
		}
	    }
	    retval = 1;
	    break;

	case COMMAND_NOTE_OFF:
	    if( pnet->synth_channel >= 0 && pnet->synth_channel < MAX_CHANNELS )
	    {
		int c = pnet->synth_channel;
		if( data->channels[ c ].id == pnet->channel_id )
		{
		    data->channels[ c ].sustain = 0;
		}
	    }
	    retval = 1;
	    break;

	case COMMAND_ALL_NOTES_OFF:
	    for( int c = 0; c < MAX_CHANNELS; c++ )
	    {
		data->channels[ c ].playing = 0;
	    }
	    retval = 1;
	    break;

	case COMMAND_SET_LOCAL_CONTROLLER:
	    if( pnet->ctl_num == 1 )
	    {
		if( pnet->synth_channel >= 0 && pnet->synth_channel < MAX_CHANNELS )
		{
		    int c = pnet->synth_channel;
		    if( data->channels[ c ].id == pnet->channel_id )
		    {
			//Set "TYPE" local controller of this channel:
			data->channels[ c ].local_type = pnet->ctl_val;
		    }
		}
		retval = 1;
	    }
	    break;

	case COMMAND_CLOSE:
	    retval = 1;
	    break;
    }

    return retval;
}
