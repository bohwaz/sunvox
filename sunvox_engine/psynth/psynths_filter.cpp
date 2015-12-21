/*
    psynths_filter.cpp.
    This file is part of the SunVox engine.
    Copyright (C) 2002 - 2008 Alex Zolotov <nightradio@gmail.com>
*/

#include "psynth.h"

//Unique names for objects in your synth:
#define SYNTH_DATA	filter_data
#define SYNTH_HANDLER	psynth_filter
//And unique parameters:
#define SYNTH_INPUTS	2
#define SYNTH_OUTPUTS	2

struct filter_channel
{
    STYPE_CALC	d1, d2, d3;
};

/*
uint16 g_exp_table[ 256 ] = {
256, 256, 257, 258, 258, 259, 260, 260, 261, 262, 263, 263, 264, 265, 265, 266, 
267, 268, 268, 269, 270, 270, 271, 272, 273, 273, 274, 275, 276, 276, 277, 278, 
279, 279, 280, 281, 282, 282, 283, 284, 285, 286, 286, 287, 288, 289, 289, 290, 
291, 292, 293, 293, 294, 295, 296, 297, 297, 298, 299, 300, 301, 301, 302, 303, 
304, 305, 306, 306, 307, 308, 309, 310, 311, 311, 312, 313, 314, 315, 316, 317, 
317, 318, 319, 320, 321, 322, 323, 323, 324, 325, 326, 327, 328, 329, 330, 331, 
331, 332, 333, 334, 335, 336, 337, 338, 339, 340, 341, 342, 342, 343, 344, 345, 
346, 347, 348, 349, 350, 351, 352, 353, 354, 355, 356, 357, 358, 359, 360, 361, 
362, 363, 364, 364, 365, 366, 367, 368, 369, 370, 371, 372, 373, 375, 376, 377, 
378, 379, 380, 381, 382, 383, 384, 385, 386, 387, 388, 389, 390, 391, 392, 393, 
394, 395, 396, 398, 399, 400, 401, 402, 403, 404, 405, 406, 407, 408, 410, 411, 
412, 413, 414, 415, 416, 417, 419, 420, 421, 422, 423, 424, 425, 427, 428, 429, 
430, 431, 432, 434, 435, 436, 437, 438, 439, 441, 442, 443, 444, 445, 447, 448, 
449, 450, 452, 453, 454, 455, 456, 458, 459, 460, 461, 463, 464, 465, 466, 468, 
469, 470, 472, 473, 474, 475, 477, 478, 479, 481, 482, 483, 485, 486, 487, 488, 
490, 491, 492, 494, 495, 496, 498, 499, 501, 502, 503, 505, 506, 507, 509, 510,
};
*/

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
    CTYPE   ctl_volume;
    CTYPE   ctl_cutoff_freq;
    CTYPE   ctl_resonance;
    CTYPE   ctl_type;
    CTYPE   ctl_response;
    CTYPE   ctl_mode;
    CTYPE   ctl_impulse;
    CTYPE   ctl_mix;
    //Synth data: ##########################################################
    filter_channel fchan[ SYNTH_OUTPUTS ];
    uint16  *exp_table;
    int	    tick_counter;   //From 0 to tick_size
    CTYPE   floating_volume;
    CTYPE   floating_cutoff;
    CTYPE   floating_resonance;
};

