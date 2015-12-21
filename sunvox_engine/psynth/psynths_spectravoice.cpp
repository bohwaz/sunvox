/*
    psynths_spectravoice.cpp.
    This file is part of the SunVox engine.
    Copyright (C) 2002 - 2008 Alex Zolotov <nightradio@gmail.com>
*/

#include "psynth.h"
#include "utils/utils.h"
#include "core/debug.h"
#include "sound/sound.h"
#ifdef SUNVOX_GUI
#include "window_manager/wmanager.h"
#endif

//Unique names for objects in your synth:
#define SYNTH_DATA	psynth_spectravoice_data
#define SYNTH_HANDLER	psynth_spectravoice
//And unique parameters:
#define SYNTH_INPUTS	0
#define SYNTH_OUTPUTS	2

#define MAX_CHANNELS	16
#define MAX_HARMONICS	16
#define MAX_SAMPLES	1

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
    CTYPE   local_pan;
};

enum {
    MODE_HQ = 0,
    MODE_HQ_MONO,
    MODE_LQ,
    MODE_LQ_MONO,
    MODE_CUBIC,
    MODES
};

struct SYNTH_DATA
{
    //Controls: ############################################################
    CTYPE   ctl_volume;
    CTYPE   ctl_pan;
    CTYPE   ctl_attack;
    CTYPE   ctl_release;
    CTYPE   ctl_channels;
    CTYPE   ctl_mode;
    CTYPE   ctl_sustain;
    CTYPE   ctl_sample_size;
    CTYPE   ctl_harm;
    CTYPE   ctl_freq;
    CTYPE   ctl_freq_vol;
    CTYPE   ctl_freq_band;
    CTYPE   ctl_freq_type;
    CTYPE   ctl_samples;
    //Synth data: ##########################################################
    gen_channel   channels[ MAX_CHANNELS ];
    int	    search_ptr;
    ulong   *linear_tab;
    uint16  *freq;
    uchar   *freq_vol;
    uchar   *freq_band;
    uchar   *freq_type;
    int16   *samples[ MAX_SAMPLES ];
    int	    sample_size;
    int	    note_offset;
};

#define SVOX_PREC (long)16	    //sample pointer (fixed point) precision
#define SIGNED_SUB64( v1, v1_p, v2, v2_p ) \
{ v1 -= v2; v1_p -= v2_p; \
if( v1_p < 0 ) { v1--; v1_p += ( (long)1 << SVOX_PREC ); } }
#define SIGNED_ADD64( v1, v1_p, v2, v2_p ) \
{ v1 += v2; v1_p += v2_p; \
if( v1_p > ( (long)1 << SVOX_PREC ) - 1 ) { v1++; v1_p -= ( (long)1 << SVOX_PREC ); } }

#define APPLY_VOLUME \
    s *= vol; \
    s /= 256;
#define APPLY_ENV_VOLUME \
    s *= ( env_vol >> 20 ); \
    s /= 1024;
#define ENV_CONTROL_REPLACE \
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
#define ENV_CONTROL \
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

float g_fft_sin[ 22 ] = 
{
    (float)-5.89793E-10,
    (float)-1,
    (float)-0.707106781,
    (float)-0.382683432,
    (float)-0.195090322,
    (float)-0.09801714,
    (float)-0.049067674,
    (float)-0.024541229,
    (float)-0.012271538,
    (float)-0.006135885,
    (float)-0.003067957,
    (float)-0.00153398,
    (float)-0.00076699,
    (float)-0.000383495,
    (float)-0.000191748,
    (float)-9.58738E-05,
    (float)-4.79369E-05,
    (float)-2.39684E-05,
    (float)-1.19842E-05,
    (float)-5.99211E-06,
    (float)-2.99606E-06,
    (float)-1.49803E-06
};

float g_fft_sin2[ 22 ] = 
{
    (float)-1,
    (float)-0.707106781,
    (float)-0.382683432,
    (float)-0.195090322,
    (float)-0.09801714,
    (float)-0.049067674,
    (float)-0.024541229,
    (float)-0.012271538,
    (float)-0.006135885,
    (float)-0.003067957,
    (float)-0.00153398,
    (float)-0.00076699,
    (float)-0.000383495,
    (float)-0.000191748,
    (float)-9.58738E-05,
    (float)-4.79369E-05,
    (float)-2.39684E-05,
    (float)-1.19842E-05,
    (float)-5.99211E-06,
    (float)-2.99606E-06,
    (float)-1.49803E-06,
    (float)-7.49014E-07
};

