/*
    PsyTexx: xm_song.cpp. Functions for working with song
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

void close_song(xm_struct *xm)
{
    module *song = xm->song;
    if( song ) mem_free( song );
    xm->song = 0;
}

void clear_song(xm_struct *xm)
{
    ulong a;
    //clear patterns and instruments:
    for(a=0;a<256;a++) clear_pattern((uint16)a,xm);
    for(a=0;a<128;a++) clear_instrument((uint16)a,xm);

    module *song = xm->song;
    char *temp_s = "Extended Module: ";
    int tp;
    for( tp = 0; tp < 17; tp++ ) song->id_text[ tp ] = temp_s[ tp ];
    song->reserved1 = 0x1A;
    temp_s = "PsyTexx2            ";
    for( tp = 0; tp < 20; tp++ ) song->tracker_name[ tp ] = temp_s[ tp ];
    song->version = 2;
    song->header_size = 0x114;
    song->length = 1;
    song->patterns_num = 1;
    song->patterntable[ 0 ] = 0;
    song->instruments_num = 0;
    song->restart_position = 0;
    xm->tablepos = 0;
    xm->patternpos = 0;
}


void new_song( xm_struct *xm )
{
    ulong a;
    module *song = xm->song;
    
    //create new song:
    if(song != 0){
	clear_song(xm);
	mem_free(song);
	
	song = (module*) mem_new( 0, sizeof( module ), "song", 0 );
	xm->song = song;
    }else{ //for FIRST song creation:
	song = (module*) mem_new( 0, sizeof( module ), "song", 0 );
	xm->song = song;
	//clear new song:
	for( a = 0; a < 128; a++ ) song->instruments[ a ] = 0;
	for( a = 0; a < 256; a++ ) song->patterns[ a ] = 0;
	//===============
    }
    
    clear_song(xm);
}

void create_silent_song(xm_struct *xm)
{
    module *song = xm->song;
    //Create simple silent song:
    song->channels = 4;
    new_pattern( 0, 64, 4, xm );
    clean_pattern( 0, xm );
}

void set_bpm( long bpm, xm_struct *xm )
{
    xm->bpm = bpm;
    xm->onetick = ( ( xm->freq * 25 ) << 8 ) / ( xm->bpm * 10 );
    xm->patternticks = xm->onetick + 1;
}

void set_speed( long speed, xm_struct *xm )
{
    xm->speed = speed;
    xm->sp = xm->speed;
}
