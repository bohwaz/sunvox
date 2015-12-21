/*
    user_code.cpp.
    This file is part of the SunVox API Example #1.
    Copyright (C) 2002 - 2009 Alex Zolotov <nightradio@gmail.com>
*/

#include "main/user_code.h"

#include "../../../sunvox_engine/sunvox_engine.h"

const UTF8_CHAR *user_window_name = "SunVox API Example #1 (" __DATE__ ")";
const UTF8_CHAR *user_profile_name = "sunvox_config.ini";
const UTF8_CHAR *user_debug_log_file_name = "sunvox_log.txt";
int user_window_xsize = 240;
int user_window_ysize = 240;
int user_window_flags = WIN_INIT_FLAG_SCALABLE;

sunvox_engine g_sv;

int render_piece_of_sound( sound_struct *ss )
{
    int handled = 0;
    
    if( ss && ss->user_data )
    {
	int buffer_type;
        if( ss->mode & SOUND_MODE_INT16 )
            buffer_type = 0;
        if( ss->mode & SOUND_MODE_FLOAT32 )
            buffer_type = 1;
        sunvox_render_piece_of_sound(
            buffer_type,
            ss->out_buffer,
            ss->out_frames,
            ss->channels,
            ss->freq,
            ss->out_time,
            (sunvox_engine*)ss->user_data );
        handled = 1;    
    }
    
    return handled;
}

