/*
    wm_unixgraphics_sdl.h. Platform-dependent module : Unix SDL
    This file is part of the SunDog engine.
    Copyright (C) 2002 - 2009 Alex Zolotov <nightradio@gmail.com>
*/

#ifndef __WINMANAGER_UNIX_SDL__
#define __WINMANAGER_UNIX_SDL__

//#################################
//## DEVICE DEPENDENT FUNCTIONS: ##
//#################################

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>   	//for getenv()
#include <sys/time.h> 	//timeval struct
#include <sched.h>	//for sched_yield()

#include "SDL/SDL.h"

COLORPTR framebuffer;

pthread_t g_evt_pth;

void small_pause( long milliseconds )
{
    timeval t;
    t.tv_sec = 0;
    t.tv_usec = (long) milliseconds * 1000;
    select( 0 + 1, 0, 0, 0, &t );
}

volatile int g_sdl_thread_finished = 0;

void *event_thread( void *arg )
{
    window_manager *wm = (window_manager*)arg;
    SDL_Event event;
    volatile int need_exit = 0;
    int mod = 0; //Shift / alt / ctrl...
    int key = 0;
    int exit_request = 0;
    while( exit_request == 0 )
    {
	int key = 0;
	SDL_WaitEvent( &event );
	switch( event.type ) 
	{
	    case SDL_MOUSEBUTTONDOWN:
	    case SDL_MOUSEBUTTONUP:
		key = 0;
		switch( event.button.button )
		{
		    case SDL_BUTTON_LEFT: key = MOUSE_BUTTON_LEFT; break;
		    case SDL_BUTTON_MIDDLE: key = MOUSE_BUTTON_MIDDLE; break;
		    case SDL_BUTTON_RIGHT: key = MOUSE_BUTTON_RIGHT; break;
		    case SDL_BUTTON_WHEELUP: key = MOUSE_BUTTON_SCROLLUP; break;
		    case SDL_BUTTON_WHEELDOWN: key = MOUSE_BUTTON_SCROLLDOWN; break;
		}
		if( key )
		{
		    if( event.type == SDL_MOUSEBUTTONDOWN )
			send_event( 0, EVT_MOUSEBUTTONDOWN, mod, event.button.x, event.button.y, key, 0, 1024, 0, wm );
		    else
			send_event( 0, EVT_MOUSEBUTTONUP, mod, event.button.x, event.button.y, key, 0, 1024, 0, wm );
		}
		break;
	    case SDL_MOUSEMOTION:
		key = 0;
		if( event.motion.state & SDL_BUTTON( SDL_BUTTON_LEFT ) ) key |= MOUSE_BUTTON_LEFT;
		if( event.motion.state & SDL_BUTTON( SDL_BUTTON_MIDDLE ) ) key |= MOUSE_BUTTON_MIDDLE;
		if( event.motion.state & SDL_BUTTON( SDL_BUTTON_RIGHT ) ) key |= MOUSE_BUTTON_RIGHT;
		send_event( 0, EVT_MOUSEMOVE, mod, event.motion.x, event.motion.y, key, 0, 1024, 0, wm );
		break;
	    case SDL_KEYDOWN:
	    case SDL_KEYUP:
		key = event.key.keysym.sym;
		if( key > 255 )
		{
		    switch( key )
		    {
			case SDLK_UP: key = KEY_UP; break;
			case SDLK_DOWN: key = KEY_DOWN; break;
			case SDLK_LEFT: key = KEY_LEFT; break;
			case SDLK_RIGHT: key = KEY_RIGHT; break;
			case SDLK_INSERT: key = KEY_INSERT; break;
			case SDLK_HOME: key = KEY_HOME; break;
			case SDLK_END: key = KEY_END; break;
			case SDLK_PAGEUP: key = KEY_PAGEUP; break;
			case SDLK_PAGEDOWN: key = KEY_PAGEDOWN; break;
			case SDLK_F1: key = KEY_F1; break;
			case SDLK_F2: key = KEY_F2; break;
			case SDLK_F3: key = KEY_F3; break;
			case SDLK_F4: key = KEY_F4; break;
			case SDLK_F5: key = KEY_F5; break;
			case SDLK_F6: key = KEY_F6; break;
			case SDLK_F7: key = KEY_F7; break;
			case SDLK_F8: key = KEY_F8; break;
			case SDLK_F9: key = KEY_F9; break;
			case SDLK_F10: key = KEY_F10; break;
			case SDLK_F11: key = KEY_F11; break;
			case SDLK_F12: key = KEY_F12; break;
			case SDLK_CAPSLOCK: key = KEY_CAPS; break;
			case SDLK_RSHIFT:
			case SDLK_LSHIFT:
			    if( event.type == SDL_KEYDOWN ) mod |= EVT_FLAG_SHIFT; else mod &= ~EVT_FLAG_SHIFT;
			    key = KEY_SHIFT;
			    break;
			case SDLK_RCTRL:
			case SDLK_LCTRL:
			    if( event.type == SDL_KEYDOWN ) mod |= EVT_FLAG_CTRL; else mod &= ~EVT_FLAG_CTRL;
			    key = KEY_CTRL;
			    break;
			case SDLK_RALT:
			case SDLK_LALT:
			    if( event.type == SDL_KEYDOWN ) mod |= EVT_FLAG_ALT; else mod &= ~EVT_FLAG_ALT;
			    key = KEY_ALT;
			    break;
			default: key = 0; break;
		    }	    
		}
		else
		{
		    switch( key )
		    {
			case SDLK_RETURN: key = KEY_ENTER; break;
			case SDLK_DELETE: key = KEY_DELETE; break;
			case SDLK_BACKSPACE: key = KEY_BACKSPACE; break;
		    }
		}
		if( key != 0 )
		{
		    if( event.type == SDL_KEYDOWN )
	    		send_event( 0, EVT_BUTTONDOWN, mod, 0, 0, key, event.key.keysym.scancode, 1024, event.key.keysym.unicode, wm );
		    else
	    		send_event( 0, EVT_BUTTONUP, mod, 0, 0, key, event.key.keysym.scancode, 1024, event.key.keysym.unicode, wm );
		}
		break;
	    case SDL_VIDEORESIZE:
    		pthread_mutex_lock( &wm->sdl_lock_mutex );
		SDL_SetVideoMode( 
		    event.resize.w, event.resize.h, COLORBITS,
                    SDL_HWSURFACE | SDL_RESIZABLE ); // Resize window
		wm->screen_xsize = event.resize.w;
		wm->screen_ysize = event.resize.h;
		if( wm->root_win )
		{
		    int need_recalc = 0;
		    if( wm->root_win->x + wm->root_win->xsize > wm->screen_xsize ) 
		    {
			wm->root_win->xsize = wm->screen_xsize - wm->root_win->x;
			need_recalc = 1;
		    }
		    if( wm->root_win->y + wm->root_win->ysize > wm->screen_ysize ) 
		    {
			wm->root_win->ysize = wm->screen_ysize - wm->root_win->y;
			need_recalc = 1;
		    }
		    if( need_recalc ) recalc_regions( wm );
		}
    		pthread_mutex_unlock( &wm->sdl_lock_mutex );
	    	send_event( wm->root_win, EVT_SCREENRESIZE, 0, 0, 0, 0, 0, 0, 0, wm );
		break;
	    case SDL_QUIT:
		send_event( 0, EVT_QUIT, 0, 0, 0, 0, 0, 1024, 0, wm );
		break;
	    case SDL_USEREVENT:
		if( event.user.code == 33 )
		{
		    exit_request = 1;
		}
		break;
	}
    }
    g_sdl_thread_finished = 1;
    pthread_exit( 0 );
}

