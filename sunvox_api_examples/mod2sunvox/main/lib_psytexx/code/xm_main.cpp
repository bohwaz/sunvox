/*
    PsyTexx: xm_main.cpp. Functions for working with XM files
    Copyright (C) 2002 - 2007  Zolotov Alexandr

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
//*** Contact info: Zolotov Alexandr (NightRadio project)
//***               Ekaterinburg. Russia.
//***               Email: nightradio@gmail.com
//***               WWW: warmplace.ru

#include "../xm.h"
#include "core/core.h"
#include "core/debug.h"
#include "memory/memory.h"
#include "filesystem/v3nus_fs.h"

void xm_init( xm_struct *xm )
{
    mem_off();			//Storage memory protection OFF (for PalmOS devices)
    tables_init();		//XM tables init
    clear_struct( xm );	//Clear main sound structure
    new_channels( MAX_REAL_CHANNELS, xm );  //Create channels
    new_song( xm );		//Create new song
    create_silent_song( xm );	//Create silent song (1 empty pattern)
    mem_on();			//Storage memory protection ON (for PalmOS devices)

    int samples = 0;
#ifdef PALMOS
    samples = 2048;
#else
    samples = 32768;
#endif
    if( samples )
	xm->buffer = (long*)mem_new( HEAP_DYNAMIC, samples * 2 * sizeof( long ), "32buffer", 0 );
}

void xm_close( xm_struct *xm )
{
    mem_off(); //Storage memory protection OFF (for PalmOS devices)
    clear_song( xm );
    clear_channels( xm );
    close_song( xm );
    mem_on(); //Storage memory protection ON (for PalmOS devices)

    if( xm->buffer ) 
    {
	mem_free( xm->buffer );
	xm->buffer = 0;
    }
}

#ifdef PALMOS
    #include "PalmOS.h"
    extern SndStreamRef main_stream;
#endif

void xm_set_volume( int volume, xm_struct *xm )
{
    xm->global_volume = volume;
    /*
    #ifdef PALMOS
    #ifdef SLOWMODE
	volume <<= 4;
	if( volume > 1023 ) volume = 1023;
	SndStreamSetVolume( main_stream, volume );
    #endif
    #endif
    */
}

ulong read_long( V3_FILE f )
{
    ulong res=0, c;
    c = v3_getc(f); res=c;
    c = v3_getc(f); c<<=8; res+=c;
    c = v3_getc(f); c<<=16; res+=c;
    c = v3_getc(f); c<<=24; res+=c;
    return res;
}

uint16 read_int( V3_FILE f )
{
    uint16 res=0,c;
    c = v3_getc(f); res=c;
    c = v3_getc(f); c<<=8; res+=c;
    return res;
}

uint16 read_int68( V3_FILE f )
{
    uint16 res=0,c;
    c = v3_getc(f); res=c<<8;
    c = v3_getc(f); res+=c;
    return res;
}

void write_long( V3_FILE f, long res )
{
    v3_write( &res, 4, 1, f );
}

void write_int( V3_FILE f, uint16 res )
{
    v3_write( &res, 2, 1, f );
}

void xm_pat_rewind(xm_struct *xm)
{
    xm->patternpos = 0;
    xm->jump_flag = 0;
    xm->loop_start = 0;
    xm->loop_count = 0;
}

void xm_rewind(xm_struct *xm)
{
    xm->tablepos = 0;
    xm_pat_rewind( xm );
}