int SYNTH_HANDLER( 
    PSYTEXX_SYNTH_PARAMETERS
    )
{
    psynth_net *pnet = (psynth_net*)net;
    SYNTH_DATA *data = (SYNTH_DATA*)data_ptr;
    int retval = 0;

    switch( command )
    {
	case COMMAND_GET_DATA_SIZE:
	    retval = sizeof( SYNTH_DATA );
	    break;

	case COMMAND_GET_SYNTH_NAME:
	    retval = (int)"Filter";
	    break;

	case COMMAND_GET_SYNTH_INFO:
	    retval = (int)"State Variable Filter\n(Chamberlin version)\nDouble Sampled\n\n\
References: \n\
http://musicdsp.org/archive.php?classid=3 \n\
Hal Chamberlin, \"Musical Applications of Microprocessors\"\n\
2nd Ed, Hayden Book Company 1985. pp 490-492.\n\n\
Use low \"response\" values\nfor smooth frequency, resonance\nor volume change";
	    break;

	case COMMAND_GET_INPUTS_NUM: retval = SYNTH_INPUTS; break;
	case COMMAND_GET_OUTPUTS_NUM: retval = SYNTH_OUTPUTS; break;

	case COMMAND_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT; break;

	case COMMAND_INIT:
	    psynth_register_ctl( synth_id, "Volume", "", 0, 256, 256, 0, &data->ctl_volume, net );
	    psynth_register_ctl( synth_id, "Freq", "Hz", 0, 14000, 14000, 0, &data->ctl_cutoff_freq, net );
	    psynth_register_ctl( synth_id, "Resonance", "", 0, 1530, 0, 0, &data->ctl_resonance, net );
	    psynth_register_ctl( synth_id, "Type", "l/h/b/n", 0, 3, 0, 1, &data->ctl_type, net );
	    psynth_register_ctl( synth_id, "Response", "", 0, 256, 256, 0, &data->ctl_response, net );
	    psynth_register_ctl( synth_id, "Mode", "HQ/HQmono/LQ/LQmono", 0, MODE_LQ_MONO, MODE_HQ, 1, &data->ctl_mode, net );
	    psynth_register_ctl( synth_id, "Impulse", "Hz", 0, 14000, 0, 0, &data->ctl_impulse, net );
	    psynth_register_ctl( synth_id, "Mix", "", 0, 256, 256, 0, &data->ctl_mix, net );
	    data->floating_volume = 256 * 256;
	    data->floating_cutoff = 14000 * 256;
	    data->floating_resonance = 0 * 256;
	    for( int i = 0; i < SYNTH_OUTPUTS; i++ )
	    {
		data->fchan[ i ].d1 = 0;
		data->fchan[ i ].d2 = 0;
		data->fchan[ i ].d3 = 0;
	    }
	    data->tick_counter = 0;
	    //data->exp_table = g_exp_table;
	    retval = 1;
	    break;

	case COMMAND_CLEAN:
	case COMMAND_ALL_NOTES_OFF:
	    for( int i = 0; i < SYNTH_OUTPUTS; i++ )
	    {
		data->fchan[ i ].d1 = 0;
		data->fchan[ i ].d2 = 0;
		data->fchan[ i ].d3 = 0;
	    }
	    data->tick_counter = 0;
	    retval = 1;
	    break;

	case COMMAND_RENDER_REPLACE:
	    if( !inputs[ 0 ] || !outputs[ 0 ] ) break;
	    
	    {
		if( data->ctl_mode == MODE_HQ_MONO || data->ctl_mode == MODE_LQ_MONO )
		    psynth_set_number_of_outputs( 1, synth_id, pnet );
		else
		    psynth_set_number_of_outputs( SYNTH_OUTPUTS, synth_id, pnet );
	    
		int tick_size = pnet->sampling_freq / 200;
		int ptr = 0;

		if( data->ctl_impulse )
		{
		    data->floating_cutoff = data->ctl_impulse * 256;
		    data->ctl_impulse = 0;
		}

		while( 1 )
		{
		    int buf_size = tick_size - data->tick_counter;
		    if( ptr + buf_size > sample_frames ) buf_size = sample_frames - ptr;

		    int outputs_num = psynth_get_number_of_outputs( synth_id, pnet );
		    for( int ch = 0; ch < outputs_num; ch++ )
		    {
			STYPE *in = inputs[ ch ];
			STYPE *out = outputs[ ch ];

			//References: 
			// http://musicdsp.org/archive.php?classid=3
			// Hal Chamberlin, "Musical Applications of Microprocessors," 2nd Ed, Hayden Book Company 1985. pp 490-492.

			int fs, c, f, q, scale;

			fs = pnet->sampling_freq;

			if( data->ctl_mode == MODE_HQ || data->ctl_mode == MODE_HQ_MONO )
			{
			    //HQ:
			    c = ( ( data->floating_cutoff / 256 ) * 32768 ) / ( fs * 2 );
			    f = ( 6433 * c ) / 32768;

			    if( f >= 1024 ) f = 1023;
			    q = 1536 - ( data->floating_resonance / 256 );
			    scale = q;
			}
			if( data->ctl_mode == MODE_LQ || data->ctl_mode == MODE_LQ_MONO )
			{
			    //LQ:
			    c = ( ( ( data->floating_cutoff / 256 ) / 2 ) * 32768 ) / fs;
			    f = ( 6433 * c ) / 32768;

			    if( f >= 1024 ) f = 1023;
			    q = 1536 - ( data->floating_resonance / 256 );
			    scale = q;
			}

			filter_channel *fchan = &data->fchan[ ch ];

			STYPE_CALC d1 = fchan->d1;
			STYPE_CALC d2 = fchan->d2;
			STYPE_CALC d3 = fchan->d3;

			int vol = ( data->floating_volume /256 );

			if( data->ctl_mix == 0 )
			{
			    //Filter disabled:
			    if( vol == 256 )
			    {
				for( int i = ptr; i < ptr + buf_size; i++ )
				    out[ i ] = in[ i ];
			    }
			    else
			    {
				for( int i = ptr; i < ptr + buf_size; i++ )
				{
				    STYPE_CALC inp = in[ i ];
				    inp *= vol;
				    inp /= 256;
    				    out[ i ] = (STYPE)inp;
				}
			    }
			}
			else
			{
			    //Filter enabled start:
			    vol *= data->ctl_mix;
			    vol /= 256;

			if( data->ctl_mode == MODE_HQ || data->ctl_mode == MODE_HQ_MONO )
			switch( data->ctl_type )
			{
			    case 0:
				for( int i = ptr; i < ptr + buf_size; i++ )
				{
				    STYPE_CALC inp = in[ i ];

				    denorm_add_white_noise( inp );

#ifndef STYPE_FLOATINGPOINT
				    inp *= 32;
#endif
				    STYPE_CALC low = d2 + ( f * d1 ) / 1024;
				    STYPE_CALC high = inp - low - ( q * d1 ) / 1024;
				    STYPE_CALC band = ( f * high ) / 1024 + d1;
				    STYPE_CALC slow = low;

				    low = low + ( f * band ) / 1024;
				    high = inp - low - ( q * band ) / 1024;
				    band = ( f * high ) / 1024 + band;

				    STYPE_CALC outp;
#ifndef STYPE_FLOATINGPOINT
				    outp = ( ( d2 + slow ) / 2 ) / 32;
#else
				    outp = ( d2 + slow ) / 2;
#endif
				    outp *= vol;
				    outp /= 256;

				    d1 = band;
				    d2 = low;

				    out[ i ] = (STYPE)outp;
				}
				break;
			    case 1:
				for( int i = ptr; i < ptr + buf_size; i++ )
				{
				    STYPE_CALC inp = in[ i ];

				    denorm_add_white_noise( inp );

#ifndef STYPE_FLOATINGPOINT
				    inp *= 32;
#endif
				    STYPE_CALC low = d2 + ( f * d1 ) / 1024;
				    STYPE_CALC high = inp - low - ( q * d1 ) / 1024;
				    STYPE_CALC band = ( f * high ) / 1024 + d1;
				    STYPE_CALC shigh = high;

  				    low = low + ( f * band ) / 1024;
				    high = inp - low - ( q * band ) / 1024;
				    band = ( f * high ) / 1024 + band;

				    STYPE_CALC outp;
#ifndef STYPE_FLOATINGPOINT
				    outp = ( ( d3 + shigh ) / 2 ) / 32;
#else
				    outp = ( d3 + shigh ) / 2;
#endif
				    outp *= vol;
				    outp /= 256;

				    d1 = band;
				    d2 = low;
				    d3 = high;

				    out[ i ] = (STYPE)outp;
				}
				break;
			    case 2:
				for( int i = ptr; i < ptr + buf_size; i++ )
				{
				    STYPE_CALC inp = in[ i ];

				    denorm_add_white_noise( inp );

#ifndef STYPE_FLOATINGPOINT
				    inp *= 32;
#endif
				    STYPE_CALC low = d2 + ( f * d1 ) / 1024;
				    STYPE_CALC high = inp - low - ( q * d1 ) / 1024;
				    STYPE_CALC band = ( f * high ) / 1024 + d1;
				    STYPE_CALC sband = band;

				    low = low + ( f * band ) / 1024;
				    high = inp - low - ( q * band ) / 1024;
				    band = ( f * high ) / 1024 + band;

				    STYPE_CALC outp;
#ifndef STYPE_FLOATINGPOINT
				    outp = ( ( d1 + sband ) / 2 ) / 32;
#else
				    outp = ( d1 + sband ) / 2;
#endif
				    outp *= vol;
				    outp /= 256;

				    d1 = band;
				    d2 = low;

				    out[ i ] = (STYPE)outp;
				}
				break;
			    case 3:
				for( int i = ptr; i < ptr + buf_size; i++ )
				{
				    STYPE_CALC inp = in[ i ];

				    denorm_add_white_noise( inp );

#ifndef STYPE_FLOATINGPOINT
				    inp *= 32;
#endif
				    STYPE_CALC low = d2 + ( f * d1 ) / 1024;
				    STYPE_CALC high = inp - low - ( q * d1 ) / 1024;
				    STYPE_CALC band = ( f * high ) / 1024 + d1;
				    STYPE_CALC snotch = high + low;

				    low = low + ( f * band ) / 1024;
				    high = inp - low - ( q * band ) / 1024;
				    band = ( f * high ) / 1024 + band;

				    STYPE_CALC outp;
#ifndef STYPE_FLOATINGPOINT
				    outp = ( ( d3 + snotch ) / 2 ) / 32;
#else
				    outp = ( d3 + snotch ) / 2;
#endif
				    outp *= vol;
				    outp /= 256;

				    d1 = band;
				    d2 = low;
				    d3 = high + low;

				    out[ i ] = (STYPE)outp;
				}
				break;
			}

			if( data->ctl_mode == MODE_LQ || data->ctl_mode == MODE_LQ_MONO )
			switch( data->ctl_type )
			{
			    case 0:
				for( int i = ptr; i < ptr + buf_size; i++ )
				{
				    STYPE_CALC inp = in[ i ];

				    denorm_add_white_noise( inp );

#ifndef STYPE_FLOATINGPOINT
				    inp *= 32;
#endif
				    STYPE_CALC low = d2 + ( f * d1 ) / 1024;
				    STYPE_CALC high = inp - low - ( q * d1 ) / 1024;
				    STYPE_CALC band = ( f * high ) / 1024 + d1;

				    STYPE_CALC outp;
#ifndef STYPE_FLOATINGPOINT
				    outp = low / 32;
#else
				    outp = low;
#endif
				    outp *= vol;
				    outp /= 256;

				    d1 = band;
				    d2 = low;

				    out[ i ] = (STYPE)outp;
				}
				break;
			    case 1:
				for( int i = ptr; i < ptr + buf_size; i++ )
				{
				    STYPE_CALC inp = in[ i ];

				    denorm_add_white_noise( inp );

#ifndef STYPE_FLOATINGPOINT
				    inp *= 32;
#endif
				    STYPE_CALC low = d2 + ( f * d1 ) / 1024;
				    STYPE_CALC high = inp - low - ( q * d1 ) / 1024;
				    STYPE_CALC band = ( f * high ) / 1024 + d1;

				    STYPE_CALC outp;
#ifndef STYPE_FLOATINGPOINT
				    outp = high / 32;
#else
				    outp = high;
#endif
				    outp *= vol;
				    outp /= 256;

				    d1 = band;
				    d2 = low;

				    out[ i ] = (STYPE)outp;
				}
				break;
			    case 2:
				for( int i = ptr; i < ptr + buf_size; i++ )
				{
				    STYPE_CALC inp = in[ i ];

				    denorm_add_white_noise( inp );

#ifndef STYPE_FLOATINGPOINT
				    inp *= 32;
#endif
				    STYPE_CALC low = d2 + ( f * d1 ) / 1024;
				    STYPE_CALC high = inp - low - ( q * d1 ) / 1024;
				    STYPE_CALC band = ( f * high ) / 1024 + d1;

				    STYPE_CALC outp;
#ifndef STYPE_FLOATINGPOINT
				    outp = band / 32;
#else
				    outp = band;
#endif
				    outp *= vol;
				    outp /= 256;

				    d1 = band;
				    d2 = low;

				    out[ i ] = (STYPE)outp;
				}
				break;
			    case 3:
				for( int i = ptr; i < ptr + buf_size; i++ )
				{
				    STYPE_CALC inp = in[ i ];

				    denorm_add_white_noise( inp );

#ifndef STYPE_FLOATINGPOINT
				    inp *= 32;
#endif
				    STYPE_CALC low = d2 + ( f * d1 ) / 1024;
				    STYPE_CALC high = inp - low - ( q * d1 ) / 1024;
				    STYPE_CALC band = ( f * high ) / 1024 + d1;

				    STYPE_CALC outp;
#ifndef STYPE_FLOATINGPOINT
				    outp = ( high + low ) / 32;
#else
				    outp = high + low;
#endif
				    outp *= vol;
				    outp /= 256;

				    d1 = band;
				    d2 = low;

				    out[ i ] = (STYPE)outp;
				}
				break;
			}

			    if( data->ctl_mix < 256 )
			    {
				//Mix result with source:
				int vol2 = ( data->floating_volume / 256 );
				vol2 *= ( 256 - data->ctl_mix );
				vol2 /= 256;
				for( int i = ptr; i < ptr + buf_size; i++ )
				{
				    STYPE_CALC inp = in[ i ];
				    inp *= vol2;
				    inp /= 256;
				    inp += out[ i ];
				    out[ i ] = (STYPE)inp;
				}
			    }
			    //...filter enabled end.
			}

			fchan->d1 = d1;
			fchan->d2 = d2;
			fchan->d3 = d3;
		    }

		    ptr += buf_size;
		    data->tick_counter += buf_size;
		    if( data->tick_counter >= tick_size ) 
		    {
			//Handle filter's tick:
			if( data->floating_cutoff / 256 > data->ctl_cutoff_freq )
			{
			    data->floating_cutoff -= data->ctl_response * 14000;
			    if( data->floating_cutoff / 256 < data->ctl_cutoff_freq )
				data->floating_cutoff = data->ctl_cutoff_freq * 256;
			}
			else
			if( data->floating_cutoff / 256 < data->ctl_cutoff_freq )
			{
			    data->floating_cutoff += data->ctl_response * 14000;
			    if( data->floating_cutoff / 256 > data->ctl_cutoff_freq )
				data->floating_cutoff = data->ctl_cutoff_freq * 256;
			}
			if( data->floating_resonance / 256 > data->ctl_resonance )
			{
			    data->floating_resonance -= data->ctl_response * 1530;
			    if( data->floating_resonance / 256 < data->ctl_resonance )
				data->floating_resonance = data->ctl_resonance * 256;
			}
			else
			if( data->floating_resonance / 256 < data->ctl_resonance )
			{
			    data->floating_resonance += data->ctl_response * 1530;
			    if( data->floating_resonance / 256 > data->ctl_resonance )
				data->floating_resonance = data->ctl_resonance * 256;
			}
			if( data->floating_volume / 256 > data->ctl_volume )
			{
			    data->floating_volume -= data->ctl_response * 256;
			    if( data->floating_volume / 256 < data->ctl_volume )
				data->floating_volume = data->ctl_volume * 256;
			}
			else
			if( data->floating_volume / 256 < data->ctl_volume )
			{
			    data->floating_volume += data->ctl_volume * 256;
			    if( data->floating_volume / 256 > data->ctl_volume )
				data->floating_volume = data->ctl_volume * 256;
			}
			data->tick_counter = 0;
		    }
		    if( ptr >= sample_frames ) break;
		}
	    }
	    retval = 1;
	    break;

	case COMMAND_CLOSE:
	    retval = 1;
	    break;
    }

    return retval;
}
