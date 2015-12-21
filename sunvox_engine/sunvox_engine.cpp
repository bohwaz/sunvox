/*
    sunvox_engine.cpp.
    This file is part of the SunVox.
    Copyright (C) 2002 - 2008 Alex Zolotov <nightradio@gmail.com>
*/

#include "filesystem/v3nus_fs.h"
#include "time/timemanager.h"
#include "sound/sound.h"
#include "core/debug.h"
#include "utils/utils.h"

#include "sunvox_engine.h"

#ifdef SUNVOX_GUI
#include "../sunvox/main/sunvox_windows.h"
#endif

//#define DEBUG_MESSAGES

sunvox_engine *g_sunvox_engine = 0;

extern int psynth_fm( PSYTEXX_SYNTH_PARAMETERS );
extern int psynth_generator( PSYTEXX_SYNTH_PARAMETERS );
extern int psynth_sampler( PSYTEXX_SYNTH_PARAMETERS );
extern int psynth_spectravoice( PSYTEXX_SYNTH_PARAMETERS );
extern int psynth_kicker( PSYTEXX_SYNTH_PARAMETERS );
extern int psynth_delay( PSYTEXX_SYNTH_PARAMETERS );
extern int psynth_distortion( PSYTEXX_SYNTH_PARAMETERS );
extern int psynth_echo( PSYTEXX_SYNTH_PARAMETERS );
extern int psynth_filter( PSYTEXX_SYNTH_PARAMETERS );
extern int psynth_vfilter( PSYTEXX_SYNTH_PARAMETERS );
extern int psynth_flanger( PSYTEXX_SYNTH_PARAMETERS );
extern int psynth_lfo( PSYTEXX_SYNTH_PARAMETERS );
extern int psynth_loop( PSYTEXX_SYNTH_PARAMETERS );
extern int psynth_reverb( PSYTEXX_SYNTH_PARAMETERS );
int ( *g_synths [] )( PSYTEXX_SYNTH_PARAMETERS ) = 
{
    &psynth_fm,
    &psynth_generator,
    &psynth_sampler,
    &psynth_spectravoice,
    &psynth_kicker,
    &psynth_delay,
    &psynth_distortion,
    &psynth_echo,
    &psynth_filter,
    &psynth_vfilter,
    &psynth_flanger,
    &psynth_lfo,
    &psynth_loop,
    &psynth_reverb
};
int g_synths_num = 0;

void sunvox_select_current_playing_patterns( int first_sorted_pat, sunvox_engine *s );

void sunvox_engine_init( int flags, sunvox_engine *s )
{
    mem_set( s, sizeof( sunvox_engine ), 0 );

    //Turn off single pattern play:
    s->single_pattern_play = -1;

    //Init psynth engine:
    s->net = (psynth_net*)MEM_NEW( HEAP_DYNAMIC, sizeof( psynth_net ) );
    int psynth_flags = 0;
    if( flags & SUNVOX_FLAG_CREATE_SYNTHS )
	psynth_flags |= PSYNTH_FLAG_CREATE_SYNTHS;
    psynth_init( psynth_flags, g_snd.freq, s->net );
    
    //Init synths:
    if( g_synths_num == 0 )
	g_synths_num = sizeof( g_synths ) / sizeof( int );

    //Set bpm and speed:
    s->bpm = 125;
    s->speed = 6;

    s->cur_playing_pats[ 0 ] = -1;
    s->user_rp = 0;
    s->user_wp = 0;
    for( int i = 0; i < MAX_PATTERN_CHANNELS; i++ ) s->user_pat_info.channel_status[ i ] = 255;
    for( int i = 0; i < MAX_PLAYING_PATS; i++ ) 
	clean_std_effects_for_playing_pattern( i, s );

    if( flags & SUNVOX_FLAG_CREATE_PATTERN )
    {
	sunvox_new_pattern( 32, 4, 0, 0, s );
    }
    sunvox_sort_patterns( s );
    g_sunvox_engine = s;

    s->initialized = 1;
}

void sunvox_close_copy_pats( sunvox_engine *s )
{
    if( s->copy_pats )
    {
	for( int p = 0; p < s->copy_pats_num; p++ )
	    if( s->copy_pats[ p ].data )
		mem_free( s->copy_pats[ p ].data );
	mem_free( s->copy_pats );
	mem_free( s->copy_pats_info );
	s->copy_pats = 0;
	s->copy_pats_info = 0;
    }
    s->copy_pats_num = 0;
}

void sunvox_engine_close( sunvox_engine *s )
{
    sound_stream_stop();

    g_sunvox_engine = 0;
    
    if( s->pats )
    {
	for( int p = 0; p < s->pats_num; p++ )
	    sunvox_remove_pattern( p, s );
	mem_free( s->pats );
	s->pats = 0;
    }

    if( s->pats_info ) 
    {
	mem_free( s->pats_info );
	s->pats_info = 0;
    }

    if( s->sorted_pats )
    {
	mem_free( s->sorted_pats );
	s->sorted_pats = 0;
    }

    sunvox_close_copy_pats( s );

    if( s->song_name )
    {
	mem_free( s->song_name );
	s->song_name = 0;
    }

    //Close psynth engine:
    if( s->net )
    {
	psynth_close( s->net );
	s->net = 0;
    }

    s->initialized = 0;

    sound_stream_play();
}

#define SWAP32(n) (	((((unsigned long) n) << 24 ) & 0xFF000000) | \
			((((unsigned long) n) << 8 ) & 0x00FF0000) | \
			((((unsigned long) n) >> 8 ) & 0x0000FF00) | \
			((((unsigned long) n) >> 24 ) & 0x000000FF) )

void save_block( ulong id, int size, void *ptr, V3_FILE f )
{
    id = SWAP32( id );
    v3_write( &id, 1, 4, f );
    v3_write( &size, 1, 4, f );
    if( size )
	v3_write( ptr, 1, size, f );
}

ulong g_block_id = 0;
ulong g_block_size = 0;
char *g_block_data = 0;

void load_block( V3_FILE f )
{
    if( v3_read( &g_block_id, 1, 4, f ) != 4 ) g_block_id = 0;
    if( g_block_id )
    {
	char id[ 5 ];
	id[ 4 ] = 0;
	mem_copy( id, &g_block_id, 4 );
	//dprint( "Loading block: %s\n", id );
    }
    g_block_id = SWAP32( g_block_id );
    if( v3_read( &g_block_size, 1, 4, f ) != 4 ) g_block_id = 0;
    if( g_block_id == 0 ) return;
    if( g_block_size )
    {
	g_block_data = (char*)MEM_NEW( HEAP_DYNAMIC, g_block_size );
	v3_read( g_block_data, 1, g_block_size, f );
    }
}

int get_integer( void )
{
    int res = 0;
    mem_copy( &res, g_block_data, 4 );
    return res;
}

