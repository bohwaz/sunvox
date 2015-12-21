/*
    PsyTexx: xm_sample.cpp. Functions for working with samples
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

void new_sample( uint16 num, 
                 uint16 ins_num, 
                 const char *name,
		 long length,      /*length in bytes*/
		 long type,
                 xm_struct *xm )
{
    module *song = xm->song;
    instrument *ins = song->instruments[ ins_num ];
    sample *smp; //current sample
    signed short *data;
    int a;
    long created_size;

    smp = (sample*) mem_new( 0, sizeof(sample), "sample", ins_num+(num<<8) );
    
    //save sample parameters:
    for( a = 0 ; a < 22 ; a++ )
    {
	smp->name[a] = name[a];
	if( name[ a ] == 0 ) break;
    }
    
    //create NULL sample:
    smp->data = 0;

    //create sample data:
    dprint( "SMP: Creating the sample data\n" );
    if( length ) 
    {
	data = (signed short*) mem_new( 1, length, "sample data", ins_num+(num<<8) );
	smp->data = (signed short*) data;
	created_size = length;
    }else{ //for NULL sample
	created_size = 0;
    }
    
    smp->length = created_size;
    smp->type = (uchar) type;

    smp->volume = 0x40;
    smp->panning = 0x80;
    smp->relative_note = 0;
    smp->finetune = 0;
    smp->reppnt = 0;
    smp->replen = 0;
    
    //save created sample:
    ins->samples[num] = smp;
    dprint( "SMP: Sample was created\n" );
}

void clear_sample(uint16 num, uint16 ins_num, xm_struct *xm)
{
    module *song = xm->song;
    instrument *ins = song->instruments[ ins_num ];
    sample *smp;
    signed short *smp_data;
    
    if( ins != 0 )
    {
	smp = ins->samples[ num ];
	if( smp != 0 )
	{
	    //clear sample data:
	    smp_data = (signed short*) smp->data;
	    if( smp_data != 0 )
	    {
	        mem_free( smp_data );
	    }
	    smp->data = 0;
	    //==================
	    mem_free( smp );
	    ins->samples[num] = 0;
	}
    }
}

void bytes2frames( sample *smp, xm_struct *xm )
{
    long bits = 8;
    long channels = 1;
    if( smp->type & 16 ) bits = 16;
    if( smp->type & 64 ) channels = 2;
    smp->length = smp->length / ( ( bits / 8 ) * channels );
    smp->reppnt = smp->reppnt / ( ( bits / 8 ) * channels );
    smp->replen = smp->replen / ( ( bits / 8 ) * channels );
}

void frames2bytes( sample *smp, xm_struct *xm )
{
    long bits = 8;
    long channels = 1;
    if( smp->type & 16 ) bits = 16;
    if( smp->type & 64 ) channels = 2;
    smp->length = smp->length * ( ( bits / 8 ) * channels );
    smp->reppnt = smp->reppnt * ( ( bits / 8 ) * channels );
    smp->replen = smp->replen * ( ( bits / 8 ) * channels );
}

