/*
    mod2sunvox.cpp.
    Copyright (C) 2002 - 2009 Alex Zolotov <nightradio@gmail.com>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sunvox_engine.h"
#include "lib_psytexx/xm.h"

#include "core/debug.h"

xm_struct g_xm; //PsyTexx data
sunvox_engine g_sv; //SunVox data

void show_usage( void )
{
    printf( "MOD/XM -> SunVox converter. v1.1\nCode by Alex Zolotov (nightradio@gmail.com). (C) 2009. www.warmplace.ru\n" );
    printf( "Usage: mod2sunvox <input file(s)>\n" );
}

extern int psynth_sampler( PSYTEXX_SYNTH_PARAMETERS );
extern void load_instrument_or_sample( char *filename, int flags, int synth_id, void *net, int sample_num );

int sunvox_pats[ 256 ][ 16 ];

int main( int argc, char *argv[] )
{
    hide_debug();
    
    if( argc < 2 )
    {
	show_usage();
	return 0;
    }
    else
    {
	for( int i = 1; i < argc; i++ )
	{
	    char out_filename[ 256 + 7 ];
	    out_filename[ 0 ] = 0;
	    strcat( out_filename, argv[ i ] );
	    for( int a = strlen( out_filename ) - 1; a >= 0; a-- )
	    {
		if( out_filename[ a ] == '.' )
		{
		    a++;
		    out_filename[ a ] = 's'; a++;
		    out_filename[ a ] = 'u'; a++;
		    out_filename[ a ] = 'n'; a++;
		    out_filename[ a ] = 'v'; a++;
		    out_filename[ a ] = 'o'; a++;
		    out_filename[ a ] = 'x'; a++;
		    out_filename[ a ] = 0;
		    break;
		}
	    }
	    printf( "FILE %d: %s -> %s\n", i, argv[ i ], out_filename );
	    
	    //Open PsyTex engine and load song:
	    xm_init( &g_xm );
	    load_module( argv[ i ], &g_xm );
	    char song_name[ 21 ];
	    song_name[ 20 ] = 0;
	    memcpy( song_name, g_xm.song->name, 20 );
	    printf( "  Song name: %s\n", song_name );
	    printf( "  Number of patterns: %d\n", g_xm.song->patterns_num );
	    
	    //Open SunVox engine:
	    sunvox_engine_init( 0, &g_sv );
	    
	    //Save speed:
	    g_sv.bpm = g_xm.song->bpm;
	    g_sv.speed = g_xm.song->tempo;
	    
	    //Save song name:
	    g_sv.song_name = (char*)MEM_NEW( HEAP_DYNAMIC, strlen( song_name ) + 1 );
	    mem_copy( g_sv.song_name, song_name, strlen( song_name ) + 1 );
	    
	    //Save synths:
	    int sunvox_instr[ 128 ];
	    for( int i2 = 0; i2 < 128; i2++ ) sunvox_instr[ i2 ] = -1;
	    int synths = 0;
	    for( int ins_num = 0; ins_num < g_xm.song->instruments_num; ins_num++ )
	    {
		instrument *ins = g_xm.song->instruments[ ins_num ];
		if( ins && ins->samples_num > 0 )
		{
		    //Are there some samples with data?
		    int samples = 0;
		    for( int ss = 0; ss < ins->samples_num; ss++ )
		    {
			if( ins->samples[ ss ]->length > 0 && ins->samples[ ss ]->data )
			{
			    samples = 1;
			    break;
			}
		    }
		    if( samples )
		    {
			char instr_name[ 23 ];
			instr_name[ 22 ] = 0;
			memcpy( instr_name, ins->name, 22 );
			int name_empty = 1;
			for( int n = 0; n < 22; n++ )
			{
			    if( instr_name[ n ] != ' ' && instr_name[ n ] != 0 )
			    {
				name_empty = 0;
				break;
			    }
			    if( instr_name[ n ] == 0 ) break;
			}
			if( name_empty )
			{
			    instr_name[ 0 ] = 0;
			    sprintf( instr_name, "%02x", synths + 1 );
			}
			
			save_instrument( ins_num, "instr.xi", &g_xm );
			int sx = ( synths % 5 ) * 128;
			int sy = ( synths / 5 ) * 140;
			sunvox_instr[ ins_num ] = psynth_add_synth( psynth_sampler, instr_name, 0, sx, sy, 0, g_sv.net );
			psynth_make_link( 0, sunvox_instr[ ins_num ], g_sv.net );
			load_instrument_or_sample( "instr.xi", 0, sunvox_instr[ ins_num ], g_sv.net, 0 );
			synths++;
			remove( "instr.xi" );
		    }
		}
	    }
	    
	    //Save patterns:
	    int x = 0;
	    int p, p2;
	    for( p = 0; p < 256; p++ ) 
		for( p2 = 0; p2 < 16; p2++ )
		    sunvox_pats[ p ][ p2 ] = -1; 
	    for( p = 0; p < g_xm.song->patterns_num; p++ )
	    {
		int pat_id = g_xm.song->patterntable[ p ];
		pattern *pat = g_xm.song->patterns[ pat_id ];
		if( pat )
		{
		    if( sunvox_pats[ pat_id ][ 0 ] == -1 )
		    {
			//Pattern not created yet:
			for( int j = 0; j < pat->channels; j += MAX_PATTERN_CHANNELS )
			{
			    int jj = j / MAX_PATTERN_CHANNELS;
			    sunvox_pats[ pat_id ][ jj ] = sunvox_new_pattern( pat->rows, MAX_PATTERN_CHANNELS, x, jj * 32, &g_sv );
			}
			//Save notes:
			int synth_on_channel[ 128 ];
			int note_on_channel[ 128 ];
			for( int ss = 0; ss < 128; ss++ ) 
			{
			    synth_on_channel[ ss ] = -1;
			    note_on_channel[ ss ] = -1;
			}
			for( int ny = 0; ny < pat->rows; ny++ )
			{
			    for( int nx = 0; nx < pat->channels; nx++ )
			    {
				int sub_pattern_num = nx / MAX_PATTERN_CHANNELS;
				sunvox_pattern *spat = g_sv.pats[ sunvox_pats[ pat_id ][ sub_pattern_num ] ];
				int src_ptr = ny * pat->real_channels + nx;
				int dest_ptr = ny * spat->data_xsize + nx - ( sub_pattern_num * MAX_PATTERN_CHANNELS );
				xmnote *note = &pat->pattern_data[ src_ptr ];
				sunvox_note *snote = &spat->data[ dest_ptr ];
				//Note:
				if( note->n >= 1 && note->n <= 96 )
				{
				    snote->note = note->n;
				    note_on_channel[ nx ] = snote->note;
				}
				if( note->n == 97 ) //note off
				{
				    snote->note = 128;
				    note_on_channel[ nx ] = -1;
				}
				//Synth:
				if( note->inst > 0 )
				{
				    if( sunvox_instr[ note->inst - 1 ] == -1 )
					snote->synth = 255; //no synth
				    else
				    {
					synth_on_channel[ nx ] = snote->synth = sunvox_instr[ note->inst - 1 ] + 1;
					if( snote->note == 0 && note_on_channel[ nx ] != -1 )
					{
					    snote->note = note_on_channel[ nx ];
					}
				    }
				}
				//Volume (velocity):
				if( note->vol >= 0x10 && note->vol <= 0x50 )
				    snote->vel = ( note->vol - 0x10 ) * 2 + 1;
				//Effect:
				switch( note->fx )
				{
				    case 0x00:
					if( note->par )
					{
					    snote->ctl = 0x0008;
					    snote->ctl_val = ( (uint16)note->par & 0x0F ) + ( (uint16)( note->par & 0xF0 ) << 4 );
					}
					break;
				    case 0x01:
					snote->ctl = 0x0001;
					snote->ctl_val = (uint16)note->par * 4;
					break;
				    case 0x02:
					snote->ctl = 0x0002;
					snote->ctl_val = (uint16)note->par * 4;
					break;
				    case 0x03:
					snote->ctl = 0x0003;
					snote->ctl_val = (uint16)note->par * 4;
					break;
				    case 0x08:
					if( synth_on_channel[ nx ] != -1 )
					{
					    if( snote->note < 1 || snote->note > 128 )
					    {
						snote->synth = 0; //local controller
					    }
					    snote->ctl = 0x0200;
					    snote->ctl_val = (uint16)( note->par / 2 ) << 8;
					}
					break;
				    case 0x09:
					snote->ctl = 0x0009;
					snote->ctl_val = (uint16)note->par * 2;
					break;
				    case 0x0A:
					snote->ctl = 0x000A;
					snote->ctl_val = 
					    ( (uint16)( ( ( note->par >> 4 ) & 0xF ) * 4 ) << 8 ) |
					    ( (uint16)( ( ( note->par >> 0 ) & 0xF ) * 4 ) << 0 );
					break;
				    case 0x0C:
					if( snote->vel == 0 )
					    snote->vel = note->par * 2 + 1;
					else
					    snote->vel = ( ( snote->vel - 1 ) * note->par ) / 64 + 1;
					break;
				    case 0x0F:
					snote->ctl = 0x000F;
					snote->ctl_val = (uint16)note->par;
					break;
				}
			    }
			}
		    }
		    else
		    {
			//Pattern created already. Make a clone:
			for( int j = 0; j < 16; j++ )
			{
			    if( sunvox_pats[ pat_id ][ j ] != -1 )
				sunvox_new_pattern_clone( sunvox_pats[ pat_id ][ j ], x, j * 32, &g_sv );
			}
		    }
		    x += pat->rows;
		}
	    }

	    //Save sunvox file:
	    sunvox_save_song( out_filename, &g_sv );
	    
	    //Close all engines:
	    sunvox_engine_close( &g_sv );
	    xm_close( &g_xm );
	}
	
	mem_free_all();
    }
    return 0;
}
