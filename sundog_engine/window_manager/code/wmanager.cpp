/*
    wmanager.cpp. SunDog window manager
    This file is part of the SunDog engine.
    Copyright (C) 2002 - 2009 Alex Zolotov <nightradio@gmail.com>
*/

#include "../../main/user_code.h"
#include "../../core/debug.h"
#include "../../time/timemanager.h"
#include "../../utils/utils.h"
#include "../wmanager.h"
#include <stdarg.h>

#ifdef PALMOS
    #include <PalmOS.h>
    #include "palm_functions.h"
#endif

//Fonts (WIN1251)
#include "fonts.h"
sundog_image *g_font0_img = 0;
sundog_image *g_font1_img = 0;

sundog_event g_sundog_null_evt;

//################################
//## MAIN FUNCTIONS:            ##
//################################

int win_init( 
    const UTF8_CHAR *windowname, 
    int xsize, 
    int ysize, 
    int flags, 
    int argc, 
    UTF8_CHAR **argv,
    window_manager *wm )
{
    int retval = 0;

    wm->wm_initialized = 0;

    wm->argc = argc;
    wm->argv = argv;

    wm->events_count = 0;
    wm->current_event_num = 0;
    mem_set( &g_sundog_null_evt, sizeof( g_sundog_null_evt ), 0 );
    sundog_mutex_init( &wm->events_mutex, 0 );

    wm->flags = flags;

    wm->exit_request = 0;
    wm->exit_code = 0;

    wm->root_win = 0;

    wm->trash = 0;

    wm->window_counter = 0;

    mem_set( wm->timers, sizeof( wm->timers ), 0 );
    wm->timers_num = 0;

    wm->screen_flipped = 0;

    wm->screen_lock_counter = 0;
    wm->screen_is_active = 0;
    wm->screen_changed = 0;

    wm->pen_x = -1;
    wm->pen_y = -1;
    wm->focus_win = 0;

    wm->handler_of_unhandled_events = 0;

    wm->g_screen = 0;
    wm->cur_window = 0;
    wm->cur_font_color = COLORMASK;
    wm->cur_font_draw_bgcolor = false; 
    wm->cur_font_bgcolor = 0;
    wm->cur_transparency = 255;
    wm->cur_font_zoom = 256;

    //SECOND STEP: SCREEN SIZE SETTING and some device init
    dprint( "MAIN: device start\n" );

    wm->fb_offset = 0;
    wm->fb_xpitch = 1;
    wm->fb_ypitch = 0;
#ifdef OPENGL
    wm->gl_double_buffer = 1;
#endif
    int err = device_start( windowname, xsize, ysize, flags, wm ); //DEVICE DEPENDENT PART
    if( err )
    {
	dprint( "Error in device_start() %d\n", err );
	return 1; //Error
    }

    dprint( "MAIN: screen_xsize = %d\n", wm->screen_xsize );
    dprint( "MAIN: screen_ysize = %d\n", wm->screen_ysize );
      
    dprint( "MAIN: system palette init\n" );
    for( int c = 0; c < 16; c++ )
    {
	int r = c * 16;
	int g = c * 16;
	int b = c * 16;
	if( r >= 255 ) r = 255;
	if( g >= 255 ) g = 255;
	if( b >= 255 ) b = 255;
	if( r < 0 ) r = 0;
	if( g < 0 ) g = 0;
	if( b < 0 ) b = 0;
	wm->colors[ c ] = get_color( r, g, b );
    }
    wm->white = get_color( 255, 255, 255 );
    wm->black = get_color( 0, 0, 0 );
#ifdef GRAYSCALE
    wm->green = get_color( 64, 255, 64 );
    wm->yellow = get_color( 250, 250, 250 );
    wm->red = get_color( 230, 230, 230 );
#else
    wm->green = get_color( 0, 255, 0 );
    wm->yellow = get_color( 255, 255, 0 );
    wm->red = get_color( 255, 0, 0 );
#endif
    wm->dialog_color = wm->white;
    wm->decorator_color = blend( wm->white, wm->black, 44 );
    wm->decorator_border = blend( wm->black, wm->decorator_color, 90 );
    wm->button_color = wm->colors[ 12 ];
    wm->menu_color = blend( wm->white, wm->black, 24 );
    wm->selection_color = blend( wm->white, wm->green, 145 );
    wm->text_background = blend( wm->dialog_color, wm->black, 44 );
    wm->list_background = wm->text_background;
    wm->scroll_color = wm->colors[ 13 ];
    wm->scroll_background_color = wm->colors[ 11 ];

    wm->decor_border_size = 4;
    wm->decor_header_size = 14;
    wm->scrollbar_size = 18;
    wm->button_xsize = 70;
    if( wm->screen_xsize > 320 && wm->screen_ysize > 320 )
    {
	wm->gui_size_increment = 1;
    }
    else
    {
	wm->gui_size_increment = 0;
    }
    if( wm->gui_size_increment )
    {
	wm->decor_border_size += 1;
	wm->decor_header_size += 1;
	wm->scrollbar_size += 4;
	wm->button_xsize += 20;
    }

    wm->default_font = 1;
#if defined(OPENGL) && !defined(FRAMEBUFFER)
    //Create texture with default 8x8 font:
    uchar *tmp = (uchar*)MEM_NEW( HEAP_DYNAMIC, 8 * 8 * 256 );
    int img_ptr = 0;
    for( int y = 0; y < 8 * 256; y++ )
    {
	uchar v = g_font0[ y ];
	if( y >= 8 * 255 ) v = 0xFF;
	for( int x = 0; x < 8; x++ )
	{
	    if( v & 128 ) 
		tmp[ img_ptr ] = 255;
	    else
		tmp[ img_ptr ] = 0;
	    v <<= 1;
	    img_ptr++;
	}
    }
    g_font0_img = new_image( 8, 8 * 256, tmp, 0, 0, 8, 8 * 256, IMAGE_ALPHA8, wm );
    mem_free( tmp );
#endif

    dprint( "MAIN: wmanager initialized\n" );

    wm->wm_initialized = 1;

    return retval;
}

void win_close( window_manager *wm )
{
    //Clear trash with a deleted windows:
    if( wm->trash )
    {
	for( unsigned int a = 0; a < mem_get_size( wm->trash ) / sizeof( WINDOWPTR ); a++ )
	{
	    if( wm->trash[ a ] ) 
	    {
		if( wm->trash[ a ]->x1com ) mem_free( wm->trash[ a ]->x1com );
		if( wm->trash[ a ]->y1com ) mem_free( wm->trash[ a ]->y1com );
		if( wm->trash[ a ]->x2com ) mem_free( wm->trash[ a ]->x2com );
		if( wm->trash[ a ]->y2com ) mem_free( wm->trash[ a ]->y2com );
		mem_free( wm->trash[ a ] );
	    }
	}
	mem_free( wm->trash );
    }

    if( g_font0_img )
	remove_image( g_font0_img );
    if( g_font1_img )
	remove_image( g_font1_img );

    if( wm->screen_lock_counter > 0 )
    {
	dprint( "MAIN: WARNING. Screen is still locked (%d)\n", wm->screen_lock_counter );
	while( wm->screen_lock_counter > 0 ) device_screen_unlock( 0, wm );
    }

    if( wm->g_screen )
	mem_free( wm->g_screen );

    device_end( wm ); //DEVICE DEPENDENT PART (defined in eventloop.h)

    sundog_mutex_destroy( &wm->events_mutex );
}

//################################
//## WINDOWS FUNCTIONS:         ##
//################################

WINDOWPTR get_from_trash( window_manager *wm );

