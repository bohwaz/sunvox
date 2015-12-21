/*
    psynths_sampler.cpp.
    This file is part of the SunVox engine.
    Copyright (C) 2002 - 2008 Alex Zolotov <nightradio@gmail.com>
*/

#include "psynth.h"
#include "utils/utils.h"
#include "core/debug.h"
#include "filesystem/v3nus_fs.h"
#ifdef SUNVOX_GUI
#include "window_manager/wmanager.h"
#endif

//Unique names for objects in your synth:
#define SYNTH_DATA	psynth_sampler_data
#define SYNTH_HANDLER	psynth_sampler
//And unique parameters:
#define SYNTH_INPUTS	0
#define SYNTH_OUTPUTS	2

#define MAX_CHANNELS	16

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
#define SVOX_PREC (long)16	    //sample pointer (fixed point) precision
#define SIGNED_SUB64( v1, v1_p, v2, v2_p ) \
{ \
    v1 -= v2; v1_p -= v2_p; \
    if( v1_p < 0 ) { v1--; v1_p += ( (long)1 << SVOX_PREC ); } \
}
#define SIGNED_ADD64( v1, v1_p, v2, v2_p ) \
{ \
    v1 += v2; v1_p += v2_p; \
    if( v1_p > ( (long)1 << SVOX_PREC ) - 1 ) { v1++; v1_p -= ( (long)1 << SVOX_PREC ); } \
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
    int	    cur_period_ptr;
    int	    sustain;

    ulong   v_pos;	//volume envelope position
    ulong   p_pos;	//panning envelope position
    long    fadeout;	//fadeout volume (0..65536). it's 65536 after the note has been triggered
    long    vib_pos;	//-32..31 - autovibrato position
    long    cur_frame;	//0..vibrato_sweep - current frame for autovibrato

    long    vol;	//0..64 - start volumes (level 1)
    long    vol_after_tremolo; //0..64 - volume after tremolo
    //for volume interpolation (in the envelope() function):
    long    env_start;	//first envelope frame flag (after envelope reset)
    long    vol_step;	//current interpolation step (from xx to 0)
    long    l_delta;	//0..1024<<12 - delta for left (12 because 12bits = 4096 - max step)
    long    r_delta;	//0..1024<<12 - delta for right
    long    l_cur;	//0..1024<<12 - current volumes:
    long    r_cur;	//0..1024<<12 
    long    l_old;	//0..1024 - previous volume
    long    r_old;	//0..1024 - previous volume

    ulong   back;    //0: ptr+=delta;  1: ptr-=delta

    int	    period_delta; //real period_ptr minus internal period_ptr

    int	    smp_num;

    //Local controllers:
    CTYPE   local_pan;
};

struct SYNTH_DATA
{
    //Controls: ############################################################
    CTYPE   ctl_volume;
    CTYPE   ctl_pan;
    CTYPE   ctl_smp_int;
    CTYPE   ctl_vol_int;
    CTYPE   ctl_channels;
    //Synth data: ##########################################################
    int	    tick_counter;   //From 0 to tick_size
    gen_channel   channels[ MAX_CHANNELS ];
    int	    search_ptr;
    ulong   *linear_tab;
    uchar   *vibrato_tab;
    int	    fdialog_opened;
    sundog_mutex    render_sound_mutex;
};

#define CHUNK_INS		    0
#define CHUNK_SMP( smp_num )	    ( 1 + smp_num * 2 )
#define CHUNK_SMP_DATA( smp_num )   ( 1 + smp_num * 2 + 1 )

#define ENV_TICKS	    512
#define EXT_INST_BYTES	    3

#define XM_PREC (long)16	    //sample pointer (fixed point) precision 
#define INTERP_PREC ( XM_PREC - 1 ) //sample interpolation precision 

struct sample
{
    ulong	    length;
    ulong	    reppnt;
    ulong	    replen;
    uchar	    volume;
    signed char	    finetune;
    char	    type;
    uchar	    panning;
    signed char	    relative_note;
    uchar	    reserved2;
    UTF8_CHAR	    name[ 22 ];
	
    signed short    *data;  //Sample data
};

struct instrument
{
    //Offset in XM file is 0:
    ulong   instrument_size;   
    UTF8_CHAR name[ 22 ];
    uchar   type;
    uint16  samples_num;       

    //Offset in XM file is 29:
    //>>> Standart info block:
    ulong   sample_header_size;
    uchar   sample_number[ 96 ];
    uint16  volume_points[ 24 ];
    uint16  panning_points[ 24 ];
    uchar   volume_points_num;
    uchar   panning_points_num;
    uchar   vol_sustain;
    uchar   vol_loop_start;
    uchar   vol_loop_end;	
    uchar   pan_sustain;
    uchar   pan_loop_start;
    uchar   pan_loop_end;
    uchar   volume_type;
    uchar   panning_type;
    uchar   vibrato_type;
    uchar   vibrato_sweep;
    uchar   vibrato_depth;
    uchar   vibrato_rate;
    uint16  volume_fadeout;
    //>>> End of standart info block. Size of this block is 212 bytes

    //Offset in XM file is 241:
    //Additional 2 bytes (empty in standart XM file):
    uchar        volume;	    //[for PsyTexx only]
    signed char  finetune;	    //[for PsyTexx only]
    
    //Offset in XM file is 243:
    //EXT_INST_BYTES starts here:
    uchar   panning;		    //[for PsyTexx only]
    signed char  relative_note;	    //[for PsyTexx only]
    uchar	flags;		    //[for PsyTexx only]
    
    //System use data: (not in XM file)
    uint16  volume_env[ ENV_TICKS ];  //each value is 0..1023
    uint16  panning_env[ ENV_TICKS ];
    
    sample  *samples[ 16 ];
};

#define LOAD_XI_FLAG_SET_MAX_VOLUME	1

void refresh_instrument_envelope( uint16 *src, uint16 points, uint16 *dest );
void refresh_instrument_envelopes( instrument *ins );
void make_default_envelopes( instrument *ins );
void bytes2frames( sample *smp );
void new_instrument( const UTF8_CHAR *name, instrument *ins );
void new_sample( long length, sample *smp );
void save_wav_sample( 
    UTF8_CHAR *filename, 
    int synth_id, 
    void *net, 
    int sample_num );
void *create_raw_instrument_or_sample( 
    UTF8_CHAR *name, 
    int synth_id, 
    ulong data_bytes, 
    int bits, 
    int channels, 
    void *net, 
    int sample_num );
void load_wav_instrument_or_sample( 
    V3_FILE f, 
    UTF8_CHAR *name, 
    int synth_id, 
    void *net, 
    int sample_num );
void load_xi_instrument( 
    V3_FILE f, 
    int flags, 
    int synth_id, 
    void *net );
