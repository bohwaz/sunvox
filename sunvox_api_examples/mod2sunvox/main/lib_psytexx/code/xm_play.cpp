/*
    PsyTexx: xm_play.cpp. Main sound playing function
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
#include "filesystem/v3nus_fs.h"
#include "memory/memory.h"
#include <math.h>

#define GET_FREQ(per)  ( U->g_linear_tab[per%768] >> (per/768) )
#define GET_DELTA(f)   ( ( ( f << 10 ) / U->freq ) << ( XM_PREC - 10 ) )

int xm_callback( void *_buffer,
                 long frameCount,
		 void *userData )
{
    //main variables: ============
    signed short *buffer = (signed short*) _buffer;
    long buffer_size = frameCount;
    xm_struct *U;
    //============================
    //temp variables: ============
    long temp;
    long i2;           //current buffer pointer
    long i;            //current buffer pointer (for small block)
    long rest;         //rest size
    long size;
    long cc;           //current channel number
    channel *ch;       //current channel pointer
    sample *smp;       //current sample pointer
    long s_offset;     //sample data offset
    long left;         //left channel
    long right;        //right channel
    long s;            //current piece of sample (volume from -32767 to 32767)
    long sr;           //the same as above but for the next channel (right)
    signed short *pnt; //sample pointer (16 bit data)
    signed char *cpnt; //sample pointer (8 bit data)
    long reppnt;       //sample repeat pointer
    long replen;       //sample repeat length
    long res, res2;    //result volume
    pattern *pat;      //current pattern
    xmnote *nptr;      //pattern pointer
    xmnote *n;         //current note pointer
    long cur_tablepos;
    //interpolation variables:
    long in;           //interpolation scale
    long in2;          //interpolation scale
    long s2;           //next piece of sample
    long s_offset2;    //sample data offset (for interpolate)
    //frequency interpolation:
    long old_subtick_c;//start subtick count for current tick's piece
    long new_freq;     //new sample frequency
    //============================
    U = (xm_struct*) userData;
    long *buffer32 = U->buffer;
    
    
    if( U->song == 0 ) return 0; //If NULL song
    
    //clear buffer: ==============
#ifdef SLOWMODE
    for( i = 0; i < buffer_size * 2; i += 2 ) { buffer[i] = 0; buffer[i+1] = 0; }
#else
    for( i = 0; i < buffer_size * 2; i += 2 ) { buffer32[i] = 0; buffer32[i+1] = 0; }
#endif
    //============================
  
    //render piece of buffer: ====
    i2 = 0; rest = buffer_size;
    for( cc = 0; cc < MAX_REAL_CHANNELS; cc++ ) 
    { 
	U->channels[ cc ]->temp_lvalue = 0;
	U->channels[ cc ]->temp_rvalue = 0;
    }
new_tick:
    size = ( U->onetick - U->patternticks ) / 256; size++;
    if( size < 0 ) size = 0;
    if( size > rest ) size = rest;
    //frequency interpolation:****
    U->one_subtick = U->onetick >> 6;
    old_subtick_c = U->subtick_count;
    //****************************
    for( cc = 0; cc < U->song->channels * SUBCHANNELS; cc++ ) //For each real channel:
    {
	//Channel init:
	ch = U->channels[ cc ];
	if( ch == 0 ) continue;
	if( !ch->enable || !ch->smp || ch->paused ) continue;
	smp = ch->smp;
	if( !smp->data ) continue;
	reppnt = smp->reppnt;
	replen = smp->replen;
	U->subtick_count = old_subtick_c;
	for( i = (i2<<1); i < ( (i2+size) << 1 ); i += 2 )
	{
	    if( ch->ticks < 0 && !( smp->type & 2 ) ) { ch->ticks = 0; ch->ticks_p = 0; }
	    s_offset = ch->ticks;
	    if( s_offset >= ( reppnt + replen ) && replen )
	    { //handle loop:
		if( smp->type & 1 ) 
		{ //forward loop:
next_iteration:
		    ch->ticks -= replen;
		    s_offset = ch->ticks;
		    if( s_offset >= ( reppnt + replen ) ) goto next_iteration;
		}
		else
		if( smp->type & 2 ) 
		{ //ping-pong loop:
		    ch->back = 1;
		    SIGNED_SUB64( ch->ticks, ch->ticks_p, ch->delta, ch->delta_p );
		    s_offset = ch->ticks;
		}
	    }
	    if( ch->back ) 
	    { //ping-pong loop:
		if( s_offset < reppnt && ( smp->type & 2 ) ) 
		{
		    ch->back = 0;
		    SIGNED_ADD64( ch->ticks, ch->ticks_p, ch->delta, ch->delta_p );
		    s_offset = ch->ticks;
		}
	    }
	    if( s_offset >= (long)smp->length )
	    { //end of sample:
		ch->ticks = smp->length;
		ch->smp = 0;
		s = 0;
		sr = 0;
	    }else{ //get sample data: //##################################
#ifndef SLOWMODE
		if( U->sample_int ) 
		{
		    //interpolation: 
		    in = ch->ticks_p >> ( XM_PREC - INTERP_PREC ); in2 = ( 1 << INTERP_PREC ) - 1 - in;
		    if( s_offset < (long)smp->length - 1 ) s_offset2 = s_offset + 1; else s_offset2 = s_offset;
		    //loop correction: =====
		    if( replen )
		    {
			if( smp->type & 1 && s_offset >= ( reppnt + replen ) - 1 ) s_offset2 = reppnt; //for forward loop
			if( smp->type & 2 && s_offset >= ( reppnt + replen ) - 1 ) s_offset2 = s_offset; //for ping pong loop
		    }
		    //======================
		}
#endif
		if( smp->type & 16 ) 
		{ //16 bit data:
		    pnt = smp->data;
		    if( smp->type & 64 )
		    { //Stereo sample:
#ifndef SLOWMODE
			if( U->sample_int ) 
			{
			    s = pnt[ s_offset << 1 ]; s *= in2; //get data for left channel
			    s2 = pnt[ s_offset2 << 1 ]; s2 *= in;
			    s += s2; s >>= INTERP_PREC;
			    sr = pnt[ ( s_offset << 1 ) + 1 ]; sr *= in2; //get data for right channel
			    s2 = pnt[ ( s_offset2 << 1 ) + 1 ]; s2 *= in;
			    sr += s2; sr >>= INTERP_PREC;
			} else
#endif
			{ s = pnt[ s_offset << 1 ]; sr = pnt[ ( s_offset << 1 ) + 1 ]; } //get stereo-data without interpolation
			    
		    }
		    else
		    { //Mono sample:
#ifndef SLOWMODE
			if( U->sample_int ) 
			{
			    s = pnt[ s_offset ]; s *= in2;
			    s2 = pnt[ s_offset2 ]; s2 *= in;
			    s += s2; s >>= INTERP_PREC; sr = s;
			} else
#endif
			    s = sr = pnt[ s_offset ];
		    }
		}else{ //8 bit data:
		    cpnt = (signed char*) smp->data;
		    if( smp->type & 64 )
		    { //Stereo sample:
#ifndef SLOWMODE
			if( U->sample_int ) 
			{
			    s = cpnt[ s_offset << 1 ]; s <<= 8; s *= in2; //get data for left channel
			    s2 = cpnt[ s_offset2 << 1 ]; s2 <<= 8; s2 *= in;
			    s += s2; s >>= INTERP_PREC;
			    sr = cpnt[ ( s_offset << 1 ) + 1 ]; sr <<= 8; sr *= in2; //get data for right channel
			    s2 = cpnt[ ( s_offset2 << 1 ) + 1 ]; s2 <<= 8; s2 *= in;
			    sr += s2; s >>= INTERP_PREC;
			} else
#endif
			    { s = cpnt[ s_offset << 1 ]; s <<= 8; sr = cpnt[ ( s_offset << 1 ) + 1 ]; sr <<= 8; }
		    }
		    else
		    { //Mono sample:
#ifndef SLOWMODE
			if( U->sample_int ) 
			{
			    s = cpnt[ s_offset ]; s <<= 8; s *= in2;
			    s2 = cpnt[ s_offset2 ]; s2 <<= 8; s2 *= in;
			    s += s2; s >>= INTERP_PREC; sr = s;
			} else
#endif
			    { s = cpnt[ s_offset ]; s <<= 8; sr = s; }
		    }
		}
	    }
	    // end of get sample data ####################################

#ifndef SLOWMODE
	    //frequency interpolation: ***
	    U->subtick_count += 256;
	    if( U->subtick_count >= U->one_subtick ) 
	    {
		U->subtick_count -= U->one_subtick;
		ch->cur_period += ch->period_delta;
		if( ch->period_delta ) 
		{
		    new_freq = GET_FREQ( (ch->cur_period >> 6) );
		    ch->delta = GET_DELTA( new_freq );
		    ch->delta_p = ch->delta & ( ( (long)1 << XM_PREC ) - 1 );
		    ch->delta >>= XM_PREC;
		}
	    }
	    //****************************
#endif
	    if( ch->back ) 
		SIGNED_SUB64( ch->ticks, ch->ticks_p, ch->delta, ch->delta_p )
	    else
		SIGNED_ADD64( ch->ticks, ch->ticks_p, ch->delta, ch->delta_p )

#ifndef SLOWMODE
	    //envelope volume interpolation:
	    if( ch->vol_step ) 
	    {
		ch->l_cur += ch->l_delta;
		ch->r_cur += ch->r_delta;
		ch->vol_step --;
	    }
	    //=====================
#endif
	    //Channel effects: reduce number of bits:
	    if( ch->ch_effect_bits )
	    {
		s >>= ch->ch_effect_bits; s <<= ch->ch_effect_bits;
		sr >>= ch->ch_effect_bits; sr <<= ch->ch_effect_bits;
	    }

	    left = s * (ch->l_cur>>7);  //left = 32768max * 32768max
	    right = sr * (ch->r_cur>>7);
	    left >>= 15;                //left = -32767..32768
	    right >>= 15;

#ifdef SLOWMODE
	    //Save values for the scope:
	    temp = left;
	    if( temp < 0 ) temp = -temp;
	    if( temp > ch->temp_lvalue ) ch->temp_lvalue = temp;
	    temp = right;
	    if( temp < 0 ) temp = -temp;
	    if( temp > ch->temp_rvalue ) ch->temp_rvalue = temp;
    #if SCOPE_SIZE > 4 
	    if( ( i >> 1 ) < SCOPE_SIZE ) ch->scope[ i >> 1 ] = ( left + right ) >> 9;
    #endif

	    left *= U->global_volume; left >>= 8;
	    right *= U->global_volume; right >>= 8;
#else
	    left *= ch->vol_after_tremolo; left >>= 6;
	    right *= ch->vol_after_tremolo; right >>= 6;

	    //Save values for the scope:
	    temp = left;
	    if( temp < 0 ) temp = -temp;
	    if( temp > ch->temp_lvalue ) ch->temp_lvalue = temp;
	    temp = right;
	    if( temp < 0 ) temp = -temp;
	    if( temp > ch->temp_rvalue ) ch->temp_rvalue = temp;
    #if SCOPE_SIZE > 4 
	    if( ( i >> 1 ) < SCOPE_SIZE ) ch->scope[ i >> 1 ] = ( left + right ) >> 9;
    #endif

	    left *= U->global_volume; left >>= 6;
	    right *= U->global_volume; right >>= 6;
#endif

	    //Channel effects. Reduce sampling freq:
	    if( ch->ch_effect_freq )
	    {
		if( ch->freq_ptr > ch->ch_effect_freq )
		{
		    ch->freq_ptr = 0;
		    ch->freq_r = right;
		    ch->freq_l = left;
		}
		else
		{
		    ch->freq_ptr++;
		    if( ch->freq_r != -999999 ) right = ch->freq_r;
		    if( ch->freq_l != -999999 ) left = ch->freq_l;
		}
	    }

#ifdef SLOWMODE
	    res = buffer[i] + left;
	    if(res > 32767)  buffer[i] = 32767;  else  
	    if(res < -32768) buffer[i] = -32768; else buffer[i] = (signed short) res;
	    res = buffer[i+1] + right;
	    if(res > 32767)  buffer[i+1] = 32767;  else  
	    if(res < -32768) buffer[i+1] = -32768; else buffer[i+1] = (signed short) res;
#else
	    buffer32[ i ] += left;
	    buffer32[ i + 1 ] += right;
#endif
	}
    } //...for each channel (end)
    //===========================

    //check for a tick: ===========
    if( U->patternticks > U->onetick ) 
    { //handle new tick:
	U->patternticks -= U->onetick;
	U->subtick_count = 0;
        if( U->sp-- == 1 ) 
	{ //handle a new line:
	    U->sp = U->speed;
	    U->tick_number = 0;
	    if( U->status != XM_STATUS_STOP ) 
	    {
		cur_tablepos = U->tablepos;
		pat = U->song->patterns[U->song->patterntable[U->tablepos]];
		nptr = pat->pattern_data;
        	nptr = nptr + ( U->patternpos * pat->real_channels );
        	U->patternpos ++;

		U->jump_flag = 0;
		for( cc = 0; cc < U->song->channels; cc++ ) 
		{
		    n = nptr + cc;
		    int rc; //Real channel number
		    rc = ( cc * SUBCHANNELS ) + ( U->song->real_channel_num[ cc ] & ( SUBCHANNELS - 1 ) );
		    U->channels[ rc ]->note_delay = 0;
		    if( n->fx == 0xE && (n->par >> 4) == 0xD && (n->par&0xF) != 0 ) { //Note delay (EDx effect):
		        U->channels[rc]->note_delay = ( n->par & 0xF ) + 1; //set delay
		        U->channels[rc]->note_pointer = n;
		    } else worknote( n, cc, rc, U );
		}

		if( U->patternpos >= pat->rows )  { //end of pattern:
		    U->loop_start = 0; //reset loop pattern effect
		    U->tablepos++;
		    if( U->tablepos >= U->song->length ) U->tablepos = U->song->restart_position;
		    U->patternpos = 0;		
		}
		
		if( U->tablepos != cur_tablepos && U->status == XM_STATUS_PPLAY )
		{ //If status = pattern playing
		    U->tablepos = cur_tablepos;
		    pat = U->song->patterns[U->song->patterntable[U->tablepos]];
		    if( U->patternpos >= pat->rows )  { //end of pattern:
			U->patternpos = 0;
		    }
		}
		
		if( ( U->tablepos < cur_tablepos || U->speed == 0 ) && U->status == XM_STATUS_PLAYLIST )
		{ //If status = playlist playing
		    U->song_finished = 1;
		}
	    }
        }

	for( cc = 0; cc < U->song->channels; cc++ ) 
	{ //note delay proccessing:
	    int rc = ( cc * SUBCHANNELS ) + ( U->song->real_channel_num[ cc ] & ( SUBCHANNELS - 1 ) );
	    ch = U->channels[ rc ];
	    if( ch->note_delay ) {
		ch->note_delay--;
		if( ch->note_delay == 0 ) worknote( ch->note_pointer, cc, rc, U );
	    }
	}
	if( U->tick_number ) 
	{ 
	    for( cc = 0; cc < U->song->channels * SUBCHANNELS; cc++ ) 
	    { //effect proccessing:
		effects( U->channels[ cc ], U );
	    }
	}
	for( cc = 0; cc < U->song->channels * SUBCHANNELS; cc++ ) 
	{
	    envelope( cc, U );
	}
	
	U->tick_number++;
    }
    U->patternticks += ( size << 8 );
    //===========================
    
    //check for new buffer pieces:
    i2 += size;
    if( i2 < buffer_size ) { rest = buffer_size - i2; goto new_tick; }

    //Correct scope values:
    for( cc = 0; cc < U->song->channels * SUBCHANNELS; cc++ ) 
    {
	U->channels[ cc ]->lvalue = U->channels[ cc ]->temp_lvalue;
	U->channels[ cc ]->rvalue = U->channels[ cc ]->temp_rvalue;
    }
    U->counter++;

#ifndef SLOWMODE
    //Copy 32bit buffer to main buffer:
    for( i = 0; i < buffer_size * 2; i += 2 ) 
    { 
        res = buffer32[ i ];
        res2 = buffer32[ i + 1 ];

	if( res > 32767 )  buffer[ i ] = 32767;  else  
	if( res < -32768 ) buffer[ i ] = -32768; else buffer[ i ] = (signed short) res;
	if( res2 > 32767 )  buffer[ i + 1 ] = 32767;  else  
	if( res2 < -32768 ) buffer[ i + 1 ] = -32768; else buffer[ i + 1 ] = (signed short) res2;
    }
#endif

    return 0;
}


#define SET_VOLUME_FROM_INSTRUMENT \
cptr->pan = ins->samples[sample_num]->panning; \
cptr->vol = ins->samples[sample_num]->volume; \

#define SET_VEFFECT \
switch( vol >> 4 ) { \
    case 0x6: cptr->v_down = vol & 0xF; break; \
    case 0x7: cptr->v_up = vol & 0xF; break; \
    case 0x8: cptr->vol -= (vol & 0xF); if(cptr->vol < 0) cptr->vol = 0; break; \
    case 0x9: cptr->vol += (vol & 0xF); if(cptr->vol > 64) cptr->vol = 64; break; \
    case 0xA: cptr->vibrato_speed = vol & 0xF; break; \
    case 0xB: cptr->vibrato_depth = vol & 0xF; break; \
    case 0xC: SET_VOLUME_FROM_INSTRUMENT \
              cptr->pan = (vol&0xF) << 4; \
	      break; \
    case 0xD: cptr->pan_down = vol&0xF; break; \
    case 0xE: cptr->pan_up = vol&0xF; break; \
} 

#define SET_VOL_EFFECT \
if( vol<=0x50 ) { \
    cptr->vol = vol - 0x10; \
}else{ \
    SET_VEFFECT; \
}

#define SET_VOL_EFFECT2 \
if( vol<=0x50 ) { \
    cptr->pan = ins->samples[sample_num]->panning; \
    cptr->vol = vol - 0x10; \
}else{ \
    SET_VOLUME_FROM_INSTRUMENT \
    SET_VEFFECT; \
}

#define SET_VOL_EFFECT3 \
if( vol<=0x50 ) { \
    cptr->pan = ins->samples[sample_num]->panning; \
    cptr->vol = vol - 0x10; \
}else{ \
    cptr->vol = 64; \
    cptr->pan = ins->samples[sample_num]->panning; \
    SET_VEFFECT; \
}

#define RETRIG_ENVELOPE \
cptr->v_pos = 0; \
cptr->p_pos = 0; \
cptr->sustain = 1; \
cptr->fadeout = 65536; \
cptr->vib_pos = 0; \
cptr->cur_frame = 0; \
cptr->env_start = 1;

#define ENV_RESET \
if( note!=97 ) { \
    if( !cptr->tone_porta ) { \
	RETRIG_ENVELOPE \
    } \
    else { \
	if( !cptr->sustain ) { \
	    RETRIG_ENVELOPE \
	} \
    } \
}

#define RETRIG_SAMPLE \
cptr->ticks = 0; \
cptr->ticks_p = 0; \
cptr->back = 0;

#define SET_NOTE \
if(note<97) { \
    if(!cptr->tone_porta) { \
	note = note - 1 + ins->samples[ sample_num ]->relative_note + ins->relative_note; \
	period = 7680 - (note*64) - (ins->samples[ sample_num ]->finetune>>1) - (ins->finetune>>1); \
	cptr->p_period = cptr->period = period; \
	cptr->new_period = period; \
	freq = GET_FREQ(period); \
	cptr->delta = GET_DELTA(freq); \
	cptr->delta_p = cptr->delta & ( ( (long)1 << XM_PREC ) - 1 ); \
	cptr->delta >>= XM_PREC; \
	RETRIG_SAMPLE \
    } \
    else { \
	note = note - 1 + ins->samples[ sample_num ]->relative_note + ins->relative_note; \
	period = 7680 - (note*64) - (ins->samples[ sample_num ]->finetune>>1) - (ins->finetune>>1); \
	cptr->p_period = period; \
    } \
}

#define SET_INSTRUMENT \
ins = U->song->instruments[inst-1]; \
if( ins != 0 ) { \
    sample_num = ins->sample_number[ cptr->note - 1 ]; \
    smp = ins->samples[ sample_num ]; \
} else break; \
if( smp != 0 ) { \
    cptr->ins = ins; \
    cptr->smp = smp; \
} else break; 

#define GET_INSTRUMENT \
ins = cptr->ins; \
if(!ins) break; \
sample_num = ins->sample_number[ cptr->note - 1 ];

#define CLEAR_EFFECTS \
cptr->tone_porta = 0; \
cptr->v_up = 0; \
cptr->v_down = 0; \
cptr->pan_up = 0; \
cptr->pan_down = 0; \
cptr->p_speed = 0; \
cptr->vibrato_depth = 0; \
cptr->tremolo_depth = 0; \
cptr->arpeggio_periods[1] = 0; \
cptr->arpeggio_periods[2] = 0; \
cptr->arpeggio_counter = 0; \
cptr->retrig_rate = 0; \
cptr->note_cut = 0;


void worknote( xmnote *nptr, long virt_cnum, long cnum, xm_struct *U )
{
    if( virt_cnum >= MAX_CHANNELS ) return; //Temp fix
    if( cnum >= MAX_REAL_CHANNELS ) return;

    if( nptr->n == 98 )
    {	
	//Go to a previous virtual channel:
	virt_cnum--;
	if( virt_cnum < 0 ) virt_cnum = U->song->channels - 1;
	cnum = virt_cnum * SUBCHANNELS + ( U->song->real_channel_num[ virt_cnum ] & ( SUBCHANNELS - 1 ) );
    }

    if( nptr->n != 0 && nptr->n < 97 )
    if( nptr->fx != 3 && nptr->fx != 5 )
    {
	//Now you can switch to another real channel (if need)
	if( U->channels[ cnum ]->ins && U->channels[ cnum ]->ins->flags & INST_FLAG_NOTE_OFF )
	{
	    U->channels[ cnum ]->sustain = 0; //Stop previous note
	    U->song->real_channel_num[ virt_cnum ]++;
	    U->song->real_channel_num[ virt_cnum ] &= ( SUBCHANNELS - 1 );
	    cnum = virt_cnum * SUBCHANNELS + U->song->real_channel_num[ virt_cnum ];
	}
    }

    channel *cptr = U->channels[ cnum ];
    ulong note, inst, vol, period, freq, fx, par;
    ulong sample_num = 0;
    ulong events;
    int i;
    instrument *ins;
    sample *smp;
    ins = 0; smp = 0;
    
    note = nptr->n;
    inst = nptr->inst;
    vol = nptr->vol;
    fx = nptr->fx;
    par = nptr->par;

#ifdef DEMOENGINE
    if( note ) 
    {
	cptr->demo_note = note;
	cptr->demo_inst = inst;
	cptr->demo_count++;
    }
    if( vol > 15 ) cptr->demo_vol = vol - 0x10;
    cptr->demo_fx = fx;
    cptr->demo_par = par;
#endif
    
    
    if( note != 98 )
    {
	CLEAR_EFFECTS;
    }


    if( fx == 3 ) cptr->tone_porta = 1;
    if( fx == 5 ) cptr->tone_porta = 1;
    

    events = 0;
    if( vol > 15 ) events |= 1;
    if( inst != 0 ) 
    {
	events |= 2;
    }
    if( note != 0 && note < 97 ) 
    { 
	cptr->note = note; events |= 4; 
    }
    
    if( note == 97 ) { cptr->sustain = 0; events &= 0xD; } //For NOTE OFF: sustain = 0; clear instrument event;

    switch( events ) 
    {
	case 0: break;
	case 1: // ..V
	        GET_INSTRUMENT;
		SET_VOL_EFFECT; 
		break;
	case 2: // .I.
	        GET_INSTRUMENT;
		SET_VOLUME_FROM_INSTRUMENT; 
		ENV_RESET; 
		break;
	case 3: // .IV
	        GET_INSTRUMENT;
		SET_VOL_EFFECT2; 
		ENV_RESET; 
		break;
	case 4: // N..
		GET_INSTRUMENT;
		SET_NOTE; 
		ENV_RESET;
		break;
	case 5: // N.V
	        GET_INSTRUMENT;
		SET_VOL_EFFECT; 
		SET_NOTE; 
		ENV_RESET;
		break;
	case 6: // NI.
		SET_INSTRUMENT;
		SET_VOLUME_FROM_INSTRUMENT;
		ENV_RESET;
		SET_NOTE;
		break;
	case 7: // NIV
		SET_INSTRUMENT;
		SET_VOL_EFFECT3;
		ENV_RESET;
		SET_NOTE;
		break;
    }


    switch( fx )
    {
	case 0x0: cptr->arpeggio_periods[1] = (par>>4) << 6;
	          cptr->arpeggio_periods[2] = (par&0xF) << 6;
		  break;
	
	case 0x1: if( !par ) cptr->p_speed = cptr->old_p_speed2;
	          else cptr->old_p_speed2 = cptr->p_speed = par << 2;
	          cptr->p_period = 0;
	          break;
		  
	case 0x2: if( !par ) cptr->p_speed = cptr->old_p_speed2;
	          else cptr->old_p_speed2 = cptr->p_speed = par << 2;
		  cptr->p_period = 7680;
	          break;
		  
	case 0x3: if( !par ) cptr->p_speed = cptr->old_p_speed;
	          else cptr->old_p_speed = cptr->p_speed = par << 2;
		  break;
	
	case 0x4: if(!(par&0xF)) {
			cptr->vibrato_depth = cptr->old_depth;
	          } else {
			cptr->vibrato_depth = par & 0xF;
	          }
		  if(!(par>>4)) {
			cptr->vibrato_speed = cptr->old_speed;
	          } else {
			cptr->vibrato_speed = par >> 4;
	          }
		  if(!(cptr->vibrato_type&4) && events&4) cptr->vibrato_pos = 0; //vibrato retrigger
		  cptr->old_depth = cptr->vibrato_depth;
		  cptr->old_speed = cptr->vibrato_speed;
		  break;
		  
	case 0x5: cptr->p_speed = cptr->old_p_speed;
	          cptr->v_up = par >> 4; 
	          cptr->v_down = par & 0xF;
		  break;

	case 0x6: cptr->v_up = par >> 4; 
	          cptr->v_down = par & 0xF;
		  if(!(cptr->vibrato_type&4) && events&4) cptr->vibrato_pos = 0; //vibrato retrigger
		  cptr->vibrato_depth = cptr->old_depth;
		  cptr->vibrato_speed = cptr->old_speed;
		  break;
		  
	case 0x7: if(!(par&0xF)) {
			cptr->tremolo_depth = cptr->old_tremolo_depth;
	          } else {
			cptr->tremolo_depth = par & 0xF;
	          }
		  if(!(par>>4)) {
			cptr->tremolo_speed = cptr->old_tremolo_speed;
	          } else {
			cptr->tremolo_speed = par >> 4;
	          }
		  if(!(cptr->tremolo_type&4) && events&4) cptr->tremolo_pos = 0; //vibrato retrigger
		  cptr->old_tremolo_depth = cptr->tremolo_depth;
		  cptr->old_tremolo_speed = cptr->tremolo_speed;
		  break;

	case 0x8: cptr->pan = par;
		  break;
		  
	case 0x9: if( inst || note ) {
	                cptr->ticks_p = 0;
			if( !par ) cptr->ticks = cptr->old_ticks;
			else cptr->old_ticks = cptr->ticks = ( par * 256 );
	          }
		  break;
	
	case 0xA: if( par >> 4 ) cptr->old_vol_down = 0;
	          if( par & 0xF ) cptr->old_vol_up = 0;
	          if( !( par >> 4 ) ) cptr->v_up = cptr->old_vol_up;
	        	else cptr->v_up = par >> 4;
		  if( !( par & 0xF ) ) cptr->v_down = cptr->old_vol_down;
			else cptr->v_down = par & 0xF;
		  cptr->old_vol_up = cptr->v_up;
		  cptr->old_vol_down = cptr->v_down;
		  break;
		  
	case 0xB: if( !U->jump_flag ) 
		  {
			if( par < U->song->length ) 
			{ 
			    if( U->status == XM_STATUS_PLAYLIST && par <= (ulong)U->tablepos )
				U->song_finished = 1;
			    U->tablepos = par; U->patternpos = 0; 
			}
			U->jump_flag = 1;
	          }
	          break;
		  
	case 0xC: cptr->vol = par; break;
	
	case 0xD: if(!U->jump_flag) {
			if( U->tablepos < (U->song->length-1) )  U->tablepos++; else U->tablepos = 0;
			U->patternpos = ((par>>4)*10)+(par&0xF);
			U->jump_flag = 1;
	          }
		  break;
	
	case 0xE: switch( par >> 4 ) {
			case 0x1: if( (par&0xF) == 0 ) cptr->period -= cptr->old_fine_up;
				  else { cptr->old_fine_up = (par&0xF) << 2; cptr->period -= cptr->old_fine_up; }
				  cptr->new_period = cptr->period;
				  break;
			case 0x2: if( (par&0xF) == 0 ) cptr->period += cptr->old_fine_down;
				  else { cptr->old_fine_down = (par&0xF) << 2; cptr->period += cptr->old_fine_down; }
				  cptr->new_period = cptr->period;
				  break;
			case 0x4: cptr->vibrato_type = par & 0xF; break;
			case 0x5: if( (events&4) || (events&2) ) {  //When NOTE/INSTR events exist
					cptr->new_period -= (par & 0xF) << 3; 
					cptr->period = cptr->new_period;
				  }
			          break;
			case 0x6: if( (par&0xF) == 0 ) U->loop_start = U->patternpos;
			          else {
					if( U->loop_count == 1 ) { U->loop_count = 0; break; }
					if(U->loop_count) { U->loop_count--; U->patternpos = U->loop_start - 1; break; }
					U->loop_count = par & 0xF;
					U->patternpos = U->loop_start - 1;
				  }
				  break;
			case 0x7: cptr->tremolo_type = par & 0xF; break;
			case 0x9: cptr->retrig_rate = par & 0xF; cptr->retrig_count = par & 0xF; break;
			case 0xA: if( (par&0xF) == 0 ) cptr->vol += cptr->old_slide_up;
			          else { cptr->old_slide_up = par & 0xF; cptr->vol += par & 0xF; }
			          if(cptr->vol > 64) cptr->vol = 64; 
				  break;
			case 0xB: if( (par&0xF) == 0 ) cptr->vol -= cptr->old_slide_down;
			          else { cptr->old_slide_down = par & 0xF; cptr->vol -= par & 0xF; }
			          if(cptr->vol < 0) cptr->vol = 0; 
				  break;
			case 0xC: if( (par&0xF) == 0 ) cptr->note_cut = 1; else cptr->note_cut = par & 0xF; break;
		  }
		  break;
		  
	case 18:  switch( par >> 4 ) {
			case 0x0:
			    if( ( par & 0xF ) == 0 ) { cptr->back = 0; cptr->paused = 0; }
			    else
			    if( ( par & 0xF ) == 1 ) { cptr->back = 1; cptr->paused = 0; }
			    else
			    if( ( par & 0xF ) == 2 ) cptr->paused = 1;
			    break;
			case 0x1:
			    for( i = 0; i < SUBCHANNELS; i++ )
			    {
				if( U->channels[ virt_cnum * SUBCHANNELS + i ]->ch_effect_freq == 0 )
				{
				    //Set start values:
				    U->channels[ virt_cnum * SUBCHANNELS + i ]->freq_r = -999999;
				    U->channels[ virt_cnum * SUBCHANNELS + i ]->freq_l = -999999;
				}
				U->channels[ virt_cnum * SUBCHANNELS + i ]->ch_effect_freq = par & 0xF;
			    }
			    break;
			case 0x2:
			    for( i = 0; i < SUBCHANNELS; i++ )
				U->channels[ virt_cnum * SUBCHANNELS + i ]->ch_effect_bits = par & 0xF;
			    break;
		  }
		  break;

	case 0xF: if(par<32) { U->speed = par; U->sp = par; }
	          else { U->bpm = par; U->onetick = ((U->freq*25)<<8) / (U->bpm*10); }
		  break;
    }
    
    //Tremolo control:
    if( cptr->tremolo_depth ) 
    {
	if( !cptr->old_tremolo ) cptr->vol_after_tremolo = cptr->vol;
    } else cptr->vol_after_tremolo = cptr->vol;
    cptr->old_tremolo = cptr->tremolo_depth;
}


void envelope( long cnum, xm_struct *U ) //***> every tick <***
{
    channel *cptr = U->channels[ cnum ];
    if( !cptr->smp ) return;
    instrument *ins = cptr->ins;
    long pan_value = 128;
    long pan_mul;
    long l_vol, r_vol; //current envelope volume (0..1024)

    long pos;      //vibrato position
    long temp = 0; //temp variable
    long new_period;
    long new_freq;
    

    if( cptr->enable && ins != 0 )
    {
	//VOLUME ENVELOPE:
	if(ins->volume_type&1) { //volume envelope ON:
	    if(ins->volume_type&4) { //envelope loop ON:
		if( cptr->v_pos >= ins->volume_points[ ins->vol_loop_end<<1 ] )  
		    cptr->v_pos = ins->volume_points[ ins->vol_loop_start<<1 ];
	    }
	    if( cptr->v_pos >= ins->volume_points[ (ins->volume_points_num-1)<<1 ] ) 
		cptr->v_pos = ins->volume_points[ (ins->volume_points_num-1)<<1 ];
	    if(!cptr->sustain) {
		cptr->fadeout -= ins->volume_fadeout<<1;
		if(cptr->fadeout < 0) cptr->fadeout = 0;
	    }
	    
	    r_vol = l_vol = ( cptr->fadeout * ins->volume_env[ cptr->v_pos ] ) >> 16;

	    if( cptr->v_pos >= ins->volume_points[ ins->vol_sustain<<1 ] ) {
		if(ins->volume_type&2) { if(!cptr->sustain) cptr->v_pos++; } else cptr->v_pos++;
	    }else{
		cptr->v_pos++;
	    }
	}else{ //volume envelope OFF:
	    if(cptr->sustain == 0) {
		r_vol = l_vol = 0;
	    } else {
		r_vol = l_vol = 1024;
	    }
	}
	
	//PANNING ENVELOPE:
	if(ins->panning_type&1) { //panning envelope ON:
	    if(ins->panning_type&4) { //envelope loop ON:
		if( cptr->p_pos >= ins->panning_points[ ins->pan_loop_end<<1 ] )  
		    cptr->p_pos = ins->panning_points[ ins->pan_loop_start<<1 ];
	    }
	    if( cptr->p_pos >= ins->panning_points[ (ins->panning_points_num-1)<<1 ] ) cptr->p_pos--;
	    
	    pan_value = ins->panning_env[ cptr->p_pos ] >> 2;

	    if( cptr->p_pos >= ins->panning_points[ ins->pan_sustain<<1 ] ) {
		if(ins->panning_type&2) { if(!cptr->sustain) cptr->p_pos++; } else cptr->p_pos++;
	    }else{
		cptr->p_pos++;
	    }
	}
	
	
	//SET PANNING:
	pan_mul = cptr->pan - 128;
	if( pan_mul < 0 ) pan_mul = -pan_mul;
	pan_value = ( ( cptr->pan * pan_mul ) + ( pan_value * ( 128 - pan_mul ) ) ) >> 7;
	if( cptr->ins )
	{
	    //Set instrument panning:
	    if( cptr->ins->panning != 0x80 )
	    {
		pan_mul = cptr->ins->panning - 128;
		if( pan_mul < 0 ) pan_mul = -pan_mul;
		pan_value = ( ( cptr->ins->panning * pan_mul ) + ( pan_value * ( 128 - pan_mul ) ) ) >> 7;
	    }
	    //Set instrument volume:
	    if( cptr->ins->volume < 0x40 )
	    {
		l_vol *= cptr->ins->volume; l_vol >>= 6;
		r_vol *= cptr->ins->volume; r_vol >>= 6;
	    }
	}
	l_vol = ( l_vol * U->stereo_tab[ pan_value << 1 ] ) >> 10;
	r_vol = ( r_vol * U->stereo_tab[ ( pan_value << 1 ) + 1 ] ) >> 10;

#ifndef SLOWMODE
	//FOR VOLUME INTERPOLATION:
	if(cptr->env_start) { //first tick:
	    cptr->l_old = l_vol;
	    cptr->r_old = r_vol;
	    cptr->env_start = 0;
	}
	cptr->vol_step = U->onetick >> 8;
	cptr->l_delta = ( ( l_vol - cptr->l_old ) << 12 ) / cptr->vol_step;  //1 = 4096
	cptr->r_delta = ( ( r_vol - cptr->r_old ) << 12 ) / cptr->vol_step;
    	cptr->l_cur = cptr->l_old << 12;
	cptr->r_cur = cptr->r_old << 12;
	cptr->l_old = l_vol;
	cptr->r_old = r_vol;
#else
	cptr->l_cur = ( (l_vol * cptr->vol_after_tremolo) >> 6 ) << 12; //It is ((1024max * 64max) / 64) * 4096
	cptr->r_cur = ( (r_vol * cptr->vol_after_tremolo) >> 6 ) << 12; //the same
#endif


	//AUTOVIBRATO:
	if( ins->vibrato_depth )  {
	    pos = cptr->vib_pos & 255;
	    switch( ins->vibrato_type & 3 ) {
		case 0: //sine:
	        	temp = ( U->g_vibrato_tab[pos] * ins->vibrato_depth ) >> 8;
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

	    cptr->vib_pos += ins->vibrato_rate<<1;
	    if(cptr->vib_pos > 255) cptr->vib_pos -= 512;
	
	    new_period = cptr->new_period - cptr->arpeggio_periods[ cptr->arpeggio_counter ] + temp;
	} 
	else
	{ 
	    new_period = cptr->new_period - cptr->arpeggio_periods[ cptr->arpeggio_counter ];
	}


	cptr->arpeggio_counter++;
	if( cptr->arpeggio_counter > 2 ) cptr->arpeggio_counter = 0;
	
	
        //Frequency handling STEP 2 (last step) is over:   <***
        //resulted period in new_period;                   <***
	//start frequency interpolation: ***
	if( cptr->p_speed && U->tick_number && U->freq_int ) { //only for 1/2/3xx effect, when tick > 0
	    cptr->period_delta = new_period - cptr->old_period;
	    cptr->cur_period = cptr->old_period << 6;
	    cptr->old_period = new_period;
	} else { cptr->old_period = new_period; cptr->period_delta = 0; cptr->cur_period = new_period << 6; }
	//**********************************
        //calculate new frequency and delta:
        new_freq = GET_FREQ( new_period );
        cptr->delta = GET_DELTA( new_freq );
	cptr->delta_p = cptr->delta & ( ( (long)1 << XM_PREC ) - 1 );
	cptr->delta >>= XM_PREC;
    }
}

void effects(channel *cptr, xm_struct *U)
{
    long pos;  //vibrato position
    long temp; //temp variable
    instrument *ins = cptr->ins;
    if( !cptr->smp ) return;

    
    //retrigger sample effect:
    if(cptr->retrig_rate) {
	cptr->retrig_count--;
	if(!cptr->retrig_count) {
	    RETRIG_SAMPLE;
	    RETRIG_ENVELOPE;
	    cptr->retrig_count = cptr->retrig_rate;
	}
    }
    //note cut effect:
    if(cptr->note_cut) {
	cptr->note_cut--;
	if(!cptr->note_cut) cptr->vol = 0;
    }


    //volume effects:
    if(cptr->v_up) {
	cptr->vol += cptr->v_up; 
	if(cptr->vol>64) { cptr->vol = 64; cptr->v_up = 0; }
    }
    if(cptr->v_down) {
	cptr->vol -= cptr->v_down; 
	if(cptr->vol<0) { cptr->vol = 0; cptr->v_down = 0; }
    }
    
    if(cptr->pan_up) {
	cptr->pan += cptr->pan_up; 
	if(cptr->pan>255) { cptr->pan = 255; cptr->pan_up = 0; }
    }
    if(cptr->pan_down) {
	cptr->pan -= cptr->pan_down; 
	if(cptr->pan<0) { cptr->pan = 0; cptr->pan_down = 0; }
    }
    
    if(cptr->tremolo_depth) {
	pos = cptr->tremolo_pos & 255;
	switch( cptr->tremolo_type & 3 ) {
	    case 0: //sine:
	            temp = ( U->g_vibrato_tab[pos] * cptr->tremolo_depth ) >> 6;
		    if(cptr->tremolo_pos < 0) temp = -temp;
		    break;
	    case 1: //ramp down:
		    temp = -cptr->tremolo_pos >> 1;
		    temp = ( temp * cptr->tremolo_depth ) >> 5;
		    break;
	    case 2: //square:
		    temp = (cptr->tremolo_depth * 127) >> 5;
		    if(cptr->tremolo_pos < 0) temp = -temp;
		    break;
	    case 3: //random:
		    break;
	}
	
	cptr->tremolo_pos += cptr->tremolo_speed<<3;
	if(cptr->tremolo_pos > 255) cptr->tremolo_pos -= 512;
	
	cptr->vol_after_tremolo = cptr->vol + temp;
	if(cptr->vol_after_tremolo>64) cptr->vol_after_tremolo = 64;
	if(cptr->vol_after_tremolo<0) cptr->vol_after_tremolo = 0;
    
    } else { //no tremolo:
	cptr->vol_after_tremolo = cptr->vol;
    }


    //frequency effects:
    if(cptr->p_speed) {
	if(cptr->period < cptr->p_period) {
	    cptr->period += cptr->p_speed;
	    if(cptr->period > cptr->p_period) { cptr->period = cptr->p_period; cptr->p_speed = 0; }
	}
	if(cptr->period > cptr->p_period) {
	    cptr->period -= cptr->p_speed;
	    if(cptr->period < cptr->p_period) { cptr->period = cptr->p_period; cptr->p_speed = 0; }
	}
    }


    if(cptr->vibrato_depth) {
	pos = cptr->vibrato_pos & 255;
	switch( cptr->vibrato_type & 3 ) {
	    case 0: //sine:
	            temp = ( U->g_vibrato_tab[pos] * cptr->vibrato_depth ) >> 5;
		    if(cptr->vibrato_pos < 0) temp = -temp;
		    break;
	    case 1: //ramp down:
		    temp = -cptr->vibrato_pos >> 1;
		    temp = ( temp * cptr->vibrato_depth ) >> 4;
		    break;
	    case 2: //square:
		    temp = (cptr->vibrato_depth * 127) >> 4;
		    if(cptr->vibrato_pos < 0) temp = -temp;
		    break;
	    case 3: //random:
		    break;
	}
	
	cptr->vibrato_pos += cptr->vibrato_speed<<3;
	if(cptr->vibrato_pos > 255) cptr->vibrato_pos -= 512;
	
	cptr->new_period = cptr->period + temp;
    
    } else { //no vibrato:
	cptr->new_period = cptr->period;
    }
    
    //Frequency handling STEP 1 (for tick!=0) is over:   <***
    //resulted period in cptr->new_period;               <***
}