WINDOWPTR new_window( 
    const UTF8_CHAR *name, 
    int x, 
    int y, 
    int xsize, 
    int ysize, 
    COLOR color, 
    WINDOWPTR parent, 
    int (*win_handler)( sundog_event*, window_manager* ),
    window_manager *wm )
{
    //Create window structure:
    WINDOWPTR win = get_from_trash( wm );
    if( win == 0 ) 
    {
	win = (WINDOWPTR)mem_new( HEAP_DYNAMIC, sizeof( WINDOW ), name, 0 );
	mem_set( win, sizeof( WINDOW ), 0 );
	win->id = (int16)wm->window_counter;
	wm->window_counter++;
    }

    //Setup properties:
    win->name = name;
    win->x = x;
    win->y = y;
    win->xsize = xsize;
    win->ysize = ysize;
    win->color = color;
    win->parent = parent;
    win->win_handler = ( int (*)( void*, void* ) )win_handler;
    win->click_time = time_ticks() - time_ticks_per_second() * 10;

    win->controllers_calculated = 0;
    win->controllers_defined = 0;

    win->action_handler = 0;
    win->handler_data = 0;
    win->action_result = 0;
    
    win->font = wm->default_font;

    //Start init:
    if( win_handler )
    {
	sundog_event evt;
	mem_set( &evt, sizeof( evt ), 0 );
	evt.win = win;
	evt.type = EVT_GETDATASIZE;
	int datasize = win_handler( &evt, wm );
	if( datasize > 0 )
	{
	    win->data = mem_new( HEAP_DYNAMIC, datasize, "win data", 0 );
	}
	evt.type = EVT_AFTERCREATE;
	win_handler( &evt, wm );
    }

    //Save it to window manager:
    if( wm->root_win == 0 )
        wm->root_win = win;
    add_child( parent, win, wm );
    return win;
}

void set_window_controller( WINDOWPTR win, int ctrl_num, window_manager *wm, ... )
{
    va_list p;
    va_start( p, wm );
    int ptr = 0;
    win->controllers_defined = 1;
    int *cmds = 0;
    switch( ctrl_num )
    {
	case 0: cmds = win->x1com; break;
	case 1: cmds = win->y1com; break;
	case 2: cmds = win->x2com; break;
	case 3: cmds = win->y2com; break;
    }
    if( cmds == 0 )
	cmds = (int*)MEM_NEW( HEAP_DYNAMIC, sizeof( int ) * 4 );
    while( 1 )
    {
	int command = va_arg( p, int );
	if( mem_get_size( cmds ) / sizeof( int ) <= ptr )
	    cmds = (int*)mem_resize( cmds, sizeof( int ) * ( ptr + 4 ) );
	cmds[ ptr ] = command; 
	if( command == CEND ) break;
	ptr++;
    }
    switch( ctrl_num )
    {
	case 0: win->x1com = cmds; break;
	case 1: win->y1com = cmds; break;
	case 2: win->x2com = cmds; break;
	case 3: win->y2com = cmds; break;
    }
    va_end( p );
}

void move_to_trash( WINDOWPTR win, window_manager *wm )
{
    if( win )
    {
	win->visible = 0;
	win->flags |= WIN_FLAG_TRASH;
	if( wm->trash == 0 )
	{
	    wm->trash = (WINDOWPTR*)MEM_NEW( HEAP_DYNAMIC, sizeof( WINDOWPTR ) * 16 );
	    mem_set( wm->trash, sizeof( WINDOWPTR ) * 16, 0 );
	    wm->trash[ 0 ] = win;
	}
	else
	{
	    unsigned int w = 0;
	    for( w = 0; w < mem_get_size( wm->trash ) / sizeof( WINDOWPTR ); w++ )
	    {
		if( wm->trash[ w ] == 0 ) break;
	    }
	    if( w < mem_get_size( wm->trash ) / sizeof( WINDOWPTR ) ) 
		wm->trash[ w ] = win;
	    else
	    {
		wm->trash = (WINDOWPTR*)mem_resize( wm->trash, mem_get_size( wm->trash ) + sizeof( WINDOWPTR ) * 16 );
		wm->trash[ w ] = win;
	    }
	}
    }
}

WINDOWPTR get_from_trash( window_manager *wm )
{
    if( wm->trash == 0 ) return 0;
    unsigned int w = 0;
    for( w = 0; w < mem_get_size( wm->trash ) / sizeof( WINDOWPTR ); w++ )
    {
	if( wm->trash[ w ] != 0 ) 
	{
	    WINDOWPTR win = wm->trash[ w ];
	    wm->trash[ w ] = 0;
	    win->id++;
	    win->flags = 0;
	    return win;
	}
    }
    return 0;
}

void remove_window( WINDOWPTR win, window_manager *wm )
{
    if( win )
    {
	if( win->flags & WIN_FLAG_TRASH )
	{
	    dprint( "ERROR: can't remove already removed window (%s)\n", win->name );
	    return;
	}
	if( win->win_handler )
	{
	    //Sent EVT_BEFORECLOSE to window handler:
	    sundog_event evt;
	    mem_set( &evt, sizeof( evt ), 0 );
	    evt.win = win;
	    evt.type = EVT_BEFORECLOSE;
	    win->win_handler( &evt, wm );
	}
	if( win->childs )
	{
	    //Remove childs:
	    while( win->childs_num )
		remove_window( win->childs[ 0 ], wm );
	    mem_free( win->childs );
	    win->childs = 0;
	}
	//Remove data:
	if( win->data ) 
	    mem_free( win->data );
	win->data = 0;
	//Remove region:
	if( win->reg ) 
	    GdDestroyRegion( win->reg );
	win->reg = 0;
	//Remove commands:
	if( win->x1com ) mem_free( win->x1com ); win->x1com = 0;
	if( win->y1com ) mem_free( win->y1com ); win->y1com = 0;
	if( win->x2com ) mem_free( win->x2com ); win->x2com = 0;
	if( win->y2com ) mem_free( win->y2com ); win->y2com = 0;
	//Remove window:
	if( win == wm->focus_win )
	    wm->focus_win = 0;
	if( win == wm->root_win )
	{
	    //It's root win:
	    move_to_trash( win, wm );
	    wm->root_win = 0;
	}
	else
	{
	    remove_child( win->parent, win, wm );
	    //Full remove:
	    move_to_trash( win, wm );
	}
    }
}

void add_child( WINDOWPTR win, WINDOWPTR child, window_manager *wm )
{
    if( win == 0 || child == 0 ) return;

    if( win->childs == 0 )
    {
	win->childs = (WINDOWPTR*)mem_new( HEAP_DYNAMIC, 4 * 4, "childs", 0 );
    }
    else
    {
	int old_size = mem_get_size( win->childs ) / 4;
	if( win->childs_num >= old_size )
	    win->childs = (WINDOWPTR*)mem_resize( win->childs, ( old_size + 4 ) * 4 );
    }
    win->childs[ win->childs_num ] = child;
    win->childs_num++;
    child->parent = win;
}

void remove_child( WINDOWPTR win, WINDOWPTR child, window_manager *wm )
{
    if( win == 0 || child == 0 ) return;

    //Remove link from parent window:
    int c;
    for( c = 0; c < win->childs_num; c++ )
    {
	if( win->childs[ c ] == child ) break;
    }
    if( c < win->childs_num )
    {
	for( ; c < win->childs_num - 1; c++ )
	{
	    win->childs[ c ] = win->childs[ c + 1 ];
	}
	win->childs_num--;
	child->parent = 0;
    }
}

void set_handler( WINDOWPTR win, int (*handler)(void*,WINDOWPTR,window_manager*), void *handler_data, window_manager *wm )
{
    if( win )
    {
	win->action_handler = (int (*)(void*,void*,void*)) handler;;
	win->handler_data = handler_data;
    }
}

void draw_window( WINDOWPTR win, window_manager *wm )
{
    win_draw_lock( win, wm );
    if( win && win->visible && win->reg )
    {
	if( win->reg->numRects )
	{
	    sundog_event evt;
	    mem_set( &evt, sizeof( evt ), 0 );
    	    evt.win = win;
	    evt.type = EVT_DRAW;
	    if( win->win_handler && win->win_handler( &evt, wm ) )
	    {
		//Draw event was handled
	    }
	    else
	    {
		win_draw_box( win, 0, 0, win->xsize, win->ysize, win->color, wm );
	    }
	}
	if( win->childs_num )
	{
	    for( int c = 0; c < win->childs_num; c++ )
	    {
		draw_window( win->childs[ c ], wm );
	    }
	}
    }
    win_draw_unlock( win, wm );
}