int device_start( const UTF8_CHAR *windowname, int xsize, int ysize, int flags, window_manager *wm )
{
    int retval = 0;

    //Create SDL screen lock mutex:
    if( pthread_mutex_init( &wm->sdl_lock_mutex, 0 ) != 0 )
    {
	dprint( "Can't create SDL screen lock mutex\n" );
	return 1;
    }

    if( profile_get_int_value( KEY_SCREENX, 0 ) != -1 ) xsize = profile_get_int_value( KEY_SCREENX, 0 );
    if( profile_get_int_value( KEY_SCREENY, 0 ) != -1 ) ysize = profile_get_int_value( KEY_SCREENY, 0 );

    if( SDL_Init( SDL_INIT_VIDEO ) < 0 ) 
    {
	dprint( "Can't initialize SDL: %s\n", SDL_GetError() );
	return 1;
    }
    
    int fs = 0;
    if( profile_get_int_value( KEY_FULLSCREEN, 0 ) != -1 ) fs = 1;
    
    if( fs )
    {
	fix_fullscreen_resolution( &xsize, &ysize, wm );

	//Get list of available video-modes:
	SDL_Rect **modes;
	modes = SDL_ListModes( NULL, SDL_FULLSCREEN | SDL_HWSURFACE );
	if( modes != (SDL_Rect **)-1 )
	{
	    dprint( "Available Video Modes:\n" );
	    for( int i = 0; modes[ i ]; ++i )
		printf( "  %d x %d\n", modes[ i ]->w, modes[ i ]->h );
	}
    }
    
    int video_flags = 0;
    if( flags & WIN_INIT_FLAG_SCALABLE ) video_flags |= SDL_RESIZABLE;
    if( flags & WIN_INIT_FLAG_NOBORDER ) video_flags |= SDL_NOFRAME;
    if( fs ) video_flags = SDL_FULLSCREEN;
    wm->sdl_screen = SDL_SetVideoMode( xsize, ysize, COLORBITS, SDL_HWSURFACE | video_flags );
    if( wm->sdl_screen == 0 ) 
    {
	dprint( "SDL. Can't set videomode: %s\n", SDL_GetError() );
	return 1;
    }

    wm->screen_xsize = xsize;
    wm->screen_ysize = ysize;

    //Set window name:
    if( fs == 0 )
    {
	SDL_WM_SetCaption( windowname, windowname );
    }

    SDL_EnableKeyRepeat( SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL );

    //Start event thread:
    if( pthread_create( &g_evt_pth, NULL, event_thread, wm ) != 0 )
    {
	dprint( "Can't create event thread\n" );
	return 1;
    }

    return retval;
}

