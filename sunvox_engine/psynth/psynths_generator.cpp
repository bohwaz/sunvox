/*
    psynths_generator.cpp.
    This file is part of the SunVox engine.
    Copyright (C) 2002 - 2008 Alex Zolotov <nightradio@gmail.com>
*/

#include "psynth.h"
#include "utils/utils.h"

//Unique names for objects in your synth:
#define SYNTH_DATA	psynth_generator_data
#define SYNTH_HANDLER	psynth_generator
//And unique parameters:
#define SYNTH_INPUTS	1
#define SYNTH_OUTPUTS	2

#define MAX_CHANNELS	16

#define LOWMEM	//For devices with low memory
signed char *g_noise_data = 0;
int	    g_noise_data_users_counter = 0;

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
    int	    ptr_h;
    int	    ptr_l;
    ulong   delta_h;
    ulong   delta_l;
    ulong   env_vol;
    int	    sustain;

    //Local controllers:
    CTYPE   local_type;
    CTYPE   local_pan;
};

enum {
    MODE_HQ = 0,
    MODE_HQ_MONO,
    MODES
};

struct SYNTH_DATA
{
    //Controls: ############################################################
    CTYPE   ctl_volume;
    CTYPE   ctl_type;
    CTYPE   ctl_pan;
    CTYPE   ctl_attack;
    CTYPE   ctl_release;
    CTYPE   ctl_channels;
    CTYPE   ctl_mode;
    CTYPE   ctl_sustain;
    CTYPE   ctl_mod;
    //Synth data: ##########################################################
    gen_channel   channels[ MAX_CHANNELS ];
    signed char    *noise_data;
    signed char    dirty_wave[ 32 ];
    int	    search_ptr;
    int	    wrong_saw_generation;
    ulong   *linear_tab;
};

#define SVOX_PREC (long)16	    //sample pointer (fixed point) precision
#define SIGNED_SUB64( v1, v1_p, v2, v2_p ) \
{ v1 -= v2; v1_p -= v2_p; \
if( v1_p < 0 ) { v1--; v1_p += ( (long)1 << SVOX_PREC ); } }
#define SIGNED_ADD64( v1, v1_p, v2, v2_p ) \
{ v1 += v2; v1_p += v2_p; \
if( v1_p > ( (long)1 << SVOX_PREC ) - 1 ) { v1++; v1_p -= ( (long)1 << SVOX_PREC ); } }
#define UNSIGNED_MUL64( v1, v1_p, v2_p ) \
{ \
    v1 *= ( v2_p ); \
    v1_p *= ( v2_p ); \
    v1_p >>= SVOX_PREC; \
    v1_p += v1 & ( ( 1 << SVOX_PREC ) - 1 ); \
    v1 >>= SVOX_PREC; \
    v1 += v1_p >> SVOX_PREC; \
    v1_p &= ( 1 << SVOX_PREC ) - 1; \
}

#define GET_VAL_TRIANGLE \
    if( ( ptr_h & 15 ) < 8 ) \
    { \
	INT16_TO_STYPE( val, -32767 + ( ptr_l / 8 + ( ( ptr_h & 7 ) * 8192 ) ) ); \
    } \
    else \
    { \
	INT16_TO_STYPE( val, 32767 - ( ptr_l / 8 + ( ( ptr_h & 7 ) * 8192 ) ) ); \
    }
#define GET_VAL_SAW2 \
    INT16_TO_STYPE( val, 32767 - ( ptr_l / 32 + ( ( ptr_h & 31 ) * 4096 ) ) );
#define GET_VAL_SAW \
    INT16_TO_STYPE( val, 32767 - ( ptr_l / 16 + ( ( ptr_h & 15 ) * 4096 ) ) );
#define GET_VAL_RECTANGLE \
    val = 0; \
    if( ptr_h & 16 ) \
	val = max_val; \
    else \
	val = min_val;
#define GET_VAL_NOISE \
    INT16_TO_STYPE( val, noise_data[ ( ptr_h + add ) & 32767 ] * 256 );
#define GET_VAL_DIRTY \
    INT16_TO_STYPE( val, dirty_wave[ ( ptr_h >> 2 ) & 31 ] * 256 );
#define APPLY_VOLUME \
    val *= vol; \
    val /= 256;
#define APPLY_ENV_VOLUME \
    val *= ( env_vol >> 20 ); \
    val /= 1024;