void clear_struct(xm_struct *xm)
{
    long a;
    long freq;
    long bpm;
    long speed;
    long onetick;
    long patternticks;
    
    xm->buffer = 0;
    xm->compression_volume = 32768;

    xm->song = 0;
    for( a = 0; a < MAX_REAL_CHANNELS; a++ ) { xm->channels[ a ] = 0; xm->channel_busy[ a ] = 0; }

    xm->g_linear_tab = g_linear_tab;
    xm->stereo_tab = stereo_tab;
    xm->g_vibrato_tab = g_vibrato_tab;
    
    freq = 44100;
    bpm = 125;
    speed = 6;
    onetick = ((freq*25) << 8) / (bpm*10);
    patternticks = onetick + 1;

    xm->status = XM_STATUS_STOP;
    xm->song_finished = 0;
    
    xm->global_volume = 32;
    xm->freq = freq;
    xm->bpm = bpm;
    xm->speed = speed;
    xm->sp = xm->speed;
    xm->onetick = onetick;
    xm->tick_number = 0;
    xm->patternticks = patternticks;
    xm->subtick_count = 0;
    
    xm->cur_channel = 0;
    
    xm_rewind( xm );
        
    xm->sample_int = 1;
    xm->freq_int = 1;

    xm->ch_read_ptr = 0;
    xm->ch_write_ptr = 0;
}

void create_envelope(uint16 *src, uint16 points, uint16 *dest)
{
    long b;
    long cur_pnt; //current point;
    long val;
    long delta;
    long f1,v1,f2,v2,max = 0;
    
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
	    
	    val = v1 << 8;
	    if( f2 - f1 == 0 ) delta = 0; else delta = ((v2 - v1) << 8) / (f2 - f1);
	    for( b = f1; b <= f2; b++ )
	    {
		dest[b] = (uint16) val >> 4;
		dest[b] = dest[b];
    		val += delta;
	    }
	}
    }
}

void load_module( char *filename, xm_struct *xm )
{
    mem_off();
    char temp_str[4];
    V3_FILE f = v3_open( filename, "rb" );
    if( f )
    {
	v3_read( temp_str, 4, 1, f );
	if( temp_str[0] == 'E' && temp_str[1] == 'x' && temp_str[2] == 't' && temp_str[3] == 'e' )
	{ //Load XM:
	    xm_load( f, xm );
	}
	else
	{ //Load MOD:
	    mod_load( f, xm );
	}
	v3_close( f );
    }
    mem_on();
}