void sunvox_load_song( const UTF8_CHAR *name, sunvox_engine *s )
{
    sunvox_stop( s );
    
    sound_stream_stop();

    sunvox_engine_close( s );
    sunvox_engine_init( 0, s );
    for( int sn = 1; sn < s->net->items_num; sn++ )
    {
	if( s->net->items[ sn ].flags & PSYNTH_FLAG_EXISTS )
	    psynth_remove_synth( sn, s->net );
    }

    V3_FILE f = v3_open( name, "rb" );
    if( f )
    {
	void *pat_data = 0;
	int pat_channels = 0;
	int pat_lines = 0;
	int pat_ysize = 0;
	void *pat_icon = 0;
	int pat_parent = -1;
	int pat_flags = 0;
	int pat_x = 0;
	int pat_y = 0;
	uchar pat_fg[ 3 ];
	uchar pat_bg[ 3 ];
	pat_fg[ 0 ] = 0;
	pat_fg[ 1 ] = 0;
	pat_fg[ 2 ] = 0;
	pat_bg[ 0 ] = 255;
	pat_bg[ 1 ] = 255;
	pat_bg[ 2 ] = 255;

	int s_flags = 0;
	UTF8_CHAR s_name[ 32 ];
	int (*s_synth)( PSYTEXX_SYNTH_PARAMETERS ) = 0;
	int s_x = 0;
	int s_y = 0;
	int s_finetune = 0;
	int s_relative_note = 0;
	int *s_links = 0;
	int s_ctls[ PSYNTH_MAX_CONTROLLERS ];
	int c_num = 0;
	int chunk_num = 0;
	int chunks_num = 0;
	char **chunks = 0;
	int *chunk_flags = 0;
	int first_synth = 1;

	while( 1 )
	{
	    g_block_data = 0;
	    load_block( f );
	    if( g_block_id == 0 ) break;
	    if( g_block_id == 'BPM ' ) { s->bpm = get_integer(); goto bfound; }
	    if( g_block_id == 'SPED' ) { s->speed = get_integer(); goto bfound; }
	    if( g_block_id == 'NAME' ) { s->song_name = g_block_data; g_block_data = 0; goto bfound; }
	    if( g_block_id == 'GVOL' ) { s->net->global_volume = get_integer(); goto bfound; }
	    if( g_block_id == 'PDTA' ) { pat_data = g_block_data; g_block_data = 0; goto bfound; }
	    if( g_block_id == 'PLIN' ) { pat_lines = get_integer(); goto bfound; }
	    if( g_block_id == 'PCHN' ) { pat_channels = get_integer(); goto bfound; }
	    if( g_block_id == 'PYSZ' ) { pat_ysize = get_integer(); goto bfound; }
	    if( g_block_id == 'PICO' ) { pat_icon = g_block_data; g_block_data = 0; goto bfound; }
	    if( g_block_id == 'PPAR' ) { pat_parent = get_integer(); goto bfound; }
	    if( g_block_id == 'PFFF' ) { pat_flags = get_integer(); goto bfound; }
	    if( g_block_id == 'PXXX' ) { pat_x = get_integer(); goto bfound; }
	    if( g_block_id == 'PYYY' ) { pat_y = get_integer(); goto bfound; }
	    if( g_block_id == 'PFGC' ) { mem_copy( pat_fg, g_block_data, 3 ); goto bfound; }
	    if( g_block_id == 'PBGC' ) { mem_copy( pat_bg, g_block_data, 3 ); goto bfound; }
	    if( g_block_id == 'PEND' )
	    {
		//End of pattern. Create it:
		int pat_num = sunvox_new_pattern( pat_lines, pat_channels, pat_x, pat_y, s );
		if( pat_parent >= 0 )
		{
		    //It's a clone:
		    sunvox_remove_pattern( pat_num, s );
		    s->pats[ pat_num ] = (sunvox_pattern*)1;
		    sunvox_pattern_info *pat_info = &s->pats_info[ pat_num ];
		    pat_info->flags = pat_flags;
		    pat_info->parent_num = pat_parent;
		    pat_info->x = pat_x;
		    pat_info->y = pat_y;
		}
		else
		{
		    //It's a normal pattern:
		    if( pat_icon == 0 )
		    {
			//Empty pattern:
			sunvox_remove_pattern( pat_num, s );
			s->pats[ pat_num ] = (sunvox_pattern*)1;
		    }
		    else
		    {
			//Normal pattern with notes and icon:
			sunvox_pattern *pat = s->pats[ pat_num ];
			if( pat->data )
			    mem_free( pat->data );
			pat->data = (sunvox_note*)pat_data;
			pat_data = 0;
			pat->data_xsize = pat_channels;
			pat->data_ysize = pat_lines;
			mem_copy( pat->icon, pat_icon, 32 );
			mem_copy( pat->fg, pat_fg, 3 );
			mem_copy( pat->bg, pat_bg, 3 );
			mem_free( pat_icon );
			pat->ysize = pat_ysize;
		    }
		}
		pat_icon = 0;
		pat_parent = -1;
		goto bfound;
	    }
	    if( g_block_id == 'SFFF' ) { s_flags = get_integer(); goto bfound; }
	    if( g_block_id == 'SNAM' ) { mem_copy( s_name, g_block_data, 32 ); goto bfound; }
	    if( g_block_id == 'STYP' )
	    {
		//Synth's type loaded. Try to find pointer to this synth:
		if( s_flags & PSYNTH_FLAG_EXISTS )
		{
		    for( int ss = 0; ss < g_synths_num; ss++ )
		    {
			UTF8_CHAR *s_type = (UTF8_CHAR*)g_synths[ ss ]( 0, 0, 0, 0, 0, COMMAND_GET_SYNTH_NAME, s->net );
			if( mem_strcmp( s_type, g_block_data ) == 0 ) 
			{
			    s_synth = g_synths[ ss ];
			    break;
			}
		    }
		}
		else s_synth = 0;
		goto bfound;
	    }
	    if( g_block_id == 'SFIN' ) { s_finetune = get_integer(); goto bfound; }
	    if( g_block_id == 'SREL' ) { s_relative_note = get_integer(); goto bfound; }
	    if( g_block_id == 'SXXX' ) { s_x = get_integer(); goto bfound; }
	    if( g_block_id == 'SYYY' ) { s_y = get_integer(); goto bfound; }
	    if( g_block_id == 'SLNK' ) { s_links = (int*)g_block_data; g_block_data = 0; goto bfound; }
	    if( g_block_id == 'CVAL' ) { s_ctls[ c_num++ ] = get_integer(); goto bfound; }
	    if( g_block_id == 'CHNK' ) 
	    {
		chunks_num = get_integer();
		chunks = (char**)MEM_NEW( HEAP_DYNAMIC, chunks_num * sizeof( char* ) );
		chunk_flags = (int*)MEM_NEW( HEAP_DYNAMIC, chunks_num * sizeof( int ) );
		mem_set( chunks, chunks_num * sizeof( char* ), 0 );
		mem_set( chunk_flags, chunks_num * sizeof( int ), 0 );
		goto bfound;
	    }
	    if( g_block_id == 'CHNM' ) { chunk_num = get_integer(); goto bfound; }
	    if( g_block_id == 'CHDT' )
	    {
		//Get chunk data:
		chunks[ chunk_num ] = (char*)g_block_data; 
		g_block_data = 0;
		goto bfound;
	    }
	    if( g_block_id == 'CHFF' )
	    {
		//Get chunk flags:
		chunk_flags[ chunk_num ] = get_integer();
		goto bfound;
	    }
	    if( g_block_id == 'SEND' )
	    {
		//End of synth:
		if( first_synth )
		{
		    //OUT is already created. Lets change it:
		    s->net->items[ 0 ].finetune = s_finetune;
		    s->net->items[ 0 ].relative_note = s_relative_note;
		    s->net->items[ 0 ].x = s_x;
		    s->net->items[ 0 ].y = s_y;
		    if( s->net->items[ 0 ].input_links ) mem_free( s->net->items[ 0 ].input_links );
		    s->net->items[ 0 ].input_links = s_links;
		    if( s_links )
		    {
			s->net->items[ 0 ].input_num = mem_get_size( s_links ) / sizeof( int );
		    }
		}
		else
		{
		    int s_num = psynth_add_synth( s_synth, s_name, s_flags, s_x, s_y, 0, s->net );
		    if( s_flags == 0 )
		    {
			//Empty synth:
			psynth_remove_synth( s_num, s->net );
			s->net->items[ s_num ].flags = PSYNTH_FLAG_EXISTS | PSYNTH_FLAG_LAST; //Busy state
		    }
		    else
		    {
			//Save standart properties:
			s->net->items[ s_num ].finetune = s_finetune;
			s->net->items[ s_num ].relative_note = s_relative_note;
			s_finetune = 0;
			s_relative_note = 0;

			s->net->items[ s_num ].input_links = s_links;
			if( s_links )
			{
			    s->net->items[ s_num ].input_num = mem_get_size( s_links ) / sizeof( int );
			}
			if( s_flags & PSYNTH_FLAG_EXISTS )
			{
			    int ctls = c_num;
			    if( ctls > s->net->items[ s_num ].ctls_num ) 
				ctls = s->net->items[ s_num ].ctls_num;
			    for( int cc = 0; cc < ctls; cc++ )
			    {
				*s->net->items[ s_num ].ctls[ cc ].ctl_val = s_ctls[ cc ];
			    }
			    //Save chunks:
			    if( chunks )
			    {
				if( s->net->items[ s_num ].chunks )
				    psynth_clear_chunks( s_num, s->net );
				s->net->items[ s_num ].chunks = chunks;
				s->net->items[ s_num ].chunk_flags = chunk_flags;
				chunks = 0;
				chunk_flags = 0;
			    }
			}
			else
			{
			    s->net->items[ s_num ].flags &= ~PSYNTH_FLAG_EXISTS;
			}
			psynth_synth_setup_finished( s_num, s->net );
		    }
		}
		first_synth = 0;
		s_links = 0;
		s_flags = 0;
		c_num = 0;
	    }
bfound:
	    if( g_block_data )
	    {
		mem_free( g_block_data );
		g_block_data = 0;
	    }
	}
	//Fix patterns:
	for( int i = 0; i < s->pats_num; i++ )
	{
	    if( s->pats_info[ i ].flags & SUNVOX_PATTERN_FLAG_CLONE )
		s->pats[ i ] = s->pats[ s->pats_info[ i ].parent_num ];
	    if( s->pats[ i ] == (sunvox_pattern*)1 )
		s->pats[ i ] = 0;
	}
	for( int i = 0; i < s->net->items_num; i++ )
	{
	    if( s->net->items[ i ].flags == ( PSYNTH_FLAG_EXISTS | PSYNTH_FLAG_LAST ) )
		s->net->items[ i ].flags = 0;
	}
	v3_close( f );
    }

#ifdef DEBUG_MESSAGES
    sunvox_print_patterns( s );
#endif

    sound_stream_play();
}

void sunvox_save_song( const UTF8_CHAR *name, sunvox_engine *s )
{
    //Remove empty patterns from the end of the song:
    int pats_num = s->pats_num;
    for( int i = s->pats_num - 1; i >= 0; i-- )
    {
	if( s->pats[ i ] == 0 ) 
	    pats_num--;
	else
	    break;
    }
    
    //Save:
    V3_FILE f = v3_open( name, "wb" );
    if( f )
    {
	int version = 0x0103;
	save_block( 'SVOX', 0, 0, f );
	save_block( 'VERS', 4, &version, f );
	save_block( 'BPM ', 4, &s->bpm, f );
	save_block( 'SPED', 4, &s->speed, f );
	save_block( 'GVOL', 4, &s->net->global_volume, f );
	if( s->song_name )
	    save_block( 'NAME', mem_get_size( s->song_name ), s->song_name, f );
	//Save full table with patterns:
	for( int i = 0; i < pats_num; i++ )
	{
	    if( s->pats[ i ] ) //If pattern is not empty:
	    {
		if( !( s->pats_info[ i ].flags & SUNVOX_PATTERN_FLAG_CLONE ) )
		{
		    //Normal pattern. Not a clone.
		    int max_channels = 0;
		    if( s->pats[ i ]->data )
		    {
			sunvox_pattern *pat = s->pats[ i ];
			//Get max number of channels:
			for( int yy = 0; yy < pat->lines; yy++ )
			{
			    int pat_ptr = yy * pat->data_xsize;
			    for( int xx = 0; xx < pat->channels; xx++ )
			    {
				sunvox_note *snote = &pat->data[ pat_ptr ];
				if( snote->note ||
				    snote->vel ||
				    snote->synth ||
				    snote->ctl ||
				    snote->ctl_val )
				{
				    if( xx + 1 > max_channels )
					max_channels = xx + 1;
				}
				pat_ptr++;
			    }
			}
			if( max_channels == 0 ) max_channels = 1;
			//Packing:
			sunvox_note *tpat = (sunvox_note*)MEM_NEW( HEAP_DYNAMIC, max_channels * pat->lines * sizeof( sunvox_note ) );
			int pptr = 0;
			for( int yy = 0; yy < pat->lines; yy++ )
			{
			    int pat_ptr = yy * pat->data_xsize;
			    for( int xx = 0; xx < max_channels; xx++ )
			    {
				sunvox_note *snote = &pat->data[ pat_ptr ];
				mem_copy( &tpat[ pptr ], &pat->data[ pat_ptr ], sizeof( sunvox_note ) );
				pptr++;
				pat_ptr++;
			    }
			}
			//Save pattern:
			save_block( 'PDTA', pptr * sizeof( sunvox_note ), tpat, f );
			mem_free( tpat );
		    }
		    save_block( 'PCHN', 4, &max_channels, f );
		    save_block( 'PLIN', 4, &s->pats[ i ]->lines, f );
		    save_block( 'PYSZ', 4, &s->pats[ i ]->ysize, f );
		    save_block( 'PICO', 32, s->pats[ i ]->icon, f );
		    save_block( 'PFGC', 3, s->pats[ i ]->fg, f );
		    save_block( 'PBGC', 3, s->pats[ i ]->bg, f );
		}
		else
		{
		    //It's clone of some another pattern. Save number of parent pattern:
		    save_block( 'PPAR', 4, &s->pats_info[ i ].parent_num, f );
		}
		//Save flags and coordinates:
		save_block( 'PFFF', 4, &s->pats_info[ i ].flags, f );
		save_block( 'PXXX', 4, &s->pats_info[ i ].x, f );
		save_block( 'PYYY', 4, &s->pats_info[ i ].y, f );
	    }
	    //End of pattern:
	    save_block( 'PEND', 0, 0, f );
	}
	psynth_net *net = s->net;
	//Save synths (psynth_net items):
	for( int i = 0; i < net->items_num; i++ )
	{
	    psynth_net_item *synth = &net->items[ i ];
	    if( synth->flags & PSYNTH_FLAG_EXISTS ) //If synth is not empty:
	    {
		save_block( 'SFFF', 4, &synth->flags, f );
		save_block( 'SNAM', 32, &synth->item_name, f );
		if( synth->synth )
		{
		    //Save synth's type (string):
		    UTF8_CHAR *synth_type = (UTF8_CHAR*)synth->synth( synth->data_ptr, i, 0, 0, 0, COMMAND_GET_SYNTH_NAME, net );
		    if( synth_type )
			save_block( 'STYP', mem_strlen( synth_type ) + 1, synth_type, f );
		}
		//Save standart properties:
		save_block( 'SFIN', 4, &synth->finetune, f );
		save_block( 'SREL', 4, &synth->relative_note, f );
		//Save coordinates:
		save_block( 'SXXX', 4, &synth->x, f );
		save_block( 'SYYY', 4, &synth->y, f );
		//Save array of links:
		save_block( 'SLNK', synth->input_num * 4, synth->input_links, f );
		//Save controllers:
		if( synth->ctls_num )
		{
		    for( int c = 0; c < synth->ctls_num; c++ )
		    {
			psynth_control *ctl = &synth->ctls[ c ];
			save_block( 'CVAL', 4, ctl->ctl_val, f );
		    }
		}
		//Save data chunks:
		if( synth->chunks )
		{
		    int chunks = mem_get_size( synth->chunks ) / sizeof( char* );
		    save_block( 'CHNK', 4, &chunks, f );
		    for( int c = 0; c < chunks; c++ )
		    {
			if( synth->chunks[ c ] )
			{
			    save_block( 'CHNM', 4, &c, f );
			    save_block( 'CHDT', mem_get_size( synth->chunks[ c ] ), synth->chunks[ c ], f );
			    save_block( 'CHFF', 4, &synth->chunk_flags[ c ], f );
			}
		    }
		}
	    }
	    //End of synth:
    	    save_block( 'SEND', 0, 0, f );
	}
	v3_close( f );
    }
}