int load_instrument_or_sample( 
    UTF8_CHAR *filename, 
    int flags, 
    int synth_id, 
    void *net, 
    int sample_num );
void envelope( 
    gen_channel *cptr, 
    SYNTH_DATA *data,
    int synth_id, 
    void *net );

#ifdef SUNVOX_GUI
//###########################################################
//###########################################################
//## GUI (Visual)
//###########################################################
//###########################################################
#include "../../sunvox/main/psynths_sampler_gui.h"
//###########################################################
//###########################################################
//## End of GUI
//###########################################################
//###########################################################
#endif

//src values: 0..64
//dest values: 0..1024
void refresh_instrument_envelope( uint16 *src, uint16 points, uint16 *dest )
{
    long b;
    long cur_pnt; //current point;
    long val;
    long delta;
    long f1, v1, f2, v2, max = 0;
    
    for( b = 0; b < ENV_TICKS; b++ ) dest[ b ] = 0;
    
    if( points >= 2 )
    {
	for( cur_pnt = 0; cur_pnt < ( points - 1 ) * 2; cur_pnt += 2 )
	{
	    f1 = src[ cur_pnt ];
	    v1 = src[ cur_pnt + 1 ];
	    f2 = src[ cur_pnt + 2 ];
	    v2 = src[ cur_pnt + 3 ];
	    if( f2 < max ) break;
	    max = f2;
	    
	    val = v1 << 10; //val = 6+10 = 16 bits
	    if( f2 - f1 == 0 ) delta = 0; else delta = ( ( v2 - v1 ) << 10 ) / ( f2 - f1 );
	    for( b = f1; b <= f2; b++ )
	    {
		dest[ b ] = (uint16) ( val >> 6 ); //16-4 = 10 bits
    		val += delta;
	    }
	    dest[ f2 ] = (uint16)v2 << 4; //6+4 = 10 bits
	}
    }
}

void refresh_instrument_envelopes( instrument *ins ) 
{
    refresh_instrument_envelope( ins->volume_points, ins->volume_points_num, ins->volume_env );
    refresh_instrument_envelope( ins->panning_points, ins->panning_points_num, ins->panning_env );
}

void make_default_envelopes( instrument *ins ) 
{
    ins->volume_points[ 0 ] = 0;
    ins->volume_points[ 0 + 1 ] = 0;
    ins->volume_points[ 2 ] = 32;
    ins->volume_points[ 2 + 1 ] = 64;
    ins->volume_points[ 4 ] = 128;
    ins->volume_points[ 4 + 1 ] = 32;
    ins->volume_points[ 6 ] = 256;
    ins->volume_points[ 6 + 1 ] = 0;
    ins->volume_points_num = 4;
    ins->vol_sustain = 1;

    ins->panning_points[ 0 ] = 0;
    ins->panning_points[ 0 + 1 ] = 32;
    ins->panning_points[ 2 ] = 64;
    ins->panning_points[ 2 + 1 ] = 0;
    ins->panning_points[ 4 ] = 128;
    ins->panning_points[ 4 + 1 ] = 64;
    ins->panning_points[ 6 ] = 180;
    ins->panning_points[ 6 + 1 ] = 32;
    ins->panning_points_num = 4;
}

void bytes2frames( sample *smp )
{
    long bits = 8;
    long channels = 1;
    if( ( ( smp->type >> 4 ) & 3 ) == 0 ) bits = 8;
    if( ( ( smp->type >> 4 ) & 3 ) == 1 ) bits = 16;
    if( ( ( smp->type >> 4 ) & 3 ) == 2 ) bits = 32;
    if( smp->type & 64 ) channels = 2;
    smp->length = smp->length / ( ( bits / 8 ) * channels );
    smp->reppnt = smp->reppnt / ( ( bits / 8 ) * channels );
    smp->replen = smp->replen / ( ( bits / 8 ) * channels );
}

void new_instrument( const UTF8_CHAR *name, instrument *ins )
{
    int a;
    
    //save info:
    for( a = 0; a < 22; a++ )
    {
	ins->name[ a ] = name[ a ];
	if( name[ a ] == 0 ) break;
    }

    //Set some default info about instrument:
    ins->volume_points_num = 0;
    ins->panning_points_num = 0;
    ins->volume = 64;
    ins->panning = 128;

    make_default_envelopes( ins );
}

void new_sample( long length, sample *smp )
{
    //create NULL sample:
    smp->data = 0;
  
    smp->length = length;

    smp->volume = 0x40;
    smp->panning = 0x80;
    smp->relative_note = 0;
    smp->finetune = 0;
    smp->reppnt = 0;
    smp->replen = 0;
}

void save_wav_sample( 
    UTF8_CHAR *filename, 
    int synth_id, 
    void *net, 
    int sample_num )
{
    instrument *ins = (instrument*)psynth_get_chunk( synth_id, CHUNK_INS, net );
    sample *smp = (sample*)psynth_get_chunk( synth_id, CHUNK_SMP( sample_num ), net );
    void *smp_data = psynth_get_chunk( synth_id, CHUNK_SMP_DATA( sample_num ), net );
    if( ins && smp && smp_data )
    {
	int flags;
	ulong sdata_size;
	if( psynth_get_chunk_info( synth_id, CHUNK_SMP_DATA( sample_num ), net, &sdata_size, &flags ) )
	{
	    dprint( "ERROR. Can't get sample properties\n" );
	    return;
	}

	int bits = 8;
	int channels = 1;
	if( flags & PSYNTH_CHUNK_FLAG_SAMPLE_8BIT ) bits = 8;
	if( flags & PSYNTH_CHUNK_FLAG_SAMPLE_16BIT ) bits = 16;
	if( flags & PSYNTH_CHUNK_FLAG_SAMPLE_FLOAT ) bits = 32;
	if( flags & PSYNTH_CHUNK_FLAG_SAMPLE_STEREO ) channels = 2;

	V3_FILE f = v3_open( filename, "wb" );
	if( f )
	{
	    //WAV header:
	    v3_write( (void*)"RIFF", 1, 4, f );
	    ulong val;
	    val = 4 + 24 + 8 + sdata_size; v3_write( &val, 1, 4, f );
	    v3_write( (void*)"WAVE", 1, 4, f );

	    //WAV FORMAT:
	    v3_write( (void*)"fmt ", 1, 4, f );
	    val = 16; v3_write( &val, 1, 4, f );
	    val = 1; 
	    if( bits == 32 ) val = 3; 
	    v3_write( &val, 1, 2, f ); //format
	    val = channels; v3_write( &val, 1, 2, f ); //channels
	    val = 44100; v3_write( &val, 1, 4, f ); //samples per second
	    val = 44100 * channels * (bits/8); v3_write( &val, 1, 4, f ); //bytes per second
	    val = channels * ( bits / 8 ); v3_write( &val, 1, 2, f ); //sample size (bytes)
	    val = bits; v3_write( &val, 1, 2, f ); //bits

	    //WAV DATA:
	    v3_write( (void*)"data", 1, 4, f );
	    v3_write( &sdata_size, 1, 4, f );
	    if( bits == 8 )
	    {
		signed char *sdata = (signed char*)smp_data;
		for( ulong s = 0; s < sdata_size; s++ )
		{
		    int v = sdata[ s ] + 128;
		    v3_putc( v, f );
		}
	    }
	    else
	    {
		v3_write( smp_data, 1, sdata_size, f );
	    }

	    v3_close( f );
	}
    }
}