int mod_load( V3_FILE f, xm_struct *xm )
{
    module *song = xm->song; //main song
    instrument *ins;
    sample *smp;

    dprint( "Loading Amiga MOD\n" );

    //Prepare song:
    clear_song( xm );     //Clear old song
    xm_rewind( xm );      //Rewind to start
    clean_channels( xm ); //Clean all channels
    song->instruments_num = 31;
    song->tempo = 6;

    //Is it an old MOD (15 instruments) or not?
    int oldMOD = 1;
    v3_rewind( f );
    v3_seek( f, 1080, 1 );
    char tt[4];
    v3_read( tt, 4, 1, f );
    if( tt[0] == 'M' && tt[2] == 'K' ) oldMOD = 0;
    if( tt[0] == 'F' && tt[1] == 'L' && tt[2] == 'T' ) oldMOD = 0;
    if( tt[1] == 'C' && tt[2] == 'H' && tt[3] == 'N' ) oldMOD = 0;
    if( tt[0] == 'C' && tt[1] == 'D' && tt[2] == '8' && tt[3] == '1' ) oldMOD = 0;
    if( tt[0] == 'O' && tt[1] == 'K' && tt[2] == 'T' && tt[3] == 'A' ) oldMOD = 0;
    if( tt[2] == 'C' && tt[3] == 'N' ) oldMOD = 0;
    if( oldMOD ) dprint( "MOD: It's old MOD with 15 samples\n" );
    int inst_num = 31;
    if( oldMOD ) inst_num = 15;
    v3_rewind( f );

    //Load name:
    dprint( "MOD: Loading name\n" );
    char *name = (char*)song->name;
    v3_read( name, 20, 1, f );
    
    //Load instruments:
    dprint( "MOD: Loading instruments\n" );
    int i;
    char sample_name[22];
    long sample_size = 0;
    char sample_type = 0;
    char sample_finetune = 0;
    uchar sample_volume = 0;
    long sample_repoff = 0;
    long sample_replen = 0;
    for( i = 0; i < inst_num; i++ )
    {
	dprint( "MOD: instrument %d\n", i );
	v3_read( sample_name, 22, 1, f );
	sample_size = read_int68( f ); sample_size *= 2;
	sample_finetune = v3_getc( f );
	sample_volume = v3_getc( f );
	sample_repoff = read_int68( f ); sample_repoff *= 2;
	sample_replen = read_int68( f ); sample_replen *= 2;
	if( sample_replen <= 2 ) sample_replen = 0;
	if( sample_replen ) sample_type = 1; //forward loop
	    else sample_type = 0;
	dprint( "MOD: Add new instrument\n" );
	new_instrument( i, sample_name, 1, xm );
	ins = song->instruments[ i ];
	ins->finetune = 0;
	ins->panning = 80;
	ins->volume = 0x40;
	ins->relative_note = 0;
	if( sample_size )
	{
	    dprint( "MOD: New sample\n" );
	    new_sample( 0, i, "", sample_size, sample_type, xm );
	    smp = ins->samples[ 0 ];
	    //save parameters:
	    smp = ins->samples[ 0 ];
	    smp->reppnt = sample_repoff;
	    smp->replen = sample_replen;
	    smp->volume = sample_volume;
	    //finetune:
	    uchar ftune = sample_finetune; ftune <<= 4;
	    smp->finetune = (signed char)ftune;
	}
	else
	{
	    ins->samples_num = 0;
	}
    }
    
    //Some info about song:
    uint16 song_len;
    uint16 song_restart;
    song_len = v3_getc( f );
    song_restart = v3_getc( f );
    v3_read( song->patterntable, 128, 1, f );
    song->length = song_len;
    char modtype[5]; modtype[4] = 0;
    uint16 channels = 4;
    if( !oldMOD )
    {
	v3_read( modtype, 4, 1, f );
	if( modtype[0] == '6' ) channels = 6;
	if( modtype[0] == '8' || modtype[0] == 'O' ) channels = 8;
    }
    song->channels = channels;
    song->bpm = 125;
    
    //Load patterns:
    dprint( "MOD: Loading patterns\n" );
    char *pp = (char*)mem_new( HEAP_STORAGE, 1900, "MOD periods", 0 );   //standart period table
    char *pp2 = (char*)mem_new( HEAP_STORAGE, 1900, "MOD periods2", 0 ); //improved table
    mem_set( pp, 1900, 0 );
    pp[1712]=0;pp[1616]=1;pp[1524]=2;pp[1440]=3;pp[1356]=4;pp[1280]=5;
    pp[1208]=6;pp[1140]=7;pp[1076]=8;pp[1016]=9;pp[960]=10;pp[906]=11;
    pp[856]=12;pp[808]=13;pp[762]=14;pp[720]=15;pp[678]=16;pp[640]=17;
    pp[604]=18;pp[570]=19;pp[538]=20;pp[508]=21;pp[480]=22;pp[453]=23;
    pp[428]=24;pp[404]=25;pp[381]=26;pp[360]=27;pp[339]=28;pp[320]=29;
    pp[302]=30;pp[285]=31;pp[269]=32;pp[254]=33;pp[240]=34;pp[226]=35;
    pp[214]=36;pp[202]=37;pp[190]=38;pp[180]=39;pp[170]=40;pp[160]=41;
    pp[151]=42;pp[143]=43;pp[135]=44;pp[127]=45;pp[120]=46;pp[113]=47;
    pp[107]=48;pp[101]=49;pp[95]=50;pp[90]=51;pp[85]=52;pp[80]=53;
    pp[75]=54;pp[71]=55;pp[67]=56;pp[63]=57;pp[60]=58;pp[56]=59;
    //Improve standart period table:
    int pptr;
    for( pptr = 1712; pptr > 0; pptr-- ) 
    {
	if( pp[ pptr ] ) 
	{
	    pp2[ pptr ] = pp[ pptr ];
	    if( pp[ pptr+1 ] == 0 ) pp2[ pptr+1 ] = pp[ pptr ];
	    if( pp[ pptr+2 ] == 0 ) pp2[ pptr+2 ] = pp[ pptr ];
	    if( pp[ pptr-1 ] == 0 ) pp2[ pptr-1 ] = pp[ pptr ];
	    if( pp[ pptr-2 ] == 0 ) pp2[ pptr-2 ] = pp[ pptr ];
	    if( pptr >= 80 )
	    {
		if( pp[ pptr+3 ] == 0 ) pp2[ pptr+3 ] = pp[ pptr ];
		if( pp[ pptr-3 ] == 0 ) pp2[ pptr-3 ] = pp[ pptr ];
	    }
	    if( pptr >= 113 )
	    {
		if( pp[ pptr+4 ] == 0 ) pp2[ pptr+4 ] = pp[ pptr ];
		if( pp[ pptr+5 ] == 0 ) pp2[ pptr+5 ] = pp[ pptr ];
		if( pp[ pptr+6 ] == 0 ) pp2[ pptr+6 ] = pp[ pptr ];
		if( pp[ pptr+7 ] == 0 ) pp2[ pptr+7 ] = pp[ pptr ];
		if( pp[ pptr+8 ] == 0 ) pp2[ pptr+8 ] = pp[ pptr ];
		if( pp[ pptr-4 ] == 0 ) pp2[ pptr-4 ] = pp[ pptr ];
		if( pp[ pptr-5 ] == 0 ) pp2[ pptr-5 ] = pp[ pptr ];
		if( pp[ pptr-6 ] == 0 ) pp2[ pptr-6 ] = pp[ pptr ];
		if( pp[ pptr-7 ] == 0 ) pp2[ pptr-7 ] = pp[ pptr ];
		if( pp[ pptr-8 ] == 0 ) pp2[ pptr-8 ] = pp[ pptr ];
	    }
	    if( pptr >= 226 )
	    {
		if( pp[ pptr+9 ] == 0 ) pp2[ pptr+9 ] = pp[ pptr ];
		if( pp[ pptr+10 ] == 0 ) pp2[ pptr+10 ] = pp[ pptr ];
		if( pp[ pptr+11 ] == 0 ) pp2[ pptr+11 ] = pp[ pptr ];
		if( pp[ pptr+12 ] == 0 ) pp2[ pptr+12 ] = pp[ pptr ];
		if( pp[ pptr+13 ] == 0 ) pp2[ pptr+13 ] = pp[ pptr ];
		if( pp[ pptr+14 ] == 0 ) pp2[ pptr+14 ] = pp[ pptr ];
		if( pp[ pptr+15 ] == 0 ) pp2[ pptr+15 ] = pp[ pptr ];
		if( pp[ pptr-9 ] == 0 ) pp2[ pptr-9 ] = pp[ pptr ];
		if( pp[ pptr-10 ] == 0 ) pp2[ pptr-10 ] = pp[ pptr ];
		if( pp[ pptr-11 ] == 0 ) pp2[ pptr-11 ] = pp[ pptr ];
		if( pp[ pptr-12 ] == 0 ) pp2[ pptr-12 ] = pp[ pptr ];
		if( pp[ pptr-13 ] == 0 ) pp2[ pptr-13 ] = pp[ pptr ];
		if( pp[ pptr-14 ] == 0 ) pp2[ pptr-14 ] = pp[ pptr ];
		if( pp[ pptr-15 ] == 0 ) pp2[ pptr-15 ] = pp[ pptr ];
	    }
	}
    }
    //Copy improved to standart:
    mem_copy( pp, pp2, 1900 );
    mem_free( pp2 );
    //Get max number of pattern (maxp) :
    int p, maxp = 0;
    for( p = 0; p < 128; p ++ )
    {
	if( song->patterntable[ p ] > maxp ) maxp = song->patterntable[ p ];
    }
    //Load maxp+1 patterns:
    int l;
    int instr[ 32 ]; for( l = 0; l < 32; l++ ) instr[l] = 0; //current instrument number for each channel
    song->patterns_num = maxp + 1;
    for( p = 0; p <= maxp; p++ )
    {
	new_pattern( p, 64, channels, xm );
	pattern *pat = song->patterns[ p ];
	xmnote *pat_data = pat->pattern_data;
	int c, pat_ptr = 0;
	//load 64 lines of one pattern:
	for( l = 0; l < 64; l++ )
	{
	    for( c = 0; c < channels; c++ )
	    {
		//Get one MOD note:
		uchar sampperiod = v3_getc( f );
		uchar period1 = v3_getc( f );
		uchar sampeffect = v3_getc( f );
		uchar effect1 = v3_getc( f );
		//Explode it to sample, period and effect:
		uint16 sample, period, effect;
		sample = ( sampperiod & 0xF0 ) | ( sampeffect >> 4 );
		period = ( (sampperiod & 0xF) << 8 ) | period1;
		effect = ( (sampeffect & 0xF) << 8 ) | effect1;
		if( sample ) instr[ c ] = sample;
		if( period && sample == 0 ) sample = instr[ c ]; //If there is period only
		if( period )
		{
		    pat_data[ pat_ptr ].n = pp[ period ] + 24 + 1;
		}
		else pat_data[ pat_ptr ].n = 0;
		pat_data[ pat_ptr ].inst = (uchar)sample;
		pat_data[ pat_ptr ].vol = 0;
		pat_data[ pat_ptr ].fx = effect >> 8;
		pat_data[ pat_ptr ].par = effect & 0xFF;
		pat_ptr++;
	    }
	}
    }
    
    //Load samples data:
    dprint( "MOD: Loading sample data\n" );
    for( i = 0; i < inst_num; i ++ )
    {
	dprint( "MOD: sample %d\n", i );
	ins = song->instruments[ i ];
	smp = ins->samples[ 0 ];
	if( smp )
	{
	    char *smp_data = (char*) smp->data;
	    if( smp->length )
		v3_read( smp_data, smp->length, 1, f );
	}
    }
    
    mem_free( pp );
    
    //Set speed:
    long a1 = song->tempo;
    long a2 = song->bpm;
    xm->speed = a1; if( xm->speed == 0 ) xm->speed = 1;
    xm->bpm = a2;
    xm->onetick = ( ( xm->freq * 25 ) << 8 ) / ( xm->bpm * 10 );
    xm->onetick = xm->onetick;
    xm->patternticks = xm->onetick + 1;
    xm->patternticks = xm->patternticks;
    xm->sp = xm->speed;

    song->restart_position = 0;
    
    dprint( "****** MOD load OK ******\n" );

    return 1;
}

