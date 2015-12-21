/*
    psynths_reverb.cpp.
    Based on: 
	FreeVerb code by Jezar at Dreampoint;
	Fixed-Point DC Blocking Filter With Noise-Shaping by robert bristow-johnson.
	    (http://www.dspguru.com/comp.dsp/tricks/alg/dc_block.htm)
    This file is part of the SunVox engine.
    Copyright (C) 2002 - 2008 Alex Zolotov <nightradio@gmail.com>
*/

/* Denormalising:
 *
 * According to music-dsp thread 'Denormalise', Pentium processors
 * have a hardware 'feature', that is of interest here, related to
 * numeric underflow.  We have a recursive filter. The output decays
 * exponentially, if the input stops.  So the numbers get smaller and
 * smaller... At some point, they reach 'denormal' level.  This will
 * lead to drastic spikes in the CPU load.  The effect was reproduced
 * with the reverb - sometimes the average load over 10 s doubles!!.
 *
 * The 'undenormalise' macro fixes the problem: As soon as the number
 * is close enough to denormal level, the macro forces the number to
 * 0.0f.
 *
 */

#include "psynth.h"

//Unique names for objects in your synth:
#define SYNTH_DATA	psynth_reverb_data
#define SYNTH_HANDLER	psynth_reverb
//And unique parameters:
#define SYNTH_INPUTS	2
#define SYNTH_OUTPUTS	2

#define MAX_COMBS		8
#define MAX_ALLPASSES		4
#ifdef ONLY44100
    #define MAX_COMB_BUF_SIZE		1700
    #define MAX_ALLPASS_BUF_SIZE	600
#else
    #define MAX_COMB_BUF_SIZE		( 1700 * 10 )
    #define MAX_ALLPASS_BUF_SIZE	( 600 * 10 )
#endif
#define STEREO_SPREAD		23
#ifdef STYPE_FLOATINGPOINT
    #define INPUT_MUL			1
#else
    #define INPUT_MUL			4
#endif

struct comb_filter
{
    STYPE_CALC	*buf;
    int		buf_size;
    int		buf_ptr;
    STYPE_CALC	feedback;
    STYPE_CALC  filterstore;
    STYPE_CALC  damp1;
    STYPE_CALC  damp2;
};

struct allpass_filter
{
    STYPE_CALC	*buf;
    int		buf_size;
    int		buf_ptr;
    STYPE_CALC	feedback;
};

enum {
    MODE_HQ = 0,
    MODE_HQ_MONO,
    MODE_LQ,
    MODE_LQ_MONO,
    MODES
};

struct SYNTH_DATA
{
    //Controls: ############################################################
    CTYPE   ctl_dry;
    CTYPE   ctl_wet;
    CTYPE   ctl_feedback;
    CTYPE   ctl_damp;
    CTYPE   ctl_width;
    CTYPE   ctl_freeze;
    CTYPE   ctl_mode;
    //Synth data: ##########################################################
    comb_filter	    combs[ MAX_COMBS * 2 ];
    int		    combs_num;
    allpass_filter  allpasses[ MAX_ALLPASSES * 2 ];
    int		    allpasses_num;
    int		    filters_reinit_request;

    STYPE_CALC	    dc_block_pole;
    STYPE_CALC	    dc_block_acc_L;
    STYPE_CALC	    dc_block_prev_x_L;
    STYPE_CALC	    dc_block_prev_y_L;
    STYPE_CALC	    dc_block_acc_R;
    STYPE_CALC	    dc_block_prev_x_R;
    STYPE_CALC	    dc_block_prev_y_R;
};