#define LAST_PART_REPLACE \
    out[ i ] = val; \
    SIGNED_ADD64( ptr_h, ptr_l, delta_h, delta_l ); \
    if( sustain ) \
    { \
	env_vol += attack_delta; \
	if( env_vol >= ( 1 << 30 ) ) { env_vol = ( 1 << 30 ); if( sustain_enabled == 0 ) sustain = 0; } \
    } \
    else \
    { \
	env_vol -= release_delta; \
	if( env_vol > ( 1 << 30 ) ) { env_vol = 0; playing = 0; while( i < sample_frames ) { out[ i ] = 0; i++; } break; } \
    }
#define LAST_PART_ADD \
    out[ i ] += val; \
    SIGNED_ADD64( ptr_h, ptr_l, delta_h, delta_l ); \
    if( sustain ) \
    { \
	env_vol += attack_delta; \
	if( env_vol >= ( 1 << 30 ) ) { env_vol = ( 1 << 30 ); if( sustain_enabled == 0 ) sustain = 0; } \
    } \
    else \
    { \
	env_vol -= release_delta; \
	if( env_vol > ( 1 << 30 ) ) { env_vol = 0; playing = 0; break; } \
    }
#define LAST_PART_REPLACE2 \
    out[ i ] = val; \
    SIGNED_ADD64( ptr_h, ptr_l, delta_h, delta_l );
#define LAST_PART_ADD2 \
    out[ i ] += val; \
    SIGNED_ADD64( ptr_h, ptr_l, delta_h, delta_l );

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
	    retval = (int)"Generator";
	    break;

	case COMMAND_GET_SYNTH_INFO:
	    retval = (int)"Universal generator.\n\nAvailable local controllers:\n * Type\n * Pan";
	    break;

	case COMMAND_GET_INPUTS_NUM: retval = SYNTH_INPUTS; break;
	case COMMAND_GET_OUTPUTS_NUM: retval = SYNTH_OUTPUTS; break;

	case COMMAND_GET_FLAGS: retval = PSYNTH_FLAG_GENERATOR | PSYNTH_FLAG_EFFECT; break;

	case COMMAND_INIT:
	    psynth_register_ctl( synth_id, "Volume", "", 0, 256, 128, 0, &data->ctl_volume, net );
	    psynth_register_ctl( synth_id, "Type", "tri/saw/rect/noise/dirty", 0, 4, 0, 1, &data->ctl_type, net );
	    psynth_register_ctl( synth_id, "Panning", "", 0, 256, 128, 0, &data->ctl_pan, net );
	    psynth_register_ctl( synth_id, "Attack", "", 0, 512, 0, 0, &data->ctl_attack, net );
	    psynth_register_ctl( synth_id, "Release", "", 0, 512, 0, 0, &data->ctl_release, net );
	    psynth_register_ctl( synth_id, "Polyphony", "ch.", 1, MAX_CHANNELS, 8, 1, &data->ctl_channels, net );
	    psynth_register_ctl( synth_id, "Mode", "HQ/HQmono", 0, MODES - 1, MODE_HQ, 1, &data->ctl_mode, net );
	    psynth_register_ctl( synth_id, "Sustain", "off/on", 0, 1, 1, 1, &data->ctl_sustain, net );
	    psynth_register_ctl( synth_id, "P.Modulation", "", 0, 256, 0, 0, &data->ctl_mod, net );
	    for( int c = 0; c < MAX_CHANNELS; c++ )
	    {
		data->channels[ c ].playing = 0;
		data->channels[ c ].ptr_l = 0;
		data->channels[ c ].ptr_h = 0;
		data->channels[ c ].env_vol = 0;
		data->channels[ c ].sustain = 0;
	    }
#ifdef LOWMEM
	    if( g_noise_data_users_counter == 0 )
	    {
		data->noise_data = (signed char*)MEM_NEW( HEAP_DYNAMIC, 32768 );
		for( int s = 0; s < 32768; s++ )
		{
		    data->noise_data[ s ] = (signed char)( (int)pseudo_random() - 32768 / 2 );
		}
		g_noise_data = data->noise_data;
	    }
	    else
	    {
		data->noise_data = g_noise_data;
	    }
	    g_noise_data_users_counter++;
#else
	    data->noise_data = (signed char*)MEM_NEW( HEAP_DYNAMIC, 32768 );
	    for( int s = 0; s < 32768; s++ )
	    {
		data->noise_data[ s ] = (signed char)( (int)pseudo_random() - 32768 / 2 );
	    }
