/*
    PsyTexx: xm_pattern.cpp. Functions for working with a patterns
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

void new_pattern( uint16 num,
                  uint16 rows,
		  uint16 channels,
		  xm_struct *xm )
{
    module *song = xm->song;
    pattern *pat;

    //create pattern structure:
    pat = (pattern*) mem_new( 0, sizeof(pattern), "pattern", num );
    
    //save created structure pointer:
    song->patterns[ num ] = pat;
    
    pat->header_length = 9; //pattern header length
    pat->reserved5 = 0;     //non packed
    pat->real_rows = pat->rows = rows;       //number of rows in new pattern
    pat->real_channels = pat->channels = channels;
    pat->data_size = rows * channels * sizeof(xmnote); //physical size of pattern

    //Create memory block for the current pattern:
    pat->pattern_data = (xmnote*) mem_new( HEAP_STORAGE, pat->data_size, "pattern data", num );
}

void clear_pattern( uint16 num, xm_struct *xm )
{
    module *song = xm->song;
    pattern *pat = song->patterns[ num ];
    xmnote *pat_data;
    
    if( pat != 0 )
    {
	//clear pattern data:
        pat_data = pat->pattern_data;
	if( pat_data != 0 )
	{
	    mem_free( pat_data );
	}
	pat->pattern_data = 0;
	//===================
	mem_free( pat );
    }
    song->patterns[num] = 0;
}

void clear_patterns(xm_struct *xm)
{
    ulong a;
    //clear patterns:
    for( a=0; a < 256; a++ ) clear_pattern( (uint16)a, xm );
		
    module *song = xm->song;
    song->length = 1;
    song->patterns_num = 1;
    song->patterntable[ 0 ] = 0;
    song->restart_position = 0;
    xm->tablepos = 0;
    xm->patternpos = 0;
}

void clean_pattern( uint16 num, xm_struct *xm )
{
    module *song = xm->song;
    pattern *pat = song->patterns[ num ];
    uchar *pat_data;
    ulong a, rows, channels;
    
    if( pat != 0 )
    {
	//clean pattern data:
        pat_data = (uchar*) pat->pattern_data;
	if( pat_data != 0 )
	{
	    rows = pat->rows;
	    channels = song->channels;
	    for( a = 0; a < rows * channels * sizeof(xmnote); a++ )
	    {
		pat_data[ a ] = 0;
	    }
	}
	//===================
    }
}