int sunvox_load_synth( int x, int y, const UTF8_CHAR *name, sunvox_engine *s )
{
    int retval = -1;

    sunvox_stop( s );
    
    sound_stream_stop();

    V3_FILE f = v3_open( name, "rb" );
    if( f )
    {
	int s_ctls[ PSYNTH_MAX_CONTROLLERS ];
	int c_num = 0;
	int chunk_num = 0;
	int chunks_num = 0;
	char **chunks = 0;
	UTF8_CHAR *s_name = 0;
	int flags = 0;
	int finetune = 0;
	int relative = 0;
	int (*s_synth)( PSYTEXX_SYNTH_PARAMETERS ) = 0;

	while( 1 )
	{
	    g_block_data = 0;
	    load_block( f );
	    if( g_block_id == 0 ) break;
	    if( g_block_id == 'SSYN' ) { goto bfound2; }
	    if( g_block_id == 'VERS' ) { goto bfound2; }
	    if( g_block_id == 'SNAM' ) { s_name = g_block_data; g_block_data = 0; goto bfound2; }
	    if( g_block_id == 'SFFF' ) { flags = get_integer(); goto bfound2; }
	    if( g_block_id == 'SFIN' ) { finetune = get_integer(); goto bfound2; }
	    if( g_block_id == 'SREL' ) { relative = get_integer(); goto bfound2; }
	    if( g_block_id == 'STYP' )
	    {
		for( int ss = 0; ss < g_synths_num; ss++ )
		{
		    UTF8_CHAR *s_type = (UTF8_CHAR*)g_synths[ ss ]( 0, 0, 0, 0, 0, COMMAND_GET_SYNTH_NAME, s->net );
		    if( mem_strcmp( s_type, g_block_data ) == 0 ) 
		    {
			s_synth = g_synths[ ss ];
			break;
		    }
		}
		goto bfound2;
	    }
	    if( g_block_id == 'CVAL' ) { s_ctls[ c_num++ ] = get_integer(); goto bfound2; }
	    if( g_block_id == 'CHNK' ) 
	    {
		chunks_num = get_integer();
		chunks = (char**)MEM_NEW( HEAP_DYNAMIC, chunks_num * sizeof( char* ) );
		mem_set( chunks, chunks_num * sizeof( char* ), 0 );
		goto bfound2;
	    }
	    if( g_block_id == 'CHNM' ) { chunk_num = get_integer(); goto bfound2; }
	    if( g_block_id == 'CHDT' )
	    {
		//Get chunk data:
		chunks[ chunk_num ] = (char*)g_block_data; 
		g_block_data = 0;
		goto bfound2;
	    }
	    if( g_block_id == 'SEND' )
	    {
		int retval = psynth_add_synth( s_synth, s_name, flags, x, y, 0, s->net );
		if( s_name ) mem_free( s_name );
		s_name = 0;
		//Save standart properties:
		s->net->items[ retval ].finetune = finetune;
		s->net->items[ retval ].relative_note = relative;
	        int ctls = c_num;
	        if( ctls > s->net->items[ retval ].ctls_num ) 
		    ctls = s->net->items[ retval ].ctls_num;
		for( int cc = 0; cc < ctls; cc++ )
		{
		    *s->net->items[ retval ].ctls[ cc ].ctl_val = s_ctls[ cc ];
		}
		//Save chunks:
		if( chunks )
		{
		    if( s->net->items[ retval ].chunks )
			psynth_clear_chunks( retval, s->net );
		    s->net->items[ retval ].chunks = chunks;
		    chunks = 0;
		}
		psynth_synth_setup_finished( retval, s->net );
	    }
bfound2:
	    if( g_block_data )
	    {
		mem_free( g_block_data );
		g_block_data = 0;
	    }
	}

	v3_close( f );
    }

    sound_stream_play();

    return retval;
}

void sunvox_save_synth( int synth_id, const UTF8_CHAR *name, sunvox_engine *s )
{
    psynth_net *net = s->net;
    if( (unsigned)synth_id < (unsigned)net->items_num && ( net->items[ synth_id ].flags & PSYNTH_FLAG_EXISTS ) )
    {
	V3_FILE f = v3_open( name, "wb" );
	if( f )
	{
	    psynth_net_item *synth = &net->items[ synth_id ];
	    int version = 1;
	    save_block( 'SSYN', 0, 0, f );
	    save_block( 'VERS', 4, &version, f );
	    save_block( 'SFFF', 4, &synth->flags, f );
	    save_block( 'SNAM', 32, &synth->item_name, f );
	    if( synth->synth )
	    {
		//Save synth's type (string):
		UTF8_CHAR *synth_type = (UTF8_CHAR*)synth->synth( synth->data_ptr, synth_id, 0, 0, 0, COMMAND_GET_SYNTH_NAME, net );
		if( synth_type )
		    save_block( 'STYP', mem_strlen( synth_type ) + 1, synth_type, f );
	    }
	    //Save standart properties:
	    save_block( 'SFIN', 4, &synth->finetune, f );
	    save_block( 'SREL', 4, &synth->relative_note, f );
	    //Save controllers:
	    if( synth->ctls_num )
	    {
		for( int c = 0; c < synth->ctls_num; c++ )
		{
		    psynth_control *ctl = &synth->ctls[ c ];
		    save_block( 'CVAL', 4, ctl->ctl_val, f );
		}
	    }
	    //Save data chunks:
	    if( synth->chunks )
	    {
		int chunks = mem_get_size( synth->chunks ) / sizeof( char* );
		save_block( 'CHNK', 4, &chunks, f );
		for( int c = 0; c < chunks; c++ )
		{
		    if( synth->chunks[ c ] )
		    {
			save_block( 'CHNM', 4, &c, f );
			save_block( 'CHDT', mem_get_size( synth->chunks[ c ] ), synth->chunks[ c ], f );
		    }
		}
	    }
	    //End of synth:
    	    save_block( 'SEND', 0, 0, f );
	    v3_close( f );
	}
    }
}

unsigned int sunvox_get_song_length( sunvox_engine *s ) //ret song len in frames
{
    int number_of_lines = 0;
    for( int p = 0; p < s->pats_num; p++ )
    {
	sunvox_pattern *pat = s->pats[ p ];
	sunvox_pattern_info *pat_info = &s->pats_info[ p ];
	if( pat && pat_info->x + pat->lines > 0 )
	{
	    if( pat_info->x + pat->lines > number_of_lines )
		number_of_lines = pat_info->x + pat->lines;
	}
    }
    if( number_of_lines == 0 ) return 0;
    uchar *bpm_table = (uchar*)MEM_NEW( HEAP_DYNAMIC, number_of_lines );
    uchar *speed_table = (uchar*)MEM_NEW( HEAP_DYNAMIC, number_of_lines );
    mem_set( bpm_table, number_of_lines, 0 );
    mem_set( speed_table, number_of_lines, 0 );
    bpm_table[ 0 ] = s->bpm;
    speed_table[ 0 ] = s->speed;
    for( int p = 0; p < s->pats_num; p++ )
    {
	sunvox_pattern *pat = s->pats[ p ];
	sunvox_pattern_info *pat_info = &s->pats_info[ p ];
	if( pat )
	{
	    if( pat_info->x + pat->lines > 0 )
	    {
		if( pat->data )
		{
		    for( int y = 0; y < pat->lines; y++ )
		    {
			for( int x = 0; x < pat->channels; x++ )
			{
			    int ptr = y * pat->data_xsize + x;
			    int line_num = pat_info->x + y;
			    if( line_num >= 0 )
			    {
				sunvox_note *note = &pat->data[ ptr ];
				if( ( note->ctl & 0xFF ) == 0x0F )
				{
				    if( note->ctl_val < 32 ) 
				    { 
					speed_table[ line_num ] = (uchar)note->ctl_val; if( speed_table[ line_num ] <= 1 ) speed_table[ line_num ] = 1; 
				    }
				    else { bpm_table[ line_num ] = (uchar)note->ctl_val; }
				}
			    }
			} // for( x...
		    } //for( y...
		} //if( pat->data ...
	    } //if( pat_info->x + pat_lines > 0 ...
	} //if( pat...
    } //for( p...
    ulong len1 = 0;
    ulong len2 = 0;
    int bpm = s->bpm;
    int speed = s->speed;
    for( int l = 0; l < number_of_lines; l++ )
    {
	if( bpm_table[ l ] ) bpm = bpm_table[ l ];
	if( speed_table[ l ] ) speed = speed_table[ l ];
	int one_tick = ( ( ( s->net->sampling_freq * 60 ) << 8 ) / bpm ) / 24;
	int add = one_tick * speed;
	len1 += add >> 8;
	len2 += add & 255;
	if( len2 > 255 ) 
	{
	    len2 -= 256;
	    len1++;
	}
    }

    if( speed_table ) mem_free( speed_table );
    if( bpm_table ) mem_free( bpm_table );

    return len1;
}

int g_cancel_export_to_wav = 0;

//mode: 0 - 16bit WAV; 1 - 32bit (float) WAV
void sunvox_export_to_wav( 
    const UTF8_CHAR *name, 
    int mode,
    void (*status_handler)( void*, ulong ), 
    void *status_data, sunvox_engine *s )
{
    sunvox_stop( s );
    sound_stream_stop();
    
    //Calculate song length:
    unsigned int len = sunvox_get_song_length( s );
    dprint( "Song length (frames): %d\n", len );

    int channels = 2;
    int bytes = 2; //bytes per sample. Bytes per frame = bytes * channels

    if( mode == 1 )
    {
	bytes = 4;
    }
    
    //Start playing from beginning:
    {
	//Set tick counter to the edge of tick size
	//for immediate response (first tick ignore):
	s->tick_counter = ( ( ( s->net->sampling_freq * 60 ) << 8 ) / s->bpm ) / 24; 
    }
    s->time_counter = 0;
    sunvox_sort_patterns( s );
    sunvox_select_current_playing_patterns( 0, s );
    for( int i = 0; i < MAX_PLAYING_PATS; i++ ) 
        clean_std_effects_for_playing_pattern( i, s );
    s->time_counter--;
    //Send PLAY command:
    {
        sunvox_note snote;
        snote.vel = 0;
        snote.ctl = 0;
        snote.ctl_val = 0;
        snote.note = NOTECMD_PLAY;
        sunvox_send_user_command( &snote, 0, s );
    }
    s->single_pattern_play = -1;

    //Save data:
    V3_FILE f = v3_open( name, "wb" );
    if( f )
    {
	//Save header:
	int temp;
	v3_write( (void*)"RIFF", 1, 4, f );
	temp = 4 + 24 + 8 + ( len * channels * bytes ); //WAVE + Fmt + Data
	v3_write( &temp, 1, 4, f );
	v3_write( (void*)"WAVE", 1, 4, f );

	//Save Fmt:
	v3_write( (void*)"fmt ", 1, 4, f );
	temp = 16;
	v3_write( &temp, 1, 4, f );
	short ff = 1;
	if( mode == 1 ) ff = 3; //Float 32 bits
	v3_write( &ff, 1, 2, f ); //Format
	ff = channels; //Channels
	v3_write( &ff, 1, 2, f );
	temp = s->net->sampling_freq; //Samples per second
	v3_write( &temp, 1, 4, f );
	temp = s->net->sampling_freq * channels * bytes; //Bytes per second
	v3_write( &temp, 1, 4, f );
	ff = channels * bytes;
	v3_write( &ff, 1, 2, f );
	ff = bytes * 8; // bits
	v3_write( &ff, 1, 2, f );

	//Save data:
	v3_write( (void*)"data", 1, 4, f );
	temp = len * channels * bytes;
	v3_write( &temp, 1, 4, f );
	void *buf = MEM_NEW( HEAP_DYNAMIC, 32768 * channels * bytes );
	ulong i = 0;
	int r_cnt = 0;
	while( 1 )
	{
	    int buf_size = 32768;
	    if( len - i < 32768 ) buf_size = len - i;
	    if( mode == 1 )
	    {	    
		//Float 32 bits
		sunvox_render_piece_of_sound( 1, buf, buf_size, channels, s->net->sampling_freq, 0, s );
	    }
	    else
	    {
		//Int 16 bits
		sunvox_render_piece_of_sound( 0, buf, buf_size, channels, s->net->sampling_freq, 0, s );
	    }
	    v3_write( buf, bytes * channels, buf_size, f );
	    r_cnt += buf_size;
	    if( r_cnt > 64000 ) 
	    {
		status_handler( status_data, ( ( i >> 8 ) << 8 ) / ( len >> 8 ) );
		r_cnt = 0;
	    }
	    i += buf_size;
	    if( i >= len ) break;
	    if( g_cancel_export_to_wav ) break;
	}
	mem_free( buf );

	v3_close( f );
    }

    g_cancel_export_to_wav = 0;

    sound_stream_play();

    sunvox_stop( s );
}