void show_window( WINDOWPTR win, window_manager *wm )
{
    if( win && ( win->flags & WIN_FLAG_ALWAYS_INVISIBLE ) == 0 )
    {
	win->visible = 1;
	for( int c = 0; c < win->childs_num; c++ )
	{
	    show_window( win->childs[ c ], wm );
	}
    }
}

void hide_window( WINDOWPTR win, window_manager *wm )
{
    if( win )
    {
	win->visible = 0;
	for( int c = 0; c < win->childs_num; c++ )
	{
	    hide_window( win->childs[ c ], wm );
	}
    }
}

void recalc_controllers( WINDOWPTR win, window_manager *wm );

void run_controller( WINDOWPTR win, int *c, int *val, int size, window_manager *wm )
{
    if( c == 0 ) return;
    int p = 0;
    WINDOWPTR other_win = 0;
    int a;
    int mode = 0;
    int perc = 0;
    int backval = 0;
    int brk = 0;
    while( !brk )
    {
	switch( c[ p ] )
	{
	    case CWIN: 
		p++;
		other_win = (WINDOWPTR)c[ p ];
		if( other_win->controllers_calculated == 0 ) recalc_controllers( other_win, wm );
		break;
	    case CX1:
		a = other_win->x;
		switch( mode )
		{
		    case 0: *val = a; break;
		    case 1: *val -= a; break;
		    case 2: *val += a; break;
		    case 3: if( *val > a ) *val = a; break;
		    case 4: if( *val < a ) *val = a; break;
		}
		mode = 0;
		perc = 0;
		break;
	    case CY1: 
		a = other_win->y;
		switch( mode )
		{
		    case 0: *val = a; break;
		    case 1: *val -= a; break;
		    case 2: *val += a; break;
		    case 3: if( *val > a ) *val = a; break;
		    case 4: if( *val < a ) *val = a; break;
		}
		mode = 0;
		perc = 0;
		break;
	    case CX2:
		a = other_win->x + other_win->xsize;
		switch( mode )
		{
		    case 0: *val = a; break;
		    case 1: *val -= a; break;
		    case 2: *val += a; break;
		    case 3: if( *val > a ) *val = a; break;
		    case 4: if( *val < a ) *val = a; break;
		}
		mode = 0;
		perc = 0;
		break;
	    case CY2: 
		a = other_win->y + other_win->ysize;
		switch( mode )
		{
		    case 0: *val = a; break;
		    case 1: *val -= a; break;
		    case 2: *val += a; break;
		    case 3: if( *val > a ) *val = a; break;
		    case 4: if( *val < a ) *val = a; break;
		}
		mode = 0;
		perc = 0;
		break;
	    case CSUB: mode = 1; break;
	    case CADD: mode = 2; break;
	    case CPERC: perc = 1; break;
	    case CBACKVAL0: backval = 0; break;
	    case CBACKVAL1: backval = 1; break;
	    case CMAXVAL: mode = 3; break;
	    case CMINVAL: mode = 4; break;
	    case CPUTR0: wm->creg0 = *val; break;
	    case CGETR0: *val = wm->creg0; break;
	    case CEND: brk = 1; break;
	    default: 
		switch( mode )
		{
		    case 0:
			if( perc )
			{
			    *val = ( c[ p ] * size ) / 100;
			}
			else
			{
			    if( backval )
			    {
				*val = size - c[ p ];
			    }
			    else
			    {
				*val = c[ p ]; //Simple number
			    }
			}
			break;
		
		    case 1:
			//Sub:
			if( perc )
			{
			    *val -= ( c[ p ] * size ) / 100;
			}
			else
			{
			    if( backval )
			    {
				*val -= size - c[ p ];
			    }
			    else
			    {
				*val -= c[ p ]; //Simple number
			    }
			}
			break;

		    case 2:
			//Add:
			if( perc )
			{
			    *val += ( c[ p ] * size ) / 100;
			}
			else
			{
			    if( backval )
			    {
				*val += size - c[ p ];
			    }
			    else
			    {
				*val += c[ p ]; //Simple number
			    }
			}
			break;
		
		    case 3:
			//Max val:
			if( perc )
			{
			    if( *val > ( c[ p ] * size ) / 100 ) *val = ( c[ p ] * size ) / 100;
			}
			else
			{
			    if( backval )
			    {
				if( *val > size - c[ p ] ) *val = size - c[ p ];
			    }
			    else
			    {
				if( *val > c[ p ] ) *val = c[ p ]; //Simple number
			    }
			}
			break;	
		    
		    case 4:
			//Min val:
			if( perc )
			{
			    if( *val < ( c[ p ] * size ) / 100 ) *val = ( c[ p ] * size ) / 100;
			}
			else
			{
			    if( backval )
			    {
				if( *val < size - c[ p ] ) *val = size - c[ p ];
			    }
			    else
			    {
				if( *val < c[ p ] ) *val = c[ p ]; //Simple number
			    }
			}
			break;
		}
		mode = 0;
		perc = 0;
		break;
	}
	p++;
    }
}

void recalc_controllers( WINDOWPTR win, window_manager *wm )
{
    if( win && win->controllers_calculated == 0 && win->parent )
    {
	if( !win->controllers_defined ) 
	    win->controllers_calculated = 1;
	else
	{
	    int x1 = win->x;
	    int y1 = win->y;
	    int x2 = win->x + win->xsize;
	    int y2 = win->y + win->ysize;
	    run_controller( win, win->x1com, &x1, win->parent->xsize, wm );
	    run_controller( win, win->x2com, &x2, win->parent->xsize, wm );
	    run_controller( win, win->y1com, &y1, win->parent->ysize, wm );
	    run_controller( win, win->y2com, &y2, win->parent->ysize, wm );
	    int temp;
	    if( x1 > x2 ) { temp = x1; x1 = x2; x2 = temp; }
	    if( y1 > y2 ) { temp = y1; y1 = y2; y2 = temp; }
	    win->x = x1;
	    win->y = y1;
	    win->xsize = x2 - x1;
	    win->ysize = y2 - y1;
	    win->controllers_calculated = 1;
	}
    }
}

void recalc_region( WINDOWPTR win, MWCLIPREGION *reg, int cut_x, int cut_y, int cut_x2, int cut_y2, int px, int py, window_manager *wm )
{
    if( !win->visible )
    {
	if( win->reg ) GdDestroyRegion( win->reg );
	win->reg = 0;
	return;
    }
    if( win->controllers_defined && win->controllers_calculated == 0 )
    {
	recalc_controllers( win, wm );
    }
    win->screen_x = win->x + px;
    win->screen_y = win->y + py;
    int x1 = win->x + px;
    int y1 = win->y + py;
    int x2 = win->x + px + win->xsize;
    int y2 = win->y + py + win->ysize;
    if( cut_x > x1 ) x1 = cut_x;
    if( cut_y > y1 ) y1 = cut_y;
    if( cut_x2 < x2 ) x2 = cut_x2;
    if( cut_y2 < y2 ) y2 = cut_y2;
    if( win->childs_num && !( x1 > x2 || y1 > y2 ) )
    {
	for( int c = win->childs_num - 1; c >= 0; c-- )
	{
	    recalc_region( 
		win->childs[ c ], 
		reg, 
		x1, y1, 
		x2, y2, 
		win->x + px, 
		win->y + py, 
		wm );
	}
    }
    if( win->reg == 0 )
    {
	if( x1 > x2 || y1 > y2 )
	    win->reg = GdAllocRegion();
	else
	    win->reg = GdAllocRectRegion( x1, y1, x2, y2 );
    }
    else
    {
	if( x1 > x2 || y1 > y2 )
	{
	    GdSetRectRegion( win->reg, 0, 0, 0, 0 );
	}
	else
	{
	    GdSetRectRegion( win->reg, x1, y1, x2, y2 );
	}
    }
    //reg - current invisible region.
    //Calc corrected win region:
    GdSubtractRegion( win->reg, win->reg, reg );
    //Calc corrected invisible region:
    GdUnionRegion( reg, reg, win->reg );
}

