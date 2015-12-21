/*
    psynths_vocal_filter.cpp.
    This file is part of the SunVox engine.
    Copyright (C) 2002 - 2008 Alex Zolotov <nightradio@gmail.com>
*/

#include "psynth.h"

#ifdef STYPE_FLOATINGPOINT
#else
    #define COEF_SIZE	11
#endif

//Unique names for objects in your synth:
#define SYNTH_DATA	psynth_vfilter_data
#define SYNTH_HANDLER	psynth_vfilter
//And unique parameters:
#define SYNTH_INPUTS	2
#define SYNTH_OUTPUTS	2

inline int INTERP( int v1, int v2, int p ) 
{
    return ( ( (v1*(256-p)) + (v2*p) ) / 256 );
}

#define SINUS_TABLE_STEP ( 0.72F / 128.0F )
float g_fcos[ 128 ] = { 1.000000F, 0.999984F, 0.999937F, 0.999858F, 0.999747F, 0.999605F, 0.999431F, 0.999225F, 0.998988F, 0.998719F, 0.998418F, 0.998086F, 0.997723F, 0.997328F, 0.996901F, 0.996443F, 0.995953F, 0.995431F, 0.994879F, 0.994294F, 0.993679F, 0.993031F, 0.992353F, 0.991643F, 0.990901F, 0.990129F, 0.989325F, 0.988489F, 0.987622F, 0.986725F, 0.985795F, 0.984835F, 0.983844F, 0.982821F, 0.981767F, 0.980683F, 0.979567F, 0.978420F, 0.977242F, 0.976034F, 0.974794F, 0.973524F, 0.972223F, 0.970891F, 0.969528F, 0.968135F, 0.966711F, 0.965256F, 0.963771F, 0.962255F, 0.960709F, 0.959133F, 0.957526F, 0.955889F, 0.954222F, 0.952524F, 0.950796F, 0.949039F, 0.947251F, 0.945433F, 0.943585F, 0.941708F, 0.939801F, 0.937864F, 0.935897F, 0.933900F, 0.931875F, 0.929819F, 0.927734F, 0.925620F, 0.923477F, 0.921304F, 0.919102F, 0.916871F, 0.914612F, 0.912323F, 0.910005F, 0.907659F, 0.905283F, 0.902879F, 0.900447F, 0.897986F, 0.895497F, 0.892979F, 0.890433F, 0.887859F, 0.885257F, 0.882627F, 0.879969F, 0.877283F, 0.874569F, 0.871827F, 0.869058F, 0.866262F, 0.863438F, 0.860587F, 0.857709F, 0.854803F, 0.851870F, 0.848911F, 0.845924F, 0.842911F, 0.839871F, 0.836805F, 0.833712F, 0.830593F, 0.827447F, 0.824275F, 0.821077F, 0.817854F, 0.814604F, 0.811328F, 0.808027F, 0.804700F, 0.801348F, 0.797971F, 0.794568F, 0.791140F, 0.787687F, 0.784209F, 0.780707F, 0.777179F, 0.773627F, 0.770051F, 0.766450F, 0.762825F, 0.759176F, 0.755502F };
float g_fsin[ 128 ] = { 0.000000F, 0.005625F, 0.011250F, 0.016874F, 0.022498F, 0.028121F, 0.033744F, 0.039365F, 0.044985F, 0.050603F, 0.056220F, 0.061836F, 0.067449F, 0.073060F, 0.078669F, 0.084275F, 0.089879F, 0.095479F, 0.101077F, 0.106672F, 0.112263F, 0.117851F, 0.123434F, 0.129014F, 0.134590F, 0.140162F, 0.145729F, 0.151292F, 0.156850F, 0.162402F, 0.167950F, 0.173493F, 0.179030F, 0.184561F, 0.190086F, 0.195606F, 0.201119F, 0.206626F, 0.212126F, 0.217620F, 0.223106F, 0.228586F, 0.234058F, 0.239523F, 0.244981F, 0.250431F, 0.255872F, 0.261306F, 0.266731F, 0.272148F, 0.277557F, 0.282956F, 0.288347F, 0.293728F, 0.299101F, 0.304463F, 0.309817F, 0.315160F, 0.320493F, 0.325816F, 0.331129F, 0.336432F, 0.341723F, 0.347004F, 0.352274F, 0.357533F, 0.362781F, 0.368017F, 0.373241F, 0.378454F, 0.383654F, 0.388843F, 0.394019F, 0.399183F, 0.404334F, 0.409472F, 0.414597F, 0.419709F, 0.424808F, 0.429894F, 0.434966F, 0.440024F, 0.445068F, 0.450098F, 0.455114F, 0.460116F, 0.465102F, 0.470075F, 0.475032F, 0.479974F, 0.484901F, 0.489813F, 0.494709F, 0.499590F, 0.504455F, 0.509304F, 0.514136F, 0.518953F, 0.523753F, 0.528536F, 0.533303F, 0.538053F, 0.542786F, 0.547501F, 0.552200F, 0.556881F, 0.561544F, 0.566189F, 0.570817F, 0.575426F, 0.580018F, 0.584591F, 0.589145F, 0.593681F, 0.598198F, 0.602696F, 0.607175F, 0.611635F, 0.616075F, 0.620496F, 0.624898F, 0.629279F, 0.633641F, 0.637983F, 0.642304F, 0.646605F, 0.650886F, 0.655146F };