void sunvox_sort_patterns( sunvox_engine *s )
{
    s->song_lines = 999999;
    if( s->pats && s->pats_num )
    {
	if( s->sorted_pats == 0 )
	{
	    s->sorted_pats = (int*)MEM_NEW( HEAP_DYNAMIC, s->pats_num * sizeof( int ) );
	}
	else
	{
	    if( s->pats_num > mem_get_size( s->sorted_pats ) / (int)sizeof( int ) )
	    {
		s->sorted_pats = (int*)mem_resize( s->sorted_pats, s->pats_num * sizeof( int ) );
	    }
	}
	s->sorted_pats_num = 0;
	for( int i = 0; i < s->pats_num; i++ )
	{
	    if( s->pats[ i ] )
	    {
		s->sorted_pats[ s->sorted_pats_num ] = i;
		s->sorted_pats_num++;
	    }
	}
	//Sort it:
	s->song_lines = 0;
	while( 1 )
	{
	    int exc = 0;
	    int i;
	    for( i = 0; i < s->sorted_pats_num; i++ )
	    {
		if( i < s->sorted_pats_num - 1 )
		{
		    sunvox_pattern *pat1 = s->pats[ s->sorted_pats[ i ] ];
		    sunvox_pattern *pat2 = s->pats[ s->sorted_pats[ i + 1 ] ];
		    sunvox_pattern_info *pat_info1 = &s->pats_info[ s->sorted_pats[ i ] ];
		    sunvox_pattern_info *pat_info2 = &s->pats_info[ s->sorted_pats[ i + 1 ] ];
		    if( pat_info1->x > pat_info2->x )
		    {
			int temp = s->sorted_pats[ i ];
			s->sorted_pats[ i ] = s->sorted_pats[ i + 1 ];
			s->sorted_pats[ i + 1 ] = temp;
			exc = 1;
		    }
		    else
		    {
			if( pat_info1->x == pat_info2->x &&
			    pat_info1->x + pat1->lines > pat_info2->x + pat2->lines )
			{
			    int temp = s->sorted_pats[ i ];
			    s->sorted_pats[ i ] = s->sorted_pats[ i + 1 ];
			    s->sorted_pats[ i + 1 ] = temp;
			    exc = 1;
			}
		    }
		}
	    }
	    for( i = 0; i < s->sorted_pats_num; i++ )
	    {
		if( s->pats_info[ s->sorted_pats[ i ] ].x + s->pats[ s->sorted_pats[ i ] ]->lines > s->song_lines )
		    s->song_lines = s->pats_info[ s->sorted_pats[ i ] ].x + s->pats[ s->sorted_pats[ i ] ]->lines;
	    }
	    if( exc == 0 ) break;
	}
    }
    else
    {
	s->sorted_pats_num = 0;
    }
}

unsigned int g_icon_seed_counter = 0;

int sunvox_new_pattern( int lines, int channels, int x, int y, sunvox_engine *s )
{
    if( s->pats == 0 )
    {
	s->pats = (sunvox_pattern**)MEM_NEW( HEAP_DYNAMIC, 16 * sizeof( sunvox_pattern* ) );
	s->pats_info = (sunvox_pattern_info*)MEM_NEW( HEAP_DYNAMIC, 16 * sizeof( sunvox_pattern_info ) );
	mem_set( s->pats, 16 * sizeof( sunvox_pattern* ), 0 );
	mem_set( s->pats_info, 16 * sizeof( sunvox_pattern_info ), 0 );
	s->pats_num = 16;
    }
    int p = 0;
    for( p = 0; p < s->pats_num; p++ )
    {
	if( s->pats[ p ] == 0 ) break;
    }
    if( p >= s->pats_num )
    {
	s->pats = (sunvox_pattern**)mem_resize( s->pats, ( s->pats_num + 16 ) * sizeof( sunvox_pattern* ) );
	s->pats_info = (sunvox_pattern_info*)mem_resize( s->pats_info, ( s->pats_num + 16 ) * sizeof( sunvox_pattern_info ) );
	s->pats_num += 16;
    }
    s->pats[ p ] = (sunvox_pattern*)MEM_NEW( HEAP_DYNAMIC, sizeof( sunvox_pattern ) );
    sunvox_pattern *pat = s->pats[ p ];
    sunvox_pattern_info *pat_info = &s->pats_info[ p ];
    pat->lines = lines;
    pat->channels = channels;
    pat->data = (sunvox_note*)MEM_NEW( HEAP_DYNAMIC, sizeof( sunvox_note ) * channels * lines );
    mem_set( pat->data, mem_get_size( pat->data ), 0 );
    pat->data_xsize = channels;
    pat->data_ysize = lines;
    pat_info->x = x;
    pat_info->y = y;
    s->pats_info[ p ].flags = 0;
    pat->ysize = 32;
    
    //Create nice pattern icon :)
    set_seed( (ulong)time_ticks() + g_icon_seed_counter );
    g_icon_seed_counter++;
    int mirror = pseudo_random();
    for( int i = 0; i < 16; i++ ) 
    {
	pat->icon[ i ] = (uint16)pseudo_random() * 233;
	if( mirror & 16 )
	{
	    if( mirror & 2 )
	    {
		if( i > 0 && ( pseudo_random() & 1 ) )
		    pat->icon[ i ] = pat->icon[ i - 1 ] + 2;
	    }
	    else
	    if( mirror & 4 )
	    {
		if( i > 0 && ( pseudo_random() & 1 ) )
		    pat->icon[ i ] = pat->icon[ i - 1 ] << 1;
	    }
	    else
	    if( mirror & 8 )
	    {
		if( i > 0 && ( pseudo_random() & 1 ) )
		    pat->icon[ i ] = pat->icon[ i - 1 ] >> 1;
	    }
	}
	for( int i2 = 0; i2 < 8; i2++ )
	{
	    if( pat->icon[ i ] & ( 1 << i2 ) )
		pat->icon[ i ] |= ( 0x8000 >> i2 );
	    else
		pat->icon[ i ] &= ~( 0x8000 >> i2 );
	}
    }
    if( mirror & 1 )
    {
	for( int i = 0; i < 8; i++ ) pat->icon[ 15 - i ] = pat->icon[ i ];
    }
    pat->fg[ 0 ] = 0;
    pat->fg[ 1 ] = 0;
    pat->fg[ 2 ] = 0;
    pat->bg[ 0 ] = 255;
    pat->bg[ 1 ] = 255;
    pat->bg[ 2 ] = 255;

    for( int i = 0; i < MAX_PATTERN_CHANNELS; i++ ) pat_info->channel_status[ i ] = 0xFF;
    return p;
}

int sunvox_new_pattern_clone( int pat_num, int x, int y, sunvox_engine *s )
{
    if( pat_num >= 0 && pat_num < s->pats_num && s->pats[ pat_num ] )
    {
	if( s->pats_info[ pat_num ].flags & SUNVOX_PATTERN_FLAG_CLONE )
	{
	    //pat_num is already clone of somebody. Lets find parent:
	    for( int i = 0; i < s->pats_num; i++ )
	    {
		if( s->pats[ pat_num ] == s->pats[ i ] && 
		    !( s->pats_info[ i ].flags & SUNVOX_PATTERN_FLAG_CLONE ) )
		    pat_num = i;
	    }
	}
	int p;
	for( p = 0; p < s->pats_num; p++ )
	{
	    if( s->pats[ p ] == 0 ) break;
	}
	if( p >= s->pats_num )
	{
	    s->pats = (sunvox_pattern**)mem_resize( s->pats, ( s->pats_num + 16 ) * sizeof( sunvox_pattern* ) );
	    s->pats_info = (sunvox_pattern_info*)mem_resize( s->pats_info, ( s->pats_num + 16 ) * sizeof( sunvox_pattern_info ) );
	    s->pats_num += 16;
	}
	s->pats[ p ] = s->pats[ pat_num ];
	s->pats_info[ p ].x = x;
	s->pats_info[ p ].y = y;
	s->pats_info[ p ].flags = SUNVOX_PATTERN_FLAG_CLONE;
	s->pats_info[ p ].parent_num = pat_num;
	for( int i = 0; i < MAX_PATTERN_CHANNELS; i++ ) s->pats_info[ p ].channel_status[ i ] = 0xFF;
	return p;
    }
    return -1;
}

void sunvox_copy_selected_patterns( sunvox_engine *s )
{
    int pats_to_copy = 0;
    for( int i = 0; i < s->pats_num; i++ )
    {
	if( s->pats[ i ] && s->pats_info[ i ].flags & SUNVOX_PATTERN_FLAG_SELECTED )
	{
	    pats_to_copy++;
	}
    }
    if( pats_to_copy > 0 )
    {
	sunvox_close_copy_pats( s );
	s->copy_pats = (sunvox_pattern*)MEM_NEW( HEAP_DYNAMIC, pats_to_copy * sizeof( sunvox_pattern ) );
	s->copy_pats_info = (sunvox_pattern_info*)MEM_NEW( HEAP_DYNAMIC, pats_to_copy * sizeof( sunvox_pattern_info ) );
	s->copy_pats_num = pats_to_copy;
	int p = 0;
	for( int i = 0; i < s->pats_num; i++ )
	{
	    if( s->pats[ i ] && s->pats_info[ i ].flags & SUNVOX_PATTERN_FLAG_SELECTED )
	    {
		sunvox_pattern *copy_pat = &s->copy_pats[ p ];
		sunvox_pattern_info *copy_pat_info = &s->copy_pats_info[ p ];
		mem_copy( copy_pat, s->pats[ i ], sizeof( sunvox_pattern ) );
		mem_copy( copy_pat_info, &s->pats_info[ i ], sizeof( sunvox_pattern_info ) );
		if( s->pats[ i ]->data )
		{
		    copy_pat->data = (sunvox_note*)MEM_NEW( HEAP_DYNAMIC, mem_get_size( s->pats[ i ]->data ) );
		    mem_copy( copy_pat->data, s->pats[ i ]->data, mem_get_size( s->pats[ i ]->data ) );
		}
		p++;
	    }
	}
    }
}

void sunvox_paste_patterns( int x, int y, sunvox_engine *s )
{
    if( s->copy_pats_num )
    {
	//Get top left corner of patterns in copy buffer:
	int top = 999999;
	int left = 999999;
	for( int p = 0; p < s->copy_pats_num; p++ )
	{
	    sunvox_pattern_info *copy_pat_info = &s->copy_pats_info[ p ];
	    if( copy_pat_info->x < left ) left = copy_pat_info->x;
	    if( copy_pat_info->y < top ) top = copy_pat_info->y;
	}
	//Paste:
	for( int p = 0; p < s->copy_pats_num; p++ )
	{
	    sunvox_pattern *copy_pat = &s->copy_pats[ p ];
	    sunvox_pattern_info *copy_pat_info = &s->copy_pats_info[ p ];

	    int pat_num = sunvox_new_pattern( 
		copy_pat->lines, 
		copy_pat->channels,
		0, 0, 
		s );
	    if( copy_pat_info->flags & SUNVOX_PATTERN_FLAG_CLONE )
	    {
		mem_free( s->pats[ pat_num ]->data );
		mem_free( s->pats[ pat_num ] );
		s->pats[ pat_num ] = s->pats[ copy_pat_info->parent_num ];
	    }
	    else
	    {
		mem_free( s->pats[ pat_num ]->data );
		mem_copy( s->pats[ pat_num ], copy_pat, sizeof( sunvox_pattern ) );
		s->pats[ pat_num ]->data = (sunvox_note*)MEM_NEW( HEAP_DYNAMIC, mem_get_size( copy_pat->data ) );
		mem_copy( s->pats[ pat_num ]->data, copy_pat->data, mem_get_size( copy_pat->data ) );
	    }
	    mem_copy( &s->pats_info[ pat_num ], copy_pat_info, sizeof( sunvox_pattern_info ) );
	    s->pats_info[ pat_num ].x = copy_pat_info->x - left + x;
	    s->pats_info[ pat_num ].y = copy_pat_info->y - top + y;
	}
    }
}