void clean_regions( WINDOWPTR win, window_manager *wm )
{
    if( win )
    {
	for( int c = 0; c < win->childs_num; c++ )
	    clean_regions( win->childs[ c ], wm );
	if( win->reg ) 
	{
	    GdSetRectRegion( win->reg, 0, 0, 0, 0 );
	}
	win->controllers_calculated = 0;
    }
}

void recalc_regions( window_manager *wm )
{
    MWCLIPREGION *reg = GdAllocRegion();
    if( wm->root_win )
    {
	//Control ALWAYS_ON_TOP flag: ===
	int size = wm->root_win->childs_num;
	for( int i = 0; i < size; i++ )
	{
	    WINDOWPTR win = wm->root_win->childs[ i ];
	    if( win && win->flags & WIN_FLAG_ALWAYS_ON_TOP && i < size - 1 )
	    {
		//Bring this window to front:
		for( int i2 = i; i2 < size - 1; i2++ )
		{
		    wm->root_win->childs[ i2 ] = wm->root_win->childs[ i2 + 1 ];
		}
		wm->root_win->childs[ size - 1 ] = win;
	    }
	}
	//===============================
	clean_regions( wm->root_win, wm );
	recalc_region( wm->root_win, reg, 0, 0, wm->screen_xsize, wm->screen_ysize, 0, 0, wm );
    }
    if( reg ) GdDestroyRegion( reg );
}

void set_focus_win( WINDOWPTR win, window_manager *wm )
{
    if( win && win->flags & WIN_FLAG_TRASH )
    {
	//This window removed by someone. But we can't focus on removed window.
	//So.. Just focus on NULL window:
	win = 0;
    }

    WINDOWPTR prev_focus_win = wm->focus_win;
    uint16 prev_focus_win_id = wm->focus_win_id;

    WINDOWPTR new_focus_win = win;
    uint16 new_focus_win_id = 0;
    if( win ) new_focus_win_id = win->id;

    wm->focus_win = new_focus_win;
    wm->focus_win_id = new_focus_win_id;

    if( prev_focus_win != new_focus_win || 
	( prev_focus_win == new_focus_win && prev_focus_win_id != new_focus_win_id ) )
    {
	//Focus changed:

	sundog_event evt2;
	mem_set( &evt2, sizeof( evt2 ), 0 );

        wm->prev_focus_win = prev_focus_win;
        wm->prev_focus_win_id = prev_focus_win_id;

	if( new_focus_win )
	{
	    //Send FOCUS event:
	    //In this event's handling user can remember previous focused window (wm->prev_focus_win)
	    evt2.type = EVT_FOCUS;
	    evt2.win = new_focus_win;
	    handle_event( &evt2, wm );
	}

	if( prev_focus_win != new_focus_win ) //Only, if prev_focus_win is not removed
	{
	    if( prev_focus_win )
	    {
		//Send UNFOCUS event:
		evt2.type = EVT_UNFOCUS;
		evt2.win = prev_focus_win;
		handle_event( &evt2, wm );
	    }
	}
    }
}

int find_focus_window( WINDOWPTR win, WINDOW **focus_win, window_manager *wm )
{
    if( win == 0 ) return 0;
    if( win->reg && GdPtInRegion( win->reg, wm->pen_x, wm->pen_y ) )
    {
	*focus_win = win;
	return 1;
    }
    for( int c = 0; c < win->childs_num; c++ )
    {
	if( find_focus_window( win->childs[ c ], focus_win, wm ) )
	    return 1;
    }
    return 0;
}

int send_event(
    WINDOWPTR win,
    int type,
    int flags,
    int x,
    int y,
    int key,
    int scancode,
    int pressure,
    UTF32_CHAR unicode,
    window_manager* wm )
{
    int retval = 0;
    
    sundog_mutex_lock( &wm->events_mutex );
    
    if( wm->events_count + 1 <= EVENTS )
    {
	//Get pointer to new event:
	int new_ptr = ( wm->current_event_num + wm->events_count ) & ( EVENTS - 1 );

	if( type == EVT_BUTTONDOWN || type == EVT_BUTTONUP )
	{
	    if( key >= 0x41 && key <= 0x5A ) //Capital
		key += 0x20; //Make it small
	}

	//Save new event to FIFO buffer:
	wm->events[ new_ptr ].type = (uint16)type;
	wm->events[ new_ptr ].time = time_ticks();
	wm->events[ new_ptr ].win = win;
	wm->events[ new_ptr ].flags = (uint16)flags;
	wm->events[ new_ptr ].x = (int16)x;
	wm->events[ new_ptr ].y = (int16)y;
	wm->events[ new_ptr ].key = (uint16)key;
	wm->events[ new_ptr ].pressure = (uint16)pressure;
	wm->events[ new_ptr ].unicode = unicode;

	//Increment number of unhandled events:
	volatile int new_event_count = wm->events_count + 1;
	wm->events_count = new_event_count;

	retval = 0;
    }
    else
    {
	retval = 1;
    }

    sundog_mutex_unlock( &wm->events_mutex );
    
    return retval;
}

int check_event( sundog_event *evt, window_manager *wm )
{
    if( evt == 0 ) return 1;

    if( evt->win == 0 )
    {
	if( !wm->wm_initialized ) return 1;
	
	if( evt->type == EVT_MOUSEBUTTONDOWN ||
	    evt->type == EVT_MOUSEBUTTONUP ||
	    evt->type == EVT_MOUSEMOVE )
	{
	    if( evt->x < 0 ) return 1;
	    if( evt->y < 0 ) return 1;
	    if( evt->x >= wm->screen_xsize ) return 1;
	    if( evt->y >= wm->screen_ysize ) return 1;
	    wm->pen_x = evt->x;
	    wm->pen_y = evt->y;
	    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	    if( wm->last_unfocused_window )
	    {
		evt->win = wm->last_unfocused_window;
		if( evt->type == EVT_MOUSEBUTTONUP )
		{
		    wm->last_unfocused_window = 0;
		}
		return 0;
	    }
	    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	    if( evt->type == EVT_MOUSEBUTTONDOWN )
	    { //If mouse click:
		if( evt->key & MOUSE_BUTTON_SCROLLUP ||
		    evt->key & MOUSE_BUTTON_SCROLLDOWN )
		{
		    //Mouse scroll up/down...
		    WINDOWPTR scroll_win = 0;
		    if( find_focus_window( wm->root_win, &scroll_win, wm ) )
		    {
			evt->win = scroll_win;
			return 0;
		    }
		    else
		    {
			//Window not found under the pointer:
			return 1;
		    }
		}
		else
		{
		    //Mouse click on some window...
		    WINDOWPTR focus_win = 0;
		    if( find_focus_window( wm->root_win, &focus_win, wm ) )
		    {
			//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
			if( focus_win->flags & WIN_FLAG_ALWAYS_UNFOCUSED )
			{
			    evt->win = focus_win;
			    wm->last_unfocused_window = focus_win;
			    return 0;
			}
			//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
			set_focus_win( focus_win, wm );
		    }
		    else
		    {
			//Window not found under the pointer:
			return 1;
		    }
		}
	    }
	}
	if( wm->focus_win )
	{
	    //Set pointer to window:
	    evt->win = wm->focus_win;

	    if( evt->type == EVT_MOUSEBUTTONDOWN && 
		( evt->time - wm->focus_win->click_time ) < ( DOUBLE_CLICK_PERIOD * time_ticks_per_second() ) / 1000 )
	    {
		evt->type = EVT_MOUSEDOUBLECLICK;
		wm->focus_win->click_time = evt->time - time_ticks_per_second() * 10; //Reset click time
	    }
	    if( evt->type == EVT_MOUSEBUTTONDOWN ) wm->focus_win->click_time = evt->time;
	}
    }

    return 0;
}

void handle_event( sundog_event *evt, window_manager *wm )
{
    if( evt->type == EVT_MOUSEDOUBLECLICK )
    {
	evt->type = EVT_MOUSEBUTTONDOWN;
	handle_event( evt, wm );
	evt->type = EVT_MOUSEDOUBLECLICK;
    }
    if( !user_event_handler( evt, wm ) || ( evt->flags & EVT_FLAG_AC ) )
    {
	//Event hot handled by simple event handler.
	//Send it to window:
	if( handle_event_by_window( evt, wm ) == 0 )
	{
	    evt->win = wm->handler_of_unhandled_events;
	    handle_event_by_window( evt, wm );
	}
    }
}