#define VOWELS_NUM	5
#define FORMANTS_NUM	5
uint16 g_vowels1[ FORMANTS_NUM * 3 * VOWELS_NUM ] = {
    // SOPRANO:
    // a
    800,	1150,	    2900,	3900,	    4950,
    80,		90,	    120,	130,	    140,
    256,	128,	    6,		25,	    0,
    // e
    350,	2000,	    2800,	3600,	    4950,
    60,		100,	    120,	150,	    200,
    256,	25,	    45,		2,	    0,
    // i
    270,	2140,	    2950,	3900,	    4950,
    60,		90,	    100,	120,	    120,
    256,	64,	    12,		12,	    1,
    // o
    450,	800,	    2830,	3800,	    4950,
    70,		80,	    100,	130,	    135,
    256,	72,	    20,		20,	    0,
    // u
    325,	700,	    2700,	3800,	    4950,
    50,		60,	    170,	180,	    200,
    256,	40,	    4,		2,	    0,
};
uint16 g_vowels2[ FORMANTS_NUM * 3 * VOWELS_NUM ] = {
    // ALTO:
    // a
    800,	1150,	    2800,	3500,	    4950,
    80,		90,	    120,	130,	    140,
    256,	161,	    25,		4,	    0,
    // e
    400,	1600,	    2700,	3300,	    4950,
    60,		80,	    120,	150,	    200,
    256,	16,	    8,		4,	    0,
    // i
    350,	1700,	    2700,	3700,	    4950,
    50,		100,	    120,	150,	    200,
    256,	25,	    8,		4,	    0,
    // o
    450,	800,	    2830,	3500,	    4950,
    70,		80,	    100,	130,	    135,
    256,	90,	    40,		10,	    0,
    // u
    325,	700,	    2530,	3500,	    4950,
    50,		60,	    170,	180,	    200,
    256,	64,	    8,		2,	    0,
};
uint16 g_vowels3[ FORMANTS_NUM * 3 * VOWELS_NUM ] = {
    // TENOR:
    // a
    650,	1080,	    2650,	2900,	    3250,
    80,		90,	    120,	130,	    140,
    256,	128,	    114,	101,	    20,
    // e
    400,	1700,	    2600,	3200,	    3580,
    70,		80,	    100,	120,	    120,
    256,	51,	    64,		51,	    25,
    // i
    290,	1870,	    2800,	3250,	    3540,
    40,		90,	    100,	120,	    120,
    256,	45,	    32,		25,	    8,
    // o
    400,	800,	    2600,	2800,	    3000,
    40,		80,	    100,	120,	    120,
    256,	80,	    64,		64,	    12,
    // u
    350,	600,	    2700,	2900,	    3300,
    40,		60,	    100,	120,	    120,
    256,	25,	    36,		51,	    12,
};
uint16 g_vowels4[ FORMANTS_NUM * 3 * VOWELS_NUM ] = {
    // BASS:
    // a
    600,	1040,	    2250,	2450,	    2750,
    60,		70,	    110,	120,	    130,
    256,	114,	    90,		90,	    25,
    // e
    400,	1620,	    2400,	2800,	    3100,
    40,		80,	    100,	120,	    120,
    256,	64,	    90,		64,	    32,
    // i
    250,	1750,	    2600,	3050,	    3340,
    60,		90,	    100,	120,	    120,
    256,	8,	    40,		20,	    10,
    // o
    400,	750,	    2400,	2600,	    2900,
    40,		80,	    100,	120,	    120,
    256,	72,	    22,		25,	    2,
    // u
    350,	600,	    2400,	2675,	    2950,
    40,		80,	    100,	120,	    120,
    256,	25,	    6,		10,	    4,
};