void sunvox_remove_pattern( int pat_num, sunvox_engine *s )
{
    if( pat_num >= 0 && pat_num < s->pats_num )
    {
	sunvox_pattern *pat = s->pats[ pat_num ];
	if( pat ) 
	{
	    if( s->pats_info[ pat_num ].flags & SUNVOX_PATTERN_FLAG_CLONE )
	    {
		//It's a clone
		s->pats[ pat_num ] = 0;
	    }
	    else
	    {
		if( pat->data ) mem_free( pat->data );
		mem_free( pat );
		s->pats[ pat_num ] = 0;
		//Try to find children:
		for( int i = 0; i < s->pats_num; i++ )
		{
		    if( s->pats[ i ] == pat )
		    {
			s->pats[ i ] = 0;
		    }
		}
	    }
	}
    }
}

void sunvox_print_patterns( sunvox_engine *s )
{
#ifndef PALMOS
    for( int i = 0; i < s->pats_num; i++ )
    {
	printf( "%03d ptr:%08x x:%04d y:%04d ", i, (int)s->pats[ i ], s->pats_info[ i ].x, s->pats_info[ i ].y );
	if( s->pats[ i ] == 0 ) printf( "EMPTY " );
	if( s->pats_info[ i ].flags & SUNVOX_PATTERN_FLAG_CLONE ) printf( "CLONE (parent %d) ", s->pats_info[ i ].parent_num );
	printf( "\n" );
    }
#endif
}

void sunvox_optimize_patterns( sunvox_engine *s )
{
    int i, a;

    //Remove holes with NULL patterns:
    int h = 0;
    int h_size = 0;
    for( i = 0; i < s->pats_num; i++ )
    {
	if( s->pats[ i ] )
	{
	    if( h_size )
	    {
		//Remove hole: (offset - h; size - h_size)
		for( a = h; a < s->pats_num; a++ )
		{
		    if( a + h_size < s->pats_num )
		    {
			s->pats[ a ] = s->pats[ a + h_size ];
			mem_copy( &s->pats_info[ a ], &s->pats_info[ a + h_size ], sizeof( sunvox_pattern_info ) );
		    }
		    else
		    {
			s->pats[ a ] = 0;
			mem_set( &s->pats_info[ a ], sizeof( sunvox_pattern_info ), 0 );
		    }
		}
		//Some patterns moved. Now we must recalculate links to these patterns:
		for( a = 0; a < s->pats_num; a++ )
		{
		    if( s->pats[ a ] && 
			( s->pats_info[ a ].flags & SUNVOX_PATTERN_FLAG_CLONE ) &&
			s->pats_info[ a ].parent_num >= h + h_size )
		    {
			s->pats_info[ a ].parent_num -= h_size;
		    }
		}
		for( a = 0; a < s->copy_pats_num; a++ )
		{
		    if( ( s->copy_pats_info[ a ].flags & SUNVOX_PATTERN_FLAG_CLONE ) &&
			s->copy_pats_info[ a ].parent_num >= h + h_size )
		    {
			s->copy_pats_info[ a ].parent_num -= h_size;
		    }
		}
		for( a = 0; a < s->sorted_pats_num; a++ )
		{
		    if( s->sorted_pats[ a ] >= h + h_size )
			s->sorted_pats[ a ] -= h_size;
		}
#ifdef SUNVOX_GUI
		if( g_sunvox_data->pat_num >= h + h_size )
		    g_sunvox_data->pat_num -= h_size;
#endif
	    }
	    h = i + 1;
	    h_size = 0;
	} //if( s->pats[ i ] )...
	else
	{
	    h_size++;
	}
    }

#ifdef DEBUG_MESSAGES    
    sunvox_print_patterns( s );
#endif
}

void sunvox_pattern_set_number_of_channels( int pat_num, int cnum, sunvox_engine *s )
{
    if( pat_num >= 0 && pat_num < s->pats_num && s->pats[ pat_num ] )
    {
	sunvox_pattern *pat = s->pats[ pat_num ];
	if( cnum > pat->data_xsize )
	{
	    sunvox_note *new_data = (sunvox_note*)MEM_NEW( HEAP_DYNAMIC, cnum * pat->data_ysize * sizeof( sunvox_note ) );
	    mem_set( new_data, cnum * pat->data_ysize * sizeof( sunvox_note ), 0 );
	    for( int y = 0; y < pat->lines; y++ )
	    {
		for( int x = 0; x < pat->data_xsize; x++ )
		{
		    mem_copy( &new_data[ y * cnum + x ], &pat->data[ y * pat->data_xsize + x ], sizeof( sunvox_note ) );
		}
	    }
	    mem_free( pat->data );
	    pat->data = new_data;
	    pat->data_xsize = cnum;
	}
	pat->channels = cnum;
    }
}

void sunvox_pattern_set_number_of_lines( int pat_num, int lnum, sunvox_engine *s )
{
    if( pat_num >= 0 && pat_num < s->pats_num && s->pats[ pat_num ] )
    {
	sunvox_pattern *pat = s->pats[ pat_num ];
	if( lnum > pat->data_ysize )
	{
	    sunvox_note *new_data = (sunvox_note*)MEM_NEW( HEAP_DYNAMIC, pat->data_xsize * lnum * sizeof( sunvox_note ) );
	    mem_set( new_data, pat->data_xsize * lnum * sizeof( sunvox_note ), 0 );
	    for( int y = 0; y < pat->lines; y++ )
	    {
		for( int x = 0; x < pat->data_xsize; x++ )
		{
		    mem_copy( &new_data[ y * pat->data_xsize + x ], &pat->data[ y * pat->data_xsize + x ], sizeof( sunvox_note ) );
		}
	    }
	    mem_free( pat->data );
	    pat->data = new_data;
	    pat->data_ysize = lnum;
	}
	pat->lines = lnum;
    }
}

void sunvox_select_current_playing_patterns( int first_sorted_pat, sunvox_engine *s )
{
    if( first_sorted_pat < 0 ) first_sorted_pat = 0;
    s->cur_playing_pats[ 0 ] = -1;
    s->last_sort_pat = -1;
    if( s->sorted_pats_num )
    {
	int p = 0;
	int std_eff_ptr = 0;
	for( int i = first_sorted_pat; i < s->sorted_pats_num; i++ )
	{
	    if( s->time_counter >= s->pats_info[ s->sorted_pats[ i ] ].x &&
		s->time_counter < s->pats_info[ s->sorted_pats[ i ] ].x + s->pats[ s->sorted_pats[ i ] ]->lines )
	    {
		for( int sp = 0; sp < MAX_PLAYING_PATS; sp++ )
		{
		    if( s->std_eff_busy[ std_eff_ptr ] == 0 )
		    {
			s->std_eff_busy[ std_eff_ptr ] = 1;
			s->pats_info[ s->sorted_pats[ i ] ].std_eff_ptr = std_eff_ptr;
			break;
		    }
		    std_eff_ptr++;
		    if( std_eff_ptr >= MAX_PLAYING_PATS ) std_eff_ptr = 0;
		}
		s->cur_playing_pats[ p ] = i; //Add new playing pattern
		p++;
		if( p + 1 >= MAX_PLAYING_PATS ) break;
	    }
	    if( s->pats_info[ s->sorted_pats[ i ] ].x > s->time_counter )
	    {
		break;
	    }
	    s->last_sort_pat = i;
	}
	s->cur_playing_pats[ p ] = -1;
	//Select current note pointers in patterns:
	for( int i = 0; i < s->pats_num; i++ )
	{
	    sunvox_pattern *pat = s->pats[ i ];
	    sunvox_pattern_info *pat_info = &s->pats_info[ i ];
	    if( pat )
	    {
		pat_info->cur_time_ptr = -1;
	    }
	}
	for( int i = 0; i < MAX_PLAYING_PATS; i++ )
	{
	    if( s->cur_playing_pats[ i ] == -1 ) break;
	    int sort_pat_num = s->cur_playing_pats[ i ];
	    if( sort_pat_num >= 0 && sort_pat_num < s->sorted_pats_num )
	    {
		int pat_num = s->sorted_pats[ sort_pat_num ];
		if( pat_num >= 0 && pat_num < s->pats_num && s->pats[ pat_num ] )
		{
		    sunvox_pattern *pat = s->pats[ pat_num ];
		    sunvox_pattern_info *pat_info = &s->pats_info[ pat_num ];
		    pat_info->cur_time_ptr = s->time_counter - pat_info->x;
		    for( int c = 0; c < MAX_PATTERN_CHANNELS; c++ ) pat_info->channel_status[ p ] = 0xFF;
		}
	    }
	}
    }
}

void remove_link_to_effects_for_playing_pattern( int pat_num, sunvox_engine *s )
{
    s->std_eff_busy[ pat_num ] = 0;
}

void clean_std_effects_for_playing_pattern( int pat_num, sunvox_engine *s )
{
    pat_num *= MAX_PATTERN_CHANNELS;
    for( int i = 0; i < MAX_PATTERN_CHANNELS; i++ )
    {
	sunvox_std_eff *eff = &s->std_eff[ pat_num ];
	eff->flags = 0;
	eff->vel_speed = 0;
	pat_num++;
    }
}

int sunvox_get_playing_status( sunvox_engine *s )
{
    return s->playing;
}

void sunvox_play( sunvox_engine *s )
{
    if( s->playing == 0 )
    {
	sunvox_sort_patterns( s );
	sunvox_select_current_playing_patterns( 0, s );
	for( int i = 0; i < MAX_PLAYING_PATS; i++ ) 
	    clean_std_effects_for_playing_pattern( i, s );
	s->time_counter--;
	//Calc song length:
	s->song_len = sunvox_get_song_length( s );
	//Send PLAY command:
	{
	    sunvox_note snote;
	    snote.vel = 0;
	    snote.ctl = 0;
	    snote.ctl_val = 0;
	    snote.note = NOTECMD_PLAY;
	    sunvox_send_user_command( &snote, 0, s );
	}
	while( s->playing == 0 ) { /*still not playing...*/ }
    }
    s->start_time = time_ticks();
    s->single_pattern_play = -1;
    s->end_of_song = 0;
    s->stop_at_the_end_of_song = 0;
}

void sunvox_play_from_beginning( sunvox_engine *s )
{
    if( s->playing == 0 )
    {
	s->time_counter = 0;
	sunvox_sort_patterns( s );
	sunvox_select_current_playing_patterns( 0, s );
	for( int i = 0; i < MAX_PLAYING_PATS; i++ ) 
	    clean_std_effects_for_playing_pattern( i, s );
	s->time_counter--;
	//Calc song length:
	s->song_len = sunvox_get_song_length( s );
	//Send PLAY command:
	{
	    sunvox_note snote;
	    snote.vel = 0;
	    snote.ctl = 0;
	    snote.ctl_val = 0;
	    snote.note = NOTECMD_PLAY;
	    sunvox_send_user_command( &snote, 0, s );
	}
	while( s->playing == 0 ) { /*still not playing...*/ }
    }
    else
    {
	sunvox_rewind( 0, s );
    }
    s->single_pattern_play = -1;
    s->start_time = time_ticks();
    s->end_of_song = 0;
    s->stop_at_the_end_of_song = 0;
}

