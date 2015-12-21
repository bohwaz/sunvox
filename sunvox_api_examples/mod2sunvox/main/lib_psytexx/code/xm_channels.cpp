/*
    PsyTexx: xm_channels.cpp. Functions for working with channels
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

#define GET_FREQ(per)  ( g_linear_tab[ per % 768 ] >> (per/768) )
#define GET_DELTA(f)   ( ( ( f << 10 ) / xm->freq ) << ( XM_PREC - 10 ) )

void clear_channels(xm_struct *xm)
{
    long a;
    
    for( a = 0; a < MAX_REAL_CHANNELS; a++ )
    {
	if( xm->channels[ a ] != 0 ) 
	{
	    mem_free( (void*) xm->channels[ a ] );
	    xm->channels[ a ] = 0;
	}
    }
}

void new_channels( long number_of_channels, xm_struct *xm )
{
    long a;
    channel *c;
    
    xm->chans = number_of_channels;

    for( a = 0; a < number_of_channels; a++ )
    {
	if( xm->channels[ a ] == 0 )
	{
	    c = (channel*) mem_new( 0, sizeof(channel), "channel", a );
	    mem_set( c, sizeof(channel), 0 );
	    c->enable = 1;
	    c->recordable = 1;

	    xm->channels[ a ] = c;
	}
    }
}

void clean_channels( xm_struct *xm )
{
    long number_of_channels = xm->chans;
    long a;
    channel* c;

    for( a = 0; a < number_of_channels; a++ )
    {
	if( xm->channels[ a ] != 0 )
	{
	    c = xm->channels[ a ];
	    long enable = c->enable;
	    mem_set( c, sizeof(channel), 0 );
	    c->enable = enable;
	    c->recordable = 1;
	}
    }
}