enum {
    FF_TYPE_BANDPASS2, //H(s) = (s/Q) / (s^2 + s/Q + 1)	    (constant 0 dB peak gain)
};

struct ff
{
    int		type;
    int		amp; //0...256
    STYPE_CALC	x1, x2;
    STYPE_CALC	y1, y2;
    STYPE_CALC	b0a0, b2a0, a1a0, a2a0;
};

struct SYNTH_DATA
{
    //Controls: ############################################################
    CTYPE   ctl_volume;
    CTYPE   ctl_bw;
    CTYPE   ctl_amp_add;
    CTYPE   ctl_formants_num;
    CTYPE   ctl_vowel;
    CTYPE   ctl_character;
    CTYPE   ctl_mono;
    //Synth data: ##########################################################
    ff	    filters[ FORMANTS_NUM * SYNTH_OUTPUTS ];
    uint16  *vowels1;
    uint16  *vowels2;
    uint16  *vowels3;
    uint16  *vowels4;
    float   *fsin;
    float   *fcos;
};

void ff_init( ff *f )
{
    mem_set( f, sizeof( ff ), 0 );
}

void ff_stop( ff *f )
{
    f->x1 = 0;
    f->x2 = 0;
    f->y1 = 0;
    f->y2 = 0;
}

void ff_set( ff *f, int type, int fs, int f0, int bw, int amp, float *fsin, float *fcos )
{
    f->type = type;
    f->amp = amp;

    float w0 = 2 * 3.141592F * ( (float)f0 / (float)fs );
    float cs, sn;
#ifdef STYPE_FLOATINGPOINT
    cs = cos( w0 );
    sn = sin( w0 );
#else
    cs = fcos[ (int)( w0 / SINUS_TABLE_STEP ) ];
    sn = fsin[ (int)( w0 / SINUS_TABLE_STEP ) ];
#endif
    if( bw >= fs / 2 ) bw = fs / 2;
    if( bw <= 0 ) bw = 1;
    float q = (float)f0 / (float)bw;
    float alpha = sn / ( 2 * q );

    float b0, b1, b2;
    float a0, a1, a2;

    switch( type )
    {
	case FF_TYPE_BANDPASS2:
	    b0 = alpha;
	    b1 = 0;
	    b2 = -alpha;
	    a0 = 1 + alpha;
	    a1 = -2 * cs;
	    a2 = 1 - alpha;
	    break;
    }

#ifdef STYPE_FLOATINGPOINT
    f->b0a0 = b0 / a0;
    f->b2a0 = b2 / a0;
    f->a1a0 = a1 / a0;
    f->a2a0 = a2 / a0;
#else
    f->b0a0 = (STYPE_CALC)( ( b0 / a0 ) * (float)(1<<COEF_SIZE) );
    f->b2a0 = (STYPE_CALC)( ( b2 / a0 ) * (float)(1<<COEF_SIZE) );
    f->a1a0 = (STYPE_CALC)( ( a1 / a0 ) * (float)(1<<COEF_SIZE) );
    f->a2a0 = (STYPE_CALC)( ( a2 / a0 ) * (float)(1<<COEF_SIZE) );
#endif
}