int16 g_fft_rsin[ 128 ] = {
0, 27572, 29794, 4624, -24798, -31421, -9155, 21527, 32418, 13503, -17825, -32766, -17581, 13767, 32459, 21307, 
-9433, -31502, -24607, 4911, 29914, 27414, -290, -27728, -29673, -4336, 24986, 31337, 8876, -21745, -32374, -13239, 
18068, 32764, 17336, -14030, -32497, -21086, 9711, 31580, 24415, -5197, -30031, -27254, 580, 27881, 29548, 4049, 
-25173, -31251, -8597, 21961, 32328, 12973, -18309, -32758, -17089, 14291, 32533, 20863, -9987, -31656, -24220, 5483, 
30146, 27092, -870, -28032, -29422, -3761, 25358, 31163, 8317, -22175, -32280, -12706, 18549, 32751, 16841, -14552, 
-32566, -20639, 10263, 31730, 24024, -5769, -30258, -26928, 1159, 28181, 29293, 3472, -25540, -31072, -8036, 22388, 
32229, 12438, -18788, -32741, -16592, 14811, 32597, 20413, -10538, -31801, -23826, 6054, 30369, 26762, -1449, -28328, 
-29162, -3184, 25721, 30979, 7754, -22599, -32175, -12169, 19024, 32728, 16341, -15069, -32625, -20185, 10812, 31870
};
int16 g_fft_rcos[ 128 ] = {
32767, 17704, -13635, -32439, -21417, 9294, 31461, 24703, -4767, -29855, -27493, 145, 27650, 29734, 4480, -24892, 
-31379, -9016, 21636, 32396, 13371, -17947, -32765, -17459, 13899, 32478, 21197, -9572, -31541, -24511, 5054, 29973, 
27334, -435, -27805, -29611, -4192, 25080, 31294, 8737, -21853, -32352, -13106, 18189, 32761, 17213, -14161, -32515, 
-20975, 9849, 31619, 24318, -5340, -30089, -27173, 725, 27957, 29485, 3905, -25265, -31207, -8457, 22068, 32304, 
12839, -18429, -32755, -16965, 14422, 32550, 20751, -10125, -31693, -24122, 5626, 30203, 27010, -1014, -28107, -29358, 
-3617, 25449, 31118, 8176, -22282, -32255, -12572, 18669, 32746, 16716, -14682, -32582, -20526, 10401, 31766, 23925, 
-5912, -30314, -26845, 1304, 28255, 29228, 3328, -25631, -31026, -7895, 22494, 32202, 12304, -18906, -32734, -16466, 
14940, 32611, 20299, -10675, -31836, -23726, 6197, 30423, 26678, -1594, -28401, -29096, -3039, 25811, 30931, 7613
};

void fft( int16 *result, float *fi, float *fr, int fft_size );
void recalc_samples( SYNTH_DATA *data, int synth_id, psynth_net *pnet );

#ifdef SUNVOX_GUI
struct spectravoice_visual_data
{
    SYNTH_DATA	*synth_data;
    int		synth_id;
    psynth_net	*pnet;
    WINDOWPTR	render_button;
};
int spectravoice_render_handler( void *user_data, WINDOWPTR win, window_manager *wm )
{
    spectravoice_visual_data *data = (spectravoice_visual_data*)user_data;
    //Render samples:
    recalc_samples( data->synth_data, data->synth_id, data->pnet );
    return 0;
}
int spectravoice_visual_handler( sundog_event *evt, window_manager *wm )
{
    int retval = 0;
    WINDOWPTR win = evt->win;
    spectravoice_visual_data *data = (spectravoice_visual_data*)win->data;
    int but_ysize = BUTTON_YSIZE( win, wm );
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    return sizeof( spectravoice_visual_data );
	    break;
	case EVT_AFTERCREATE:
	    data->render_button = new_window( "Render", 0, 0, 1, 1, wm->colors[ 11 ], win, button_handler, wm );
	    set_handler( data->render_button, spectravoice_render_handler, data, wm );
	    set_window_controller( data->render_button, 0, wm, 0, CEND );
	    set_window_controller( data->render_button, 1, wm, 0, CEND );
	    set_window_controller( data->render_button, 2, wm, BUTTON_XSIZE( win, wm ), CEND );
	    set_window_controller( data->render_button, 3, wm, but_ysize, CEND );
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONDOWN:
	case EVT_MOUSEDOUBLECLICK:
	case EVT_MOUSEMOVE:
	case EVT_MOUSEBUTTONUP:
	    retval = 1;
	    break;
	case EVT_DRAW:
	    break;
    }
    return retval;
}
#endif