int handle_event_by_window( sundog_event *evt, window_manager *wm )
{
    int retval = 0;
    WINDOWPTR win = evt->win;
    if( win )
    {
	if( win->flags & WIN_FLAG_TRASH )
	{
	    dprint( "ERROR: can't handle event by removed window (%s)\n", win->name );
	    retval = 1;
	}
	else
	if( evt->type == EVT_DRAW ) 
	{
	    draw_window( win, wm );
	    retval = 1;
	}
	else
	{
	    if( win->win_handler )
	    {
		if( !win->win_handler( evt, wm ) || ( evt->flags & EVT_FLAG_AC ) )
		{
		    //Send event to children:
		    for( int c = 0; c < win->childs_num; c++ )
		    {
			evt->win = win->childs[ c ];
			if( handle_event_by_window( evt, wm ) && !( evt->flags & EVT_FLAG_AC ) )
			{
			    evt->win = win;
			    retval = 1;
			    goto end_of_handle;
			}
		    }
		    evt->win = win;
		}
		else
		{
		    retval = 1;
		    goto end_of_handle;
		}
	    }
end_of_handle:
	    if( win == wm->root_win )
	    {
		if( evt->type == EVT_SCREENRESIZE )
		{
		    //On screen resize:
		    recalc_regions( wm );
		    draw_window( wm->root_win, wm );
		}
	    }
	}
    }
    return retval;
}

int EVENT_LOOP_BEGIN( sundog_event *evt, window_manager *wm )
{
    int rv = 0;
    if( wm->timers_num )
    {
	ticks_t cur_time = time_ticks();
	for( int t = 0; t < TIMERS; t++ )
	{
	    if( wm->timers[ t ].handler )
	    {
		if( wm->timers[ t ].deadline <= cur_time )
		{
		    wm->timers[ t ].handler( wm->timers[ t ].data, &wm->timers[ t ], wm ); 
		    wm->timers[ t ].deadline += wm->timers[ t ].delay;
		}
	    }
	}
    }
    device_event_handler( wm );
    if( wm->events_count )
    {
	sundog_mutex_lock( &wm->events_mutex );

	//There are unhandled events:
	//Copy current event to "evt" buffer (prepare it for handling):
	mem_copy( evt, &wm->events[ wm->current_event_num ], sizeof( sundog_event ) );
	//This event will be handled. So decrement count of events:
	wm->events_count--;
	//And increment FIFO pointer:
	wm->current_event_num = ( wm->current_event_num + 1 ) & ( EVENTS - 1 );

	sundog_mutex_unlock( &wm->events_mutex );
	
	//Check the event and handle it:
	if( check_event( evt, wm ) == 0 )
	{
	    handle_event( evt, wm );
	    rv = 1;
	}
    }
    return rv;
}

int EVENT_LOOP_END( window_manager *wm )
{
    user_event_handler( &g_sundog_null_evt, wm );
    device_redraw_framebuffer( wm );
    if( wm->exit_request ) return 1;
    return 0;
}

int add_timer( void (*handler)( void*, sundog_timer*, window_manager* ), void *data, ticks_t delay, window_manager *wm )
{
    int t = -1;
    for( t = 0; t < TIMERS; t++ )
    {
	if( wm->timers[ t ].handler == 0 )
	{
	    break;
	}
    }
    if( t < TIMERS )
    {
	wm->timers[ t ].handler = handler;
	wm->timers[ t ].data = data;
	wm->timers[ t ].deadline = time_ticks() + delay;
	wm->timers[ t ].delay = delay;
	wm->timers_num++;
    }
    else
    {
	t = -1;
    }
    return t;
}

void remove_timer( int timer, window_manager *wm )
{
    if( timer >= 0 && timer < TIMERS )
    {
	if( wm->timers[ timer ].handler )
	{
	    wm->timers[ timer ].handler = 0;
	    wm->timers_num--;
	}
    }
}

//################################
//## WINDOWS DECORATIONS:       ##
//################################

#define DRAG_LEFT	1
#define DRAG_RIGHT	2
#define DRAG_TOP	4
#define DRAG_BOTTOM	8
#define DRAG_MOVE	16
struct decorator_data
{
    int start_win_x;
    int start_win_y;
    int start_win_xs;
    int start_win_ys;
    int start_pen_x;
    int start_pen_y;
    int drag_mode;
    int fullscreen;
    int prev_x;
    int prev_y;
    int prev_xsize;
    int prev_ysize;
};

int decorator_handler( sundog_event *evt, window_manager *wm )
{
    int retval = 0;
    WINDOWPTR win = evt->win;
    decorator_data *data = (decorator_data*)win->data;
    int dx, dy;
    int rx = evt->x - win->screen_x;
    int ry = evt->y - win->screen_y;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( decorator_data );
	    break;
	case EVT_AFTERCREATE:
	    data->fullscreen = 0;
	    break;
	case EVT_MOUSEDOUBLECLICK:
	    if( evt->key == MOUSE_BUTTON_LEFT )
	    {
		//Make fullscreen:
		if( data->fullscreen == 1 )
		{
		    win->x = data->prev_x;
		    win->y = data->prev_y;
		    win->xsize = data->prev_xsize;
		    win->ysize = data->prev_ysize;
		}
		else
		{
		    data->prev_x = win->x;
		    data->prev_y = win->y;
		    data->prev_xsize = win->xsize;
		    data->prev_ysize = win->ysize;
		    win->x = 0; 
		    win->y = 0;
		    win->xsize = win->parent->xsize;
		    win->ysize = win->parent->ysize;
		}
		win->childs[ 0 ]->xsize = win->xsize - DECOR_BORDER_SIZE * 2;
		win->childs[ 0 ]->ysize = win->ysize - DECOR_BORDER_SIZE * 2 - DECOR_HEADER_SIZE;
		data->fullscreen ^= 1;
		data->drag_mode = 0;
		recalc_regions( wm );
		draw_window( wm->root_win, wm );
	    }
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONDOWN:
	    if( evt->key == MOUSE_BUTTON_LEFT )
	    {
		//Bring to front: ================================================
		int i;
		for( i = 0; i < win->parent->childs_num; i++ )
		{
		    if( win->parent->childs[ i ] == win ) break;
		}
		if( i < win->parent->childs_num - 1 )
		{
		    for( int i2 = i; i2 < win->parent->childs_num - 1; i2++ )
			win->parent->childs[ i2 ] = win->parent->childs[ i2 + 1 ];
		    win->parent->childs[ win->parent->childs_num - 1 ] = win;
		    recalc_regions( wm );
		}
		//================================================================
		data->start_pen_x = evt->x;
		data->start_pen_y = evt->y;
		data->start_win_x = win->x;
		data->start_win_y = win->y;
		data->start_win_xs = win->xsize;
		data->start_win_ys = win->ysize;
		data->drag_mode = 0;
		if( ry >= DECOR_HEADER_SIZE )
		{
		    if( rx < DECOR_BORDER_SIZE + 8 ) data->drag_mode |= DRAG_LEFT;
		    if( rx >= win->xsize - DECOR_BORDER_SIZE - 8 ) data->drag_mode |= DRAG_RIGHT;
		    if( ry >= win->ysize - DECOR_BORDER_SIZE - 8 ) data->drag_mode |= DRAG_BOTTOM;
		}
		else data->drag_mode = DRAG_MOVE;
		draw_window( win, wm );
	    }
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONUP:
	    data->drag_mode = 0;
	    retval = 1;
	    break;
	case EVT_MOUSEMOVE:
	    if( evt->key == MOUSE_BUTTON_LEFT )
	    {
		dx = evt->x - data->start_pen_x;
		dy = evt->y - data->start_pen_y;
		if( data->drag_mode == DRAG_MOVE )
		{
		    //Move:
		    int prev_x = win->x;
		    int prev_y = win->y;
		    win->x = data->start_win_x + dx;
		    win->y = data->start_win_y + dy;
		    if( prev_x != win->x || prev_y != win->y )
		    {
			if( data->fullscreen == 1 )
			{
			    win->xsize = data->prev_xsize;
			    win->ysize = data->prev_ysize;
			    data->fullscreen = 0;
			}
		    }
		}
		if( data->drag_mode & DRAG_LEFT )
		{
		    int prev_x = win->x;
		    int prev_xsize = win->xsize;
		    win->x = data->start_win_x + dx;
		    win->xsize = data->start_win_xs - dx;
		    if( prev_x != win->x || prev_xsize != win->xsize )
		    {
			if( win->xsize < 16 ) win->xsize = 16;
			data->fullscreen = 0;
		    }
		}
		if( data->drag_mode & DRAG_RIGHT )
		{
		    int prev_xsize = win->xsize;
		    win->xsize = data->start_win_xs + dx;
		    if( prev_xsize != win->xsize )
		    {
			if( win->xsize < 16 ) win->xsize = 16;
			data->fullscreen = 0;
		    }
		}
		if( data->drag_mode & DRAG_BOTTOM )
		{
		    int prev_ysize = win->ysize;
		    win->ysize = data->start_win_ys + dy;
		    if( prev_ysize != win->ysize )
		    {
			if( win->ysize < 16 ) win->ysize = 16;
			data->fullscreen = 0;
		    }
		}
		if( win->childs_num )
		{
		    win->childs[ 0 ]->xsize = win->xsize - DECOR_BORDER_SIZE * 2;
		    win->childs[ 0 ]->ysize = win->ysize - DECOR_BORDER_SIZE * 2 - DECOR_HEADER_SIZE;
		}
		recalc_regions( wm );
		draw_window( wm->root_win, wm );
	    }
	    retval = 1;
	    break;
	case EVT_DRAW:
	    win_draw_lock( win, wm );
	    win_draw_box( win, 0, 0, win->xsize, win->ysize, win->color, wm );
	    if( 0 )
	    {
		int ssize = string_size( win, win->childs[0]->name, wm );
		if( ssize > 0 )
		{
		    for( int ll = 0; ll < 8; ll++ )
			win_draw_line( win, ssize + 8 + ll, ll, win->xsize, ll, blend( wm->white, win->color, ll * 32 ), wm );
		}
		else
		{
		    for( int ll = 0; ll < 8; ll++ )
			win_draw_line( win, 0, ll, win->xsize, ll, blend( wm->white, win->color, ll * 32 ), wm );
		}
	    }
	    if( win->childs_num )
	    {
		win_draw_string( win, win->childs[ 0 ]->name, DECOR_BORDER_SIZE, ( DECOR_HEADER_SIZE + DECOR_BORDER_SIZE - char_y_size( win, wm ) ) / 2, wm->black, win->color, wm );
		win_draw_frame( win, 0, 0, win->xsize, win->ysize, wm->decorator_border, wm );
	    }
	    else
	    {
		//No childs more :( Empty decorator. Lets remove it:
		remove_window( win, wm );
		recalc_regions( wm );
		draw_window( wm->root_win, wm );
	    }
	    win_draw_unlock( win, wm );
	    retval = 1;
	    break;
    }
    return retval;
}