void device_end( window_manager *wm )
{
    SDL_Event quit_event;
    quit_event.type = SDL_USEREVENT;
    quit_event.user.code = 33;
    SDL_PushEvent( &quit_event );
    while( g_sdl_thread_finished == 0 ) {};

    dprint( "SDL_Quit()...\n" );
    SDL_Quit();
    
    //Remove mutexes:
    dprint( "Removing mutexes...\n" );
    pthread_mutex_destroy( &wm->sdl_lock_mutex );
}

long device_event_handler( window_manager *wm )
{
    if( wm->flags & WIN_INIT_FLAG_FULL_CPU_USAGE )
    {
	sched_yield();
    }
    else
    {
	small_pause( 1 );
    }
    if( wm->exit_request ) return 1;
    return 0;
}

void device_screen_lock( WINDOWPTR win, window_manager *wm )
{
    if( wm->screen_lock_counter == 0 )
    {
	pthread_mutex_lock( &wm->sdl_lock_mutex );
	if( SDL_MUSTLOCK( wm->sdl_screen ) ) 
	{
    	    if( SDL_LockSurface( wm->sdl_screen ) < 0 ) 
	    {
        	return;
	    }
        }
	framebuffer = (COLORPTR)wm->sdl_screen->pixels;
	wm->fb_ypitch = wm->sdl_screen->pitch / COLORLEN;
	wm->fb_xpitch = 1;
    }
    wm->screen_lock_counter++;

    if( wm->screen_lock_counter > 0 )
	wm->screen_is_active = 1;
    else
	wm->screen_is_active = 0;
}

void device_screen_unlock( WINDOWPTR win, window_manager *wm )
{
    if( wm->screen_lock_counter == 1 )
    {
	if( SDL_MUSTLOCK( wm->sdl_screen ) ) 
	{
	    SDL_UnlockSurface( wm->sdl_screen );
	}
	framebuffer = 0;
	pthread_mutex_unlock( &wm->sdl_lock_mutex );
    }

    /*if( win )
    {
	if( win->reg && win->reg->numRects )
    	{
    	    for( int r = 0; r < win->reg->numRects; r++ )
    	    {
        	int rx1 = win->reg->rects[ r ].left;
            	int rx2 = win->reg->rects[ r ].right;
            	int ry1 = win->reg->rects[ r ].top;
            	int ry2 = win->reg->rects[ r ].bottom;
		int xsize = rx2 - rx1;
		int ysize = ry2 - ry1;
		if( xsize > 0 && ysize > 0 )
		{
		    SDL_UpdateRect( wm->sdl_screen, rx1, ry1, xsize, ysize );
		}
    	    }
        }
    }*/
    
    if( wm->screen_lock_counter > 0 )
    {
	wm->screen_lock_counter--;
    }

    if( wm->screen_lock_counter > 0 )
	wm->screen_is_active = 1;
    else
	wm->screen_is_active = 0;
}

#ifdef FRAMEBUFFER

#include "wm_framebuffer.h"

#endif

//#################################
//#################################
//#################################

#endif