void fft( int16 *result, float *fi, float *fr, int fft_size )
{
    int i;
    int nn = fft_size;
    int nm1 = nn - 1;
    int nd2 = nn / 2;
    int m = 0;
    while( 1 )
    {
	if( 1 << m == fft_size ) break;
	m++;
    }
    int j = nd2;
    int k;
    for( i = 1; i <= nn - 2; i++ )
    {
	if( i < j )
	{
	    float tr = fr[ j ];
	    float ti = fi[ j ];
	    fr[ j ] = fr[ i ];
	    fi[ j ] = fi[ i ];
	    fr[ i ] = tr;
	    fi[ i ] = ti;
	}
	k = nd2;
nit:
	if( k <= j )
	{
	    j = j - k;
	    k = k / 2;
	    goto nit;
	}
	j = j + k;
    }

    int mmax = 1;
    int n = fft_size;
    int istep;
    int cnt = 0;
    while( n > mmax )
    {
	istep = mmax << 1; //istep = 2; mmax = 1; ... istep = fft_size; mmax = fft_size >> 1;
	float wtemp = g_fft_sin2[ cnt ];
	float wpr = -2.0F * wtemp * wtemp;
	float wpi = g_fft_sin[ cnt ];
	float wr = 1.0;
	float wi = 0.0;
	for( m = 0; m < mmax; m ++ ) //m = 0..1; 0..2; ... 0..fft_size >> 1;
	{
	    for( i = m; i < n; i += istep ) 
	    {
		j = i + mmax;
		float tempr = wr * fr[ j ] - wi * fi[ j ];
		float tempi = wr * fi[ j ] + wi * fr[ j ];
		fr[ j ] = fr[ i ] - tempr;
		fi[ j ] = fi[ i ] - tempi;
		fr[ i ] += tempr;
		fi[ i ] += tempi;
	    }
	    wr = ( wtemp = wr ) * wpr - wi * wpi + wr;
	    wi = wi * wpr + wtemp * wpi + wi;
	}
	mmax = istep;
	cnt++;
    }
    //Normalize:
    float max = 0;
    for( i = 0; i < fft_size; i++ )
    {
	float v = fr[ i ];
	if( v < 0 ) v = -v;
	if( v > max ) max = v;
    }
    float coef = 1.0F / max;
    for( i = 0; i < fft_size; i++ )
    {
	fr[ i ] = fr[ i ] * coef;
    }
    //Save result:
    for( i = 0; i < fft_size; i++ )
    {
	int ires = (int)( fr[ i ] * 32768 );
	if( ires > 32767 ) ires = 32767;
	if( ires < -32767 ) ires = -32767;
	result[ i ] = (int16)ires;
    }
}