WINDOWPTR new_window_with_decorator( 
    const UTF8_CHAR *name, 
    int x, 
    int y, 
    int xsize, 
    int ysize, 
    COLOR color,
    WINDOWPTR parent, 
    int (*win_handler)( sundog_event*, window_manager* ),
    int flags,
    window_manager *wm )
{
    int border = DECOR_BORDER_SIZE;
    int header = DECOR_HEADER_SIZE;
    x -= border;
    y -= border + header;
    int dec_xsize = xsize + border * 2;
    int dec_ysize = ysize + border * 2 + header;
    if( flags & DECOR_FLAG_CENTERED )
    {
	x = ( parent->xsize - dec_xsize ) / 2;
	y = ( parent->ysize - dec_ysize ) / 2;
    }
    if( flags & DECOR_FLAG_CHECK_SIZE )
    {
	if( x < 0 )
	{
	    dec_xsize -= ( -x );
	    x = 0;
	}
	if( y < 0 )
	{
	    dec_ysize -= ( -y );
	    y = 0;
	}
	if( x + dec_xsize > parent->xsize )
	{
	    dec_xsize -= ( ( x + dec_xsize ) - parent->xsize );
	}
	if( y + dec_ysize > parent->ysize )
	{
	    dec_ysize -= ( ( y + dec_ysize ) - parent->ysize );
	}
    }
    WINDOWPTR dec = new_window( 
	"decorator", 
	x, y,
	dec_xsize, dec_ysize,
	wm->decorator_color,
	parent,
	decorator_handler,
	wm );
    xsize = dec_xsize - border * 2;
    ysize = dec_ysize - ( header + border * 2 );
    WINDOWPTR win = new_window( name, border, header + border, xsize, ysize, color, dec, win_handler, wm );
    return dec;
}

//################################
//## DRAWING FUNCTIONS:         ##
//################################

int char_x_size( WINDOWPTR win, UTF32_CHAR c, window_manager *wm )
{
    if( c <= 0x1F ) return 0;
    if( win->font == 0 )
    {
        return 8;
    }
    else
    {
        utf32_to_win1251( c, c );
        return g_font1[ 1 + c ];
    }
}

int char_y_size( WINDOWPTR win, window_manager *wm )
{
    if( win->font == 0 )
    {
        return 8;
    }
    else
    {
        return g_font1[ 0 ];
    }
}

int string_size( WINDOWPTR win, const UTF8_CHAR *str, window_manager *wm )
{
    int size = 0;
    while( *str )
    {
	UTF32_CHAR c32;
	str += utf8_to_utf32_char( str, &c32 );
	size += char_x_size( win, c32, wm );
    }    
    return size;
}

void win_draw_lock( WINDOWPTR win, window_manager *wm )
{
    wm->screen_changed++;
    device_screen_lock( win, wm );
}

void win_draw_unlock( WINDOWPTR win, window_manager *wm )
{
    device_screen_unlock( win, wm );
}

void win_draw_box( WINDOWPTR win, int x, int y, int xsize, int ysize, COLOR color, window_manager *wm )
{
    if( win && win->visible && win->reg && win->reg->numRects )
    {
	x += win->screen_x;
	y += win->screen_y;
	if( win->reg->numRects )
	{
	    for( int r = 0; r < win->reg->numRects; r++ )
	    {
		int rx1 = win->reg->rects[ r ].left;
		int rx2 = win->reg->rects[ r ].right;
		int ry1 = win->reg->rects[ r ].top;
		int ry2 = win->reg->rects[ r ].bottom;

		//Control box size:
		int nx = x;
		int ny = y;
		int nxsize = xsize;
		int nysize = ysize;
		if( nx < rx1 ) { nxsize -= ( rx1 - nx ); nx = rx1; }
		if( ny < ry1 ) { nysize -= ( ry1 - ny ); ny = ry1; }
		if( nx + nxsize <= rx1 ) continue;
		if( ny + nysize <= ry1 ) continue;
		if( nx + nxsize > rx2 ) nxsize -= nx + nxsize - rx2;
		if( ny + nysize > ry2 ) nysize -= ny + nysize - ry2;
		if( nx >= rx2 ) continue;
		if( ny >= ry2 ) continue;
		if( nxsize < 0 ) continue;
		if( nysize < 0 ) continue;
        	
		//Draw it:
		device_draw_box( nx, ny, nxsize, nysize, color, wm );
	    }
	}
    }
}

void win_draw_frame( WINDOWPTR win, int x, int y, int xsize, int ysize, COLOR color, window_manager *wm )
{
    win_draw_line( win, x, y, x + xsize - 1, y, color, wm );
    win_draw_line( win, x + xsize - 1, y, x + xsize - 1, y + ysize - 1, color, wm );
    win_draw_line( win, x + xsize - 1, y + ysize - 1, x, y + ysize - 1, color, wm );
    win_draw_line( win, x, y + ysize - 1, x, y, color, wm );
}