//if sample_num < 0 then load WAV as new instrument
//(frames must be signed)
void *create_raw_instrument_or_sample( 
    UTF8_CHAR *name, 
    int synth_id, 
    ulong data_bytes, 
    int bits, 
    int channels, 
    void *net, 
    int sample_num )
{
    //Create instrument structure:
    if( sample_num < 0 ) psynth_new_chunk( synth_id, CHUNK_INS, sizeof( instrument ), 0, net );
    instrument *ins = (instrument*)psynth_get_chunk( synth_id, CHUNK_INS, net );
    if( ins == 0 ) return 0;
 
    if( sample_num < 0 )
    {
	new_instrument( name, ins );
	refresh_instrument_envelopes( ins );
    }

    int new_sample_num = 0;
    if( sample_num >= 0 ) new_sample_num = sample_num;
    sample *smp;
    void *smp_data;

    psynth_new_chunk( synth_id, CHUNK_SMP( new_sample_num ), sizeof( sample ), 0, net );
    smp = (sample*)psynth_get_chunk( synth_id, CHUNK_SMP( new_sample_num ), net );
    new_sample( data_bytes, smp );
    ins->samples[ new_sample_num ] = smp;
    if( smp )
    {
	int chunk_flags = 0;
	if( bits == 32 ) chunk_flags |= PSYNTH_CHUNK_FLAG_SAMPLE_FLOAT;
	if( bits == 16 ) chunk_flags |= PSYNTH_CHUNK_FLAG_SAMPLE_16BIT;
	if( bits == 8 ) chunk_flags |= PSYNTH_CHUNK_FLAG_SAMPLE_8BIT;
	if( channels == 2 ) chunk_flags |= PSYNTH_CHUNK_FLAG_SAMPLE_STEREO;
	psynth_new_chunk( synth_id, CHUNK_SMP_DATA( new_sample_num ), data_bytes, chunk_flags, net );
	smp_data = psynth_get_chunk( synth_id, CHUNK_SMP_DATA( new_sample_num ), net );
	smp->data = (short*)smp_data;
        if( smp_data )
	{
	    //set sample info:
	    smp->type = 0;
	    if( bits == 16 ) smp->type = 1 << 4;
	    if( bits == 32 ) smp->type = 2 << 4;
	    if( channels == 2 ) smp->type |= 64;
	    smp->finetune = -22;
	    //convert sample info:
	    bytes2frames( smp );
	    //set number of samples:
    	    ins->samples_num = 0;
    	    for( int ss = 0; ss < 16; ss++ )
    	    {
    		if( psynth_get_chunk( synth_id, CHUNK_SMP( ss ), net ) )
		    ins->samples_num = ss + 1;
	    }
	    return smp_data;
	}
    }
    return 0;
}

//if sample_num < 0 then load WAV as new instrument
void load_wav_instrument_or_sample( 
    V3_FILE f, 
    UTF8_CHAR *name, 
    int synth_id, 
    void *net, 
    int sample_num )
{
    ulong chunk[ 2 ]; //Chunk type and size
    int other_info;
    uint16 channels = 1;
    uint16 bits = 16;
    
    while( 1 )
    {
	if( v3_eof( f ) != 0 ) break;
	v3_read( &chunk, 8, 1, f );
	if( chunk[ 0 ] == 0x20746D66 ) //'fmt ':
	{
	    v3_seek( f, 2, 1 ); //Format
	    v3_read( &channels, 2, 1, f );
	    v3_seek( f, 10, 1 ); //Some info
	    v3_read( &bits, 2, 1, f );
	    other_info = 16 - chunk[ 1 ];
	    if( other_info ) v3_seek( f, other_info, 1 );
	} 
	else
	if( chunk[ 0 ] == 0x61746164 ) //'data':
	{
	    void *smp_data = create_raw_instrument_or_sample( name, synth_id, chunk[ 1 ], bits, channels, net, sample_num );
	    if( smp_data )
	    {
	        v3_read( smp_data, chunk[ 1 ], 1, f ); //read sample data
    		if( bits == 8 )
		{
		    //Convert 8bit data:
		    uchar *smp_data_u = (uchar*)smp_data;
		    signed char *smp_data_s = (signed char*)smp_data;
		    for( ulong s = 0; s < chunk[ 1 ]; s++ )
		    {
		        int v = smp_data_u[ s ];
		        v += 128;
		        smp_data_s[ s ] = (signed char)v;
		    }
		}
	    }
	    break;
	}
	else v3_seek( f, chunk[ 1 ], 1 );
    }
}

