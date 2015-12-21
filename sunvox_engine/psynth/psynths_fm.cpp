/*
    psynths_fm.cpp.
    This file is part of the SunVox engine.
    Copyright (C) 2002 - 2008 Alex Zolotov <nightradio@gmail.com>
*/

#include "psynth.h"
#include "core/debug.h"

//Unique names for objects in your synth:
#define SYNTH_DATA	psynth_fm_data
#define SYNTH_HANDLER	psynth_fm
//And unique parameters:
#define SYNTH_INPUTS	0
#define SYNTH_OUTPUTS	2

#define MAX_CHANNELS	16

#define FM_SFREQ	44100
#define FM_ENV_STEP	256
#define FM_TABLE_AMP_SH	14
#define FM_TABLE_AMP	( 1 << FM_TABLE_AMP_SH )
#define FM_SINUS_SH	12
#define FM_SINUS_SIZE	( 1 << FM_SINUS_SH )
#define FM_MOD_DIV	( 32768 / ( FM_SINUS_SIZE * 8 ) ) /* max phase offset = FM_SINUS_SIZE * 8 */

#define FM_PI		3.141592653
#define FM_2PI		( FM_PI * 2 )

#define GET_FREQ(res,per)  \
{ \
    if( per >= 0 ) \
	res = ( data->linear_tab[ per % 768 ] >> ( per / 768 ) ); \
    else \
	res = ( data->linear_tab[ (7680*4+per) % 768 ] << -( ( (7680*4+per) / 768 ) - (7680*4)/768 ) ); /*if period is negative value*/ \
}
#define GET_DELTA(f,resh,resl)   \
{ \
    resh = f / FM_SFREQ; \
    resl = ( ( ( f % FM_SFREQ ) << 14 ) / FM_SFREQ ) << 2; /*Max sampling freq = 256 kHz*/ \
}

struct gen_channel
{
    int	    playing;
    ulong   id;
    int	    vel;
    ulong   cptr;
    ulong   mptr;
    int	    c_env_ptr;
    int	    m_env_ptr;
    ulong   cdelta;
    int	    sustain;
    int	    note;
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
    CTYPE   ctl_cvolume;
    CTYPE   ctl_mvolume;
    CTYPE   ctl_pan;
    CTYPE   ctl_cmul;
    CTYPE   ctl_mmul;
    CTYPE   ctl_mfeedback;
    CTYPE   ctl_ca;
    CTYPE   ctl_cd;
    CTYPE   ctl_cs;
    CTYPE   ctl_cr;
    CTYPE   ctl_ma;
    CTYPE   ctl_md;
    CTYPE   ctl_ms;
    CTYPE   ctl_mr;
    CTYPE   ctl_mscaling;
    CTYPE   ctl_channels;
    CTYPE   ctl_mode;
    //Synth data: ##########################################################
    gen_channel   channels[ MAX_CHANNELS ];
    int	    search_ptr;
    ulong   *linear_tab;
    int16   *sin_tab;
    int16   *cvolume_table;
    int16   *mvolume_table;
    int	    cvolume_table_size;
    int	    mvolume_table_size;
    int	    tables_render_request;
    ulong   resample_ptr;
};

void render_level( int16 *table, int x1, int y1, int x2, int y2 )
{
    int size = ( x2 - x1 );
    if( size == 0 ) size = 1;
    int delta = ( ( y2 - y1 ) << 16 ) / size;
    int cy = y1 << 16;
    for( int cx = x1; cx <= x2; cx++ )
    {
	table[ cx ] = (int16)( cy >> 16 );
	cy += delta;
    }
}