void recalc_samples( SYNTH_DATA *data, int synth_id, psynth_net *pnet )
{
    data->freq = (uint16*)psynth_get_chunk( synth_id, 0, pnet );
    data->freq_vol = (uchar*)psynth_get_chunk( synth_id, 1, pnet );
    data->freq_band = (uchar*)psynth_get_chunk( synth_id, 2, pnet );
    data->freq_type = (uchar*)psynth_get_chunk( synth_id, 3, pnet );

    sound_stream_stop();
    mem_off();
    int prev_ss = data->sample_size;
    data->sample_size = 4096 << data->ctl_sample_size;
    if( data->sample_size != prev_ss )
    {
	if( data->samples[ 0 ] ) mem_free( data->samples[ 0 ] );
	data->samples[ 0 ] = (int16*)MEM_NEW( HEAP_STORAGE, data->sample_size * sizeof( int16 ) );
    }
    mem_on();
    sound_stream_play();

    int16 *smp = data->samples[ 0 ];

    mem_off();
    float *hr = (float*)MEM_NEW( HEAP_STORAGE, data->sample_size * sizeof( float ) );
    float *hi = (float*)MEM_NEW( HEAP_STORAGE, data->sample_size * sizeof( float ) );
    uchar *distribution = (uchar*)MEM_NEW( HEAP_DYNAMIC, 256 );
    mem_set( hr, data->sample_size * sizeof( float ), 0 );
    mem_set( hi, data->sample_size * sizeof( float ), 0 );
    set_seed( 0 );
    int a;
    for( int n = 0; n < MAX_HARMONICS; n++ )
    {
	switch( data->freq_type[ n ] )
	{
	    case 0:
	    case 2:
	    case 3:
	    case 4:
	    case 5:
		//Half sin:
		for( a = 0; a < 256; a++ ) distribution[ a ] = g_vibrato_tab[ a ];
		break;
	    case 6:
		//Gaussian:
		for( a = 0; a < 128; a++ ) 
		{
		    distribution[ ( a + 64 ) & 255 ] = g_vibrato_tab[ a * 2 ] / 2 + 127;
		    distribution[ ( ( a + 128 ) + 64 ) & 255 ] = -( g_vibrato_tab[ a * 2 ] / 2 ) + 127;
		}
		break;
	    case 1:
		//Square:
		for( a = 0; a < 256; a++ ) distribution[ a ] = 255;
		break;
	}
	int clones = 1;
	int harmonic_vol = data->freq_vol[ n ];
	int bandwidth = data->freq_band[ n ];
	if( data->freq_type[ n ] >= 2 && data->freq_type[ n ] < 6 ) clones = 8;
	for( int cc = 0; cc < clones; cc++ )
	{
	    int ptr = ( data->freq[ n ] * data->sample_size ) / 22050;
	    if( ptr >= data->sample_size ) ptr = data->sample_size - 1;
	    if( cc > 0 ) 
	    {
		ptr += ptr * cc;
		if( data->freq_type[ n ] == 3 )
		    bandwidth += 2;
		if( data->freq_type[ n ] == 4 )
		    bandwidth += 4;
		if( data->freq_type[ n ] == 5 )
		    bandwidth += 8;
	    }
	    ptr /= 2;
	    int bw = ( ( bandwidth + 1 ) * ( data->sample_size / 16 ) ) / 256;
	    bw /= 2;
	    if( bw <= 0 ) bw = 1;
	    int p1 = ptr - bw;
	    int p2 = ptr + bw;
	    if( p1 < 0 ) p1 = 0;
	    if( p2 >= data->sample_size / 2 ) p2 = data->sample_size / 2 - 1;
	    for( int i = p1; i <= p2; i++ )
	    {
		int d = i - ptr;
		d *= ( (128<<14) / bw );
		d >>= 14;
		if( d < 128 && d >= -128 ) 
		{
		    int vol = distribution[ 128 + d ];
		    int amp = ( harmonic_vol * vol ) / 256;
		    int phase = pseudo_random();
		    hi[ i ] += (float)( ( g_fft_rsin[ phase&127 ] * amp ) / 256 ) / 32767;
		    hr[ i ] += (float)( ( g_fft_rcos[ phase&127 ] * amp ) / 256 ) / 32767;
		}
	    }
	}
    }
    //Make mirror part of FFT:
    for( int i = 0; i < data->sample_size / 2; i++ )
    {
	hr[ data->sample_size - 1 - i ] = -hr[ i ];
	hi[ data->sample_size - 1 - i ] = -hi[ i ];
    }
    fft( smp, hi, hr, data->sample_size );
    mem_free( hr );
    mem_free( hi );
    mem_free( distribution );
    mem_on();
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
	    retval = (int)"SpectraVoice";
	    break;

	case COMMAND_GET_SYNTH_INFO:
	    retval = (int)"FFT Based Synth\nMin. sample size: 4096 words\nMax. sample size: 65536 words\n\nAvailable local controllers:\n * Pan";
	    break;

	case COMMAND_GET_INPUTS_NUM: retval = SYNTH_INPUTS; break;
	case COMMAND_GET_OUTPUTS_NUM: retval = SYNTH_OUTPUTS; break;

	case COMMAND_GET_FLAGS: retval = PSYNTH_FLAG_GENERATOR; break;

	case COMMAND_INIT:
	    psynth_register_ctl( synth_id, "Volume", "", 0, 256, 128, 0, &data->ctl_volume, net );
	    psynth_register_ctl( synth_id, "Panning", "", 0, 256, 128, 0, &data->ctl_pan, net );
	    psynth_register_ctl( synth_id, "Attack", "", 0, 512, 10, 0, &data->ctl_attack, net );
	    psynth_register_ctl( synth_id, "Release", "", 0, 512, 512, 0, &data->ctl_release, net );
	    psynth_register_ctl( synth_id, "Polyphony", "ch.", 1, MAX_CHANNELS, 8, 1, &data->ctl_channels, net );
	    psynth_register_ctl( synth_id, "Mode", "HQ/HQmono/LQ/LQmono/Cubic", 0, MODES - 1, MODE_CUBIC, 1, &data->ctl_mode, net );
	    psynth_register_ctl( synth_id, "Sustain", "off/on", 0, 1, 1, 1, &data->ctl_sustain, net );
	    psynth_register_ctl( synth_id, "Sample size", "", 0, 4, 1, 1, &data->ctl_sample_size, net );
	    psynth_register_ctl( synth_id, "Harmonic", "", 0, MAX_HARMONICS-1, 0, 1, &data->ctl_harm, net );
	    psynth_register_ctl( synth_id, "h.freq", "Hz", 0, 22050, 1098, 1, &data->ctl_freq, net );
	    psynth_register_ctl( synth_id, "h.volume", "", 0, 255, 255, 1, &data->ctl_freq_vol, net );
	    psynth_register_ctl( synth_id, "h.bandwidth", "", 0, 255, 3, 1, &data->ctl_freq_band, net );
	    psynth_register_ctl( synth_id, "h.bandtype", "hsin/square/org1/2/3/4/sin", 0, 6, 0, 1, &data->ctl_freq_type, net );
	    for( int c = 0; c < MAX_CHANNELS; c++ )
	    {
		data->channels[ c ].playing = 0;
		data->channels[ c ].ptr_l = 0;
		data->channels[ c ].ptr_h = 0;
		data->channels[ c ].env_vol = 0;
		data->channels[ c ].sustain = 0;
	    }
	    for( int s = 0; s < MAX_SAMPLES; s++ )
	    {
		data->samples[ s ] = 0;
	    }
	    data->sample_size = 0;
	    
	    psynth_new_chunk( synth_id, 0, MAX_HARMONICS * sizeof( uint16 ), 0, pnet );
	    psynth_new_chunk( synth_id, 1, MAX_HARMONICS, 0, pnet );
	    psynth_new_chunk( synth_id, 2, MAX_HARMONICS, 0, pnet );
	    psynth_new_chunk( synth_id, 3, MAX_HARMONICS, 0, pnet );
	    data->freq = (uint16*)psynth_get_chunk( synth_id, 0, pnet );
	    data->freq_vol = (uchar*)psynth_get_chunk( synth_id, 1, pnet );
	    data->freq_band = (uchar*)psynth_get_chunk( synth_id, 2, pnet );
	    data->freq_type = (uchar*)psynth_get_chunk( synth_id, 3, pnet );
	    for( int h = 0; h < MAX_HARMONICS; h++ )
	    {
		data->freq[ h ] = 0;
		data->freq_vol[ h ] = 0;
		data->freq_band[ h ] = 0;
		data->freq_type[ h ] = 0;
	    }
	    data->freq[ 0 ] = data->ctl_freq;
	    data->freq_vol[ 0 ] = data->ctl_freq_vol;
	    data->freq_band[ 0 ] = data->ctl_freq_band;
	    data->freq_type[ 0 ] = data->ctl_freq_type;
	    
	    data->note_offset = 0;

	    data->search_ptr = 0;
	    data->linear_tab = g_linear_tab;

#ifdef SUNVOX_GUI
	    {
		pnet->items[ synth_id ].visual = new_window( "Spectravoice GUI", 0, 0, 10, 10, g_wm->colors[ 5 ], 0, spectravoice_visual_handler, g_wm );
		spectravoice_visual_data *sv_data = (spectravoice_visual_data*)pnet->items[ synth_id ].visual->data;
		sv_data->synth_data = data;
		sv_data->synth_id = synth_id;
		sv_data->pnet = pnet;
	    }
#endif

	    retval = 1;
	    break;

	case COMMAND_SETUP_FINISHED:
	    //Render samples:
            recalc_samples( data, synth_id, pnet );
	    retval = 1;
	    break;

	case COMMAND_RENDER_REPLACE:
	    if( !outputs[ 0 ] ) break;

	    if( data->ctl_mode == MODE_HQ_MONO || data->ctl_mode == MODE_LQ_MONO )
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
			STYPE *in = inputs[ ch ];
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
			int add = ch * 32768 / 2;
			
			int16 *smp = 0;
			smp = data->samples[ 0 ];
			if( smp == 0 ) break;
			int sample_size = data->sample_size;
			int sample_size_mask = data->sample_size - 1;
			STYPE_CALC res;
			int poff = 0;
			if( ch == 1 ) poff = data->sample_size / 2;
			int s;
			if( retval == 0 ) 
			{
			    if( data->ctl_attack == 0 && data->ctl_release == 0 )
			    {
				//No attack. No release.
				if( sustain_enabled == 0 ) sustain = 0;
				if( sustain == 0 ) { for( i = 0; i < sample_frames; i++ ) out[ i ] = 0; playing = 0; }
				else
				{
				    if( data->ctl_mode == MODE_LQ_MONO || data->ctl_mode == MODE_LQ )
				    {
					//Low Quality (No interpolation):
					for( i = 0; i < sample_frames; i++ )
					{
					    s = smp[ (ptr_h+poff) & sample_size_mask ];
					    APPLY_VOLUME;
					    INT16_TO_STYPE( res, s );
					    out[ i ] = (STYPE)res;
					    SIGNED_ADD64( ptr_h, ptr_l, delta_h, delta_l );
					}
				    }
#ifdef STYPE_FLOATINGPOINT
				    if( data->ctl_mode == MODE_HQ_MONO || data->ctl_mode == MODE_HQ )
#else
				    else
#endif
				    {
					//High Quality (Linear interpolation):
					for( i = 0; i < sample_frames; i++ )
					{
					    s = smp[ (ptr_h+poff) & sample_size_mask ] * (32767-(ptr_l>>1));
					    s += smp[ (ptr_h+poff+1) & sample_size_mask ] * (ptr_l>>1);
					    s >>= 15;
					    APPLY_VOLUME;
					    INT16_TO_STYPE( res, s );
					    out[ i ] = (STYPE)res;
					    SIGNED_ADD64( ptr_h, ptr_l, delta_h, delta_l );
					}
				    }
#ifdef STYPE_FLOATINGPOINT
				    if( data->ctl_mode == MODE_CUBIC )
				    {
					//High Quality (Cubic interpolation):
					for( i = 0; i < sample_frames; i++ )
					{
					    STYPE_CALC y0 = smp[ (ptr_h+poff-1) & sample_size_mask ];
					    STYPE_CALC y1 = smp[ (ptr_h+poff) & sample_size_mask ];
					    STYPE_CALC y2 = smp[ (ptr_h+poff+1) & sample_size_mask ];
					    STYPE_CALC y3 = smp[ (ptr_h+poff+2) & sample_size_mask ];
					    STYPE_CALC mu = (STYPE_CALC)ptr_l / (STYPE_CALC)0x10000;
					    STYPE_CALC a = (3 * (y1-y2) - y0 + y3) / 2;
					    STYPE_CALC b = 2 * y2 + y0 - (5*y1 + y3) / 2;
					    STYPE_CALC c = ( y2 - y0 ) / 2;
					    s = (int)( (((a * mu) + b) * mu + c) * mu + y1 );
					    APPLY_VOLUME;
					    INT16_TO_STYPE( res, s );
					    out[ i ] = (STYPE)res;
					    SIGNED_ADD64( ptr_h, ptr_l, delta_h, delta_l );
					}
				    }
#endif
				}
			    }
			    else
			    {
				//Attack and release enabled:
				if( data->ctl_mode == MODE_LQ_MONO || data->ctl_mode == MODE_LQ )
				{
				    //Low Quality (No interpolation):
				    for( i = 0; i < sample_frames; i++ )
				    {
					s = smp[ (ptr_h+poff) & sample_size_mask ];
					APPLY_VOLUME;
					APPLY_ENV_VOLUME;
					INT16_TO_STYPE( res, s );
					out[ i ] = (STYPE)res;
					SIGNED_ADD64( ptr_h, ptr_l, delta_h, delta_l );
					ENV_CONTROL_REPLACE;
				    }
				}
#ifdef STYPE_FLOATINGPOINT
				if( data->ctl_mode == MODE_HQ_MONO || data->ctl_mode == MODE_HQ )
#else
				else
#endif
				{
				    //High Quality (Linear interpolation):
				    for( i = 0; i < sample_frames; i++ )
				    {
					s = smp[ (ptr_h+poff) & sample_size_mask ] * (32767-(ptr_l>>1));
					s += smp[ (ptr_h+poff+1) & sample_size_mask ] * (ptr_l>>1);
					s >>= 15;
					APPLY_VOLUME;
					APPLY_ENV_VOLUME;
					INT16_TO_STYPE( res, s );
					out[ i ] = (STYPE)res;
					SIGNED_ADD64( ptr_h, ptr_l, delta_h, delta_l );
					ENV_CONTROL_REPLACE;
				    }
				}
#ifdef STYPE_FLOATINGPOINT
				if( data->ctl_mode == MODE_CUBIC )
				{
				    //High Quality (Cubic interpolation):
				    for( i = 0; i < sample_frames; i++ )
				    {
					STYPE_CALC y0 = smp[ (ptr_h+poff-1) & sample_size_mask ];
					STYPE_CALC y1 = smp[ (ptr_h+poff) & sample_size_mask ];
					STYPE_CALC y2 = smp[ (ptr_h+poff+1) & sample_size_mask ];
					STYPE_CALC y3 = smp[ (ptr_h+poff+2) & sample_size_mask ];
					STYPE_CALC mu = (STYPE_CALC)ptr_l / (STYPE_CALC)0x10000;
					STYPE_CALC a = (3 * (y1-y2) - y0 + y3) / 2;
					STYPE_CALC b = 2 * y2 + y0 - (5*y1 + y3) / 2;
					STYPE_CALC c = ( y2 - y0 ) / 2;
					s = (int)( (((a * mu) + b) * mu + c) * mu + y1 );
					APPLY_VOLUME;
					APPLY_ENV_VOLUME;
					INT16_TO_STYPE( res, s );
					out[ i ] = (STYPE)res;
					SIGNED_ADD64( ptr_h, ptr_l, delta_h, delta_l );
					ENV_CONTROL_REPLACE;
				    }
				}
#endif
			    }
			}
			else
			{
			    if( data->ctl_attack == 0 && data->ctl_release == 0 )
			    {
				//No attack. No release.
				if( sustain == 0 ) { playing = 0; }
				else
				{
				    if( data->ctl_mode == MODE_LQ_MONO || data->ctl_mode == MODE_LQ )
				    {
					//Low Quality (No interpolation):
					for( i = 0; i < sample_frames; i++ )
					{
					    s = smp[ (ptr_h+poff) & sample_size_mask ];
					    APPLY_VOLUME;
					    INT16_TO_STYPE( res, s );
					    out[ i ] += (STYPE)res;
					    SIGNED_ADD64( ptr_h, ptr_l, delta_h, delta_l );
					}
				    }
#ifdef STYPE_FLOATINGPOINT
				    if( data->ctl_mode == MODE_HQ_MONO || data->ctl_mode == MODE_HQ )
#else
				    else
#endif
				    {
					//High Quality (Linear Interpolation):
					for( i = 0; i < sample_frames; i++ )
					{
					    s = smp[ (ptr_h+poff) & sample_size_mask ] * (32767-(ptr_l>>1));
					    s += smp[ (ptr_h+poff+1) & sample_size_mask ] * (ptr_l>>1);
					    s >>= 15;
					    APPLY_VOLUME;
					    INT16_TO_STYPE( res, s );
					    out[ i ] += (STYPE)res;
					    SIGNED_ADD64( ptr_h, ptr_l, delta_h, delta_l );
					}
				    }
#ifdef STYPE_FLOATINGPOINT
				    if( data->ctl_mode == MODE_CUBIC )
				    {
					//High Quality (Cubic interpolation):
					for( i = 0; i < sample_frames; i++ )
					{
					    STYPE_CALC y0 = smp[ (ptr_h+poff-1) & sample_size_mask ];
					    STYPE_CALC y1 = smp[ (ptr_h+poff) & sample_size_mask ];
					    STYPE_CALC y2 = smp[ (ptr_h+poff+1) & sample_size_mask ];
					    STYPE_CALC y3 = smp[ (ptr_h+poff+2) & sample_size_mask ];
					    STYPE_CALC mu = (STYPE_CALC)ptr_l / (STYPE_CALC)0x10000;
					    STYPE_CALC a = (3 * (y1-y2) - y0 + y3) / 2;
					    STYPE_CALC b = 2 * y2 + y0 - (5*y1 + y3) / 2;
					    STYPE_CALC c = ( y2 - y0 ) / 2;
					    s = (int)( (((a * mu) + b) * mu + c) * mu + y1 );
					    APPLY_VOLUME;
					    INT16_TO_STYPE( res, s );
					    out[ i ] += (STYPE)res;
					    SIGNED_ADD64( ptr_h, ptr_l, delta_h, delta_l );
					}
				    }
#endif
				}
			    }
			    else
			    {
				//Attack and release enabled:
				if( data->ctl_mode == MODE_LQ_MONO || data->ctl_mode == MODE_LQ )
				{
				    //Low Quality (No interpolation):
				    for( i = 0; i < sample_frames; i++ )
				    {
					s = smp[ (ptr_h+poff) & sample_size_mask ];
					APPLY_VOLUME;
					APPLY_ENV_VOLUME;
					INT16_TO_STYPE( res, s );
					out[ i ] += (STYPE)res;
					SIGNED_ADD64( ptr_h, ptr_l, delta_h, delta_l );
					ENV_CONTROL;
				    }
				}
#ifdef STYPE_FLOATINGPOINT				
				if( data->ctl_mode == MODE_HQ_MONO || data->ctl_mode == MODE_HQ )
#else
				else
#endif
				{
				    //High Quality (Linear interpolation):
				    for( i = 0; i < sample_frames; i++ )
				    {
					s = smp[ (ptr_h+poff) & sample_size_mask ] * (32767-(ptr_l>>1));
					s += smp[ (ptr_h+poff+1) & sample_size_mask ] * (ptr_l>>1);
					s >>= 15;
					APPLY_VOLUME;
					APPLY_ENV_VOLUME;
					INT16_TO_STYPE( res, s );
					out[ i ] += (STYPE)res;
					SIGNED_ADD64( ptr_h, ptr_l, delta_h, delta_l );
					ENV_CONTROL;
				    }
				}
#ifdef STYPE_FLOATINGPOINT
				if( data->ctl_mode == MODE_CUBIC )
				{
				    //High Quality (Cubic interpolation):
				    for( i = 0; i < sample_frames; i++ )
				    {
					STYPE_CALC y0 = smp[ (ptr_h+poff-1) & sample_size_mask ];
					STYPE_CALC y1 = smp[ (ptr_h+poff) & sample_size_mask ];
					STYPE_CALC y2 = smp[ (ptr_h+poff+1) & sample_size_mask ];
					STYPE_CALC y3 = smp[ (ptr_h+poff+2) & sample_size_mask ];
					STYPE_CALC mu = (STYPE_CALC)ptr_l / (STYPE_CALC)0x10000;
					STYPE_CALC a = (3 * (y1-y2) - y0 + y3) / 2;
					STYPE_CALC b = 2 * y2 + y0 - (5*y1 + y3) / 2;
					STYPE_CALC c = ( y2 - y0 ) / 2;
					s = (int)( (((a * mu) + b) * mu + c) * mu + y1 );
					APPLY_VOLUME;
					APPLY_ENV_VOLUME;
					INT16_TO_STYPE( res, s );
					out[ i ] += (STYPE)res;
					SIGNED_ADD64( ptr_h, ptr_l, delta_h, delta_l );
					ENV_CONTROL;
				    }
				}
#endif
			    }
			}
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
		data->channels[ c ].ptr_h = ( data->note_offset & (data->sample_size-1) );
		data->note_offset += 333;
		data->channels[ c ].ptr_l = 0;
		data->channels[ c ].id = pnet->channel_id;
		data->channels[ c ].env_vol = 0;
		data->channels[ c ].sustain = 1;
		
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

	case COMMAND_SET_GLOBAL_CONTROLLER:
	    if( pnet->ctl_num == 8 )
	    {
		//Harmonic number:
		data->freq = (uint16*)psynth_get_chunk( synth_id, 0, pnet );
		data->freq_vol = (uchar*)psynth_get_chunk( synth_id, 1, pnet );
		data->freq_band = (uchar*)psynth_get_chunk( synth_id, 2, pnet );
		data->freq_type = (uchar*)psynth_get_chunk( synth_id, 3, pnet );
		data->ctl_harm = pnet->ctl_val;
		data->ctl_freq = data->freq[ data->ctl_harm ];
		data->ctl_freq_vol = data->freq_vol[ data->ctl_harm ];
		data->ctl_freq_band = data->freq_band[ data->ctl_harm ];
		data->ctl_freq_type = data->freq_type[ data->ctl_harm ];
		pnet->draw_request = 1;
		retval = 1;
	    }
	    if( pnet->ctl_num == 9 )
	    {
		//Harmonic frequency:
		data->freq = (uint16*)psynth_get_chunk( synth_id, 0, pnet );
		data->freq[ data->ctl_harm ] = pnet->ctl_val;
	    }
	    if( pnet->ctl_num == 10 )
	    {
		//Harmonic volume:
		data->freq_vol = (uchar*)psynth_get_chunk( synth_id, 1, pnet );
		data->freq_vol[ data->ctl_harm ] = pnet->ctl_val;
	    }
	    if( pnet->ctl_num == 11 )
	    {
		//Harmonic bandwidth:
		data->freq_band = (uchar*)psynth_get_chunk( synth_id, 2, pnet );
		data->freq_band[ data->ctl_harm ] = pnet->ctl_val;
	    }
	    if( pnet->ctl_num == 12 )
	    {
		//Harmonic bandwidth type:
		data->freq_type = (uchar*)psynth_get_chunk( synth_id, 3, pnet );
		data->freq_type[ data->ctl_harm ] = pnet->ctl_val;
	    }
	    break;

	case COMMAND_SET_LOCAL_CONTROLLER:
	    if( pnet->ctl_num == 1 )
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

	case COMMAND_CLOSE:
	    mem_off();
	    for( int s = 0; s < MAX_SAMPLES; s++ )
	    {
		if( data->samples[ s ] ) mem_free( data->samples[ s ] );
		data->samples[ s ] = 0;
	    }
	    mem_on();

#ifdef SUNVOX_GUI
	    remove_window( pnet->items[ synth_id ].visual, g_wm );
#endif

	    retval = 1;
	    break;
    }

    return retval;
}