void load_xi_instrument( 
    V3_FILE f, 
    int flags, 
    int synth_id, 
    void *net )
{
    //Create instrument structure:
    psynth_new_chunk( synth_id, CHUNK_INS, sizeof( instrument ), 0, net );
    instrument *ins = (instrument*)psynth_get_chunk( synth_id, CHUNK_INS, net );
    if( ins == 0 ) return;

    UTF8_CHAR name[ 32 ];
    char temp[ 32 ];
    if( f )
    {
	v3_read( name, 21 - 12, 1, f ); // "Extended instrument: " 
	v3_read( name, 22, 1, f ); // Instrument name
	v3_read( temp, 23, 1, f );

	//Get version:
	int v1 = temp[ 21 ];
	int v2 = temp[ 22 ];
	int psytexx_ins = 0;
	if( v1 == 0x50 && v2 == 0x50 ) psytexx_ins = 1;
	
	new_instrument( name, ins );
	
	v3_read( ins->sample_number, 208, 1, f ); //Instrument info
	ins->volume = 64;
	if( !psytexx_ins )
	{
	    //Unused 22 bytes :(
	    v3_read( temp, 22, 1, f );
	}
	else
	{
	    //Yea. 22 bytes for me :)
	    v3_read( &ins->volume, 2 + EXT_INST_BYTES, 1, f );
	    v3_read( temp, 22 - ( 2 + EXT_INST_BYTES ), 1, f );
	}
	v3_read( &ins->samples_num, 2, 1, f ); //Samples number
	
	//Create envelopes:
	refresh_instrument_envelopes( ins );
	
	//Load samples:
	int s;
	sample *smp;
	for( s = 0; s < ins->samples_num; s++ )
	{
	    psynth_new_chunk( synth_id, CHUNK_SMP( s ), sizeof( sample ), 0, net );
	    smp = (sample*)psynth_get_chunk( synth_id, CHUNK_SMP( s ), net );
	    v3_read( smp, 40, 1, f ); //Load sample info
	    if( flags & LOAD_XI_FLAG_SET_MAX_VOLUME )
		smp->volume = 64;
	    smp->data = 0;
	    if( smp->length )
	    {
		int chunk_flags = 0;
		if( ( ( smp->type >> 4 ) & 3 ) == 0 ) 
		    chunk_flags |= PSYNTH_CHUNK_FLAG_SAMPLE_8BIT;
		if( ( ( smp->type >> 4 ) & 3 ) == 1 ) 
		    chunk_flags |= PSYNTH_CHUNK_FLAG_SAMPLE_16BIT;
		if( ( ( smp->type >> 4 ) & 3 ) == 2 ) 
		    chunk_flags |= PSYNTH_CHUNK_FLAG_SAMPLE_FLOAT;
		if( smp->type & 64 ) chunk_flags |= PSYNTH_CHUNK_FLAG_SAMPLE_STEREO;
		psynth_new_chunk( synth_id, CHUNK_SMP_DATA( s ), smp->length, chunk_flags, net );
		smp->data = (signed short*)psynth_get_chunk( synth_id, CHUNK_SMP_DATA( s ), net );
	    }
	    ins->samples[ s ] = smp;
	}
	//Load samples data:
	for( s = 0; s < ins->samples_num; s++ )
	{
	    smp = ins->samples[ s ];
	    char *smp_data = (char*) smp->data;
	    if( smp_data )
	    {
		long len = smp->length;
		v3_read( smp_data, len, 1, f ); //load data
		//convert it:
		long sp; 
		if( ( ( smp->type >> 4 ) & 3 ) == 1 )
		{ //16bit sample:
		    signed short old_s = 0;
		    signed short *s_data = (signed short*) smp_data;
		    signed short new_s;
		    for( sp = 0; sp < len/2; sp++ )
		    {
			new_s = s_data[ sp ] + old_s;
			s_data[ sp ] = new_s;
			old_s = new_s;
		    }
		}
		if( ( ( smp->type >> 4 ) & 3 ) == 0 )
		{ //8bit sample:
		    signed char c_old_s = 0;
		    signed char *cs_data = (signed char*) smp_data;
		    signed char c_new_s;
		    for( sp = 0; sp < len; sp++ )
		    {
			c_new_s = cs_data[sp] + c_old_s;
			cs_data[sp] = c_new_s;
			c_old_s = c_new_s;
		    }
		}
		if( ( ( smp->type >> 4 ) & 3 ) > 0 )
		{
		    //convert sample info:
		    bytes2frames( smp );
		}
	    }
	}
    } //if( f )
}

//if sample_num < 0 then load sample as new instrument
int load_instrument_or_sample( 
    UTF8_CHAR *filename, 
    int flags, 
    int synth_id, 
    void *net, 
    int sample_num )
{
    int instr_created = 0;
    char temp[ 8 ];
    int fn;
    for( fn = 0; ; fn++ ) if( filename[ fn ] == 0 ) break;
    for( ; fn >= 0; fn-- ) if( filename[ fn ] == '/' ) { fn++; break; }
    V3_FILE f = v3_open( filename, "rb" );
    if( f )
    {
	v3_read( temp, 4, 1, f );

	if( temp[ 0 ] == 'E' && temp[ 1 ] == 'x' && temp[ 2 ] == 't' )
	{
	    v3_read( temp, 8, 1, f );
	    if( temp[ 5 ] == 'I' )
	    {
		//Clear all info about previous instrument:
		psynth_clear_chunks( synth_id, net );

		load_xi_instrument( f, flags, synth_id, net );
		instr_created = 1;
	    }
	}
	if( temp[ 0 ] == 'R' && temp[ 1 ] == 'I' && temp[ 2 ] == 'F' )
	{
	    v3_read( temp, 8, 1, f );
	    if( temp[ 4 ] == 'W' && temp[ 5 ] == 'A' && temp[ 6 ] == 'V' )
	    {
		//Clear all info about previous instrument:
		if( sample_num < 0 ) psynth_clear_chunks( synth_id, net );

		load_wav_instrument_or_sample( f, filename + fn, synth_id, net, sample_num );
		instr_created = 1;
	    }
	}
	
	v3_close( f );
    }
    if( !instr_created )
    {
	//Instrument type not recognized:
	return 1;
    }
    return 0;
}