void sunvox_rewind( int t, sunvox_engine *s )
{
    int playing;
    playing = s->playing;
    if( playing ) sunvox_stop( s );
    s->single_pattern_play = -1;
    s->time_counter = t;
    sunvox_sort_patterns( s );
    sunvox_select_current_playing_patterns( 0, s );
    for( int i = 0; i < MAX_PLAYING_PATS; i++ ) 
	clean_std_effects_for_playing_pattern( i, s );
    if( playing ) sunvox_play( s );
}

void sunvox_stop( sunvox_engine *s )
{
    sunvox_note snote;
    snote.vel = 0;
    snote.ctl = 0;
    snote.ctl_val = 0;
    if( s->playing )
	snote.note = NOTECMD_STOP;
    else
	snote.note = NOTECMD_CLEAN_SYNTHS; //Second STOP will clean all synths
    sunvox_send_user_command( &snote, 0, s );
    while( s->playing ) { /*still playing...*/ }
    for( int i = 0; i < MAX_PLAYING_PATS; i++ ) 
	remove_link_to_effects_for_playing_pattern( i, s );
}

#define GET_PERIOD_PTR( p, snum, n ) \
{ \
    p = ( 7680 * 4 ) - ( n ) * 256 - net->items[ snum ].finetune - net->items[ snum ].relative_note * 256; \
    if( p >= 7680 * 4 ) p = 7680 * 4; \
}

void sunvox_handle_command( 
    sunvox_note *snote, 
    psynth_net *net,
    int pat_num,
    int playing_pat_num,
    int channel_num,
    sunvox_engine *s )
{
    //
    // Get pattern pointer
    //

    sunvox_pattern_info *pat_info;
    if( pat_num >= 0 ) 
    {
	pat_info = &s->pats_info[ pat_num ];
    }
    else
    {
	//pat_num == -1. It's command from user - not linked to song pattern.
	pat_info = &s->user_pat_info;
	pat_num = 0xFFFF;
    }

    //
    // Get pointer to structure with standart effect's variables
    //
    
    sunvox_std_eff *eff = 0;
    if( pat_num != 0xFFFF )
	eff = &s->std_eff[ pat_info->std_eff_ptr * MAX_PATTERN_CHANNELS + channel_num ];

    //
    // Handle new note
    //

    int note = snote->note;
    int note_num;
    if( note > 0 && note < 128 ) 
	note_num = 1;
    else
	note_num = 0;
    int vel = snote->vel;
    int ctl = snote->ctl;
    int ctl_val = snote->ctl_val;

    if( note_num && ctl != 0x3 )
    {
	//Note ON:
	int synth_num = snote->synth - 1;
	if( synth_num >= 0 && synth_num < net->items_num ) 
	{
	    //Set note:
	    net->note = note - 1;
	    
	    //Set velocity:
	    if( snote->vel )
		net->velocity = ( vel - 1 ) * 2;
	    else
		net->velocity = 256;
	    if( eff ) eff->cur_vel = net->velocity;
	    
	    //Set channel ID:
	    net->channel_id = ( pat_num << 16 ) | channel_num;
	    
	    //Set period:
	    int period;
	    GET_PERIOD_PTR( period, synth_num, net->note );
	    if( eff ) eff->cur_period = period;
	    net->period_ptr = period;

	    //Execute synth handler:
	    if( net->items[ synth_num ].synth )
	    {
		if( pat_info->channel_status[ channel_num ] < 128 )
		{
		    //Turn OFF previous note:
		    int prev_synth_num = pat_info->channel_synth[ channel_num ];
		    if( prev_synth_num < net->items_num && net->items[ prev_synth_num ].synth )
		    {
			net->synth_channel = pat_info->channel_status[ channel_num ] & 127;
			net->items[ prev_synth_num ].synth( 
			    net->items[ prev_synth_num ].data_ptr,
			    prev_synth_num,
			    0, 0, 0,
			    COMMAND_NOTE_OFF,
			    net );
		    }
		}
		//Play new note:
		int synth_channel_number = 
		    net->items[ synth_num ].synth( 
			net->items[ synth_num ].data_ptr,
			synth_num,
			0, 0, 0,
			COMMAND_NOTE_ON,
			net );
		pat_info->channel_status[ channel_num ] = synth_channel_number;
		pat_info->channel_synth[ channel_num ] = synth_num;
	    }
	}
    }
    
    if( note == 128 )
    {
	//Note OFF:
	int synth_num = pat_info->channel_synth[ channel_num ];
	if( synth_num < net->items_num && net->items[ synth_num ].synth )
	{
	    if( pat_info->channel_status[ channel_num ] < 128 )
	    {
		net->channel_id = ( pat_num << 16 ) | channel_num;
		net->synth_channel = pat_info->channel_status[ channel_num ] & 127;
		net->items[ synth_num ].synth( 
		    net->items[ synth_num ].data_ptr,
		    synth_num,
		    0, 0, 0,
		    COMMAND_NOTE_OFF,
		    net );
		pat_info->channel_status[ channel_num ] |= 128;
	    }
	}
    }

    if( note == NOTECMD_ALL_NOTES_OFF )
    {
	//All notes OFF on all synths:
	for( int i = 0; i < net->items_num; i++ )
	{
	    if( net->items[ i ].synth )
	    {
		net->items[ i ].synth( 
		    net->items[ i ].data_ptr,
		    i,
		    0, 0, 0,
		    COMMAND_ALL_NOTES_OFF,
		    net );
	    }
	}
    }

    if( note == NOTECMD_CLEAN_SYNTHS )
    {
	//Stop and clean all synths:
	for( int i = 0; i < net->items_num; i++ )
	{
	    if( net->items[ i ].synth )
	    {
		net->items[ i ].synth( 
		    net->items[ i ].data_ptr,
		    i,
		    0, 0, 0,
		    COMMAND_CLEAN,
		    net );
	    }
	}
    }

    //
    // Set new velocity
    //

    if( vel && ( note_num == 0 || ctl == 0x3 ) )
    {
	int synth_num = pat_info->channel_synth[ channel_num ];
	if( synth_num < net->items_num && net->items[ synth_num ].synth )
	{
	    net->channel_id = ( pat_num << 16 ) | channel_num;
	    net->synth_channel = pat_info->channel_status[ channel_num ] & 127;
	    net->velocity = ( vel - 1 ) * 2;
	    if( eff ) eff->cur_vel = net->velocity;
	    net->items[ synth_num ].synth( 
		net->items[ synth_num ].data_ptr,
		synth_num,
		0, 0, 0,
		COMMAND_SET_VELOCITY,
		net );
	}
    }

    //
    // Handle controller's values
    //

    if( ctl & 0xFF00 )
    {
	//Global or local controller:
	int synth_num = snote->synth - 1;
	int command_type = COMMAND_SET_LOCAL_CONTROLLER;
	if( synth_num >= 0 && note == 0 )
	{
	    //It's a global controller:
	    command_type = COMMAND_SET_GLOBAL_CONTROLLER;
	}
	if( synth_num == -1 )
	{
	    //Synth not selected. Use previous on this channel:
	    synth_num = pat_info->channel_synth[ channel_num ];
	}
	if( synth_num < net->items_num )
	{
	    if( command_type == COMMAND_SET_LOCAL_CONTROLLER )
	    {
		net->channel_id = ( pat_num << 16 ) | channel_num;
		net->synth_channel = pat_info->channel_status[ channel_num ] & 127;
	    }
	    int ctl_num = net->ctl_num = ( snote->ctl >> 8 ) - 1;
	    int ctl_val = net->ctl_val = snote->ctl_val;
	    if( net->items[ synth_num ].synth )
	    {
		int handled = 
		    net->items[ synth_num ].synth( 
			net->items[ synth_num ].data_ptr,
			synth_num,
			0, 0, 0,
			command_type,
			net );
		if( handled == 0 )
		{
		    psynth_net_item *sitem = &net->items[ synth_num ];
		    if( ctl_num >= 0 && ctl_num < sitem->ctls_num )
		    {
			if( ctl_val > 0x8000 ) ctl_val = 0x8000;
			psynth_control *ctl = &sitem->ctls[ ctl_num ];
			if( ctl->type == 0 )
			{
			    ulong val = (ulong)( ctl->ctl_max - ctl->ctl_min ) * (ulong)ctl_val;
			    val >>= 15;
			    *ctl->ctl_val = (CTYPE)( val + ctl->ctl_min );
			}
			else
			{
			    *ctl->ctl_val = (CTYPE)ctl_val;
			    if( *ctl->ctl_val < ctl->ctl_min ) *ctl->ctl_val = ctl->ctl_min;
			    if( *ctl->ctl_val > ctl->ctl_max ) *ctl->ctl_val = ctl->ctl_max;
			}
		    }
		}
	    }
	}
    }

    //
    // Handle standart effects
    //

    //Clean std effects for this channel:
    if( eff )
    {
	eff->flags &= EFF_FLAG_ARPEGGIO_IN_PREVIOUS_TICK;
	eff->vel_speed = 0;
	eff->arpeggio = 0;
    }

    if( ctl & 0x00FF )
    {
	int synth_num = snote->synth - 1;
	if( synth_num == -1 )
	{
	    //Synth not selected. Use previous on this channel:
	    synth_num = pat_info->channel_synth[ channel_num ];
	}
	switch( ctl & 0x00FF )
	{
	    case 0x1:
		//Porta up:
		if( eff )
		{
		    eff->flags |= EFF_FLAG_TONE_PORTA;
		    eff->target_period = -7680 * 4;
		    if( ctl_val )
			eff->porta_speed = ctl_val * 4;
		}
		break;
	    case 0x2:
		//Porta down:
		if( eff )
		{
		    eff->flags |= EFF_FLAG_TONE_PORTA;
		    eff->target_period = 7680 * 4;
		    if( ctl_val )
			eff->porta_speed = ctl_val * 4;
		}
		break;
	    case 0x3:
		//Tone portamento:
		if( eff )
		{
		    eff->flags |= EFF_FLAG_TONE_PORTA;
		    if( synth_num < net->items_num )
		    {
			if( note_num )
			{
			    int period;
			    GET_PERIOD_PTR( period, synth_num, note - 1 );
			    eff->target_period = (int16)period;
			}
		    }
		    if( ctl_val )
			eff->porta_speed = ctl_val * 4;
		}
		break;
	    case 0x8:
		//Arpeggio:
		if( eff )
		{
		    eff->arpeggio = (uint16)ctl_val;
		}
		break;
	    case 0x9:
		//Set sample offset:
		if( synth_num < net->items_num && net->items[ synth_num ].synth )
		{
		    net->channel_id = ( pat_num << 16 ) | channel_num;
		    net->synth_channel = pat_info->channel_status[ channel_num ] & 127;
		    net->sample_offset = (ulong)ctl_val * 256;
		    net->items[ synth_num ].synth( 
			net->items[ synth_num ].data_ptr,
			synth_num,
			0, 0, 0,
			COMMAND_SET_SAMPLE_OFFSET,
			net );
		}
		break;
	    case 0xA:
		//Slide volume up/down:
		if( eff )
		{
		    if( ctl_val & 0xFF00 ) eff->vel_speed = ctl_val >> 8;
		    if( ctl_val & 0xFF ) eff->vel_speed = -( ctl_val & 0xFF );
		}
		break;
	    case 0xF:
		//Set speed/bpm:
		if( ctl_val < 32 ) { s->speed = ctl_val; if( s->speed <= 1 ) s->speed = 1; }
		    else { s->bpm = ctl_val; }
		break;
	}
    }
}