int my_desktop_handler( sundog_event *evt, window_manager *wm )
{
    int retval = 0;
    WINDOWPTR win = evt->win;
    switch( evt->type )
    {
	case EVT_AFTERCREATE:
	    //Init sound engine:
	    sunvox_engine_init( 0, &g_sv );
	    g_snd.user_data = (void*)&g_sv;
	    
	    //Load song:
	    sunvox_load_song( "tune.sunvox", &g_sv );
	    
	    //Play:
	    sunvox_play_from_beginning( &g_sv );

	    retval = 1;
	    break;
	case EVT_BEFORECLOSE:
	    //Deinit sound engine:
	    sound_stream_stop();
	    sunvox_engine_close( &g_sv );
	    g_snd.user_data = 0;
	    sound_stream_play();
	    retval = 1;
	    break;
	case EVT_DRAW:
	    win_draw_lock( win, wm );
	    
	    //Background:
	    win_draw_box( win, 0, 0, win->xsize, win->ysize, win->color, wm );
	    
	    win_draw_string( win, "EXIT", 2, 2, wm->black, win->color, wm );

	    win_draw_string( win, "SYNTHS:", 2, 2 * 10, wm->black, win->color, wm );
	    win_draw_string( win, " synth1", 2, 3 * 10, wm->black, win->color, wm );
	    win_draw_string( win, " synth2", 2, 4 * 10, wm->black, win->color, wm );
	    win_draw_string( win, " synth3", 2, 5 * 10, wm->black, win->color, wm );
	    win_draw_string( win, " synth4", 2, 6 * 10, wm->black, win->color, wm );

	    win_draw_string( win, "LIVE AREA:", 2, 8 * 10, wm->black, win->color, wm );

	    //Draw volumes:
	    {
		int s, x;
		s = psynth_get_synth_by_name( "pad", g_sv.net );
		if( s >= 0 ) 
		{
		    x = ( g_sv.net->items[ s ].ctls[ 0 ].ctl_val[ 0 ] * win->xsize ) /
			  g_sv.net->items[ s ].ctls[ 0 ].ctl_max;
		    win_draw_box( win, x, 3 * 10, 8, 10, wm->green, wm );
		}
		s = psynth_get_synth_by_name( "string", g_sv.net );
		if( s >= 0 ) 
		{
		    x = ( g_sv.net->items[ s ].ctls[ 0 ].ctl_val[ 0 ] * win->xsize ) /
			  g_sv.net->items[ s ].ctls[ 0 ].ctl_max;		
		    win_draw_box( win, x, 4 * 10, 8, 10, wm->green, wm );
		}
		s = psynth_get_synth_by_name( "bass", g_sv.net );
		if( s >= 0 ) 
		{
		    x = ( g_sv.net->items[ s ].ctls[ 0 ].ctl_val[ 0 ] * win->xsize ) /
			  g_sv.net->items[ s ].ctls[ 0 ].ctl_max;		
		    win_draw_box( win, x, 5 * 10, 8, 10, wm->green, wm );
		}
		s = psynth_get_synth_by_name( "drum", g_sv.net );
		if( s >= 0 ) 
		{
		    x = ( g_sv.net->items[ s ].ctls[ 0 ].ctl_val[ 0 ] * win->xsize ) /
			  g_sv.net->items[ s ].ctls[ 0 ].ctl_max;		
		    win_draw_box( win, x, 6 * 10, 8, 10, wm->green, wm );
		}
	    }
	    
	    win_draw_unlock( win, wm );
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONDOWN:
	case EVT_MOUSEMOVE:
	    if( evt->key == MOUSE_BUTTON_LEFT )
	    {
		int y = evt->y / 10;
		int s = -1;
		int v = 0;
		switch( y )
		{
		    case 0:
			wm->exit_request = 1;
			break;
		    case 3:
			//Set controller of "pad" synth:
			s = psynth_get_synth_by_name( "pad", g_sv.net );
			break;
		    case 4:
			//Set controller of "string" synth:
			s = psynth_get_synth_by_name( "string", g_sv.net );
			break;
		    case 5:
			//Set controller of "bass" synth:
			s = psynth_get_synth_by_name( "bass", g_sv.net );
			break;
		    case 6:
			//Set controller of "drum" synth:
			s = psynth_get_synth_by_name( "drum", g_sv.net );
			break;
		}
			
		if( s >= 0 )
		{
		    sunvox_note n;
		    n.note = 0;
		    n.vel = 0;
		    n.synth = s + 1;
		    n.ctl = 0x0100; //XXYY: XX - Ctrl number; YY - Std effect
		    n.ctl_val = ( (int)evt->x * 0x8000 ) / win->xsize;
		    sunvox_send_user_command( &n, 0, &g_sv );
		}
		
		if( y >= 8 )
		{
		    //Live playing:
		    s = psynth_get_synth_by_name( "live", g_sv.net );
		    if( s >= 0 )
		    {
			int note_num = evt->x / 2;
			int octave = note_num / 12;
			note_num -= octave * 12;
			switch( note_num )
			{
			    case 0: note_num = 3; break;
			    case 1: note_num = 3; break;
			    case 2: note_num = 3; break;
			    case 3: note_num = 3; break;
			    case 4: note_num = 6; break;
			    case 5: note_num = 6; break;
			    case 6: note_num = 6; break;
			    case 7: note_num = 10; break;
			    case 8: note_num = 10; break;
			    case 9: note_num = 10; break;
			    case 10: note_num = 10; break;
			    case 11: note_num = 10; break;
			}
			note_num += octave * 12;
			if( note_num < 120 )
			{
			    sunvox_note n;
			    n.note = note_num;
			    n.vel = 0x80;
			    n.synth = s + 1;
			    n.ctl = 0x0000; //XXYY: XX - Ctrl number; YY - Std effect
			    n.ctl_val = 0;
			    sunvox_send_user_command( &n, 0, &g_sv );
			}
		    }
		}

		draw_window( win, wm );
	    }
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONUP:
	    retval = 1;
	    break;
    }
    return retval;
}

void user_init( window_manager *wm )
{
    //Desktop:
    wm->root_win = new_window( 
	"Desktop", 
	0, 0, 
	wm->screen_xsize, wm->screen_ysize, 
	wm->white, 
	0, 
	my_desktop_handler,
	wm );
    wm->root_win->font = 0;

    show_window( wm->root_win, wm );
    recalc_regions( wm );
    draw_window( wm->root_win, wm );
}

int user_event_handler( sundog_event *evt, window_manager *wm )
{
    int handled = 0;
    return handled;
}

void user_close( window_manager *wm )
{
    remove_window( wm->root_win, wm );
}
