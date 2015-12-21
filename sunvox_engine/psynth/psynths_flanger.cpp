/*
    psynths_flanger.cpp.
    This file is part of the SunVox engine.
    Copyright (C) 2002 - 2008 Alex Zolotov <nightradio@gmail.com>
*/

#include "psynth.h"
#include "core/debug.h"

//Unique names for objects in your synth:
#define SYNTH_DATA	psynth_flanger_data
#define SYNTH_HANDLER	psynth_flanger
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
    CTYPE   ctl_response;
    CTYPE   ctl_vibrato_speed;
    CTYPE   ctl_vibrato_power;
    CTYPE   ctl_vibrato_type;
    CTYPE   ctl_set_vibrato_phase;
    //Synth data: ##########################################################
    int	    buf_size;
    STYPE   *buf[ SYNTH_OUTPUTS ];
    int	    buf_ptr;
    int	    tick_counter; //From 0 to tick_size
    CTYPE   floating_delay;
    CTYPE   prev_floating_delay;
    ulong   vibrato_ptr; //From 0 to 4095
    uchar   *vibrato_tab;
};

#define RESP_PREC		    19
#define PREC			    10
#define MASK			    1023
#define INTERP( val1, val2, ptr )   ( ( val1 * (MASK - ((ptr)&MASK)) + val2 * ((ptr)&MASK) ) / ( 1 << PREC ) )

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
	    retval = (int)"Flanger";
	    break;

	case COMMAND_GET_SYNTH_INFO:
	    retval = (int)"Flanger.\nMax delay is 1/64 second\n\nUse low \"response\" values\nfor smooth delay changing";
	    break;

	case COMMAND_GET_INPUTS_NUM: retval = SYNTH_INPUTS; break;
	case COMMAND_GET_OUTPUTS_NUM: retval = SYNTH_OUTPUTS; break;

	case COMMAND_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT; break;

	case COMMAND_INIT:
	    psynth_register_ctl( synth_id, "Dry", "", 0, 256, 256, 0, &data->ctl_dry, net );
	    psynth_register_ctl( synth_id, "Wet", "", 0, 256, 128, 0, &data->ctl_wet, net );
	    psynth_register_ctl( synth_id, "Feedback", "", 0, 256, 128, 0, &data->ctl_feedback, net );
	    psynth_register_ctl( synth_id, "Delay", "", 8, 1000, 200, 0, &data->ctl_delay, net );
	    psynth_register_ctl( synth_id, "Response", "", 0, 256, 10, 0, &data->ctl_response, net );
	    psynth_register_ctl( synth_id, "Vibrato speed", "", 0, 512, 8, 0, &data->ctl_vibrato_speed, net );
	    psynth_register_ctl( synth_id, "Vibrato power", "", 0, 256, 32, 0, &data->ctl_vibrato_power, net );
	    psynth_register_ctl( synth_id, "Vibrato type", "hsin/sin", 0, 1, 0, 1, &data->ctl_vibrato_type, net );
	    psynth_register_ctl( synth_id, "Set vib. phase", "", 0, 256, 0, 0, &data->ctl_set_vibrato_phase, net );
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
	    data->floating_delay = 1000 << RESP_PREC;
	    data->tick_counter = 0;
	    data->vibrato_ptr = 0;
	    data->vibrato_tab = g_vibrato_tab;
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
	    data->tick_counter = 0;
	    data->vibrato_ptr = 0;
	    retval = 1;
	    break;

	case COMMAND_RENDER_REPLACE:
	    if( !inputs[ 0 ] || !outputs[ 0 ] ) break;
	    
	    {
		int tick_size = pnet->sampling_freq / 200;
		int ptr = 0;

		while( 1 )
		{
		    int new_sample_frames = tick_size - data->tick_counter;
		    if( ptr + new_sample_frames > sample_frames ) new_sample_frames = sample_frames - ptr;

		    int vib_value;
		    if( data->ctl_vibrato_type == 0 )
		    {
			//hsin:
			vib_value = (int)data->vibrato_tab[ data->vibrato_ptr / 16 ] * 2 - 256;
		    }
		    else
		    if( data->ctl_vibrato_type == 1 )
		    {
			//sin:
			if( data->vibrato_ptr / 8 < 256 )
			    vib_value = (int)data->vibrato_tab[ data->vibrato_ptr / 8 ];
			else
			    vib_value = -(int)data->vibrato_tab[ ( data->vibrato_ptr / 8 ) & 255 ];
		    }
		    vib_value *= data->ctl_vibrato_power;
		    vib_value /= 256;

		    int cur_delay = data->ctl_delay;
		    //Control delay center:
		    if( cur_delay - data->ctl_vibrato_power < 8 )
			cur_delay = 8 + data->ctl_vibrato_power;
		    if( cur_delay + data->ctl_vibrato_power > 1000 )
			cur_delay = 1000 - data->ctl_vibrato_power;
		    //Add vibrato:
		    cur_delay += vib_value;

		    ulong floating_delay_step = (ulong)( ( data->ctl_response * 1024 ) / 256 ) * ( ( 256 << RESP_PREC ) / (ulong)pnet->sampling_freq );

		    int buf_ptr = 0;
		    int floating_delay = 0;
		    for( ch = 0; ch < SYNTH_OUTPUTS; ch++ )
		    {
			STYPE *in = inputs[ ch ];
			STYPE *out = outputs[ ch ];
			STYPE *cbuf = data->buf[ ch ];
			buf_ptr = data->buf_ptr;
			floating_delay = data->floating_delay;
			for( i = ptr; i < ptr + new_sample_frames; i++ )
			{
			    int sub1 = ( ( floating_delay >> RESP_PREC ) * data->buf_size ) / 1024;
			    int sub2 = ( ( ( floating_delay >> RESP_PREC ) + 1 ) * data->buf_size ) / 1024;

			    int ptr2 = buf_ptr - sub1;
			    if( ptr2 < 0 ) ptr2 += data->buf_size;
			    int ptr3 = buf_ptr - sub2;
			    if( ptr3 < 0 ) ptr3 += data->buf_size;

			    //STYPE_CALC buf_val = cbuf[ ptr2 ];
			    STYPE_CALC buf_val = INTERP( cbuf[ ptr2 ], cbuf[ ptr3 ], ( floating_delay >> (RESP_PREC-PREC) ) & MASK );
			    
			    STYPE_CALC out_val = buf_val;
			    out_val *= data->ctl_wet;
			    out_val /= 256;
			    out[ i ] = out_val;

			    out_val = in[ i ];
			    out_val *= data->ctl_dry;
			    out_val /= 256;
			    out[ i ] += out_val;

			    out_val = buf_val;
			    out_val *= data->ctl_feedback;
			    out_val /= 256;
			    denorm_add_white_noise( out_val );
			    cbuf[ buf_ptr ] = in[ i ] + out_val;

			    buf_ptr++;
			    if( buf_ptr >= data->buf_size ) buf_ptr = 0;

			    if( ( floating_delay >> RESP_PREC ) > cur_delay )
			    {
				floating_delay -= floating_delay_step;
				if( ( floating_delay >> RESP_PREC ) < cur_delay )
				    floating_delay = cur_delay << RESP_PREC;
			    }
			    else
			    if( ( floating_delay >> RESP_PREC ) < cur_delay )
			    {
				floating_delay += floating_delay_step;
				if( ( floating_delay >> RESP_PREC ) > cur_delay )
				    floating_delay = cur_delay << RESP_PREC;
			    }
			}
		    }
		    data->buf_ptr = buf_ptr;
		    data->floating_delay = floating_delay;

		    ptr += new_sample_frames;
		    data->tick_counter += new_sample_frames;
		    if( data->tick_counter >= tick_size ) 
		    {
			//Handle one tick:
			//Vibrato:
			data->vibrato_ptr += data->ctl_vibrato_speed;
			data->vibrato_ptr &= 4095;
			data->tick_counter = 0;
		    }
		    if( ptr >= sample_frames ) break;
		}
	    }
	    retval = 1;
	    break;

	case COMMAND_SET_GLOBAL_CONTROLLER:
	    if( pnet->ctl_num == 8 )
	    {
		// "set vibrato phase" controller selected:
		data->vibrato_ptr = ( pnet->ctl_val / 8 ) & 4095;
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