int ff_do( ff *f, STYPE *out, STYPE *in, int samples_num, int add )
{
    if( f->amp == 0 ) return 0;
    STYPE_CALC b0a0 = f->b0a0;
    STYPE_CALC b2a0 = f->b2a0;
    STYPE_CALC a1a0 = f->a1a0;
    STYPE_CALC a2a0 = f->a2a0;
    STYPE_CALC x1 = f->x1;
    STYPE_CALC x2 = f->x2;
    STYPE_CALC y1 = f->y1;
    STYPE_CALC y2 = f->y2;
#ifdef STYPE_FLOATINGPOINT
    STYPE_CALC amp = (STYPE_CALC)f->amp / (STYPE_CALC)256;
    for( int i = 0; i < samples_num; i++ )
    {
	STYPE_CALC inp = in[ i ];
	denorm_add_white_noise( inp );
	STYPE_CALC val = 
	    b0a0 * inp + b2a0 * x2 -
	    a1a0 * y1 - a2a0 * y2;
	//val = undenormalise( val );
	x2 = x1;
	x1 = inp;
	y2 = y1;
	y1 = val;
	if( add )
	    out[ i ] += (STYPE)( val * amp );
	else
	    out[ i ] = (STYPE)( val * amp );
    }
#else
    STYPE_CALC amp = (STYPE_CALC)f->amp;
    if( amp == 256 )
    {
	if( add )
	{
	    for( int i = 0; i < samples_num; i++ )
	    {
		STYPE_CALC inp = in[ i ] << (14-COEF_SIZE);
		STYPE_CALC val = 
		    ( b0a0 * inp + b2a0 * x2 -
		    a1a0 * y1 - a2a0 * y2 ) >> COEF_SIZE;
		x2 = x1;
		x1 = inp;
		y2 = y1;
		y1 = val;
		val >>= (14-COEF_SIZE);
		out[ i ] += (STYPE)val;
	    }
	}
	else
	{
	    for( int i = 0; i < samples_num; i++ )
	    {
		STYPE_CALC inp = in[ i ] << (14-COEF_SIZE);
		STYPE_CALC val = 
		    ( b0a0 * inp + b2a0 * x2 -
		    a1a0 * y1 - a2a0 * y2 ) >> COEF_SIZE;
		x2 = x1;
		x1 = inp;
		y2 = y1;
		y1 = val;
		val >>= (14-COEF_SIZE);
		out[ i ] = (STYPE)val;
	    }
	}
    }
    else
    {
	if( add )
	{
	    for( int i = 0; i < samples_num; i++ )
	    {
		STYPE_CALC inp = in[ i ] << (14-COEF_SIZE);
		STYPE_CALC val = 
		    ( b0a0 * inp + b2a0 * x2 -
		    a1a0 * y1 - a2a0 * y2 ) >> COEF_SIZE;
		x2 = x1;
		x1 = inp;
		y2 = y1;
		y1 = val;
		val >>= (14-COEF_SIZE);
		out[ i ] += ( ( val * amp ) / 256 );
	    }
	}
	else
	{
	    for( int i = 0; i < samples_num; i++ )
	    {
		STYPE_CALC inp = in[ i ] << (14-COEF_SIZE);
		STYPE_CALC val = 
		    ( b0a0 * inp + b2a0 * x2 -
		    a1a0 * y1 - a2a0 * y2 ) >> COEF_SIZE;
		x2 = x1;
		x1 = inp;
		y2 = y1;
		y1 = val;
		val >>= (14-COEF_SIZE);
		out[ i ] = ( ( val * amp ) / 256 );
	    }
	}
    }
#endif
    f->x1 = x1;
    f->x2 = x2;
    f->y1 = y1;
    f->y2 = y2;
    return 1; //done
}