void render_volume_tables( SYNTH_DATA *data )
{
    //Carrier:
    int x = data->ctl_ca;
    int y = FM_TABLE_AMP;
    render_level( data->cvolume_table, 0, 0, x, y );
    int x2 = x + data->ctl_cd;
    int y2 = data->ctl_cs * ( FM_TABLE_AMP / 256 );
    render_level( data->cvolume_table, x, y, x2, y2 );
    x = x2 + data->ctl_cr;
    y = 0;
    render_level( data->cvolume_table, x2, y2, x, y );
    data->cvolume_table_size = x;

    //Modulator:
    x = data->ctl_ma;
    y = data->ctl_mvolume * ( FM_TABLE_AMP / 256 );
    render_level( data->mvolume_table, 0, 0, x, y );
    x2 = x + data->ctl_md;
    y2 = ( ( data->ctl_mvolume * data->ctl_ms ) / 256 ) * ( FM_TABLE_AMP / 256 );
    render_level( data->mvolume_table, x, y, x2, y2 );
    x = x2 + data->ctl_mr;
    y = 0;
    render_level( data->mvolume_table, x2, y2, x, y );
    data->mvolume_table_size = x;
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
    ulong resample_delta;
    int resample_frames;

    switch( command )
    {
	case COMMAND_GET_DATA_SIZE:
	    retval = sizeof( SYNTH_DATA );
	    break;

	case COMMAND_GET_SYNTH_NAME:
	    retval = (int)"FM";
	    break;

	case COMMAND_GET_SYNTH_INFO:
	    retval = (int)"FM Synthesizer.\n(Frequency Modulation)\n\"C\" - carrier\n\"M\" - modulator\nInternal sampling frequency: 44100 Hz";
	    break;

	case COMMAND_GET_INPUTS_NUM: retval = SYNTH_INPUTS; break;
	case COMMAND_GET_OUTPUTS_NUM: retval = SYNTH_OUTPUTS; break;

	case COMMAND_GET_FLAGS: retval = PSYNTH_FLAG_GENERATOR; break;

	case COMMAND_INIT:
	    psynth_register_ctl( synth_id, "C.Volume", "", 0, 256, 128, 0, &data->ctl_cvolume, net );
	    psynth_register_ctl( synth_id, "M.Volume", "", 0, 256, 48, 0, &data->ctl_mvolume, net );
	    psynth_register_ctl( synth_id, "Panning", "", 0, 256, 128, 0, &data->ctl_pan, net );
	    psynth_register_ctl( synth_id, "C.Freq mul", "", 0, 16, 1, 0, &data->ctl_cmul, net );
	    psynth_register_ctl( synth_id, "M.Freq mul", "", 0, 16, 1, 0, &data->ctl_mmul, net );
	    psynth_register_ctl( synth_id, "M.Feedback", "", 0, 256, 0, 0, &data->ctl_mfeedback, net );
	    psynth_register_ctl( synth_id, "C.Attack", "", 0, 512, 32, 0, &data->ctl_ca, net );
	    psynth_register_ctl( synth_id, "C.Decay", "", 0, 512, 32, 0, &data->ctl_cd, net );
	    psynth_register_ctl( synth_id, "C.Sustain", "", 0, 256, 128, 0, &data->ctl_cs, net );
	    psynth_register_ctl( synth_id, "C.Release", "", 0, 512, 64, 0, &data->ctl_cr, net );
	    psynth_register_ctl( synth_id, "M.Attack", "", 0, 512, 32, 0, &data->ctl_ma, net );
	    psynth_register_ctl( synth_id, "M.Decay", "", 0, 512, 32, 0, &data->ctl_md, net );
	    psynth_register_ctl( synth_id, "M.Sustain", "", 0, 256, 128, 0, &data->ctl_ms, net );
	    psynth_register_ctl( synth_id, "M.Release", "", 0, 512, 64, 0, &data->ctl_mr, net );
	    psynth_register_ctl( synth_id, "M.Scaling", "", 0, 4, 0, 0, &data->ctl_mscaling, net );
	    psynth_register_ctl( synth_id, "Polyphony", "ch.", 1, MAX_CHANNELS, 4, 1, &data->ctl_channels, net );
	    psynth_register_ctl( synth_id, "Mode", "HQ/HQmono/LQ/LQmono", 0, MODES - 1, MODE_HQ, 1, &data->ctl_mode, net );
	    for( int c = 0; c < MAX_CHANNELS; c++ )
	    {
		data->channels[ c ].playing = 0;
		data->channels[ c ].cptr = 0;
		data->channels[ c ].mptr = 0;
		data->channels[ c ].c_env_ptr = 0;
		data->channels[ c ].m_env_ptr = 0;
		data->channels[ c ].sustain = 0;
	    }
	    data->search_ptr = 0;
	    data->linear_tab = g_linear_tab;
	    data->resample_ptr = 0;
	    data->cvolume_table = (int16*)MEM_NEW( HEAP_DYNAMIC, 1540 * sizeof( int16 ) );
	    data->mvolume_table = (int16*)MEM_NEW( HEAP_DYNAMIC, 1540 * sizeof( int16 ) );
	    data->cvolume_table_size = 0;
	    data->mvolume_table_size = 0;
	    data->tables_render_request = 1;
	    mem_off();
	    data->sin_tab = (int16*)MEM_NEW( HEAP_STORAGE, FM_SINUS_SIZE * sizeof( int16 ) );
	    {
		for( i = 0; i < 256; i++ )
		{
		    data->sin_tab[ i ] = (int16)g_vibrato_tab[ i ] << 7;
		    data->sin_tab[ i + 256 ] = -(int16)g_vibrato_tab[ i ] << 7;
		}
		int i2;
		for( i2 = 0; i2 < 16; i2++ )
		{
		    for( i = 0; i < 512; i++ )
		    {
			int res = ( 
			    (int)data->sin_tab[ ( i - 1 ) & 511 ] + 
			    (int)data->sin_tab[ ( i + 1 ) & 511 ]
			    ) / 2;
			data->sin_tab[ i ] = (int16)res;
		    }
		}
		for( i = FM_SINUS_SIZE - 1; i >= 0; i-- )
		    data->sin_tab[ i ] = data->sin_tab[ i / ( FM_SINUS_SIZE / 512 ) ];
		for( i2 = 0; i2 < ( FM_SINUS_SIZE / 512 ); i2++ )
		{
		    for( i = 0; i < FM_SINUS_SIZE; i++ )
		    {
			int res = ( 
			    (int)data->sin_tab[ ( i - 1 ) & ( FM_SINUS_SIZE - 1 ) ] + 
			    (int)data->sin_tab[ ( i + 1 ) & ( FM_SINUS_SIZE - 1 ) ]
			    ) / 2;
			data->sin_tab[ i ] = (int16)res;
		    }
		}
	    }
	    mem_on();
	    retval = 1;
	    break;

	case COMMAND_CLEAN:
	    for( int c = 0; c < MAX_CHANNELS; c++ )
	    {
		data->channels[ c ].playing = 0;
	    }
	    data->resample_ptr = 0;
	    retval = 1;
	    break;

	case COMMAND_RENDER_REPLACE:
	    if( !outputs[ 0 ] ) break;
	    //Check number of channels: ************************************************
	    if( data->ctl_mode == MODE_HQ_MONO || data->ctl_mode == MODE_LQ_MONO )
		psynth_set_number_of_outputs( 1, synth_id, pnet );
	    else
		psynth_set_number_of_outputs( SYNTH_OUTPUTS, synth_id, pnet );
	    //**************************************************************************
	    if( data->tables_render_request )
	    {
		render_volume_tables( data );
		data->tables_render_request = 0;
	    }
#ifndef ONLY44100
	    resample_delta = ( FM_SFREQ << 10 ) / pnet->sampling_freq;
	    resample_frames = ( ( data->resample_ptr + sample_frames * resample_delta ) >> 10 ) - ( data->resample_ptr >> 10 );
	    if( resample_frames == 0 ) resample_frames = 1;
#else
	    resample_frames = sample_frames;
#endif
	    for( int c = 0; c < data->ctl_channels; c++ )
	    {
		if( data->channels[ c ].playing )
		{
		    gen_channel *chan = &data->channels[ c ];
		    ulong cdelta = chan->cdelta;
		    if( data->ctl_cmul > 0 )
			cdelta *= data->ctl_cmul;
		    else
			cdelta /= 2;
		    ulong mdelta;
		    if( data->ctl_mmul > 0 )
			mdelta = chan->cdelta * data->ctl_mmul;
		    else
			mdelta = chan->cdelta / 2;
		    ulong cptr;
		    ulong mptr;
		    int c_env_ptr;
		    int m_env_ptr;
		    int c_env_sustain_ptr = ( data->ctl_ca + data->ctl_cd ) * FM_ENV_STEP;
		    int m_env_sustain_ptr = ( data->ctl_ma + data->ctl_md ) * FM_ENV_STEP;
		    int mfeedback = data->ctl_mfeedback;
		    int16 *sin_tab = data->sin_tab;
		    int16 *cvolume_table = data->cvolume_table;
		    int16 *mvolume_table = data->mvolume_table;
		    int cvolume_table_size = data->cvolume_table_size;
		    int mvolume_table_size = data->mvolume_table_size;
		    int ctl_mscaling = data->ctl_mscaling;
		    int mscaling;
		    for( i = 0; i < ctl_mscaling; i++ )
		    {
			if( i == 0 )
			    mscaling = 128 - chan->note;
			else
			    mscaling = ( mscaling * ( 128 - chan->note ) ) / 128;
		    }
		    int ctl_mode = data->ctl_mode;
		    int sustain;
		    int playing;
		    int outputs_num = psynth_get_number_of_outputs( synth_id, pnet );
		    for( ch = 0; ch < outputs_num; ch++ )
		    {
			int cvol = ( data->ctl_cvolume * chan->vel ) >> 8;
			STYPE *in = inputs[ ch ];
			STYPE *out = outputs[ ch ];
        		cptr = chan->cptr;
        		mptr = chan->mptr;
			c_env_ptr = chan->c_env_ptr;
			m_env_ptr = chan->m_env_ptr;
			sustain = chan->sustain;
			playing = chan->playing;
			if( data->ctl_pan < 128 )
			{
			    if( ch == 1 ) { cvol *= data->ctl_pan; cvol >>= 7; }
			}
			else
			{
			    if( ch == 0 ) { cvol *= 128 - ( data->ctl_pan - 128 ); cvol >>= 7; }
			}
			for( i = 0; i < resample_frames; i++ )
			{
			    int cval;
			    int mvol;
			    if( ctl_mode < MODE_LQ )
			    {
				int coef = m_env_ptr & ( FM_ENV_STEP - 1 );
				mvol = 
				    ( coef * mvolume_table[ ( m_env_ptr / FM_ENV_STEP ) + 1 ] +
				    ( FM_ENV_STEP - coef ) * mvolume_table[ m_env_ptr / FM_ENV_STEP ] ) / FM_ENV_STEP;
			    }
			    else
			    {
				mvol = mvolume_table[ m_env_ptr / FM_ENV_STEP ];
			    }
			    if( ctl_mscaling ) mvol = ( mvol * mscaling ) / 128;
			    if( mfeedback == 0 )
			    {
				cval = sin_tab[ ( ( sin_tab[ ( mptr >> (22-FM_SINUS_SH) ) & (FM_SINUS_SIZE-1) ] * mvol ) / (FM_TABLE_AMP*FM_MOD_DIV) + ( cptr >> (22-FM_SINUS_SH) ) ) & (FM_SINUS_SIZE-1) ];
				/*cval = 
				    (int)( sin( 
					sin( 
					    FM_2PI * ( (float)(mptr&((1<<22)-1)) / (float)(1<<22) )
					) 
					* ( (float)mvol / (float)FM_TABLE_AMP )
					* FM_2PI * 8
					+ ( FM_2PI * ( (float)(cptr&((1<<22)-1)) / (float)(1<<22) ) )
				    ) * 32767 );*/
			    }
			    else
			    {
				int fb = ( sin_tab[ ( mptr >> (22-FM_SINUS_SH) ) & (FM_SINUS_SIZE-1) ] * mfeedback ) / (256*FM_MOD_DIV);
				cval = sin_tab[ ( ( sin_tab[ ( ( mptr >> (22-FM_SINUS_SH) ) + fb ) & (FM_SINUS_SIZE-1) ] * mvol ) / (FM_TABLE_AMP*FM_MOD_DIV) + ( cptr >> (22-FM_SINUS_SH) ) ) & (FM_SINUS_SIZE-1) ];
			    }
			    cval *= cvol;
			    cval /= 256;
			    if( ctl_mode < MODE_LQ )
			    {
				int coef = c_env_ptr & ( FM_ENV_STEP - 1 );
				cval *= ( coef * cvolume_table[ ( c_env_ptr / FM_ENV_STEP ) + 1 ] +
					( FM_ENV_STEP - coef ) * cvolume_table[ c_env_ptr / FM_ENV_STEP ] ) / FM_ENV_STEP;
				cval /= FM_TABLE_AMP;
			    }
			    else
			    {
				cval *= cvolume_table[ c_env_ptr / FM_ENV_STEP ];
				cval /= FM_TABLE_AMP;
			    }
			    STYPE_CALC out_val;
			    INT16_TO_STYPE( out_val, cval );
			    if( retval ) 
				out[ i ] += out_val;
			    else
				out[ i ] = out_val;
			    cptr += cdelta;
			    mptr += mdelta;
			    c_env_ptr++;
			    m_env_ptr++;
			    if( sustain )
			    {
				if( c_env_ptr > c_env_sustain_ptr )
				    c_env_ptr = c_env_sustain_ptr;
				if( m_env_ptr > m_env_sustain_ptr )
				    m_env_ptr = m_env_sustain_ptr;
			    }
			    else
			    {
				if( c_env_ptr >= cvolume_table_size * FM_ENV_STEP )
				{
				    playing = 0;
				    break;
				}
				if( m_env_ptr >= mvolume_table_size * FM_ENV_STEP )
				    m_env_ptr = ( mvolume_table_size - 1 ) * FM_ENV_STEP;
			    }
			}
			if( playing == 0 && retval == 0 )
			{
			    for( i; i < resample_frames; i++ ) out[ i ] = 0;
			}
		    }
		    chan->cptr = cptr;
		    chan->mptr = mptr;
		    chan->c_env_ptr = c_env_ptr;
		    chan->m_env_ptr = m_env_ptr;
		    chan->sustain = sustain;
		    chan->playing = playing;
		    retval = 1;
		}
	    }
#ifndef ONLY44100
	    data->resample_ptr += sample_frames * resample_delta;
	    if( retval && resample_frames != sample_frames )
	    {
		for( ch = 0; ch < SYNTH_OUTPUTS; ch++ )
		{
		    STYPE *out = outputs[ ch ];
		    int rp = 0;
		    int rp_delta = ( resample_frames << 15 ) / sample_frames;
		    rp = rp_delta * sample_frames;
    	    	    for( i = sample_frames - 1; i >= 0; i-- )
		    {
			rp -= rp_delta;
			if( rp < 0 ) rp = 0;
			int rp2 = rp >> 15;
			int v = rp & 32767;
			if( rp2 >= resample_frames - 1 )
			{
			    out[ i ] = out[ rp2 ];
			}
			else
			{
			    STYPE_CALC res = ( v * out[ rp2 + 1 ] + (32767-v) * out[ rp2 ] ) / 32768;
			    out[ i ] = res;
			}
		    }
		}
	    }
#endif
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
		data->channels[ c ].cdelta = delta_l | ( delta_h << 16 );
		data->channels[ c ].cptr = 0;
		data->channels[ c ].mptr = 0;
		data->channels[ c ].c_env_ptr = 0;
		data->channels[ c ].m_env_ptr = 0;
		data->channels[ c ].id = pnet->channel_id;
		data->channels[ c ].note = pnet->note;
		data->channels[ c ].sustain = 1;
		
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

		    data->channels[ c ].cdelta = delta_l | ( delta_h << 16 );
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

	case COMMAND_SET_GLOBAL_CONTROLLER:
	case COMMAND_SET_LOCAL_CONTROLLER:
	    data->tables_render_request = 1;
	    break;

	case COMMAND_CLOSE:
	    mem_off();
	    mem_free( data->sin_tab );
	    mem_on();
	    mem_free( data->cvolume_table );
	    mem_free( data->mvolume_table );
	    retval = 1;
	    break;
    }

    return retval;
}