#endif
	    data->dirty_wave[ 0 ] = 0;
	    data->dirty_wave[ 1 ] = -100;
	    data->dirty_wave[ 2 ] = -90;
	    data->dirty_wave[ 3 ] = 0;
	    data->dirty_wave[ 4 ] = 90;
	    data->dirty_wave[ 5 ] = -119;
	    data->dirty_wave[ 6 ] = -20;
	    data->dirty_wave[ 7 ] = 45;
	    data->dirty_wave[ 8 ] = 2;
	    data->dirty_wave[ 9 ] = -20;
	    data->dirty_wave[ 10 ] = 111;
	    data->dirty_wave[ 11 ] = -23;
	    data->dirty_wave[ 12 ] = 2;
	    data->dirty_wave[ 13 ] = -98;
	    data->dirty_wave[ 14 ] = 60;
	    data->dirty_wave[ 15 ] = 32;
	    data->dirty_wave[ 16 ] = 100;
	    data->dirty_wave[ 17 ] = 50;
	    data->dirty_wave[ 18 ] = 0;
	    data->dirty_wave[ 19 ] = -50;
	    data->dirty_wave[ 20 ] = 65;
	    data->dirty_wave[ 21 ] = 98;
	    data->dirty_wave[ 22 ] = 50;
	    data->dirty_wave[ 23 ] = 32;
	    data->dirty_wave[ 24 ] = -90;
	    data->dirty_wave[ 25 ] = -120;
	    data->dirty_wave[ 26 ] = 100;
	    data->dirty_wave[ 27 ] = 90;
	    data->dirty_wave[ 28 ] = 59;
	    data->dirty_wave[ 29 ] = 21;
	    data->dirty_wave[ 30 ] = 0;
	    data->dirty_wave[ 31 ] = 54;
	    data->search_ptr = 0;
	    data->wrong_saw_generation = 0;
	    data->linear_tab = g_linear_tab;
	    retval = 1;
	    break;

	case COMMAND_RENDER_REPLACE:
	    if( !outputs[ 0 ] ) break;

	    if( data->ctl_mode == MODE_HQ_MONO )
		psynth_set_number_of_outputs( 1, synth_id, pnet );
	    else
		psynth_set_number_of_outputs( SYNTH_OUTPUTS, synth_id, pnet );

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
		    ulong delta_h = chan->delta_h;
		    ulong delta_l = chan->delta_l;
		    int sustain = chan->sustain;
		    int ptr_h = 0;
		    int ptr_l = 0;
		    int playing = 0;
		    int sustain_enabled = data->ctl_sustain;
		    ulong env_vol = 0;

		    int outputs_num = psynth_get_number_of_outputs( synth_id, pnet );
		    for( ch = 0; ch < outputs_num; ch++ )
		    {
			STYPE *in = inputs[ 0 ];
			STYPE *out = outputs[ ch ];
			sustain = chan->sustain;
        		ptr_h = chan->ptr_h;
			ptr_l = chan->ptr_l;
			env_vol = chan->env_vol;
			playing = chan->playing;
			int vol = ( data->ctl_volume * chan->vel ) >> 8;
			int ctl_pan = data->ctl_pan + ( chan->local_pan - 128 );
			if( ctl_pan < 0 ) ctl_pan = 0;
			if( ctl_pan > 256 ) ctl_pan = 256;
			if( ctl_pan < 128 )
			{
			    if( ch == 1 ) { vol *= ctl_pan; vol >>= 7; }
			}
			else
			{
			    if( ch == 0 ) { vol *= 128 - ( ctl_pan - 128 ); vol >>= 7; }
			}
			STYPE_CALC max_val;
			STYPE_CALC min_val;
			INT16_TO_STYPE( max_val, 32767 );
			INT16_TO_STYPE( min_val, -32767 );
			max_val *= vol; max_val /= 256;
			min_val *= vol; min_val /= 256;
			signed char *noise_data = data->noise_data;
			signed char *dirty_wave = data->dirty_wave;
			int add = ch * 32768 / 2;
			int local_type = chan->local_type;
			if( local_type == 1 && data->wrong_saw_generation )
			    local_type = 8; //saw bug fix for the first version of SunVox
			if( data->ctl_attack == 0 && data->ctl_release == 0 )
			{
			    //Can't play any sounds in this case:
			    if( sustain_enabled == 0 ) sustain = 0;
			}
			STYPE_CALC val;
			if( data->ctl_mod )
			{
			    //MODULATION:
			    ulong mod_delta_h, mod_delta_l;
			    int mod_freq;
			    GET_FREQ( mod_freq, 100 );
			    GET_DELTA( mod_freq, mod_delta_h, mod_delta_l );
			    UNSIGNED_MUL64( mod_delta_h, mod_delta_l, ( data->ctl_mod << ( SVOX_PREC - 8 ) ) / 8 );
			    if( data->ctl_attack == 0 && data->ctl_release == 0 )
			    {
				if( sustain == 0 ) playing = 0;
				if( retval == 0 )
				{
				    if( sustain == 0 )
					for( i = 0; i < sample_frames; i++ ) out[ i ] = 0;  
				}
			    }
			    if( playing )
			    for( i = 0; i < sample_frames; i++ ) 
			    {
				switch( local_type )
				{
				    case 0: //Triangle:
					GET_VAL_TRIANGLE;
					break;
				    case 1: //Saw:
					GET_VAL_SAW;
					break;
				    case 2: //Rectangle:
					GET_VAL_RECTANGLE;
					break;
				    case 3: //Noise:
					GET_VAL_NOISE;
					break;
				    case 4: //Dirty:
					GET_VAL_DIRTY;
					break;
				}
				APPLY_VOLUME;
				APPLY_ENV_VOLUME;
				if( retval == 0 )
				    out[ i ] = val;
				else
				    out[ i ] += val;
				SIGNED_ADD64( ptr_h, ptr_l, delta_h, delta_l );
				if( sustain )
				{
				    env_vol += attack_delta;
				    if( env_vol >= ( 1 << 30 ) ) { env_vol = ( 1 << 30 ); if( sustain_enabled == 0 ) sustain = 0; }
				}
				else
				{	
				    env_vol -= release_delta;
				    if( env_vol > ( 1 << 30 ) ) { env_vol = 0; playing = 0; if( retval == 0 ) { while( i < sample_frames ) { out[ i ] = 0; i++; } } break; }
				}
				STYPE_CALC in_v = in[ i ];
				int minus = 0;
				if( in_v < 0 )
				{
				    minus = 1;
				    in_v = -in_v;
				}
				if( in_v > STYPE_ONE ) in_v = STYPE_ONE;
				ulong mul;
				STYPE_TO_INT16( mul, in_v );
				ulong mod_delta_h2 = mod_delta_h;
				ulong mod_delta_l2 = mod_delta_l;
				UNSIGNED_MUL64( mod_delta_h2, mod_delta_l2, mul );
				if( minus == 0 )
				{
				    SIGNED_ADD64( ptr_h, ptr_l, mod_delta_h2, mod_delta_l2 );
				}
				else
				{
				    SIGNED_SUB64( ptr_h, ptr_l, mod_delta_h2, mod_delta_l2 );
				}
			    }
			}
			else
			{ 
			    //NO MODULATION:
			if( retval == 0 )
			{
			    if( data->ctl_attack == 0 && data->ctl_release == 0 )
			    {
				if( sustain == 0 ) { for( i = 0; i < sample_frames; i++ ) out[ i ] = 0; playing = 0; }
				else
				switch( local_type )
				{
				    case 0: if( vol == 256 ) for( i = 0; i < sample_frames; i++ ) { GET_VAL_TRIANGLE; LAST_PART_REPLACE2 } else for( i = 0; i < sample_frames; i++ ) { GET_VAL_TRIANGLE; APPLY_VOLUME; LAST_PART_REPLACE2; } break;
				    case 1: if( vol == 256 ) for( i = 0; i < sample_frames; i++ ) { GET_VAL_SAW; LAST_PART_REPLACE2 } else for( i = 0; i < sample_frames; i++ ) { GET_VAL_SAW; APPLY_VOLUME; LAST_PART_REPLACE2; } break;
				    case 2: for( i = 0; i < sample_frames; i++ ) { GET_VAL_RECTANGLE; LAST_PART_REPLACE2 } break;
				    case 3: if( vol == 256 ) for( i = 0; i < sample_frames; i++ ) { GET_VAL_NOISE; LAST_PART_REPLACE2 } else for( i = 0; i < sample_frames; i++ ) { GET_VAL_NOISE; APPLY_VOLUME; LAST_PART_REPLACE2; } break;
				    case 4: if( vol == 256 ) for( i = 0; i < sample_frames; i++ ) { GET_VAL_DIRTY; LAST_PART_REPLACE2 } else for( i = 0; i < sample_frames; i++ ) { GET_VAL_DIRTY; APPLY_VOLUME; LAST_PART_REPLACE2; } break;
				    case 8: if( vol == 256 ) for( i = 0; i < sample_frames; i++ ) { GET_VAL_SAW2; LAST_PART_REPLACE2 } else for( i = 0; i < sample_frames; i++ ) { GET_VAL_SAW2; APPLY_VOLUME; LAST_PART_REPLACE2; } break;
				}
			    }
			    else
			    {
				switch( local_type )
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
				    //Saw:
				    if( vol == 256 )
					for( i = 0; i < sample_frames; i++ ) {
					    GET_VAL_SAW;
					    APPLY_ENV_VOLUME;
					    LAST_PART_REPLACE; }
				    else
					for( i = 0; i < sample_frames; i++ ) {
					    GET_VAL_SAW;
					    APPLY_VOLUME;
					    APPLY_ENV_VOLUME;
					    LAST_PART_REPLACE; }
				    break;

				    case 2:
				    //Rectangle:
				    for( i = 0; i < sample_frames; i++ )
				    {
					GET_VAL_RECTANGLE;
					APPLY_ENV_VOLUME;
					LAST_PART_REPLACE;
				    }
				    break;

				    case 3:
				    //Noise:
				    if( vol == 256 )
					for( i = 0; i < sample_frames; i++ ) {
					    GET_VAL_NOISE;
					    APPLY_ENV_VOLUME;
					    LAST_PART_REPLACE; }
				    else
					for( i = 0; i < sample_frames; i++ ) {
					    GET_VAL_NOISE;
					    APPLY_VOLUME;
					    APPLY_ENV_VOLUME;
					    LAST_PART_REPLACE; }
				    break;

				    case 4:
				    //Dirty:
				    if( vol == 256 )
					for( i = 0; i < sample_frames; i++ ) {
					    GET_VAL_DIRTY;
					    APPLY_ENV_VOLUME;
					    LAST_PART_REPLACE; }
				    else
					for( i = 0; i < sample_frames; i++ ) {
					    GET_VAL_DIRTY;
					    APPLY_VOLUME;
					    APPLY_ENV_VOLUME;
					    LAST_PART_REPLACE; }
				    break;

				    case 8:
				    //Saw:
				    if( vol == 256 )
					for( i = 0; i < sample_frames; i++ ) {
					    GET_VAL_SAW2;
					    APPLY_ENV_VOLUME;
					    LAST_PART_REPLACE; }
				    else
					for( i = 0; i < sample_frames; i++ ) {
					    GET_VAL_SAW2;
					    APPLY_VOLUME;
					    APPLY_ENV_VOLUME;
					    LAST_PART_REPLACE; }
				    break;
				}
			    }
			}
			else
			{
			    if( data->ctl_attack == 0 && data->ctl_release == 0 )
			    {
				if( sustain == 0 ) { playing = 0; }
				else
				switch( local_type )
				{
				    case 0: if( vol == 256 ) for( i = 0; i < sample_frames; i++ ) { GET_VAL_TRIANGLE; LAST_PART_ADD2 } else for( i = 0; i < sample_frames; i++ ) { GET_VAL_TRIANGLE; APPLY_VOLUME; LAST_PART_ADD2; } break;
				    case 1: if( vol == 256 ) for( i = 0; i < sample_frames; i++ ) { GET_VAL_SAW; LAST_PART_ADD2 } else for( i = 0; i < sample_frames; i++ ) { GET_VAL_SAW; APPLY_VOLUME; LAST_PART_ADD2; } break;
				    case 2: for( i = 0; i < sample_frames; i++ ) { GET_VAL_RECTANGLE; LAST_PART_ADD2 } break;
				    case 3: if( vol == 256 ) for( i = 0; i < sample_frames; i++ ) { GET_VAL_NOISE; LAST_PART_ADD2 } else for( i = 0; i < sample_frames; i++ ) { GET_VAL_NOISE; APPLY_VOLUME; LAST_PART_ADD2; } break;
				    case 4: if( vol == 256 ) for( i = 0; i < sample_frames; i++ ) { GET_VAL_DIRTY; LAST_PART_ADD2 } else for( i = 0; i < sample_frames; i++ ) { GET_VAL_DIRTY; APPLY_VOLUME; LAST_PART_ADD2; } break;
				    case 8: if( vol == 256 ) for( i = 0; i < sample_frames; i++ ) { GET_VAL_SAW2; LAST_PART_ADD2 } else for( i = 0; i < sample_frames; i++ ) { GET_VAL_SAW2; APPLY_VOLUME; LAST_PART_ADD2; } break;
				}
			    }
			    else
			    {
				switch( local_type )
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
				    //Saw:
				    if( vol == 256 )
					for( i = 0; i < sample_frames; i++ ) {
					    GET_VAL_SAW;
					    APPLY_ENV_VOLUME;
					    LAST_PART_ADD; }
				    else
					for( i = 0; i < sample_frames; i++ ) {
					    GET_VAL_SAW;
					    APPLY_VOLUME;
					    APPLY_ENV_VOLUME;
					    LAST_PART_ADD; }
				    break;

				    case 2:
				    //Rectangle:
				    for( i = 0; i < sample_frames; i++ )
				    {
					GET_VAL_RECTANGLE;
					APPLY_ENV_VOLUME;
					LAST_PART_ADD;
				    }
				    break;

				    case 3:
				    //Noise:
				    if( vol == 256 )
					for( i = 0; i < sample_frames; i++ ) {
					    GET_VAL_NOISE;
					    APPLY_ENV_VOLUME;
					    LAST_PART_ADD; }
				    else
					for( i = 0; i < sample_frames; i++ ) {
					    GET_VAL_NOISE;
					    APPLY_VOLUME;
					    APPLY_ENV_VOLUME;
					    LAST_PART_ADD; }
				    break;

				    case 4:	    
				    //Dirty:
				    if( vol == 256 )
					for( i = 0; i < sample_frames; i++ ) {
					    GET_VAL_DIRTY;
					    APPLY_ENV_VOLUME;
					    LAST_PART_ADD; }
				    else
					for( i = 0; i < sample_frames; i++ ) {
					    GET_VAL_DIRTY;
					    APPLY_VOLUME;
					    APPLY_ENV_VOLUME;
					    LAST_PART_ADD; }
				    break;

				    case 8:
				    //Saw:
				    if( vol == 256 )
					for( i = 0; i < sample_frames; i++ ) {
					    GET_VAL_SAW2;
					    APPLY_ENV_VOLUME;
					    LAST_PART_ADD; }
				    else
					for( i = 0; i < sample_frames; i++ ) {
					    GET_VAL_SAW2;
					    APPLY_VOLUME;
					    APPLY_ENV_VOLUME;
					    LAST_PART_ADD; }
				    break;
				}
			    }
			}
			} //...NO MODULATION
		    }
		    chan->ptr_h = ptr_h;
		    chan->ptr_l = ptr_l;
		    chan->delta_h = delta_h;
		    chan->delta_l = delta_l;
		    chan->env_vol = env_vol;
		    chan->sustain = sustain;
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
		data->channels[ c ].delta_h = delta_h;
		data->channels[ c ].delta_l = delta_l;
		data->channels[ c ].ptr_h = 4;
		data->channels[ c ].ptr_l = 0;
		data->channels[ c ].id = pnet->channel_id;
		if( data->ctl_attack == 0 )
		    data->channels[ c ].env_vol = 1 << 30; //Maximum amplitude
		else
		    data->channels[ c ].env_vol = 0;
		data->channels[ c ].sustain = 1;
		
		data->channels[ c ].local_type = data->ctl_type;
		data->channels[ c ].local_pan = 128;
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

		    data->channels[ c ].delta_h = delta_h;
		    data->channels[ c ].delta_l = delta_l;
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
		data->channels[ c ].sustain = 0;
	    }
	    retval = 1;
	    break;

	case COMMAND_CLEAN:
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
	    if( pnet->ctl_num == 2 )
	    {
		if( pnet->synth_channel >= 0 && pnet->synth_channel < MAX_CHANNELS )
		{
		    int c = pnet->synth_channel;
		    if( data->channels[ c ].id == pnet->channel_id )
		    {
			//Set "PAN" local controller of this channel:
			data->channels[ c ].local_pan = pnet->ctl_val >> 7;
		    }
		}
		retval = 1;
	    }
	    break;

	case COMMAND_SET_GLOBAL_CONTROLLER:
	    if( pnet->ctl_num == 0x98 ) 
		data->wrong_saw_generation = 1;
	    break;

	case COMMAND_CLOSE:
#ifdef LOWMEM
	    g_noise_data_users_counter--;
	    if( g_noise_data_users_counter == 0 )
		mem_free( g_noise_data );
#else
	    if( data->noise_data ) mem_free( data->noise_data );
#endif
	    data->noise_data = 0;
	    retval = 1;
	    break;
    }

    return retval;
}