void envelope( 
    gen_channel *cptr, 
    SYNTH_DATA *data,
    int synth_id, 
    void *net ) //***> every tick <***
{
    instrument *ins = (instrument*)psynth_get_chunk( synth_id, CHUNK_INS, net );
    if( ins == 0 ) return;

    psynth_net *pnet = (psynth_net*)net;

    long pan_value = 128;
    long l_vol, r_vol; //current envelope volume (0..1024)

    long pos;      //vibrato position
    long temp = 0; //temp variable
    long new_period;
    long new_freq;

    //VOLUME ENVELOPE:
    if( ins->volume_type & 1 ) { //volume envelope ON:
	if( ins->volume_type & 4 ) { //envelope loop ON:
	    if( cptr->v_pos >= ins->volume_points[ ins->vol_loop_end << 1 ] )  
		cptr->v_pos = ins->volume_points[ ins->vol_loop_start << 1 ];
	}
	int envelope_finished = 0;
	if( cptr->v_pos >= ins->volume_points[ ( ins->volume_points_num - 1 ) << 1 ] ) 
	{
	    cptr->v_pos = ins->volume_points[ ( ins->volume_points_num - 1 ) << 1 ];
	    envelope_finished = 1;
	}
	if( cptr->sustain == 0 ) 
	{
	    cptr->fadeout -= ins->volume_fadeout << 1;
	    if( cptr->fadeout < 0 ) 
	    {
		cptr->fadeout = 0;
		cptr->playing = 0;
	    }
	}
	
	r_vol = l_vol = ( cptr->fadeout * ins->volume_env[ cptr->v_pos ] ) >> 16;

	if( !( ins->volume_type & 4 ) ) { //envelope loop OFF:
	    if( envelope_finished == 1 && r_vol == 0 && l_vol == 0 )
		cptr->playing = 0;
	}

	if( cptr->v_pos >= ins->volume_points[ ins->vol_sustain << 1 ] ) {
	    if( ins->volume_type & 2 ) 
		{ if( cptr->sustain == 0 ) cptr->v_pos++; } else cptr->v_pos++;
	} else 
	{
	    cptr->v_pos++;
	}
    } else { //volume envelope OFF:
	if( cptr->sustain == 0 ) {
	    r_vol = l_vol = 0;
	    cptr->playing = 0;
	} else {
	    r_vol = l_vol = 1024;
	}
    }
    
    //PANNING ENVELOPE:
    if( ins->panning_type & 1 ) { //panning envelope ON:
	if( ins->panning_type & 4 ) { //envelope loop ON:
	    if( cptr->p_pos >= ins->panning_points[ ins->pan_loop_end << 1 ] )  
		cptr->p_pos = ins->panning_points[ ins->pan_loop_start << 1 ];
	}
	if( cptr->p_pos >= ins->panning_points[ ( ins->panning_points_num - 1 ) << 1 ] ) cptr->p_pos--;
	
	pan_value = ins->panning_env[ cptr->p_pos ] >> 2;

	if( cptr->p_pos >= ins->panning_points[ ins->pan_sustain << 1 ] ) {
	    if( ins->panning_type & 2 ) 
		{ if( cptr->sustain == 0 ) cptr->p_pos++; } else cptr->p_pos++;
	} else {
	    cptr->p_pos++;
	}
    }

    sample *smp = (sample*)psynth_get_chunk( synth_id, CHUNK_SMP( cptr->smp_num ), pnet );
        
    //SET PANNING:
    pan_value += data->ctl_pan - 128;
    pan_value += cptr->local_pan - 128;
    pan_value += (int)smp->panning - 128;
    if( pan_value < 0 ) pan_value = 0;
    if( pan_value > 256 ) pan_value = 256;
    //Instrument panning ignored.
    //Set instrument volume:
    if( ins->volume < 0x40 )
    {
	l_vol *= ins->volume; l_vol >>= 6;
	r_vol *= ins->volume; r_vol >>= 6;
    }
    int lpan = ( 256 - pan_value ) << 2;
    int rpan = pan_value << 2;
    if( lpan > 512 ) lpan = 512;
    if( rpan > 512 ) rpan = 512;
    l_vol = ( l_vol * lpan ) >> 10;
    r_vol = ( r_vol * rpan ) >> 10;

    //Set sample volume:
    l_vol = ( l_vol * smp->volume ) / 64;
    r_vol = ( r_vol * smp->volume ) / 64;

    //Set global volume of the synth:
    l_vol = ( l_vol * data->ctl_volume ) / 256;
    r_vol = ( r_vol * data->ctl_volume ) / 256;

    //Set velocity:
    l_vol = ( l_vol * cptr->vel ) / 256;
    r_vol = ( r_vol * cptr->vel ) / 256;

    if( data->ctl_vol_int )
    {
	//FOR VOLUME INTERPOLATION:
	if( cptr->env_start ) { //first tick:
	    cptr->l_old = l_vol;
	    cptr->r_old = r_vol;
	    cptr->env_start = 0;
	}
	cptr->vol_step = pnet->tick_size >> 8;
	cptr->l_delta = ( ( l_vol - cptr->l_old ) << 12 ) / cptr->vol_step;  //1 = 4096
	cptr->r_delta = ( ( r_vol - cptr->r_old ) << 12 ) / cptr->vol_step;
	cptr->l_cur = cptr->l_old << 12;
	cptr->r_cur = cptr->r_old << 12;
	cptr->l_old = l_vol;
	cptr->r_old = r_vol;
    }
    else
    {
	cptr->l_cur = l_vol << 12; //It is 1024max * 4096
	cptr->r_cur = r_vol << 12; //same
    }

    //AUTOVIBRATO:
    if( ins->vibrato_depth )  {
	pos = cptr->vib_pos & 255;
	switch( ins->vibrato_type & 3 ) {
	    case 0: //sine:
	            temp = ( data->vibrato_tab[pos] * ins->vibrato_depth ) >> 8;
		    if(cptr->vib_pos > 0) temp = -temp;
		    break;
	    case 1: //ramp down:
		    temp = -cptr->vib_pos >> 1;
		    temp = ( temp * ins->vibrato_depth ) >> 4;
		    break;
	    case 2: //square:
		    temp = (ins->vibrato_depth * 127) >> 4;
		    if(cptr->vib_pos < 0) temp = -temp;
		    break;
	    case 3: //random:
		    break;
	}

	//sweep control: ============
	if( ins->vibrato_sweep ) {
	    temp = ( temp * cptr->cur_frame ) / ins->vibrato_sweep;
	    cptr->cur_frame += 2;
	    if(cptr->cur_frame > ins->vibrato_sweep) cptr->cur_frame = ins->vibrato_sweep;
	}
	//===========================

	cptr->vib_pos += ins->vibrato_rate << 1;
	if( cptr->vib_pos > 255 ) cptr->vib_pos -= 512;
    
	new_period = cptr->cur_period_ptr + temp * 4;
    } 
    else
    { 
	new_period = cptr->cur_period_ptr;
    }  
    
    //Frequency handling STEP 2 (last step) is over:   <***
    //resulted period in new_period;                   <***
    //calculate new frequency and delta:
    GET_FREQ( new_freq, new_period / 4 );
    GET_DELTA( new_freq, cptr->delta_h, cptr->delta_l );
}

//************************************
//************************************
//************************************

