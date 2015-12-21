/*
    PsyTexx: xm_instrum.cpp. Functions for working with instruments
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

void new_instrument( uint16 num, 
                     char *name, 
                     uint16 samples, 
                     xm_struct *xm )
{
    module *song = xm->song;
    instrument *ins;
    int a;
    
    ins = (instrument*) mem_new( HEAP_STORAGE, sizeof(instrument), "instrument", num );
    mem_set( ins, sizeof(instrument), 0 );
    
    //save info:
    for(a=0;a<22;a++){
	ins->name[a] = name[a];
	if(name[a]==0) break;
    }
    
    ins->samples_num = samples;
    
    //add NULL samples:
    for(a=0;a<16;a++){
	ins->samples[a] = 0;
    }
    
    //Set some default info about instrument:
    ins->volume_points_num = 1;
    ins->panning_points_num = 1;
    
    //save created instrument:
    song->instruments[num] = ins;
}

void clear_instrument( uint16 num, xm_struct *xm )
{
    uint16 a;
    module *song = xm->song;
    instrument *ins = song->instruments[ num ];
    
    if(ins!=0){
	//clear samples:
	for(a=0;a<16;a++){
	    if( ins->samples[a] != 0 ){
		clear_sample(a,num,xm);
	    }
	}
	//clear instrument:
	mem_free(ins);
	song->instruments[num] = 0;
    }
}

void clear_instruments( xm_struct *xm )
{
    ulong a;
    //clear instruments:
    for( a = 0; a < 128; a++ ) clear_instrument( (uint16)a, xm );
    xm->song->instruments_num = 0;
}

void save_instrument( uint16 num, char *filename, xm_struct *xm )
{
    module *song = xm->song;
    if( song )
    {
	if( song->instruments[ num ] )
	{
	    instrument *ins = song->instruments[ num ];
	    V3_FILE f = v3_open( filename, "wb" );
	    if( f )
	    {
		v3_write( (void*)"Extended Instrument: ", 21, 1, f );
		v3_write( &ins->name, 22, 1, f ); // Instrument name
		
		//Save version:
		char temp[ 30 ];
		temp[ 21 ] = 0x50;
		temp[ 22 ] = 0x50;
		v3_write( temp, 23, 1, f );

		v3_write( ins->sample_number, 208, 1, f ); //Instrument info

		//Extended info:
		v3_write( &ins->volume, 2 + EXT_INST_BYTES, 1, f );
		v3_write( temp, 22 - ( 2 + EXT_INST_BYTES ), 1, f ); //Empty
		
		v3_write( &ins->samples_num, 2, 1, f ); //Samples number

		//Save samples:
		int s;
		sample *smp;
		for( s = 0; s < ins->samples_num; s++ )
		{
		    smp = ins->samples[ s ];
		    if( smp->type & 16 )
		    {
			frames2bytes( smp, xm );
		    }
		    v3_write( smp, 40, 1, f ); //Save sample info
		}

		//Save samples data:
		signed short *s_data; //sample data
		char *cs_data;        //char 8 bit sample data
		int b;
		for( b = 0; b < ins->samples_num; b++ )
		{
		    smp = ins->samples[ b ];
        	    
		    if( smp->length == 0 ) continue;
        	    
		    if( smp->type & 16 )
		    { //16bit sample:
			long len = smp->length;
			short old_s = 0;
			short old_s2 = 0;
			short new_s = 0;
			s_data = (signed short*) smp->data;
			//convert sample:
			long sp;
			for( sp = 0 ; sp < len/2; sp++ )
			{
			    old_s2 = s_data[ sp ];
			    s_data[sp] = old_s2 - old_s;
			    s_data[sp] = s_data[ sp ];
			    old_s = old_s2;
			}
			v3_write( s_data, len, 1, f );
			//convert sample to normal form:
			old_s = 0;
			for( sp = 0; sp < len/2; sp++ )
			{
			    new_s = s_data[ sp ] + old_s;
			    s_data[ sp ] = new_s;
			    old_s = new_s;
			}
			//convert sample info:
			bytes2frames( smp, xm );
		    }
		    else
		    { //8bit sample:
			long len = smp->length;
			signed char c_old_s = 0;
			signed char c_old_s2 = 0;
			signed char c_new_s = 0;
			cs_data = (char*) smp->data;
			//convert sample:
			long sp;
			for( sp = 0 ; sp < len; sp++ )
			{
			    c_old_s2 = cs_data[sp];
			    cs_data[sp] = c_old_s2 - c_old_s;
			    c_old_s = c_old_s2;
			}
			v3_write( cs_data, len, 1, f );
			//convert sample to normal form:
			c_old_s = 0;
			for( sp = 0; sp < len; sp++ )
			{
			    c_new_s = cs_data[sp] + c_old_s;
			    cs_data[sp] = c_new_s;
			    c_old_s = c_new_s;
			}
		    }
		}

		v3_close( f );
	    }
	}
    }
}