void sunvox_handle_std_effects( 
    psynth_net *net,
    int pat_num,
    int playing_pat_num,
    int channel_num,
    sunvox_engine *s )
{
    sunvox_std_eff *eff = 0;

    sunvox_pattern_info *pat_info;
    if( pat_num >= 0 ) 
    {
	pat_info = &s->pats_info[ pat_num ];
    }
    else
    {
	//pat_num == -1. It's command from user.
	return;
    }

    eff = &s->std_eff[ pat_info->std_eff_ptr * MAX_PATTERN_CHANNELS + channel_num ];

    int synth_num = pat_info->channel_synth[ channel_num ];
    if( synth_num >= net->items_num ) return;

    int period;
    int period_changed = 0;

    if( eff->flags & EFF_FLAG_TONE_PORTA )
    {
        if( eff->cur_period < eff->target_period ) 
        {
    	    eff->cur_period += eff->porta_speed;
	    if( eff->cur_period > eff->target_period ) eff->cur_period = eff->target_period;
	}
	else if( eff->cur_period > eff->target_period ) 
	{
	    eff->cur_period -= eff->porta_speed;
	    if( eff->cur_period < eff->target_period ) eff->cur_period = eff->target_period;
	}
	period = eff->cur_period;
	period_changed = 1;
    }
    
    if( eff->arpeggio )
    {
	period = eff->cur_period;
	switch( s->speed_counter % 3 )
	{
	    case 1: period -= ( ( eff->arpeggio >> 8 ) & 255 ) * 256; break;
	    case 2: period -= ( eff->arpeggio & 255 ) * 256; break;
	}
	eff->flags |= EFF_FLAG_ARPEGGIO_IN_PREVIOUS_TICK;
	period_changed = 1;
    }
    else if( eff->flags & EFF_FLAG_ARPEGGIO_IN_PREVIOUS_TICK )
    {
	eff->flags &= ~( EFF_FLAG_ARPEGGIO_IN_PREVIOUS_TICK );
	period = eff->cur_period;
	period_changed = 1;
    }

    if( period_changed )
    {
	if( period > 7680 * 4 ) period = 7680 * 4;
	net->period_ptr = period;
	if( net->items[ synth_num ].synth )
	{
	    net->synth_channel = pat_info->channel_status[ channel_num ] & 127;
	    net->channel_id = ( pat_num << 16 ) | channel_num;
	    net->items[ synth_num ].synth( 
		net->items[ synth_num ].data_ptr,
		synth_num,
		0, 0, 0,
		COMMAND_SET_FREQ,
		net );
	}
    }

    if( eff->vel_speed )
    {
	eff->cur_vel += eff->vel_speed;
	if( eff->cur_vel < 0 ) eff->cur_vel = 0;
	if( eff->cur_vel > 256 ) eff->cur_vel = 256;
	if( net->items[ synth_num ].synth )
	{
	    net->synth_channel = pat_info->channel_status[ channel_num ] & 127;
	    net->channel_id = ( pat_num << 16 ) | channel_num;
	    net->velocity = eff->cur_vel;
	    net->items[ synth_num ].synth( 
		net->items[ synth_num ].data_ptr,
		synth_num,
		0, 0, 0,
		COMMAND_SET_VELOCITY,
		net );
	}
    }
}

void sunvox_send_user_command( sunvox_note *snote, int channel_num, sunvox_engine *s )
{
    int wp = s->user_wp;
    int rp = s->user_rp;
    if( rp - wp == 1 ) return; //No space for new command
    mem_copy( &s->user_commands[ wp ], snote, sizeof( sunvox_note ) );
    s->user_commands_channel_num[ wp ] = channel_num;
    wp++;
    wp &= MAX_USER_COMMANDS - 1;
    s->user_wp = wp;
}