void win_draw_frame3d( WINDOWPTR win, int x, int y, int xsize, int ysize, COLOR color, int inout, window_manager *wm )
{
    int depth = inout >> 8;
    inout &= 255;
    int tr = 110;
    int wtr = 120;
    COLOR white = wm->white;
    COLOR black = wm->black;
    /*if( depth == 2 )
    {
	win_draw_line( win, x + 1, y + 1, x + xsize - 2, y + 1, blend( color, wm->white, wtr / 2 ), wm );
	win_draw_line( win, x + 1, y + ysize - 2, x + xsize - 2, y + ysize - 2, blend( color, wm->black, tr / 2 ), wm );
    }*/
    for( int l = 0; l < 1; l++ )
    {
	if( inout == 0 )
	{
	    win_draw_line( win, x, y, x + xsize - 1, y, blend( color, black, tr ), wm );
	    win_draw_line( win, x + xsize - 1, y, x + xsize - 1, y + ysize - 1, blend( color, black, tr ), wm );
	    win_draw_line( win, x, y, x, y + ysize - 1, blend( color, white, wtr ), wm );
	    win_draw_line( win, x, y + ysize - 1, x + xsize - 1, y + ysize - 1, blend( color, white, wtr ), wm );
	}
	else
	{
	    win_draw_line( win, x, y, x + xsize - 1, y, blend( color, white, wtr ), wm );
	    win_draw_line( win, x + xsize - 1, y, x + xsize - 1, y + ysize - 1, blend( color, white, wtr ), wm );
	    win_draw_line( win, x, y, x, y + ysize - 1, blend( color, black, tr ), wm );
	    win_draw_line( win, x, y + ysize - 1, x + xsize - 1, y + ysize - 1, blend( color, black, tr ), wm );
	}
	x++; 
	y++;
	xsize -= 2;
	ysize -= 2;
	tr >>= 1;
	wtr >>= 1;
    }
}

void win_draw_bitmap_ext( 
    WINDOWPTR win, 
    int x, 
    int y, 
    int dest_xsize, 
    int dest_ysize,
    int source_x,
    int source_y,
    sundog_image *data, 
    window_manager *wm )
{
    if( source_x < 0 ) { dest_xsize += source_x; x -= source_x; source_x = 0; }
    if( source_y < 0 ) { dest_ysize += source_y; y -= source_y; source_y = 0; }
    if( source_x >= data->xsize ) return;
    if( source_y >= data->ysize ) return;
    if( source_x + dest_xsize > data->xsize ) dest_xsize -= ( source_x + dest_xsize ) - data->xsize;
    if( source_y + dest_ysize > data->ysize ) dest_ysize -= ( source_y + dest_ysize ) - data->ysize;
    if( dest_xsize <= 0 ) return;
    if( dest_ysize <= 0 ) return;
    if( win && win->visible && win->reg && win->reg->numRects )
    {
	x += win->screen_x;
	y += win->screen_y;
	int xsize = dest_xsize;
	int ysize = dest_ysize;
	if( win->reg->numRects )
	{
	    for( int r = 0; r < win->reg->numRects; r++ )
	    {
		int rx1 = win->reg->rects[ r ].left;
		int rx2 = win->reg->rects[ r ].right;
		int ry1 = win->reg->rects[ r ].top;
		int ry2 = win->reg->rects[ r ].bottom;

		//Control box size:
		int src_x = source_x;
		int src_y = source_y;
		int nx = x;
		int ny = y;
		int nxsize = xsize;
		int nysize = ysize;
		if( nx < rx1 ) { nxsize -= ( rx1 - nx ); src_x += ( rx1 - nx ); nx = rx1; }
		if( ny < ry1 ) { nysize -= ( ry1 - ny ); src_y += ( ry1 - ny ); ny = ry1; }
		if( nx + nxsize <= rx1 ) continue;
		if( ny + nysize <= ry1 ) continue;
		if( nx + nxsize > rx2 ) nxsize -= nx + nxsize - rx2;
		if( ny + nysize > ry2 ) nysize -= ny + nysize - ry2;
		if( nx >= rx2 ) continue;
		if( ny >= ry2 ) continue;
		if( nxsize < 0 ) continue;
		if( nysize < 0 ) continue;
        	
		//Draw it:
		device_draw_bitmap( nx, ny, nxsize, nysize, src_x, src_y, data, wm );
	    }
	}
    }
}

void win_draw_bitmap( 
    WINDOWPTR win, 
    int x, 
    int y, 
    sundog_image *data, 
    window_manager *wm )
{
    if( win && win->visible && win->reg && win->reg->numRects )
    {
	x += win->screen_x;
	y += win->screen_y;
	if( win->reg->numRects )
	{
	    for( int r = 0; r < win->reg->numRects; r++ )
	    {
		int rx1 = win->reg->rects[ r ].left;
		int rx2 = win->reg->rects[ r ].right;
		int ry1 = win->reg->rects[ r ].top;
		int ry2 = win->reg->rects[ r ].bottom;

		//Control box size:
		int src_x = 0;
		int src_y = 0;
		int nx = x;
		int ny = y;
		int nxsize = data->xsize;
		int nysize = data->ysize;
		if( nx < rx1 ) { nxsize -= ( rx1 - nx ); src_x += ( rx1 - nx ); nx = rx1; }
		if( ny < ry1 ) { nysize -= ( ry1 - ny ); src_y += ( ry1 - ny ); ny = ry1; }
		if( nx + nxsize <= rx1 ) continue;
		if( ny + nysize <= ry1 ) continue;
		if( nx + nxsize > rx2 ) nxsize -= nx + nxsize - rx2;
		if( ny + nysize > ry2 ) nysize -= ny + nysize - ry2;
		if( nx >= rx2 ) continue;
		if( ny >= ry2 ) continue;
		if( nxsize < 0 ) continue;
		if( nysize < 0 ) continue;
        	
		//Draw it:
		device_draw_bitmap( nx, ny, nxsize, nysize, src_x, src_y, data, wm );
	    }
	}
    }
}

void win_draw_char( WINDOWPTR win, UTF32_CHAR c, int x, int y, COLOR f, COLOR b, window_manager *wm )
{
    if( c < 0x10 ) return; //Control symbols
    //Default SunDog font is win1251.
    //Convert from UTF32 to WIN1251:
    utf32_to_win1251( c, c );
    //Draw:
    uchar xsize;
    uchar ysize;
    uchar *font;
    if( win->font == 0 )
    {
	font = g_font0;
	xsize = 8;
	ysize = 8;
    }
    else
    {
	font = g_font1;
	xsize = font[ 1 + c ];
	ysize = font[ 0 ];
    }
#if defined(OPENGL) && !defined(FRAMEBUFFER)
    sundog_image *font_img;
    if( win->font == 0 )
    {
    	font_img = g_font0_img;
    }
    else
    {
	font_img = g_font1_img;
    }
    font_img->color = b;
    font_img->transp = 255;
    win_draw_bitmap_ext( 
        win, 
        x, y, 
        xsize, ysize,
        0, 255 * 8,
        font_img, 
        wm );
    font_img->color = f;
    win_draw_bitmap_ext( 
        win, 
        x, y, 
        xsize, ysize,
        0, c * 8,
        g_font0_img, 
        wm );
#else
    sundog_image tmp_img;
    tmp_img.xsize = xsize;
    tmp_img.ysize = ysize;
    tmp_img.flags = IMAGE_NATIVE_RGB | IMAGE_STATIC_SOURCE;
    COLOR bmp[ 8 * 12 ];
    tmp_img.data = bmp;
    c *= ysize;
    int ptr = 0;
    font += 257;
    for( int l = 0; l < ysize; l++ )
    {
        uchar v = font[ c ]; c++;
	for( int x = 0; x < xsize; x++ )
	{
	    if( v & 0x80 )
	        bmp[ ptr ] = f;
	    else
	        bmp[ ptr ] = b;
	    v <<= 1;
	    ptr++;
	}
    }
    win_draw_bitmap( win, x, y, &tmp_img, wm );
#endif
}