void reinit_filter_parameters( SYNTH_DATA *data, psynth_net *pnet )
{
    //Recalculate internal values after parameter change:

    int i;
    int feedback;
    int damp;

    if( data->ctl_freeze )
    {
	feedback = 256;
	damp = 256;
    }
    else
    {
    	feedback = ( data->ctl_feedback * 71 ) / 256 + 179;
	float lowpass_freq = (float)( 256 - data->ctl_damp ) * 128 + 128;
	float lowpass_rc = 1.0F / ( (2*3.1415926535F) * lowpass_freq );
	float lowpass_delta_t = 1.0F / (float)pnet->sampling_freq;
	float lowpass_alpha = lowpass_delta_t / ( lowpass_rc + lowpass_delta_t );
	damp = (int)( lowpass_alpha * 256.0F );
    }

    for( i = 0; i < data->combs_num * 2; i++ )
    {
#ifdef STYPE_FLOATINGPOINT
    	data->combs[ i ].feedback = (STYPE_CALC)feedback / (STYPE_CALC)256;
    	data->combs[ i ].damp1 = (STYPE_CALC)damp / (STYPE_CALC)256;
    	data->combs[ i ].damp2 = (STYPE_CALC)1.0 - data->combs[ i ].damp1;
#else
    	data->combs[ i ].feedback = feedback;
    	data->combs[ i ].damp1 = damp;
    	data->combs[ i ].damp2 = 256 - damp;
#endif
    }
}

void clean_filters( SYNTH_DATA *data )
{
    for( int i = 0; i < data->combs_num * 2; i++ )
    {
	for( int i2 = 0; i2 < MAX_COMB_BUF_SIZE; i2++ )
	    data->combs[ i ].buf[ i2 ] = 0;
	data->combs[ i ].filterstore = 0;
    }
    for( int i = 0; i < data->allpasses_num * 2; i++ )
    {
	for( int i2 = 0; i2 < MAX_ALLPASS_BUF_SIZE; i2++ )
	    data->allpasses[ i ].buf[ i2 ] = 0;
    }
}