//Buffer types:
// 0 - 16 bits (int)
// 1 - 32 bits (float)
void sunvox_render_piece_of_sound( 
    int buffer_type,
    void *buffer,
    int frames,
    int channels, 
    int freq, 
    ticks_t out_time,
    sunvox_engine *s )
{
    if( s == 0 ) return;
    if( s->net == 0 ) return;
    if( s->initialized == 0 ) return;

    int ptr = 0;
    int one_tick = 0;

    psynth_cpu_usage_clean( s->net );

//##########################################
//######## [ Visualization frames ] ########
//##########################################
#ifndef PALMOS
    s->f_current_buffer = ( s->f_current_buffer + 1 ) & SUNVOX_F_BUFFERS_MASK;
    int f_current_buffer = s->f_current_buffer;
    s->f_buffer_start_time[ f_current_buffer ] = out_time;
    s->f_buffer_size[ f_current_buffer ] = 0;
#endif
//##########################################
//##########################################
//##########################################
  
    //Get one tick size (one sample size = 256):
    one_tick = ( ( ( freq * 60 ) << 8 ) / s->bpm ) / 24;
    s->net->tick_size = one_tick;
    s->net->ticks_per_line = s->speed;
    //Main loop (render pieces of sound):
    while( 1 )
    {
	//Get size of current piece:
	int size = frames - ptr;
	if( size > PSYNTH_BUFFER_SIZE ) size = PSYNTH_BUFFER_SIZE;
	if( size > ( one_tick - s->tick_counter ) / 256 ) size = ( one_tick - s->tick_counter ) / 256;
	if( ( one_tick - s->tick_counter ) & 255 ) size++; //size correction
	if( size > frames - ptr ) size = frames - ptr;
	if( size < 0 ) size = 0;
	if( size > 0 )
	{
	    //Render piece of sound:
	    psynth_render_clear( size, s->net );
	    psynth_render( 0, size, s->net );
	    for( int ch = 0; ch < s->net->items[ 0 ].input_channels; ch++ )
	    {
		STYPE *chan = s->net->items[ 0 ].channels_in[ ch ];
		if( buffer_type == 0 )
		{
		    // 16 bits (int)
		    signed short *output = (signed short*)buffer;
		    int i2 = ( ptr * channels ) + ch;
		    for( int i = 0; i < size; i++ )
		    {
			signed short result;
			STYPE_TO_INT16( result, chan[ i ] );
			output[ i2 ] = result;
			i2 += channels;
		    }
		}
		else if( buffer_type == 1 )
		{
		    // 32 bits (float)
		    float *output = (float*)buffer;
		    int i2 = ( ptr * channels ) + ch;
		    for( int i = 0; i < size; i++ )
		    {
			float result;
			STYPE_TO_FLOAT( result, chan[ i ] );
			output[ i2 ] = result;
			i2 += channels;
		    }
		}
	    }
	}
//##########################################
//######## [ Visualization frames ] ########
//##########################################
#ifdef PALMOS
	s->f_lines[ 0 ] = s->time_counter;
	if( buffer_type == 0 )
	{
	    // 16 bits (int)
	    int val_l;
	    int val_r;
	    signed short *output = (signed short*)buffer;
	    val_l = output[ 0 ];
	    if( channels > 1 )
	        val_r = output[ 1 ];
	    else
		val_r = val_l;
    	    if( val_l < 0 ) val_l = -val_l;
	    if( val_r < 0 ) val_r = -val_r;
	    if( val_l > 32767 ) val_l = 32767;
	    if( val_r > 32767 ) val_r = 32767;
	    s->f_volumes_l[ 0 ] = val_l >> 7;
	    s->f_volumes_r[ 0 ] = val_r >> 7;
	}
#else
	int new_size = ( ( ( ( ptr + size ) * 256 ) / freq ) * SUNVOX_F_BUFFER_SIZE ) / 256;
	if( new_size > SUNVOX_F_BUFFER_SIZE ) new_size = SUNVOX_F_BUFFER_SIZE;
	int f_off = SUNVOX_F_BUFFER_SIZE * f_current_buffer;
	int ptr2 = ptr << 8;
	int ptr2_step = ( freq << 8 ) / SUNVOX_F_BUFFER_SIZE;
	for( int fp = s->f_buffer_size[ f_current_buffer ]; fp < new_size; fp ++ )
	{
	    s->f_lines[ f_off + fp ] = s->time_counter;
	    if( buffer_type == 0 )
	    {
		// 16 bits (int)
		int val_l;
		int val_r;
		signed short *output = (signed short*)buffer;
		val_l = output[ ( (ptr2>>8) * channels ) + 0 ];
		if( channels > 1 )
		    val_r = output[ ( (ptr2>>8) * channels ) + 1 ];
		else
		    val_r = val_l;
		if( val_l < 0 ) val_l = -val_l;
		if( val_r < 0 ) val_r = -val_r;
		if( val_l > 32767 ) val_l = 32767;
		if( val_r > 32767 ) val_r = 32767;
		s->f_volumes_l[ f_off + fp ] = val_l >> 7;
		s->f_volumes_r[ f_off + fp ] = val_r >> 7;
	    }
	    if( buffer_type == 1 )
	    {
		// 32 bits (float)
		float *output = (float*)buffer;
		int val_l;
		int val_r;
		val_l = (int)( output[ ( (ptr2>>8) * channels ) + 0 ] * 32767 );
		if( channels > 1 )
		    val_r = (int)( output[ ( (ptr2>>8) * channels ) + 1 ] * 32767 );
		else
		    val_r = val_l;
		if( val_l < 0 ) val_l = -val_l;
		if( val_r < 0 ) val_r = -val_r;
		if( val_l > 32767 ) val_l = 32767;
		if( val_r > 32767 ) val_r = 32767;
		s->f_volumes_l[ f_off + fp ] = val_l >> 7;
		s->f_volumes_r[ f_off + fp ] = val_r >> 7;
#ifdef SUNVOX_F_SYNTHS
		for( int fs = 0; fs < s->net->items_num; fs++ )
		{
		    psynth_net_item *synth = &s->net->items[ fs ];
		    int f_synth_off = fs * SUNVOX_F_BUFFER_SIZE * SUNVOX_F_BUFFERS;
		    int v_i;
		    if( ( synth->flags & PSYNTH_FLAG_EXISTS ) && synth->channels_out[ 0 ] )
		    {
			STYPE v = synth->channels_out[ 0 ][ ( ptr2 >> 8 ) - ptr ];
			v *= 32768;
			v_i = (int)v;
			if( v_i < 0 ) v_i = -v_i;
		    }
		    else
		    {
		        v_i = 0;
		    }
		    if( v_i < s->f_synth_prev_volume[ fs ] )
		    {
			if( v_i <= 1024 )
			    v_i = s->f_synth_prev_volume[ fs ] - 300;
			else
			    v_i = s->f_synth_prev_volume[ fs ] - 1024;
		    }
    		    s->f_synth_volumes[ f_off + fp + f_synth_off ] = v_i;
		    s->f_synth_prev_volume[ fs ] = v_i;
		}
#endif
	    }
	    ptr2 += ptr2_step;
	}
	s->f_buffer_size[ f_current_buffer ] = new_size;
#endif
//##########################################
//##########################################
//##########################################
	ptr += size;
	s->tick_counter += 256 * size;
	//Handle user commands:
	int jump_to_start_of_main_loop = 0;
	while( s->user_rp != s->user_wp )
	{
	    sunvox_note *snote = &s->user_commands[ s->user_rp ];
	    sunvox_handle_command( 
		snote, s->net, -1, -1,
		s->user_commands_channel_num[ s->user_rp ],
		s );
	    if( snote->note == NOTECMD_STOP )
	    {
		s->playing = 0;
		s->single_pattern_play = -1;
		snote->note = NOTECMD_ALL_NOTES_OFF;
		sunvox_handle_command( snote, s->net, -1, -1, 0, s );
		for( int i = 0; i < MAX_PATTERN_CHANNELS; i++ ) s->user_pat_info.channel_status[ i ] = 255;
		for( int i = 0; i < MAX_PLAYING_PATS; i++ ) 
		    clean_std_effects_for_playing_pattern( i, s );
	    }
	    if( snote->note == NOTECMD_PLAY )
	    {
		s->tick_counter = one_tick;
		s->speed_counter = s->speed - 1;
		s->playing = 1;
		jump_to_start_of_main_loop = 1;
	    }
	    int rp = ( s->user_rp + 1 ) & ( MAX_USER_COMMANDS - 1 );
	    s->user_rp = rp;
	    if( jump_to_start_of_main_loop ) break;
	}
	if( jump_to_start_of_main_loop ) continue;
	if( s->tick_counter >= one_tick && s->playing == 1 )
	{
	    //#####################################################################
	    // Tick handling: #####################################################
	    //#####################################################################
	    
	    s->tick_counter -= one_tick;
	    s->speed_counter++;

	    if( s->speed_counter >= s->speed )
	    {
		//New line:
		s->time_counter ++;
		s->speed_counter = 0;

		int rewind = -999999;

		//Song finished?
		if( s->time_counter >= s->song_lines )
		{
		    if( s->stop_at_the_end_of_song )
		    {
			s->end_of_song = 1;
			s->playing = 0;
			s->single_pattern_play = -1;
			sunvox_note temp_note;
			temp_note.note = NOTECMD_ALL_NOTES_OFF;
			temp_note.vel = 0;
			temp_note.synth = 0;
			temp_note.ctl = 0;
			temp_note.ctl_val = 0;
			sunvox_handle_command( &temp_note, s->net, -1, -1, 0, s );
			for( int i = 0; i < MAX_PATTERN_CHANNELS; i++ ) s->user_pat_info.channel_status[ i ] = 255;
			for( int i = 0; i < MAX_PLAYING_PATS; i++ ) 
			    clean_std_effects_for_playing_pattern( i, s );
			goto end_of_tick_handling;
		    }
		    //Rewind it to start point:
		    rewind = 0;
		}

		//Single pattern play?
		if( s->single_pattern_play >= 0 )
		{
		    if( (unsigned)s->single_pattern_play < (unsigned)s->pats_num &&
			s->pats[ s->single_pattern_play ] )
		    {
			if( s->time_counter >= s->pats_info[ s->single_pattern_play ].x + s->pats[ s->single_pattern_play ]->lines )
			{
			    rewind = s->pats_info[ s->single_pattern_play ].x;
			}
		    }
		}

		if( rewind != -999999 )
		{
		    //Notes off:
		    /*sunvox_note snote;
		    snote.vel = 0;
		    snote.ctl = 0;
		    snote.ctl_val = 0;
		    snote.note = NOTECMD_ALL_NOTES_OFF;
		    sunvox_handle_command( &snote, s->net, -1, -1, 0, s );*/
		    for( int i = 0; i < MAX_PLAYING_PATS; i++ )
		    {
			if( s->cur_playing_pats[ i ] == -1 ) break;
			if( (unsigned)s->cur_playing_pats[ i ] < (unsigned)s->sorted_pats_num )
			{
			    int pat_num = s->sorted_pats[ s->cur_playing_pats[ i ] ];
			    if( pat_num < s->pats_num && s->pats[ pat_num ] )
			    {
				int pat_num = s->sorted_pats[ s->cur_playing_pats[ i ] ];
				sunvox_pattern_info *pat_info = &s->pats_info[ pat_num ];
				for( int a = 0; a < MAX_PATTERN_CHANNELS; a++ )
				{
				    if( pat_info->channel_status[ a ] < 128 )
				    {
					int snum = pat_info->channel_synth[ a ];
					if( snum >= 0 && snum < s->net->items_num )
					{
					    psynth_net_item *synth = &s->net->items[ snum ];
					    if( synth->synth )
					    {
						s->net->channel_id = ( pat_num << 16 ) | a;
						s->net->synth_channel = pat_info->channel_status[ a ] & 127;
					        synth->synth( 
						    synth->data_ptr,
						    snum,
						    0, 0, 0,
						    COMMAND_NOTE_OFF,
						    s->net );
					    }
					    pat_info->channel_status[ a ] |= 128;
					}
				    }
				}
			    }
			}
		    }
		    //Rewind:
		    s->time_counter = rewind;
		    sunvox_select_current_playing_patterns( 0, s );
		    for( int i = 0; i < MAX_PLAYING_PATS; i++ ) 
			clean_std_effects_for_playing_pattern( i, s );
		}

		//Calculate new playing patterns:
		//1) Check current playing patterns:
		int p = 0;
		s->temp_pats[ 0 ] = -1;
		for( int i = 0; i < MAX_PLAYING_PATS; i++ )
		{
		    if( s->cur_playing_pats[ i ] == -1 ) break;
		    if( (unsigned)s->cur_playing_pats[ i ] < (unsigned)s->sorted_pats_num )
		    {
			int pat_num = s->sorted_pats[ s->cur_playing_pats[ i ] ];
			if( pat_num < s->pats_num && s->pats[ pat_num ] )
			{
			    if( s->time_counter >= s->pats_info[ pat_num ].x &&
				s->time_counter < s->pats_info[ pat_num ].x + s->pats[ pat_num ]->lines )
			    {
				//This pattern is still playing
				s->temp_pats[ p ] = s->cur_playing_pats[ i ];
				p++;
			    }
			    else
			    {
				//This pattern is not playing more:
				int end_pat_num = s->sorted_pats[ s->cur_playing_pats[ i ] ];
				//All notes off:
				sunvox_pattern_info *end_pat_info = &s->pats_info[ end_pat_num ];
				for( int a = 0; a < MAX_PATTERN_CHANNELS; a++ )
				{
				    if( end_pat_info->channel_status[ a ] < 128 )
				    {
					int snum = end_pat_info->channel_synth[ a ];
					if( snum >= 0 && snum < s->net->items_num )
					{
					    psynth_net_item *synth = &s->net->items[ snum ];
					    if( synth->synth )
					    {
						s->net->channel_id = ( end_pat_num << 16 ) | a;
						s->net->synth_channel = end_pat_info->channel_status[ a ] & 127;
						synth->synth( 
						    synth->data_ptr,
						    snum,
						    0, 0, 0,
						    COMMAND_NOTE_OFF,
						    s->net );
					    }
					    end_pat_info->channel_status[ a ] |= 128;
					}
				    }
				}
				//Free structure with effects for this pattern:
				s->std_eff_busy[ end_pat_info->std_eff_ptr ] = 0;
			    }
			}
		    }
		    s->last_sort_pat = s->cur_playing_pats[ i ];
		}
		//2) Check intersection with another patterns (next on timeline):
		int search_again = 1;
		while( search_again == 1 )
		{
		    search_again = 0;
		    if( s->last_sort_pat < s->sorted_pats_num - 1 )
		    {
			int next_sort_pat = s->last_sort_pat + 1;
			if( s->pats[ s->sorted_pats[ next_sort_pat ] ] )
			{
			    int next_time = s->pats_info[ s->sorted_pats[ next_sort_pat ] ].x;
			    if( next_time == s->time_counter )
			    {
				//Open new patterns:
				int std_eff_ptr = 0;
				for( ; next_sort_pat < s->sorted_pats_num ; next_sort_pat++ )
				{
				    sunvox_pattern *pat = s->pats[ s->sorted_pats[ next_sort_pat ] ];
				    sunvox_pattern_info *pat_info = &s->pats_info[ s->sorted_pats[ next_sort_pat ] ];
				    if( pat_info->x != next_time ) break;
				    s->temp_pats[ p ] = next_sort_pat;
				    //Get structure with std effects for this pattern:
				    for( int sp = 0; sp < MAX_PLAYING_PATS; sp++ )
				    {
					if( s->std_eff_busy[ std_eff_ptr ] == 0 )
					{
					    s->std_eff_busy[ std_eff_ptr ] = 1;
					    pat_info->std_eff_ptr = std_eff_ptr;
					    break;
					}
					std_eff_ptr++;
					if( std_eff_ptr >= MAX_PLAYING_PATS ) std_eff_ptr = 0;
				    }
				    p++;
				}
			    }
			    else
			    {
				if( next_time < s->time_counter )
				{
				    //Hmmm.. Next pattern is before cursor. Try to search again 
				    search_again = 1;
				    s->last_sort_pat++;
				}
			    }
			}
		    }
		}
		//3) Copy new playing patterns to working buffer:
		for( int i = 0; i < p; i++ )
		{
		    s->cur_playing_pats[ i ] = s->temp_pats[ i ];
		    //Set x position for cursor in each playing pattern:
		    int pat_num = s->sorted_pats[ s->temp_pats[ i ] ];
		    sunvox_pattern *pat = s->pats[ pat_num ];
		    sunvox_pattern_info *pat_info = &s->pats_info[ pat_num ];
		    pat_info->cur_time_ptr = s->time_counter - pat_info->x;
		    int pat_ptr = pat_info->cur_time_ptr * pat->data_xsize;
		    for( int ch = 0; ch < pat->channels; ch++ )
		    {
			sunvox_handle_command( &pat->data[ pat_ptr ], s->net, pat_num, i, ch, s );
			pat_ptr++;
		    }
		}
		s->cur_playing_pats[ p ] = -1;

		//May be BPM was changed. Lets reinit internal variables:
		one_tick = ( ( ( freq * 60 ) << 8 ) / s->bpm ) / 24;
		s->net->tick_size = one_tick;
		s->net->ticks_per_line = s->speed;
	    } //End of line handling

	    //Handle standart effects:
	    for( int i = 0; i < MAX_PLAYING_PATS; i++ )
	    {
		int sp = s->cur_playing_pats[ i ];
		if( sp == -1 ) break;
		int pat_num = s->sorted_pats[ s->cur_playing_pats[ i ] ];
		sunvox_pattern *pat = s->pats[ pat_num ];
		sunvox_pattern_info *pat_info = &s->pats_info[ pat_num ];
		for( int ch = 0; ch < pat->channels; ch++ )
		{
		    sunvox_handle_std_effects( s->net, pat_num, i, ch, s );
		}
	    }

	    //#####################################################################
	    // End of tick handling ###############################################
	    //#####################################################################
	}

end_of_tick_handling:

	if( s->tick_counter >= one_tick && s->playing == 0 )
	{
	    s->tick_counter -= one_tick;
	}
	if( ptr >= frames )
	{
	    //Out of buffer space:
	    break;
	}
    } //...end of main loop.

    psynth_cpu_usage_recalc( s->net );
}

int sunvox_frames_get_value( int channel, sunvox_engine *s )
{
    int buf = 0;

#ifdef PALMOS
    ulong frame = 0;
#else
    ticks_t t = time_ticks_per_second() * 30;
    ticks_t t_relative_point = time_ticks() - t;
    ticks_t t_buf_start = 0;
    for( int i = 0; i < SUNVOX_F_BUFFERS; i++ )
    {
	int t_buf_start2 = s->f_buffer_start_time[ i ] - t_relative_point;
	if( (int)t >= t_buf_start2 && t_buf_start2 > (int)t_buf_start )
	{
	    t_buf_start = t_buf_start2;
	    buf = i;
	}
    }
    ulong frame = ( ( t - t_buf_start ) * SUNVOX_F_BUFFER_SIZE ) / time_ticks_per_second();
    ulong size = s->f_buffer_size[ buf ];
    if( size > SUNVOX_F_BUFFER_SIZE ) size = SUNVOX_F_BUFFER_SIZE;
    if( frame > 0 && frame >= (unsigned)size ) frame = size - 1;
#endif

    switch( channel )
    {
	case SUNVOX_F_CHANNEL_LINES:
	    return s->f_lines[ buf * SUNVOX_F_BUFFER_SIZE + frame ];
	    break;
	case SUNVOX_F_CHANNEL_VOL0:
	    return s->f_volumes_l[ buf * SUNVOX_F_BUFFER_SIZE + frame ];
	    break;
	case SUNVOX_F_CHANNEL_VOL1:
	    return s->f_volumes_r[ buf * SUNVOX_F_BUFFER_SIZE + frame ];
	    break;
	default:
#ifdef SUNVOX_F_SYNTHS	
	    if( channel >= SUNVOX_F_CHANNEL_SYNTH_VOL )
	    {
		int off = ( channel - SUNVOX_F_CHANNEL_SYNTH_VOL ) * SUNVOX_F_BUFFER_SIZE * SUNVOX_F_BUFFERS;
		return s->f_synth_volumes[ buf * SUNVOX_F_BUFFER_SIZE + frame + off ];
	    }
#endif
	    break;
    }
    
    return 0;
}