int xm_load( V3_FILE f, xm_struct *xm )
{
    module *song = xm->song; //main song
    pattern *pat;    //current pattern
    xmnote *data;      //pattern data
    instrument *ins; //current instument
    sample *smp;     //current sample
    signed short old_s, new_s; //for sample loading
    char c_old_s, c_new_s;
    ulong len;
    signed short *s_data; //sample data
    char *cs_data;        //char 8 bit sample data
    ulong a,b,a1,a2,a3,a4,a5,a6,a7,a8; //temp vars
    ulong num_of_samples;
    ulong ins_header_size;
    ulong smp_header_size;
    ulong sp; //sample pointer
    char name[32]; 
    

    dprint( "Loading XM\n" );
    
    //Prepare song:
    clear_song( xm );     //Clear old song
    xm_rewind( xm );      //Rewind to start
    clean_channels( xm ); //Clean all channels
    
    char *sptr = (char*)song;
    v3_read(sptr+4,332,1,f); //load header
    song->id_text[0] = 'E'; song->id_text[1] = 'x'; song->id_text[2] = 't'; song->id_text[3] = 'e';
    dprint( "XM: header loaded\n" );

    //load patterns:
    dprint( "XM: length = %d\n", song->length );
    dprint( "XM: patterns = %d\n", song->patterns_num );
    for( a = 0; a < song->patterns_num; a++ )
    {
	a1 = read_long(f); //pattern header length
	a1 = v3_getc(f);      //packing type
	a1 = read_int(f);  //number of rows
	a2 = read_int(f);  //packed pattern size
	if(a2==0) continue;//for NULL patterns
	new_pattern( (uint16)a, (uint16)a1, song->channels, xm );

	//read pattern data:
	dprint( "XM: reading pattern data...\n" );
	pat = song->patterns[ a ];
	data = pat->pattern_data;
	a4 = song->channels * a1; //get real pattern size
	for( b = 0; b < a4; b++ )
	{ //uncompress pattern:
	    a3 = v3_getc(f);
	    if( a3 & 0x80 ) {
		if( a3 & 1 ) data[b].n = v3_getc(f); else data[b].n = 0;
		if( a3 & 2 ) data[b].inst = v3_getc(f); else data[b].inst = 0;
		if( a3 & 4 ) data[b].vol = v3_getc(f); else data[b].vol = 0;
		if( a3 & 8 ) data[b].fx = v3_getc(f); else data[b].fx = 0;
		if( a3 & 16 ) data[b].par = v3_getc(f); else data[b].par = 0;
	    }else{
		data[b].n = (uchar) a3;
		data[b].inst = v3_getc(f);
		data[b].vol = v3_getc(f);
		data[b].fx = v3_getc(f);
		data[b].par = v3_getc(f);
	    }
	}
    }
    
    //load instruments:
    for( a = 0; a < song->instruments_num; a++ )
    {
	dprint( "\n" );
	dprint( "XM: loading instrument %d\n", a );
	ins_header_size = read_long(f);         //instrument's header size
	dprint( "XM: ins. header size = %d\n", ins_header_size );
	dprint( "XM: instr. name\n" );
	v3_read( name, 22, 1, f );                //instrument name
	dprint( "XM: instr. type %d\n", v3_getc(f) ); //instrument type (always 0)
	num_of_samples = read_int(f);           //number of samples
	dprint( "XM: number of samples = %d\n", num_of_samples );
	
	//create instrument with NULL samples:
	dprint( "XM: creating instrument with NULL samples\n" );
	new_instrument( (uint16)a, name, (uint16)num_of_samples, xm );
	ins = song->instruments[ a ];

	if( ins_header_size <= 29 ) continue;   //if instrument header is very small

	if( num_of_samples == 0 ) 
	{
	    v3_seek( f, ins_header_size-29, 1 );  //skip instrument header
	    continue;   //no samples
	}
	
	//load inst. parameters:
	dprint( "XM: loading instr. parameters\n" );
	if( ins_header_size >= (29+214) )
	{
	    //There is minimal amount of instrument data (29+214 bytes):
	    smp_header_size = read_long(f);   //sample header size
	    v3_read(ins->sample_number,96,1,f); //sample number for all notes
	    v3_read(ins->volume_points,48,1,f); //points for volume envelope
	    v3_read(ins->panning_points,48,1,f);//points for panning envelope
	    ins->volume_points_num = v3_getc(f); //number of volume points
	    ins->panning_points_num = v3_getc(f);//number of panning points
	    ins->vol_sustain = v3_getc(f);       //vol sustain point
	    ins->vol_loop_start = v3_getc(f);    //vol loop start point
	    ins->vol_loop_end = v3_getc(f);      //vol loop end point
	    ins->pan_sustain = v3_getc(f);       //pan sustain point
	    ins->pan_loop_start = v3_getc(f);    //pan loop start point
	    ins->pan_loop_end = v3_getc(f);      //pan loop end point
	    ins->volume_type = v3_getc(f);       //volume type
	    //if( a == 24 ) ins->volume_type = 0;
    	    ins->panning_type = v3_getc(f);      //panning type
	    ins->vibrato_type = v3_getc(f);      //vibrato type
	    ins->vibrato_sweep = v3_getc(f);     //vibrato sweep
	    ins->vibrato_depth = v3_getc(f);     //vibrato depth
	    ins->vibrato_rate = v3_getc(f);      //vibrato rate
	    a1 = read_int(f);                 //volume fadeout
    	    
	    //Init extended values:
	    ins->volume = 0x40;
	    ins->finetune = 0;
	    ins->panning = 0x80;
	    ins->relative_note = 0;
	    ins->flags = 0;
	    
	    int ext_bytes = 0;
	    if( ins_header_size >= 29+214+2 && ins_header_size <= 29+214+EXT_INST_BYTES ) //If instrument header size == extened PsyTexx inst:
	    {
		//Load extended parameters: [only for PsyTexx]
		ins->volume = v3_getc(f); 
		ins->finetune = (signed char)v3_getc(f);
		v3_read( &ins->panning, 1, ins_header_size - (29+214), f );
		ext_bytes = ins_header_size - (29+214);
	    }
	    else
	    {
		read_int( f ); //two empty bytes
	    }
	    ins->volume_fadeout = (uint16)a1;
	    dprint( "XM: smp. header size = %d\n", smp_header_size );

	    dprint( "XM: instrument envelope %d\n", a );
	    create_envelope( ins->volume_points, ins->volume_points_num, ins->volume_env );
	    dprint( "XM: panning envelope:\n" );
	    create_envelope( ins->panning_points, ins->panning_points_num, ins->panning_env );

	    //seek for the samples:
	    int already_loaded = ( 29 + 214 + ext_bytes );
	    int seek_bytes = ins_header_size - already_loaded;
	    if( seek_bytes > 0 )
	    {
		v3_seek( f, ins_header_size - already_loaded, 1 );
	    }
	}
	else
	{
	    v3_seek( f, ins_header_size - 29, 1 );
	}
	    
	//load sample parameters:
	for( b = 0; b < num_of_samples; b++ )
	{
	    a1 = read_long(f); //sample length
	    a2 = read_long(f); //sample loop start
	    a3 = read_long(f); //sample loop length
	    a4 = v3_getc(f);      //volume
	    a5 = (char)v3_getc(f);//finetune
	    a6 = v3_getc(f);      //type
	    a7 = v3_getc(f);      //panning
	    a8 = (char)v3_getc(f);//relative note number
	    v3_getc(f);           //reserved
	    v3_read(name,22,1,f);//sample name
		
	    new_sample( (uint16)b, (uint16)a, name, a1, a6, xm );
	    dprint( "XM: new sample is OK\n" );
	    smp = ins->samples[ b ];
		
	    //set sample info:
	    smp->reppnt = a2;
	    smp->replen = a3;
	    smp->volume = (uchar) a4;
	    smp->finetune = (signed char) a5;
	    smp->panning = (uchar) a7;
	    smp->relative_note = (signed char) a8;
	}
	
	//load sample data:
	for( b = 0; b < num_of_samples; b++ )
	{
	    smp = ins->samples[ b ];
	    
	    if(smp->length==0) continue;
	    dprint( "XM: sample len = %d\n", smp->length );
	    
	    if( smp->type & 16 )
	    { //16bit sample:
		dprint( "XM: 16bit sample\n" );
		len = smp->length;
		old_s = 0;
		s_data = (signed short*) smp->data;
		v3_read( s_data, len, 1, f );
		//convert sample to normal form:
		for( sp = 0; sp < len / 2; sp++ ) 
		{
		    new_s = s_data[sp] + old_s;
		    s_data[sp] = new_s;
		    old_s = new_s;
		}
		//convert sample info:
		bytes2frames( smp, xm );
	    }else{ //8bit sample:
		dprint( "XM: 8bit sample\n" );
		len = smp->length;
		c_old_s = 0;
		cs_data = (char*) smp->data;
		v3_read(cs_data,len,1,f);
		//convert sample to normal form:
		for( sp = 0; sp < len; sp++ )
		{
		    c_new_s = cs_data[sp] + c_old_s;
		    cs_data[sp] = c_new_s;
		    c_old_s = c_new_s;
		}
	    }
	    dprint( "XM: load smp data OK\n" );
	}
    }

    //set speed:
    a1 = song->tempo;
    a2 = song->bpm;
    xm->speed = a1; if(xm->speed == 0) xm->speed = 1;
    xm->bpm = a2;
    xm->onetick = ( ( xm->freq * 25 ) << 8 ) / ( xm->bpm * 10 );
    xm->onetick = xm->onetick;
    xm->patternticks = xm->onetick + 1;
    xm->patternticks = xm->patternticks;
    xm->sp = xm->speed;
    
    dprint( "****** XM load OK ******\n" );
    
    return 1;
}