int SYNTH_HANDLER( 
    PSYTEXX_SYNTH_PARAMETERS
    )
{
    psynth_net *pnet = (psynth_net*)net;
    SYNTH_DATA *data = (SYNTH_DATA*)data_ptr;
    int retval = 0;
    int i;

    switch( command )
    {
	case COMMAND_GET_DATA_SIZE:
	    retval = sizeof( SYNTH_DATA );
	    break;

	case COMMAND_GET_SYNTH_NAME:
	    retval = (int)"Reverb";
	    break;

	case COMMAND_GET_SYNTH_INFO:
	    retval = (int)"Reverb + DC Blocking Filter.\n(Based on FreeVerb)";
	    break;

	case COMMAND_GET_INPUTS_NUM: retval = SYNTH_INPUTS; break;
	case COMMAND_GET_OUTPUTS_NUM: retval = SYNTH_OUTPUTS; break;

	case COMMAND_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT; break;

	case COMMAND_INIT:
	    psynth_register_ctl( synth_id, "Dry", "", 0, 256, 256, 0, &data->ctl_dry, net );
	    psynth_register_ctl( synth_id, "Wet", "", 0, 256, 64, 0, &data->ctl_wet, net );
	    psynth_register_ctl( synth_id, "Feedback", "", 0, 256, 256, 0, &data->ctl_feedback, net );
	    psynth_register_ctl( synth_id, "Damp", "", 0, 256, 128, 0, &data->ctl_damp, net );
	    psynth_register_ctl( synth_id, "Width", "", 0, 256, 256, 0, &data->ctl_width, net );
	    psynth_register_ctl( synth_id, "Freeze", "", 0, 1, 0, 1, &data->ctl_freeze, net );
	    psynth_register_ctl( synth_id, "Mode", "HQ/HQmono/LQ/LQmono", 0, MODES - 1, MODE_HQ, 1, &data->ctl_mode, net );
	    
	    //Init comb filters:
	    data->combs_num = MAX_COMBS;
	    for( i = 0; i < data->combs_num * 2; i++ )
	    {
    		data->combs[ i ].buf = (STYPE_CALC*)MEM_NEW( HEAP_DYNAMIC, MAX_COMB_BUF_SIZE * sizeof( STYPE_CALC ) );
		data->combs[ i ].buf_ptr = 0;
		data->combs[ i ].filterstore = 0;
	    }
	    i = 0;
	    data->combs[ i ].buf_size = 1116; data->combs[ i + MAX_COMBS ].buf_size = data->combs[ i ].buf_size + STEREO_SPREAD; i++;
	    data->combs[ i ].buf_size = 1188; data->combs[ i + MAX_COMBS ].buf_size = data->combs[ i ].buf_size + STEREO_SPREAD; i++;
	    data->combs[ i ].buf_size = 1277; data->combs[ i + MAX_COMBS ].buf_size = data->combs[ i ].buf_size + STEREO_SPREAD; i++;
	    data->combs[ i ].buf_size = 1356; data->combs[ i + MAX_COMBS ].buf_size = data->combs[ i ].buf_size + STEREO_SPREAD; i++;
	    data->combs[ i ].buf_size = 1422; data->combs[ i + MAX_COMBS ].buf_size = data->combs[ i ].buf_size + STEREO_SPREAD; i++;
	    data->combs[ i ].buf_size = 1491; data->combs[ i + MAX_COMBS ].buf_size = data->combs[ i ].buf_size + STEREO_SPREAD; i++;
	    data->combs[ i ].buf_size = 1557; data->combs[ i + MAX_COMBS ].buf_size = data->combs[ i ].buf_size + STEREO_SPREAD; i++;
	    data->combs[ i ].buf_size = 1617; data->combs[ i + MAX_COMBS ].buf_size = data->combs[ i ].buf_size + STEREO_SPREAD; i++;

	    //Init allpass filters:
	    data->allpasses_num = MAX_ALLPASSES;
	    for( i = 0; i < data->allpasses_num * 2; i++ )
	    {
    		data->allpasses[ i ].buf = (STYPE_CALC*)MEM_NEW( HEAP_DYNAMIC, MAX_ALLPASS_BUF_SIZE * sizeof( STYPE_CALC ) );
		data->allpasses[ i ].buf_ptr = 0;
#ifdef STYPE_FLOATINGPOINT
		data->allpasses[ i ].feedback = (STYPE_CALC)0.5;
#else
		data->allpasses[ i ].feedback = 128;
#endif
	    }
	    i = 0;
	    data->allpasses[ i ].buf_size = 556; data->allpasses[ i + MAX_ALLPASSES ].buf_size = data->allpasses[ i ].buf_size + STEREO_SPREAD; i++;
	    data->allpasses[ i ].buf_size = 441; data->allpasses[ i + MAX_ALLPASSES ].buf_size = data->allpasses[ i ].buf_size + STEREO_SPREAD; i++;
	    data->allpasses[ i ].buf_size = 341; data->allpasses[ i + MAX_ALLPASSES ].buf_size = data->allpasses[ i ].buf_size + STEREO_SPREAD; i++;
	    data->allpasses[ i ].buf_size = 225; data->allpasses[ i + MAX_ALLPASSES ].buf_size = data->allpasses[ i ].buf_size + STEREO_SPREAD; i++;

	    //Fix buffer sizes:
	    if( pnet->sampling_freq != 44100 )
	    {
		int delta = ( pnet->sampling_freq << 11 ) / 44100;
		for( int i = 0; i < data->combs_num * 2; i++ )
		    data->combs[ i ].buf_size = ( data->combs[ i ].buf_size * delta ) >> 11;
		for( int i = 0; i < data->allpasses_num * 2; i++ )
		    data->allpasses[ i ].buf_size = ( data->allpasses[ i ].buf_size * delta ) >> 11;
	    }

	    clean_filters( data );

	    //DC Offset removing filter:
	    {
		float dc_block_freq = 5.0F;
		float dc_block_pole = ( (2*3.1415926535F) * dc_block_freq ) / (float)pnet->sampling_freq;
#ifdef STYPE_FLOATINGPOINT
		data->dc_block_pole = (STYPE_CALC)1.0 - dc_block_pole;
#else
		data->dc_block_pole = (STYPE_CALC)( 32768.0F * dc_block_pole );
#endif
	    }
	    data->dc_block_acc_L = 0;
	    data->dc_block_prev_x_L = 0;
	    data->dc_block_prev_y_L = 0;
	    data->dc_block_acc_R = 0;
	    data->dc_block_prev_x_R = 0;
	    data->dc_block_prev_y_R = 0;
    
	    data->filters_reinit_request = 1;
	    retval = 1;
	    break;

	case COMMAND_CLEAN:
	    clean_filters( data );
	    data->dc_block_acc_L = 0;
	    data->dc_block_prev_x_L = 0;
	    data->dc_block_prev_y_L = 0;
	    data->dc_block_acc_R = 0;
	    data->dc_block_prev_x_R = 0;
	    data->dc_block_prev_y_R = 0;
	    retval = 1;
	    break;

	case COMMAND_RENDER_REPLACE:
	    if( !inputs[ 0 ] || !outputs[ 0 ] ) break;

	    if( data->ctl_mode == MODE_HQ_MONO || data->ctl_mode == MODE_LQ_MONO )
	        psynth_set_number_of_outputs( 1, synth_id, pnet );
	    else
	        psynth_set_number_of_outputs( SYNTH_OUTPUTS, synth_id, pnet );

	    {
		STYPE *inL_ch = inputs[ 0 ];
		STYPE *inR_ch = inputs[ 1 ];
		STYPE *outL_ch = outputs[ 0 ];
		STYPE *outR_ch = outputs[ 1 ];
		int outputs_num = psynth_get_number_of_outputs( synth_id, pnet );
		if( outputs_num == 1 )
		{
		    inR_ch = 0;
		    outR_ch = 0;
		}
		else
		{
		    outputs_num = 2; //Stereo
		}

		int filters_add;
		if( data->ctl_mode < MODE_LQ )
		    filters_add = 1;
		else
		    filters_add = 2; //Skip every second filter

		//Reinit filters:
		if( data->filters_reinit_request )
		{
		    reinit_filter_parameters( data, pnet );
		    data->filters_reinit_request = 0;
		}

		int ctl_dry = data->ctl_dry;
		int ctl_wet = data->ctl_wet;

		if( filters_add > 1 ) ctl_wet *= 2;
   	
		for( int ch = 0; ch < outputs_num; ch++ )
		{
		    STYPE *in = inputs[ ch ];
		    STYPE *out = outputs[ ch ];
		    STYPE_CALC dc_block_pole;
		    STYPE_CALC dc_block_acc;
		    STYPE_CALC dc_block_prev_x;
		    STYPE_CALC dc_block_prev_y;
		    dc_block_pole = data->dc_block_pole;
		    if( ch == 0 )
		    {
			dc_block_acc = data->dc_block_acc_L;
			dc_block_prev_x = data->dc_block_prev_x_L;
			dc_block_prev_y = data->dc_block_prev_y_L;
		    }
		    else
		    {
			dc_block_acc = data->dc_block_acc_R;
			dc_block_prev_x = data->dc_block_prev_x_R;
			dc_block_prev_y = data->dc_block_prev_y_R;
		    }
		    for( int i = 0; i < sample_frames; i++ )
		    {
			STYPE_CALC input = in[ i ];
#ifdef STYPE_FLOATINGPOINT
			//Floating-Point DC Blocking Filter:
			dc_block_prev_y = input - dc_block_prev_x + dc_block_prev_y * dc_block_pole;
			dc_block_prev_x = input;
			input = dc_block_prev_y;
			denorm_add_white_noise( input );
#else
			//Fixed-Point DC Blocking Filter With Noise-Shaping:
			dc_block_acc -= dc_block_prev_x;
			dc_block_prev_x = input << 15;
			dc_block_acc += dc_block_prev_x;
			dc_block_acc -= dc_block_pole * dc_block_prev_y;
			dc_block_prev_y = dc_block_acc >> 15; // quantization happens here
			input = dc_block_prev_y;
#endif
			out[ i ] = (STYPE)input;
		    }
		    if( ch == 0 )
		    {
			data->dc_block_acc_L = dc_block_acc;
			data->dc_block_prev_x_L = dc_block_prev_x;
			data->dc_block_prev_y_L = dc_block_prev_y;
		    }
		    else
		    {
			data->dc_block_acc_R = dc_block_acc;
			data->dc_block_prev_x_R = dc_block_prev_x;
			data->dc_block_prev_y_R = dc_block_prev_y;
		    }
		}

		for( int i = 0; i < sample_frames; i++ )
		{
		    //Output signals:
		    STYPE_CALC outL = 0;
		    STYPE_CALC outR = 0;

		    //Input signals:
		    STYPE_CALC input;
		    if( inR_ch ) 
		    {
			input = ( outL_ch[ i ] + outR_ch[ i ] ) / 2;
		    }
		    else
		    {
			input = outL_ch[ i ];
		    }

		    input *= INPUT_MUL; //decrease noise in fixed point mode

		    //Accumulate comb filters in parallel:
		    for( int a = 0; a < data->combs_num * outputs_num; a += filters_add )
		    {
			comb_filter *f = &data->combs[ a ];
			STYPE_CALC f_out = f->buf[ f->buf_ptr ];
#ifdef STYPE_FLOATINGPOINT
			f->filterstore = ( f_out * f->damp1 ) + ( f->filterstore * f->damp2 );
			//f->filterstore = undenormalize( f->filterstore );
			//f->buf[ f->buf_ptr ] = undenormalize( input + ( f->filterstore * f->feedback ) );
			f->buf[ f->buf_ptr ] = input + ( f->filterstore * f->feedback );
#else
			STYPE_CALC f_v;
			f->filterstore = ( f_out * f->damp1 + f->filterstore * f->damp2 ) / 256;
			f_v = input + ( f->filterstore * f->feedback ) / 256;
			f->buf[ f->buf_ptr ] = f_v;
#endif
			if( a < MAX_COMBS ) outL += f_out; else outR += f_out;
			if( ++f->buf_ptr >= f->buf_size ) f->buf_ptr = 0;
		    }

		    //Feed through allpasses in series:
		    for( int a = 0; a < data->allpasses_num * outputs_num; a += filters_add )
		    {
			allpass_filter *f = &data->allpasses[ a ];
			STYPE_CALC f_bufout = f->buf[ f->buf_ptr ];
			STYPE_CALC f_out;
			STYPE_CALC f_in;

			//f_bufout = undenormalize( f_bufout );

			if( a < MAX_ALLPASSES ) f_in = outL; else f_in = outR;
			f_out = -f_in + f_bufout;
#ifdef STYPE_FLOATINGPOINT
			f->buf[ f->buf_ptr ] = f_in + ( f_bufout * f->feedback );
#else
			STYPE_CALC f_v;
			f_v = f_in + ( f_bufout * f->feedback ) / 256;
			f->buf[ f->buf_ptr ] = f_v;
#endif
			if( a < MAX_ALLPASSES ) outL = f_out; else outR = f_out;
			if( ++f->buf_ptr >= f->buf_size ) f->buf_ptr = 0;
		    }

		    outL_ch[ i ] = (STYPE)( (STYPE_CALC)( ( outL / (16*INPUT_MUL) ) * ctl_wet ) / 256 );
		    outL_ch[ i ] += (STYPE)( (STYPE_CALC)( inL_ch[ i ] * ctl_dry ) / 256 );
		    if( inR_ch ) 
		    {
			outR_ch[ i ] = (STYPE)( (STYPE_CALC)( ( outR / (16*INPUT_MUL) ) * ctl_wet ) / 256 );
			outR_ch[ i ] += (STYPE)( (STYPE_CALC)( inR_ch[ i ] * ctl_dry ) / 256 );
		    }
		}

		if( outputs_num == 2 && data->ctl_width < 256 )
		{
		    int ctl_width = data->ctl_width / 2 + 128;
		    int ctl_width2 = 256 - ctl_width;
		    for( int i = 0; i < sample_frames; i++ )
		    {
			STYPE_CALC outL;
			STYPE_CALC outR;
			outL = 
			    ( (STYPE_CALC)outL_ch[ i ] * ctl_width ) / 256 + 
			    ( (STYPE_CALC)outR_ch[ i ] * ctl_width2 ) / 256;
			outR = 
			    ( (STYPE_CALC)outR_ch[ i ] * ctl_width ) / 256 + 
			    ( (STYPE_CALC)outL_ch[ i ] * ctl_width2 ) / 256;
			outL_ch[ i ] = outL;
			outR_ch[ i ] = outR;
		    }
		}
	    }

	    retval = 1;
	    break;

	case COMMAND_SET_GLOBAL_CONTROLLER:
	case COMMAND_SET_LOCAL_CONTROLLER:
	    data->filters_reinit_request = 1;
	    break;

	case COMMAND_CLOSE:
	    for( i = 0; i < data->combs_num * 2; i++ )
	    {
    		mem_free( data->combs[ i ].buf );
	    }
	    for( i = 0; i < data->allpasses_num * 2; i++ )
	    {
    		mem_free( data->allpasses[ i ].buf );
	    }
	    retval = 1;
	    break;
    }

    return retval;
}