int SYNTH_HANDLER( 
    PSYTEXX_SYNTH_PARAMETERS
    )
{
    psynth_net *pnet = (psynth_net*)net;
    SYNTH_DATA *data = (SYNTH_DATA*)data_ptr;
    int retval = 0;
    instrument *ins;

    switch( command )
    {
	case COMMAND_GET_DATA_SIZE:
	    retval = sizeof( SYNTH_DATA );
	    break;

	case COMMAND_GET_SYNTH_NAME:
	    retval = (int)"Sampler";
	    break;

	case COMMAND_GET_SYNTH_INFO:
	    retval = (int)"Sampler.\nSupported file formats: WAV, XI\n\nAvailable local controllers:\n * Pan";
	    break;

	case COMMAND_GET_INPUTS_NUM: retval = SYNTH_INPUTS; break;
	case COMMAND_GET_OUTPUTS_NUM: retval = SYNTH_OUTPUTS; break;

	case COMMAND_GET_FLAGS: retval = PSYNTH_FLAG_GENERATOR; break;

	case COMMAND_INIT:
	    psynth_register_ctl( synth_id, "Volume", "", 0, 512, 256, 0, &data->ctl_volume, net );
	    psynth_register_ctl( synth_id, "Panning", "", 0, 256, 128, 0, &data->ctl_pan, net );
	    psynth_register_ctl( synth_id, "Sample interpolation", "off/linear", 0, 1, 1, 1, &data->ctl_smp_int, net );
	    psynth_register_ctl( synth_id, "Volume interpolation", "off/linear", 0, 1, 1, 1, &data->ctl_vol_int, net );
	    psynth_register_ctl( synth_id, "Polyphony", "ch.", 1, MAX_CHANNELS, 8, 1, &data->ctl_channels, net );
	    for( int c = 0; c < MAX_CHANNELS; c++ )
	    {
		data->channels[ c ].playing = 0;
		data->channels[ c ].ptr_l = 0;
		data->channels[ c ].ptr_h = 0;
		data->channels[ c ].sustain = 0;
	    }
	    data->search_ptr = 0;
	    data->tick_counter = 0;
	    data->linear_tab = g_linear_tab;
	    data->vibrato_tab = g_vibrato_tab;
	    data->fdialog_opened = 0;

	    sundog_mutex_init( &data->render_sound_mutex, 0 );

	    //Create empty instrument:
	    {
		psynth_new_chunk( synth_id, CHUNK_INS, sizeof( instrument ), 0, pnet );
		instrument *ins = (instrument*)psynth_get_chunk( synth_id, CHUNK_INS, pnet );
		new_instrument( "", ins );
		refresh_instrument_envelopes( ins );
	    }

#ifdef SUNVOX_GUI
	    {
		pnet->items[ synth_id ].visual = new_window( "Sampler GUI", 0, 0, 10, 10, g_wm->colors[ 5 ], 0, sampler_visual_handler, g_wm );
		sampler_visual_data *smp_data = (sampler_visual_data*)pnet->items[ synth_id ].visual->data;
		
		smp_data->synth_data = data;
		smp_data->synth_id = synth_id;
		smp_data->pnet = pnet;

		sampler_editor_data *data2 = (sampler_editor_data*)smp_data->editor_window->childs[ 0 ]->data;
		data2->synth_data = data;
		data2->synth_id = synth_id;
		data2->pnet = pnet;

		sample_editor_data_l *data3 = (sample_editor_data_l*)data2->samples_l->data;
		data3->synth_data = data;
		data3->synth_id = synth_id;
		data3->pnet = pnet;

		sample_editor_data_r *data4 = (sample_editor_data_r*)data2->samples_r->data;
		data4->synth_data = data;
		data4->synth_id = synth_id;
		data4->pnet = pnet;
		data4->ldata = data3;

		envelope_editor_data_l *data5 = (envelope_editor_data_l*)data2->envelopes_l->data;
		data5->synth_data = data;
		data5->synth_id = synth_id;
		data5->pnet = pnet;

		envelope_editor_data_r *data6 = (envelope_editor_data_r*)data2->envelopes_r->data;
		data6->synth_data = data;
		data6->synth_id = synth_id;
		data6->pnet = pnet;
	    }
#endif

	    retval = 1;
	    break;

	case COMMAND_RENDER_REPLACE:
	    if( !outputs[ 0 ] ) break;

	    if( sundog_mutex_lock( &data->render_sound_mutex ) != 0 )
		break; //Error. Can't lock/unlock this mutex.

	    ins = (instrument*)psynth_get_chunk( synth_id, CHUNK_INS, pnet );
	    if( ins )
	    {
		//Make correct links inside of structure with instrument:
		for( int s = 0; s < 16; s++ )
		{
		    sample *smp = (sample*)psynth_get_chunk( synth_id, CHUNK_SMP( s ), pnet );
		    if( smp )
		    {
			smp->data = (short*)psynth_get_chunk( synth_id, CHUNK_SMP_DATA( s ), pnet );
		    }
		    ins->samples[ s ] = smp;
		}

		int all_channels_empty = 1;
		for( int c = 0; c < data->ctl_channels; c++ )
		    if( data->channels[ c ].playing ) all_channels_empty = 0;
		if( all_channels_empty == 1 ) goto render_finished; //All channels are empty
		
		for( int ch = 0; ch < SYNTH_OUTPUTS; ch++ )
		{
		    STYPE *out_ch = outputs[ ch ];
		    if( out_ch )
			for( int i = 0; i < sample_frames; i++ ) out_ch[ i ] = 0;
		}

		//Render output sound:	
		int tick_size = pnet->tick_size >> 8;
		if( data->tick_counter == 0xFFFFFFF )
		    data->tick_counter = tick_size;
		int ptr = 0;
		while( 1 )
		{
		    int buf_size = tick_size - data->tick_counter;
		    if( buf_size <= 0 ) buf_size = 0;
		    if( ptr + buf_size > sample_frames ) buf_size = sample_frames - ptr;

		    int rendered = 0;
		    for( int c = 0; c < data->ctl_channels; c++ ) //polyphony
		    {
			gen_channel *chan = &data->channels[ c ];
			if( chan->playing )
			{
			    //for( ch = 0; ch < SYNTH_OUTPUTS; ch++ )
			    {
				STYPE *out_l = outputs[ 0 ];
				STYPE *out_r = outputs[ 1 ];
				if( out_r == 0 ) out_r = out_l;
				sample *smp = (sample*)psynth_get_chunk( synth_id, CHUNK_SMP( chan->smp_num ), pnet );
				char *smp_data = (char*)psynth_get_chunk( synth_id, CHUNK_SMP_DATA( chan->smp_num ), pnet );
				if( smp == 0 ) continue;
				if( smp_data == 0 ) continue;
				long reppnt = smp->reppnt;
				long replen = smp->replen;
				if( retval == 0 ) 
				{
				}
				if( !( smp->type & 2 ) && chan->back )
				{
				    //Loop is forward or disabled, but channel playing in back direction.
				    //Fix it:
				    chan->back = 0;
				}
				long s;            	//current piece of sample (volume from -32767 to 32767)
				long sr;           	//the same as above but for the next channel (right)
			        long s2;           	//next piece of sample
				float s_f;
				float sr_f;
			        float s2_f;
				long intr;         	//interpolation scale
				long intr2;        	//interpolation scale
				long s_offset;
				long s_offset2;
				signed short *pnt; 	//sample pointer (16 bit data)
				signed char *cpnt; 	//sample pointer (8 bit data)
				float *fpnt; 		//sample pointer (32 bit data)
				long left;         	//left channel
				long right;        	//right channel
				int smp_type = ( smp->type >> 4 ) & 3;
				for( int i = ptr; i < ptr + buf_size; i++ )
				{
				    s_offset = chan->ptr_h;
				    if( replen && s_offset >= ( reppnt + replen ) )
				    { //handle loop:
					if( smp->type & 1 ) 
					{ //forward loop:
next_iteration:
					    chan->ptr_h -= replen;
					    s_offset = chan->ptr_h;
					    if( s_offset >= ( reppnt + replen ) ) goto next_iteration;
					}
					else
					if( smp->type & 2 ) 
					{ //ping-pong loop:
					    long rep_part = ( s_offset - reppnt ) / replen;
					    if( rep_part & 1 )
					    {
						chan->back = 1;
						long temp_ptr_h = chan->ptr_h;
						long temp_ptr_l = chan->ptr_l;
						chan->ptr_h = reppnt + replen * ( rep_part + 1 );
						chan->ptr_l = 0;
						SIGNED_SUB64( chan->ptr_h, chan->ptr_l, temp_ptr_h, temp_ptr_l );
						chan->ptr_h += reppnt;
					    }
					    else
					    {
						chan->back = 0;
						chan->ptr_h -= replen * rep_part;
					    }
					    s_offset = chan->ptr_h;
					}
				    }
				    if( chan->back ) 
				    { //ping-pong loop:
					if( s_offset < reppnt && ( smp->type & 2 ) ) 
					{
					    long temp_ptr_h2 = chan->ptr_h;
					    long temp_ptr_l2 = chan->ptr_l;
					    chan->ptr_h = reppnt;
					    chan->ptr_l = 0;
					    SIGNED_SUB64( chan->ptr_h, chan->ptr_l, temp_ptr_h2, temp_ptr_l2 );
					    long rep_part = chan->ptr_h / replen;
					    chan->ptr_h += reppnt;
					    if( rep_part & 1 )
					    {
						chan->back = 1;
						long temp_ptr_h = chan->ptr_h;
						long temp_ptr_l = chan->ptr_l;
						chan->ptr_h = reppnt + replen * ( rep_part + 1 );
						chan->ptr_l = 0;
						SIGNED_SUB64( chan->ptr_h, chan->ptr_l, temp_ptr_h, temp_ptr_l );
						chan->ptr_h += reppnt;
					    }
					    else
					    {
						chan->back = 0;
						chan->ptr_h -= replen * rep_part;
					    }
					    s_offset = chan->ptr_h;
					}
				    }
				    if( s_offset >= (long)smp->length )
				    { //end of sample:
					chan->ptr_h = smp->length;
					chan->playing = 0;
					s = 0;
					sr = 0;
				    } else { //get sample data: //##################################
					if( data->ctl_smp_int )
					{
					    //interpolation: 
					    intr = chan->ptr_l >> ( XM_PREC - INTERP_PREC ); intr2 = ( 1 << INTERP_PREC ) - 1 - intr;
					    if( s_offset < (long)smp->length - 1 ) s_offset2 = s_offset + 1; else s_offset2 = s_offset;
					    //loop correction: =====
					    if( replen )
					    {
						if( smp->type & 1 && s_offset >= ( reppnt + replen ) - 1 ) s_offset2 = reppnt; //for forward loop
						if( smp->type & 2 && s_offset >= ( reppnt + replen ) - 1 ) s_offset2 = s_offset; //for ping pong loop
					    }
					    //======================
					}
					if( smp_type == 1 ) 
					{ //16 bit data:
					    pnt = smp->data;
					    if( smp->type & 64 )
					    { //Stereo sample:
						if( data->ctl_smp_int ) 
						{
						    s = pnt[ s_offset << 1 ]; s *= intr2; //get data for left channel
						    s2 = pnt[ s_offset2 << 1 ]; s2 *= intr;
						    s += s2; s >>= INTERP_PREC;
						    sr = pnt[ ( s_offset << 1 ) + 1 ]; sr *= intr2; //get data for right channel
						    s2 = pnt[ ( s_offset2 << 1 ) + 1 ]; s2 *= intr;
						    sr += s2; sr >>= INTERP_PREC;
						} else
						    { s = pnt[ s_offset << 1 ]; sr = pnt[ ( s_offset << 1 ) + 1 ]; } //get stereo-data without interpolation                        			    
					    }
					    else
					    { //Mono sample:
						if( data->ctl_smp_int ) 
						{
						    s = pnt[ s_offset ]; s *= intr2;
						    s2 = pnt[ s_offset2 ]; s2 *= intr;
						    s += s2; s >>= INTERP_PREC; sr = s;
						} else
						    s = sr = pnt[ s_offset ];
					    }
					} 
					else if( smp_type == 0 )
					{ //8 bit data:
					    cpnt = (signed char*) smp->data;
					    if( smp->type & 64 )
					    { //Stereo sample:
						if( data->ctl_smp_int ) 
						{
						    s = cpnt[ s_offset << 1 ]; s <<= 8; s *= intr2; //get data for left channel
						    s2 = cpnt[ s_offset2 << 1 ]; s2 <<= 8; s2 *= intr;
						    s += s2; s >>= INTERP_PREC;
						    sr = cpnt[ ( s_offset << 1 ) + 1 ]; sr <<= 8; sr *= intr2; //get data for right channel
						    s2 = cpnt[ ( s_offset2 << 1 ) + 1 ]; s2 <<= 8; s2 *= intr;
						    sr += s2; sr >>= INTERP_PREC;
						} else
						    { s = cpnt[ s_offset << 1 ]; s <<= 8; sr = cpnt[ ( s_offset << 1 ) + 1 ]; sr <<= 8; }
					    }
					    else
					    { //Mono sample:
						if( data->ctl_smp_int )  
						{
						    s = cpnt[ s_offset ]; s <<= 8; s *= intr2;
						    s2 = cpnt[ s_offset2 ]; s2 <<= 8; s2 *= intr;
						    s += s2; s >>= INTERP_PREC; sr = s;
						} else
						    { s = cpnt[ s_offset ]; s <<= 8; sr = s; }
					    }
					}
					else if( smp_type == 2 )
					{ //32 bit (float) data:
					    fpnt = (float*) smp->data;
					    if( smp->type & 64 )
					    { //Stereo sample:
						if( data->ctl_smp_int ) 
						{
						    s_f = fpnt[ s_offset << 1 ]; s_f *= (float)intr2 / 32768.0F; //get data for left channel
						    s2_f = fpnt[ s_offset2 << 1 ]; s2_f *= (float)intr / 32768.0F;
						    s_f += s2_f;
						    sr_f = fpnt[ ( s_offset << 1 ) + 1 ]; sr_f *= (float)intr2 / 32768.0F; //get data for right channel
						    s2_f = fpnt[ ( s_offset2 << 1 ) + 1 ]; s2_f *= (float)intr / 32768.0F;
						    sr_f += s2_f;
						} else
						    { s_f = fpnt[ s_offset << 1 ]; sr_f = fpnt[ ( s_offset << 1 ) + 1 ]; }
					    }
					    else
					    { //Mono sample:
						if( data->ctl_smp_int )  
						{
						    s_f = fpnt[ s_offset ]; s_f *= (float)intr2 / 32768.0F;
						    s2_f = fpnt[ s_offset2 ]; s2_f *= (float)intr / 32768.0F;
						    s_f += s2_f; sr_f = s_f;
						} else
						    { s_f = fpnt[ s_offset ]; sr_f = s_f; }
					    }
					}
				    }
				    // end of get sample data ####################################				    

				    if( chan->back ) 
					SIGNED_SUB64( chan->ptr_h, chan->ptr_l, chan->delta_h, chan->delta_l )
				    else
					SIGNED_ADD64( chan->ptr_h, chan->ptr_l, chan->delta_h, chan->delta_l )

				    //envelope volume interpolation:
				    if( data->ctl_vol_int && chan->vol_step ) 
				    {
					chan->l_cur += chan->l_delta;
					chan->r_cur += chan->r_delta;
					chan->vol_step --;
				    }
				    //=====================

				    if( smp_type < 2 )
				    {
					left = s * ( chan->l_cur >> 7 ); //left = 32768max * 32768max
					right = sr * ( chan->r_cur >> 7 );
					left >>= 15; //left = -32767..32768
					right >>= 15;
					STYPE res_l, res_r;
					INT16_TO_STYPE( res_l, left );
					INT16_TO_STYPE( res_r, right );
					out_l[ i ] += res_l;
					out_r[ i ] += res_r;
				    }
				    else
				    {
					float res_l, res_r;
					res_l = s_f * ( (float)( chan->l_cur >> 7 ) / 32768.0F );
					res_r = sr_f * ( (float)( chan->r_cur >> 7 ) / 32768.0F );
#ifdef STYPE_FLOATINGPOINT					
					out_l[ i ] += res_l;
					out_r[ i ] += res_r;
#else
					out_l[ i ] += (STYPE)( res_l * STYPE_ONE );
					out_r[ i ] += (STYPE)( res_r * STYPE_ONE );
#endif
				    }
				}
			    }
			    rendered = 1;
			    retval = 1;
			} //if( chan->playing...
		    } //for( data->ctl_channels...

		    ptr += buf_size;
		    data->tick_counter += buf_size;
		    if( data->tick_counter >= tick_size ) 
		    {
			//Handle one tick:
			for( int c = 0; c < data->ctl_channels; c++ ) //polyphony
			{
			    gen_channel *chan = &data->channels[ c ];
			    if( chan->playing )
			    {
				if( ins->samples[ chan->smp_num ] )
				    envelope( chan, data, synth_id, net );
			    }
			}
			data->tick_counter = 0;
		    }
		    if( ptr >= sample_frames ) break;
		}
	    }

render_finished:

	    sundog_mutex_unlock( &data->render_sound_mutex );

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

		c = data->search_ptr;
		gen_channel *ch = &data->channels[ c ];

		int finetune = ( MAX_PERIOD_PTR - pnet->note * 256 ) - pnet->period_ptr;

		instrument *ins = (instrument*)psynth_get_chunk( synth_id, CHUNK_INS, pnet );
		if( ins == 0 ) break;
		int note_num = pnet->note;
		if( note_num < 0 ) note_num = 0;
		int smp_note_num = note_num;
		if( smp_note_num >= 96 ) smp_note_num = 95;
		int smp_num = ins->sample_number[ smp_note_num ];
		sample *smp;
		if( ins->samples_num && smp_num < ins->samples_num )
		{
		    smp = (sample*)psynth_get_chunk( synth_id, CHUNK_SMP( smp_num ), pnet );
		    ch->smp_num = smp_num;
		    if( smp == 0 ) break;
		}
		else
		{
		    break;
		}

		ch->playing = 1;
		ch->vel = pnet->velocity;
		ch->ptr_h = 0;
		ch->ptr_l = 0;
		ch->id = pnet->channel_id;
		ch->sustain = 1;

		//Retring. envelope:
		ch->v_pos = 0;
		ch->p_pos = 0;
		ch->sustain = 1;
		ch->fadeout = 65536;
		ch->vib_pos = 0;
		ch->cur_frame = 0;
		ch->env_start = 1;
		ch->vol_step = 0;

		//Retrig. sample:
		ch->back = 0;

		//Set note:
		note_num += smp->relative_note;
		note_num += ins->relative_note;
		//if( note_num > 119 ) note_num = 119;
		if( note_num < 0 ) note_num = 0;
		long period = 7680 - (note_num*64) - (smp->finetune>>1) - (ins->finetune>>1) - (finetune>>2);
		int freq;
		GET_FREQ( freq, period );
		GET_DELTA( freq, ch->delta_h, ch->delta_l );
		ch->cur_period_ptr = period * 4;

		ch->period_delta = pnet->period_ptr - period * 4;

		ch->local_pan = 128;

		data->tick_counter = 0xFFFFFFF; //pnet->tick_size >> 8;

		retval = c;
	    }
	    break;

    	case COMMAND_SET_FREQ:
	    if( pnet->synth_channel >= 0 && pnet->synth_channel < MAX_CHANNELS )
	    {
		int c = pnet->synth_channel;
		if( data->channels[ c ].id == pnet->channel_id &&
		    data->channels[ c ].playing )
		{
		    int period_ptr = pnet->period_ptr - data->channels[ c ].period_delta;
		    int delta_h, delta_l;
		    int freq;
		    GET_FREQ( freq, period_ptr / 4 );
		    GET_DELTA( freq, delta_h, delta_l );

		    data->channels[ c ].delta_h = delta_h;
		    data->channels[ c ].delta_l = delta_l;
		    data->channels[ c ].cur_period_ptr = period_ptr;
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

    	case COMMAND_SET_SAMPLE_OFFSET:
	    if( pnet->synth_channel >= 0 && pnet->synth_channel < MAX_CHANNELS )
	    {
		int c = pnet->synth_channel;
		if( data->channels[ c ].id == pnet->channel_id )
		{
		    instrument *ins = (instrument*)psynth_get_chunk( synth_id, CHUNK_INS, pnet );
		    if( ins == 0 ) break;
		    sample *smp;
		    if( ins->samples_num && data->channels[ c ].smp_num < ins->samples_num )
		    {
			smp = (sample*)psynth_get_chunk( synth_id, CHUNK_SMP( data->channels[ c ].smp_num ), pnet );
			if( smp == 0 ) break;
		    }
		    else
	    	    {
			break;
		    }
		    ulong new_offset = pnet->sample_offset;
		    if( new_offset >= smp->length )
			new_offset = smp->length - 1;
		    data->channels[ c ].ptr_h = new_offset;
		    data->channels[ c ].ptr_l = new_offset;
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
			//Set "PAN" local controller of this channel:
			data->channels[ c ].local_pan = pnet->ctl_val >> 7;
		    }
		}
		retval = 1;
	    }
	    break;

	case COMMAND_CLOSE:
#ifdef SUNVOX_GUI
	    remove_window( pnet->items[ synth_id ].visual, g_wm );
#endif

	    sundog_mutex_destroy( &data->render_sound_mutex );

	    retval = 1;
	    break;
    }

    return retval;
}