int SYNTH_HANDLER( 
    PSYTEXX_SYNTH_PARAMETERS
    )
{
    psynth_net *pnet = (psynth_net*)net;
    SYNTH_DATA *data = (SYNTH_DATA*)data_ptr;
    int retval = 0;
    int ch;

    switch( command )
    {
	case COMMAND_GET_DATA_SIZE:
	    retval = sizeof( SYNTH_DATA );
	    break;

	case COMMAND_GET_SYNTH_NAME:
	    retval = (int)"Vocal filter";
	    break;

	case COMMAND_GET_SYNTH_INFO:
	    retval = (int)"Vocal filter.\n(Five formants)";
	    break;

	case COMMAND_GET_INPUTS_NUM: retval = SYNTH_INPUTS; break;
	case COMMAND_GET_OUTPUTS_NUM: retval = SYNTH_OUTPUTS; break;

	case COMMAND_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT; break;

	case COMMAND_INIT:
	    psynth_register_ctl( synth_id, "Volume", "", 0, 512, 256, 0, &data->ctl_volume, net );
	    psynth_register_ctl( synth_id, "Bandwidth", "Hz", 0, 256, 128, 0, &data->ctl_bw, net );
	    psynth_register_ctl( synth_id, "Amp. add", "", 0, 256, 128, 0, &data->ctl_amp_add, net );
	    psynth_register_ctl( synth_id, "Formants", "", 1, FORMANTS_NUM, FORMANTS_NUM, 1, &data->ctl_formants_num, net );
	    psynth_register_ctl( synth_id, "Vowel (a,e,i,o,u)", "", 0, 256, 0, 0, &data->ctl_vowel, net );
	    psynth_register_ctl( synth_id, "Character", "", 0, 3, 0, 1, &data->ctl_character, net );
	    psynth_register_ctl( synth_id, "Mono", "off/on", 0, 1, 0, 1, &data->ctl_mono, net );
	    {
		for( int f = 0; f < FORMANTS_NUM; f++ )
		{
		    for( ch = 0; ch < SYNTH_OUTPUTS; ch++ )
		    {
			ff_init( &data->filters[ SYNTH_OUTPUTS * f + ch ] );
		    }
		}
	    }
	    data->vowels1 = g_vowels1;
	    data->vowels2 = g_vowels2;
	    data->vowels3 = g_vowels3;
	    data->vowels4 = g_vowels4;
	    data->fsin = g_fsin;
	    data->fcos = g_fcos;
	    /*
	    //Generate g_fsin[] and g_fcos[] tables:
	    {
		FILE *f_sin, *f_cos;
		f_sin = fopen( "sin.h", "wb" );
		f_cos = fopen( "cos.h", "wb" );
		fprintf( f_sin, "float g_fsin[ 128 ] = { " );
		fprintf( f_cos, "float g_fcos[ 128 ] = { " );
		float step = SINUS_TABLE_STEP;
		float phase = 0;
		for( int a = 0; a < 128; a++ )
		{
		    float v;
		    v = sin( phase );
		    fprintf( f_sin, "%fF, ", v );
		    v = cos( phase );
		    fprintf( f_cos, "%fF, ", v );
		    phase += step;
		}
		fprintf( f_sin, "}" );
		fprintf( f_cos, "}" );
		fclose( f_cos );
		fclose( f_sin );
	    }
	    */
	    retval = 1;
	    break;

	case COMMAND_CLEAN:
	    {
		for( int f = 0; f < FORMANTS_NUM; f++ )
		{
		    for( ch = 0; ch < SYNTH_OUTPUTS; ch++ )
		    {
			ff_stop( &data->filters[ SYNTH_OUTPUTS * f + ch ] );
		    }
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
		//Interpolate freqs:
		int vn = data->ctl_vowel;
		if( vn >= 256 ) vn = 255;
		int pos = ( ( 256 * (VOWELS_NUM-1) ) * vn ) / 256;
		int f[ FORMANTS_NUM ];
		int b[ FORMANTS_NUM ];
		int amp[ FORMANTS_NUM ];
		uint16 *vowels = 0;
		switch( data->ctl_character )
		{
		    case 0: vowels = data->vowels1; break;
		    case 1: vowels = data->vowels2; break;
		    case 2: vowels = data->vowels3; break;
		    case 3: vowels = data->vowels4; break;
		}
		for( int a = 0; a < FORMANTS_NUM; a++ )
		{
		    f[ a ] = INTERP( (int)vowels[ (pos/256)*(FORMANTS_NUM*3) + a ], (int)vowels[ ((pos/256)+1)*(FORMANTS_NUM*3) + a ], pos & 255 );
		    b[ a ] = INTERP( (int)vowels[ (pos/256)*(FORMANTS_NUM*3) + FORMANTS_NUM + a ], (int)vowels[ ((pos/256)+1)*(FORMANTS_NUM*3) + FORMANTS_NUM + a ], pos & 255 );
		    amp[ a ] = INTERP( (int)vowels[ (pos/256)*(FORMANTS_NUM*3) + FORMANTS_NUM*2 + a ], (int)vowels[ ((pos/256)+1)*(FORMANTS_NUM*3) + FORMANTS_NUM*2 + a ], pos & 255 );
		    amp[ a ] += data->ctl_amp_add;
		    if( amp[ a ] > 256 ) amp[ a ] = 256;
		    amp[ a ] = ( amp[ a ] * data->ctl_volume ) / 256;
		}
		int outputs_num = psynth_get_number_of_outputs( synth_id, pnet );
		for( ch = 0; ch < outputs_num; ch++ )
		{
		    STYPE *in = inputs[ ch ];
		    STYPE *out = outputs[ ch ];
		    int channel_done = 0;
		    for( int a = 0; a < data->ctl_formants_num; a++ )
		    {
			int bw = data->ctl_bw;
			ff_set( 
			    &data->filters[ SYNTH_OUTPUTS * a + ch ], 
			    FF_TYPE_BANDPASS2, 
			    pnet->sampling_freq, 
			    f[ a ], 
			    ( b[ a ] * bw ) / 128, 
			    amp[ a ],
			    data->fsin, 
			    data->fcos );
			if( ff_do( &data->filters[ SYNTH_OUTPUTS * a + ch ], out, in, sample_frames, channel_done ) )
			{
			    retval = 1; //some data was added to output
			    channel_done = 1;
			}
		    }
		}
	    }
	    break;

	case COMMAND_CLOSE:
	    retval = 1;
	    break;
    }

    return retval;
}