void win_draw_string( WINDOWPTR win, const UTF8_CHAR *str, int x, int y, COLOR f, COLOR b, window_manager *wm )
{
    int start_x = x;
    while( *str )
    {
	if( *str == 0xA ) 
	{ 
	    y += char_y_size( win, wm ); 
	    x = start_x; 
	    str++;
	}
	else
	{
	    UTF32_CHAR c32;
	    str += utf8_to_utf32_char( str, &c32 );
	    win_draw_char( win, c32, x, y, f, b, wm );
	    x += char_x_size( win, c32, wm );
	}
    }
}

#define cbottom 1
#define ctop 2
#define cleft 4
#define cright 8
int make_code( int x, int y, int clip_x1, int clip_y1, int clip_x2, int clip_y2 )
{
    int code = 0;
    if( y >= clip_y2 ) code = cbottom;
    else if( y < clip_y1 ) code = ctop;
    if( x >= clip_x2 ) code += cright;
    else if( x < clip_x1 ) code += cleft;
    return code;
}

void draw_line2( 
    int x1, int y1,
    int x2, int y2, 
    int clip_x1, int clip_y1,
    int clip_x2, int clip_y2,
    COLOR color, window_manager *wm )
{
    //Cohen-Sutherland line clipping algorithm:
    int code0;
    int code1;
    int out_code;
    int x, y;
    code0 = make_code( x1, y1, clip_x1, clip_y1, clip_x2, clip_y2 );
    code1 = make_code( x2, y2, clip_x1, clip_y1, clip_x2, clip_y2 );
    while( code0 || code1 )
    {
	if( code0 & code1 ) return; //Trivial reject
	else
	{
	    //Failed both tests, so calculate the line segment to clip
	    if( code0 )
		out_code = code0; //Clip the first point
	    else
		out_code = code1; //Clip the last point

	    if( out_code & cbottom )
	    {
		//Clip the line to the bottom of the viewport
		y = clip_y2 - 1;
		x = x1 + ( x2 - x1 ) * ( y - y1 ) / ( y2 - y1 );
	    }
	    else 
	    if( out_code & ctop )
	    {
		y = clip_y1;
		x = x1 + ( x2 - x1 ) * ( y - y1 ) / ( y2 - y1 );
	    }
	    else
	    if( out_code & cright )
	    {
		x = clip_x2 - 1;
		y = y1 + ( y2 - y1 ) * ( x - x1 ) / ( x2 - x1 );
	    }
	    else
	    if( out_code & cleft )
	    {
		x = clip_x1;
		y = y1 + ( y2 - y1 ) * ( x - x1 ) / ( x2 - x1 );
	    }

	    if( out_code == code0 )
	    { //Modify the first coordinate 
		x1 = x; y1 = y;
		code0 = make_code( x1, y1, clip_x1, clip_y1, clip_x2, clip_y2 );
	    }
	    else
	    { //Modify the second coordinate
		x2 = x; y2 = y;
		code1 = make_code( x2, y2, clip_x1, clip_y1, clip_x2, clip_y2 );
	    }
	}
    }

    //Draw line:
    device_draw_line( x1, y1, x2, y2, color, wm );
}

void win_draw_line( WINDOWPTR win, int x1, int y1, int x2, int y2, COLOR color, window_manager *wm )
{
    if( win && win->visible && win->reg && win->reg->numRects )
    {
	x1 += win->screen_x;
	y1 += win->screen_y;
	x2 += win->screen_x;
	y2 += win->screen_y;
	if( win->reg->numRects )
	{
	    for( int r = 0; r < win->reg->numRects; r++ )
	    {
		int rx1 = win->reg->rects[ r ].left;
		int rx2 = win->reg->rects[ r ].right;
		int ry1 = win->reg->rects[ r ].top;
		int ry2 = win->reg->rects[ r ].bottom;

		draw_line2( x1, y1, x2, y2, rx1, ry1, rx2, ry2, color, wm );
	    }
	}
    }
}

//###################################
//### DIALOGS:                    ###
//###################################

UTF8_CHAR dialog_filename[ 1024 ];

UTF8_CHAR *dialog_open_file( const UTF8_CHAR *name, const UTF8_CHAR *mask, const UTF8_CHAR *id, window_manager *wm )
{
    WINDOWPTR prev_focus = wm->focus_win;

    UTF8_CHAR *path = get_user_path();
    int path_len = mem_strlen( (const UTF8_CHAR*)path );
    int id_len = mem_strlen( id );
    UTF8_CHAR *id2 = (UTF8_CHAR*)MEM_NEW( HEAP_DYNAMIC, path_len + id_len + 1 );
    id2[ 0 ] = 0;
    mem_strcat( id2, (const UTF8_CHAR*)path );
    mem_strcat( id2, id );

    FILES_PROPS = (const UTF8_CHAR*)id2;
    FILES_MASK = mask;
    FILES_RESULTED_FILENAME = dialog_filename;
    WINDOWPTR win = new_window_with_decorator( 
	name, 
	0, 0, 
	340, 320, 
	wm->dialog_color,
	wm->root_win, 
	files_handler,
	DECOR_FLAG_CENTERED | DECOR_FLAG_CHECK_SIZE,
	wm );
    show_window( win, wm );
    recalc_regions( wm );
    draw_window( win, wm );

    while( 1 )
    {
	sundog_event evt;
	EVENT_LOOP_BEGIN( &evt, wm );
	if( EVENT_LOOP_END( wm ) ) break;
	if( win->visible == 0 ) break;
    }

    set_focus_win( prev_focus, wm );
    
    mem_free( id2 );

    if( dialog_filename[ 0 ] == 0 ) return 0;
    return dialog_filename;
}

int dialog( const UTF8_CHAR *name, const UTF8_CHAR *buttons, window_manager *wm )
{
    WINDOWPTR prev_focus = wm->focus_win;
    int result = 0;

    DIALOG_BUTTONS_TEXT = buttons;
    DIALOG_TEXT = name;
    DIALOG_RESULT = &result;
    WINDOWPTR win = new_window_with_decorator( 
	"", 
	0, 0, 
	320, 190,
	wm->dialog_color, 
	wm->root_win, 
	dialog_handler,
	DECOR_FLAG_CENTERED | DECOR_FLAG_CHECK_SIZE,
	wm );
    show_window( win, wm );
    recalc_regions( wm );
    draw_window( win, wm );

    while( 1 )
    {
	sundog_event evt;
	EVENT_LOOP_BEGIN( &evt, wm );
	if( EVENT_LOOP_END( wm ) ) break;
	if( win->visible == 0 ) break;
    }

    set_focus_win( prev_focus, wm );

    return result;
}

//###################################
//### DEVICE DEPENDENT FUNCTIONS: ###
//###################################

// device_start(), device_end() and device_event_handler() :

void fix_fullscreen_resolution( int *xsize, int *ysize, window_manager *wm )
{
    if( *xsize <= 320 ) { *xsize = 320; *ysize = 200; }
    else if( *xsize <= 640 ) { *xsize = 640; *ysize = 480; }
    else if( *xsize <= 800 ) { *xsize = 800; *ysize = 600; }
    else if( *xsize <= 1024 ) { *xsize = 1024; *ysize = 768; }
    else if( *xsize <= 1152 ) { *xsize = 1152; *ysize = 864; }
    else if( *xsize <= 1280 ) 
    {
	*xsize = 1280;
	if( *ysize <= 720 ) *ysize = 720;
	else if( *ysize <= 768 ) *ysize = 768;
	else if( *ysize <= 960 ) *ysize = 960;
	else if( *ysize <= 1024 ) *ysize = 1024;
    }
}

#ifndef FRAMEBUFFER
#endif

#ifndef NONPALM
    #include "wm_palmos.h"
#endif

#ifdef WIN
    #include "wm_win32.h"
#endif

#ifdef WINCE
    #include "wm_wince.h"
#endif

#ifdef UNIX
    #if defined(OPENGL) || defined(X11)
	#include "wm_unixgraphics.h"
    #endif
    #ifdef DIRECTDRAW
	#include "wm_unixgraphics_sdl.h"
    #endif
#endif

//###################################
//###################################
//###################################
