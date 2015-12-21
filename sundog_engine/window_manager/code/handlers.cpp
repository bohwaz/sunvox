/*
    handlers.cpp. Standart window handlers
    This file is part of the SunDog engine.
    Copyright (C) 2002 - 2009 Alex Zolotov <nightradio@gmail.com>
*/

#include "../../core/debug.h"
#include "../../time/timemanager.h"
#include "../../utils/utils.h"
#include "../../main/user_code.h"
#include "../wmanager.h"

int window_handler_check_data( sundog_event *evt, window_manager *wm )
{
    if( evt->type != EVT_GETDATASIZE )
    {
	if( evt->win->data == 0 )
	{
	    dprint( "ERROR: Event to window (%s) without data\n", evt->win->name );
	    return 1;
	}
    }
    return 0;
}

int null_handler( sundog_event *evt, window_manager *wm )
{
    return 0;
}

int desktop_handler( sundog_event *evt, window_manager *wm )
{
    int retval = 0;
    WINDOWPTR win = evt->win;
    switch( evt->type )
    {
	case EVT_SCREENRESIZE:
	    win->x = 0;
	    win->y = 0;
	    win->xsize = wm->screen_xsize;
	    win->ysize = wm->screen_ysize;
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONDOWN:
	case EVT_MOUSEDOUBLECLICK:
	case EVT_MOUSEMOVE:
	case EVT_MOUSEBUTTONUP:
	    retval = 1;
	    break;
    }
    return retval;
}

#define DIVIDER_BINDS 2

struct divider_data
{
    int start_x;
    int start_y;
    int start_wx;
    int start_wy;
    int pushed;
    COLOR color;
    WINDOWPTR binds[ DIVIDER_BINDS ];
    int binds_x[ DIVIDER_BINDS ];
    int binds_y[ DIVIDER_BINDS ];
};

int divider_handler( sundog_event *evt, window_manager *wm )
{
    int retval = 0;
    WINDOWPTR win = evt->win;
    divider_data *data = (divider_data*)win->data;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( divider_data );
	    break;
	case EVT_AFTERCREATE:
	    data->pushed = 0;
	    data->color = win->color;
	    {
		for( int a = 0; a < DIVIDER_BINDS;a++ )
		    data->binds[ a ] = 0;
	    }
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONDOWN:
	    if( evt->key == MOUSE_BUTTON_LEFT )
	    {
		data->start_x = evt->x;
		data->start_y = evt->y;
		data->start_wx = win->x;
		data->start_wy = win->y;
		for( int a = 0; a < DIVIDER_BINDS; a++ )
		{
		    if( data->binds[ a ] )
		    {
			data->binds_x[ a ] = data->binds[ a ]->x;
			data->binds_y[ a ] = data->binds[ a ]->y;
		    }
		}
		data->pushed = 1;
		win->color = PUSHED_COLOR( data->color );
		draw_window( win, wm );
		retval = 1;
	    }
	    break;
	case EVT_MOUSEMOVE:
	    if( evt->key == MOUSE_BUTTON_LEFT )
	    {
		int dx = evt->x - data->start_x;
		int dy = evt->y - data->start_y;
		divider_data *bind_data = 0;
		win->x = data->start_wx + dx;
		win->y = data->start_wy + dy;
		for( int a = 0; a < DIVIDER_BINDS; a++ )
		{
		    if( data->binds[ a ] )
		    {
			data->binds[ a ]->x = data->binds_x[ a ] + dx;
			data->binds[ a ]->y = data->binds_y[ a ] + dy;
		    }
		}
		recalc_regions( wm );
		draw_window( wm->root_win, wm );
		retval = 1;
	    }
	    break;
	case EVT_MOUSEBUTTONUP:
	    if( evt->key == MOUSE_BUTTON_LEFT )
	    {
		data->pushed = 0;
		win->color = data->color;
		draw_window( wm->root_win, wm );
		retval = 1;
	    }
	    break;
	case EVT_DRAW:
	    win_draw_lock( win, wm );
	    {
		int cx = win->xsize / 2;
		int cy = win->ysize / 2;
		COLOR color = win->color;
		win_draw_box( win, 1, 1, win->xsize - 2, win->ysize - 2, color, wm );
		win_draw_box( win, cx - 2, cy - 2, 2, 2, blend( color, wm->black, 64 ), wm );
		win_draw_box( win, cx + 1, cy - 2, 2, 2, blend( color, wm->black, 64 ), wm );
		win_draw_box( win, cx - 2, cy + 1, 2, 2, blend( color, wm->black, 64 ), wm );
		win_draw_box( win, cx + 1, cy + 1, 2, 2, blend( color, wm->black, 64 ), wm );
		win_draw_frame3d( win, 0, 0, win->xsize, win->ysize, color, 1, wm );
	    }
	    win_draw_unlock( win, wm );
	    retval = 1;
	    break;
    }
    return retval;
}

void bind_divider_to( WINDOWPTR win, WINDOWPTR bind_to, int bind_num, window_manager *wm )
{
    divider_data *data = (divider_data*)win->data;
    data->binds[ bind_num ] = bind_to;
}

struct label_data 
{
    WINDOWPTR prev_focus_win;
};

int label_handler( sundog_event *evt, window_manager *wm )
{
    if( window_handler_check_data( evt, wm ) ) return 0;
    int retval = 0;
    WINDOWPTR win = evt->win;
    label_data *data = (label_data*)win->data;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( label_data );
	    break;
	case EVT_FOCUS:
	    data->prev_focus_win = wm->prev_focus_win;
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONUP:
	    set_focus_win( data->prev_focus_win, wm );
	    retval = 1;
	    break;
	case EVT_DRAW:
	    win_draw_lock( win, wm );
	    win_draw_box( win, 0, 0, win->xsize, win->ysize, win->parent->color, wm );
	    //Draw name:
	    {
		int ty = ( win->ysize - char_y_size( win, wm ) ) / 2;
		win_draw_string( win, win->name, 0, ty, get_color(0,0,0), win->parent->color, wm );
	    }
	    retval = 1;
	    win_draw_unlock( win, wm );
	    break;
    }
    return retval;
}

int TEXT_CALL_HANDLER_ON_ANY_CHANGES = 0;
int CREATE_NUMERIC_TEXT;
int CREATE_NUMERIC_TEXT_MIN;
int CREATE_NUMERIC_TEXT_MAX;

struct text_data 
{
    WINDOWPTR this_window;
    UTF32_CHAR *text;
    UTF8_CHAR *output_str;
    int cur_pos;
    int active;
    int call_handler_on_any_changes;
    int numeric;
    int min;
    int max;
    WINDOWPTR prev_focus_win;
};

int text_dec_handler( void *user_data, WINDOWPTR win, window_manager *wm )
{
    text_data *data = (text_data*)user_data;
    int val = text_get_value( data->this_window, wm );
    val--;
    text_set_value( data->this_window, val, wm );
    if( data->this_window->action_handler )
        data->this_window->action_handler( data->this_window->handler_data, win, wm );
    draw_window( data->this_window, wm );
    return 0;
}

int text_inc_handler( void *user_data, WINDOWPTR win, window_manager *wm )
{
    text_data *data = (text_data*)user_data;
    int val = text_get_value( data->this_window, wm );
    val++;
    text_set_value( data->this_window, val, wm );
    if( data->this_window->action_handler )
        data->this_window->action_handler( data->this_window->handler_data, win, wm );
    draw_window( data->this_window, wm );
    return 0;
}

int text_handler( sundog_event *evt, window_manager *wm )
{
    if( window_handler_check_data( evt, wm ) ) return 0;
    int retval = 0;
    WINDOWPTR win = evt->win;
    text_data *data = (text_data*)win->data;
    COLOR col, col2;
    int p, i;
    UTF32_CHAR c;
    int xp;
    int rx = evt->x - win->screen_x;
    int ry = evt->y - win->screen_y;
    int changes = 0;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( text_data );
	    break;
	case EVT_AFTERCREATE:
	    data->this_window = win;
	    data->output_str = 0;
	    data->text = (UTF32_CHAR*)MEM_NEW( HEAP_DYNAMIC, 32 * sizeof( UTF32_CHAR ) );
	    data->text[ 0 ] = 0;
	    data->cur_pos = 0;
	    data->active = 0;
	    data->call_handler_on_any_changes = TEXT_CALL_HANDLER_ON_ANY_CHANGES;
	    data->numeric = CREATE_NUMERIC_TEXT;
	    data->min = CREATE_NUMERIC_TEXT_MIN;
	    data->max = CREATE_NUMERIC_TEXT_MAX;
	    if( data->numeric )
	    {
		CREATE_FLAT_BUTTON = 1;
		CREATE_BUTTON_WITH_AUTOREPEAT = 1;
		WINDOWPTR b = new_window( "-", 1, 1, 1, 1, win->color, win, button_handler, wm );
		set_window_controller( b, 0, wm, CPERC, 100, CSUB, SCROLLBAR_SIZE * 2 + 1, CEND );
		set_window_controller( b, 1, wm, 1, CEND );
		set_window_controller( b, 2, wm, CPERC, 100, CSUB, SCROLLBAR_SIZE * 1 + 1, CEND );
	        set_window_controller( b, 3, wm, CPERC, 100, CSUB, 1, CEND );
		set_handler( b, text_dec_handler, data, wm );
		CREATE_FLAT_BUTTON = 1;
		CREATE_BUTTON_WITH_AUTOREPEAT = 1;
		b = new_window( "+", 1, 1, 1, 1, win->color, win, button_handler, wm );
		set_window_controller( b, 0, wm, CPERC, 100, CSUB, SCROLLBAR_SIZE * 1 + 1, CEND );
		set_window_controller( b, 1, wm, 1, CEND );
		set_window_controller( b, 2, wm, CPERC, 100, CSUB, SCROLLBAR_SIZE * 0 + 1, CEND );
	        set_window_controller( b, 3, wm, CPERC, 100, CSUB, 1, CEND );
		set_handler( b, text_inc_handler, data, wm );
	    }
	    TEXT_CALL_HANDLER_ON_ANY_CHANGES = 0;
	    CREATE_NUMERIC_TEXT = 0;
	    CREATE_NUMERIC_TEXT_MIN = 0;
	    CREATE_NUMERIC_TEXT_MAX = 0;
	    retval = 1;
	    break;
	case EVT_FOCUS:
	    data->prev_focus_win = wm->prev_focus_win;
	    retval = 1;
	    break;
	case EVT_UNFOCUS:
	    data->active = 0;
	    draw_window( win, wm );
	    retval = 1;
	    break;
	case EVT_MOUSEMOVE:
	case EVT_MOUSEBUTTONDOWN:
	    if( evt->key == MOUSE_BUTTON_LEFT )
	    {
		data->active = 1;
		xp = 1;
		for( p = 0; p < mem_get_size( data->text ) / (int)sizeof( UTF32_CHAR ); p++ )
		{
		    c = data->text[ p ];
		    int csize = char_x_size( win, c, wm );
		    if( c == 0 ) csize = 600;
		    if( rx >= xp && rx < xp + csize )
		    {
			data->cur_pos = p;
			break;
		    }
		    xp += char_x_size( win, c, wm );
		    if( data->text[ p ] == 0 ) break;
		}
		draw_window( win, wm );
	    }
	case EVT_MOUSEBUTTONUP:
	    if( evt->key == MOUSE_BUTTON_LEFT )
	    {
		if( data->active ) 
		    retval = 1;
	    }
	    break;
	case EVT_BUTTONDOWN:
	    if( data->active == 0 ) 
	    {
		retval = 0;
		break;
	    }
	    
	    if( evt->key == KEY_ENTER || evt->key == KEY_ESCAPE )
	    {
		if( evt->key == KEY_ENTER && win->action_handler )
		    win->action_handler( win->handler_data, win, wm );
		data->active = 0;
		set_focus_win( data->prev_focus_win, wm );
		draw_window( win, wm );
		retval = 1;
		break;
	    }
	    
	    c = 0;
	    if( evt->key >= 0x20 && evt->key <= 0x7E ) c = evt->key;
	    if( evt->flags & EVT_FLAG_SHIFT )
	    {
		if( evt->key >= 'a' && evt->key <= 'z' ) 
		{
		    c = evt->key - 0x20;
		}
		else
		{
		    switch( evt->key )
		    {
			case '0': c = ')'; break;
			case '1': c = '!'; break;
			case '2': c = '@'; break;
			case '3': c = '#'; break;
			case '4': c = '$'; break;
			case '5': c = '%'; break;
			case '6': c = '^'; break;
			case '7': c = '&'; break;
			case '8': c = '*'; break;
			case '9': c = '('; break;
			case '[': c = '{'; break;
			case ']': c = '}'; break;
			case ';': c = ':'; break;
			case  39: c = '"'; break;
			case ',': c = '<'; break;
			case '.': c = '>'; break;
			case '/': c = '?'; break;
			case '-': c = '_'; break;
			case '=': c = '+'; break;
			case  92: c = '|'; break;
			case '`': c = '~'; break;
		    }
		}
	    }
	    
	    if( c == 0 )
	    {
		if( evt->key == KEY_BACKSPACE )
		{
		    if( data->cur_pos >= 1 )
		    {
			data->cur_pos--;
			for( i = data->cur_pos; i < mem_get_size( data->text ) / (int)sizeof( UTF32_CHAR ); i++ ) 
			    data->text[ i ] = data->text[ i + 1 ];
		    }
		    changes = 1;
		}
		else
		if( evt->key == KEY_DELETE )
		{
		    if( data->text[ data->cur_pos ] != 0 )
		    for( i = data->cur_pos; i < mem_get_size( data->text ) / (int)sizeof( UTF32_CHAR ); i++ ) 
			data->text[ i ] = data->text[ i + 1 ];
		    changes = 1;
		}
		else
		if( evt->key == KEY_LEFT )
		{
		    if( data->cur_pos >= 1 )
		    {
			data->cur_pos--;
		    }
		}
		else
		if( evt->key == KEY_RIGHT )
		{
		    if( data->text[ data->cur_pos ] != 0 )
			data->cur_pos++;
		}
		else
		if( evt->key == KEY_END )
		{
		    for( i = 0; i < mem_get_size( data->text ) / (int)sizeof( UTF32_CHAR ); i++ ) 
			if( data->text[ i ] == 0 ) break;
		    data->cur_pos = i;
		}
		else
		if( evt->key == KEY_HOME )
		{
		    data->cur_pos = 0;
		}
	    }
	    else
	    {
		//Add new char:
		for( i = mem_get_size( data->text ) / sizeof( UTF32_CHAR ) - 1; i >= data->cur_pos + 1; i-- )
		    data->text[ i ] = data->text[ i - 1 ];
		data->text[ data->cur_pos ] = c;
		data->cur_pos++;
		if( data->cur_pos >= mem_get_size( data->text ) / (int)sizeof( UTF32_CHAR ) )
		{
		    data->text = (UTF32_CHAR*)mem_resize( data->text, mem_get_size( data->text ) + 16 * sizeof( UTF32_CHAR ) );
		}
		changes = 1;
	    }
	    
	    if( data->call_handler_on_any_changes && changes && win->action_handler )
		win->action_handler( win->handler_data, win, wm );
	    draw_window( win, wm );
	    retval = 1;
	    break;
	case EVT_DRAW:
	    win_draw_lock( win, wm );
	    //Fill window:
	    col2 = col = win->color;
	    win_draw_box( win, 0, 0, win->xsize, win->ysize, col, wm );
	    //Draw text and cursor:
	    xp = 1;
	    for( p = 0; p < mem_get_size( data->text ) / (int)sizeof( UTF32_CHAR ); p++ )
	    {
		int ty = ( win->ysize - char_y_size( win, wm ) ) / 2;
		c = data->text[ p ];
		if( c == 0 ) c = ' ';
		if( data->cur_pos == p && data->active ) 
		    col2 = wm->white;
		else
		    col2 = col;
		win_draw_char( win, c, xp, ty, wm->black, col2, wm );
		xp += char_x_size( win, c, wm );
		if( data->text[ p ] == 0 ) break;
	    }
	    //Draw border:
	    win_draw_frame( win, 0, 0, win->xsize, win->ysize, blend( col, win->parent->color, 128 ), wm );
	    retval = 1;
	    win_draw_unlock( win, wm );
	    break;
	case EVT_BEFORECLOSE:
	    if( data->output_str ) mem_free( data->output_str );
	    if( data->text ) mem_free( data->text );
	    retval = 1;
	    break;
    }
    return retval;
}

void text_set_text( WINDOWPTR win, const UTF8_CHAR *text, window_manager *wm )
{
    if( win )
    {
	int len = mem_strlen( text ) + 1;
	UTF32_CHAR *ts = (UTF32_CHAR*)MEM_NEW( HEAP_DYNAMIC, len * sizeof( UTF32_CHAR ) );
	utf8_to_utf32( ts, len, text );
	int len2 = mem_strlen_utf32( ts ) + 1;
	text_data *data = (text_data*)win->data;
	if( mem_get_size( data->text ) / (int)sizeof( UTF32_CHAR ) < len2 )
	{
	    //Resize text buffer:
	    data->text = (UTF32_CHAR*)mem_resize( data->text, len2 * sizeof( UTF32_CHAR ) );
	}
	mem_copy( data->text, ts, len2 * sizeof( UTF32_CHAR ) );
	draw_window( win, wm );
	mem_free( ts );
    }
}

void text_set_value( WINDOWPTR win, int val, window_manager *wm )
{
    if( win )
    {
	text_data *data = (text_data*)win->data;
	if( val < data->min ) val = data->min;
	if( val > data->max ) val = data->max;
	UTF8_CHAR ts[ 16 ];
	int_to_string( val, ts );
	text_set_text( win, (const UTF8_CHAR*)ts, wm );
    }
}

UTF8_CHAR *text_get_text( WINDOWPTR win, window_manager *wm )
{
    if( win )
    {
	text_data *data = (text_data*)win->data;
	if( data->output_str ) mem_free( data->output_str );
	data->output_str = (UTF8_CHAR*)MEM_NEW( HEAP_DYNAMIC, mem_get_size( data->text ) );
	utf32_to_utf8( data->output_str, mem_get_size( data->text ), data->text );
	return data->output_str;
    }
    return 0;
}

int text_get_value( WINDOWPTR win, window_manager *wm )
{
    int val = 0;
    if( win )
    {
	text_data *data = (text_data*)win->data;
	UTF8_CHAR *s = text_get_text( win, wm );
	val = string_to_int( s );
	if( val < data->min ) val = data->min;
	if( val > data->max ) val = data->max;
    }
    return val;
}

COLORPTR CREATE_BITMAP_BUTTON = 0;
int CREATE_BITMAP_BUTTON_RECALC_COLORS = 0;
int CREATE_BITMAP_BUTTON_XSIZE = 0;
int CREATE_BITMAP_BUTTON_YSIZE = 0;
const UTF8_CHAR *CREATE_BUTTON_WITH_TEXT_MENU = 0;
int CREATE_BUTTON_WITH_LEVELS = 0;
int CREATE_BUTTON_WITH_AUTOREPEAT = 0;
int CREATE_FLAT_BUTTON = 0;

struct button_data
{
    WINDOWPTR this_window;
    sundog_image *img;
    sundog_image *pushed_img;
    const UTF8_CHAR *text_menu;
    int levels;
    int *save_level_to;
    int autorepeat;
    int timer;
    int pushed;
    int pen_inside;
    int flat;

    WINDOWPTR prev_focus_win;
};

void button_timer( void *vdata, sundog_timer *timer, window_manager *wm )
{
    //Autorepeat timer
    button_data *data = (button_data*)vdata;
    if( data->pushed && data->pen_inside )
    {
	if( data->this_window->action_handler )
	{
	    data->this_window->action_handler( data->this_window->handler_data, data->this_window, wm );
	}
    }
    timer->delay = time_ticks_per_second() / 16;
}

int button_handler( sundog_event *evt, window_manager *wm )
{
    if( window_handler_check_data( evt, wm ) ) return 0;
    int retval = 0;
    WINDOWPTR win = evt->win;
    button_data *data = (button_data*)win->data;
    COLOR col;
    int rx = evt->x - win->screen_x;
    int ry = evt->y - win->screen_y;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( button_data );
	    break;
	case EVT_AFTERCREATE:
	    data->this_window = win;
	    data->img = 0;
	    data->pushed_img = 0;
	    data->text_menu = CREATE_BUTTON_WITH_TEXT_MENU;
	    data->levels = CREATE_BUTTON_WITH_LEVELS;
	    data->save_level_to = SAVE_LEVEL_TO;
	    data->autorepeat = CREATE_BUTTON_WITH_AUTOREPEAT;
	    data->flat = CREATE_FLAT_BUTTON;
	    data->timer = -1;
	    data->pushed = 0;
	    if( CREATE_BITMAP_BUTTON )
	    {
		COLORPTR tmp = (COLORPTR)MEM_NEW( HEAP_DYNAMIC, CREATE_BITMAP_BUTTON_XSIZE * CREATE_BITMAP_BUTTON_YSIZE * COLORLEN );
		for( int a = 0; a < CREATE_BITMAP_BUTTON_XSIZE * CREATE_BITMAP_BUTTON_YSIZE; a++ )
		{
		    if( CREATE_BITMAP_BUTTON[ a ] == CW )
		    {
			tmp[ a ] = PUSHED_COLOR( win->color );
		    }
		    else
		    {
		        tmp[ a ] = CREATE_BITMAP_BUTTON[ a ];
		    }
		}
		data->pushed_img = new_image( 
		    CREATE_BITMAP_BUTTON_XSIZE,
		    CREATE_BITMAP_BUTTON_YSIZE,
		    tmp,
		    0, 0,
		    CREATE_BITMAP_BUTTON_XSIZE,
		    CREATE_BITMAP_BUTTON_YSIZE,
		    IMAGE_NATIVE_RGB,
		    wm );
		mem_free( tmp );
		if( CREATE_BITMAP_BUTTON_RECALC_COLORS )
		{
		    for( int a = 0; a < CREATE_BITMAP_BUTTON_XSIZE * CREATE_BITMAP_BUTTON_YSIZE; a++ )
		    {
			if( CREATE_BITMAP_BUTTON[ a ] == CW )
			{
			    CREATE_BITMAP_BUTTON[ a ] = win->color;
			}
		    }
		}
		data->img = new_image( 
		    CREATE_BITMAP_BUTTON_XSIZE,
		    CREATE_BITMAP_BUTTON_YSIZE,
		    CREATE_BITMAP_BUTTON,
		    0, 0,
		    CREATE_BITMAP_BUTTON_XSIZE,
		    CREATE_BITMAP_BUTTON_YSIZE,
		    IMAGE_NATIVE_RGB,
		    wm );
	    }
	    CREATE_BITMAP_BUTTON = 0;
	    CREATE_BITMAP_BUTTON_RECALC_COLORS = 0;
	    CREATE_BITMAP_BUTTON_XSIZE = 0;
	    CREATE_BITMAP_BUTTON_YSIZE = 0;
	    CREATE_BUTTON_WITH_TEXT_MENU = 0;
	    CREATE_BUTTON_WITH_LEVELS = 0;
	    CREATE_BUTTON_WITH_AUTOREPEAT = 0;
	    CREATE_FLAT_BUTTON = 0;
	    retval = 1;
	    break;
	case EVT_BEFORECLOSE:
	    if( data->img )
	    {
		remove_image( data->img );
	    }
	    if( data->pushed_img )
	    {
		remove_image( data->pushed_img );
	    }
	    retval = 1;
	    break;
	case EVT_FOCUS:
	    data->prev_focus_win = wm->prev_focus_win;
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONDOWN:
	    if( evt->key == MOUSE_BUTTON_LEFT )
	    {
		data->pushed = 1;
		data->pen_inside = 1;
		if( data->autorepeat )
		    data->timer = add_timer( button_timer, (void*)data, time_ticks_per_second() / 2, wm );
		draw_window( win, wm );
		retval = 1;
	    }
	    break;
	case EVT_MOUSEMOVE:
	    if( data->pushed )
	    {
		if( rx >= 0 && rx < win->xsize &&
		    ry >= 0 && ry < win->ysize )
		{
		    data->pen_inside = 1;
		}
		else
		{
		    data->pen_inside = 0;
		}
	    }
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONUP:
	    set_focus_win( data->prev_focus_win, wm );
	    if( evt->key == MOUSE_BUTTON_LEFT && data->pushed )
	    {
		if( data->pushed )
		{
		    if( rx >= 0 && rx < win->xsize &&
			ry >= 0 && ry < win->ysize )
		    {
			if( data->text_menu )
			{
			    //Show popup with text menu:
			    win->action_result = popup_menu( win->name, data->text_menu, 0, win->screen_x, win->screen_y, wm->menu_color, wm );
			}
		    }
		    data->pushed = 0;
		    draw_window( win, wm );
		    if( data->autorepeat && data->timer >= 0 )
		    {
			remove_timer( data->timer, wm );
			data->timer = -1;
		    }
		    if( rx >= 0 && rx < win->xsize &&
			ry >= 0 && ry < win->ysize &&
			win->action_handler )
		    {
			win->action_handler( win->handler_data, win, wm );
		    }
		}
		retval = 1;
	    }
	    break;
	case EVT_DRAW:
	    win_draw_lock( win, wm );
	    if( data->pushed )
		col = PUSHED_COLOR( win->color );
	    else
		col = win->color;
	    if( data->levels && data->save_level_to )
	    {
		COLOR prev_col = col;
		int gg = ( *data->save_level_to << 8 ) / data->levels;
		if( gg > 255 ) gg = 255;
		col = get_color( gg, gg, gg );
		win_draw_box( win, 1, 1, win->xsize - 1, win->ysize - 1, col, wm );
		col = prev_col;
	    }
	    else
	    {
		win_draw_box( win, 1, 1, win->xsize - 1, win->ysize - 1, col, wm );
	    }
	    if( data->levels )
	    {
	    }
	    else
	    if( data->img )
	    {
		//Draw bitmap:
		sundog_image *img;
		if( data->pushed )
		    img = data->pushed_img;
		else
		    img = data->img;
		win_draw_bitmap( 
		    win, 
		    ( win->xsize - data->img->xsize ) / 2,
		    ( win->ysize - data->img->ysize ) / 2 + data->pushed,
		    img,
		    wm );
	    }
	    else
	    if( win->name )
	    {
		//Draw name:
		int tx = win->xsize / 2;
		int ty = win->ysize / 2 + data->pushed;
		if( win->name[ 0 ] == 0x11 )
		{
		    //UP:
		    win_draw_box( win, tx - 3, ty + 1, 7, 1, wm->black, wm );
		    win_draw_box( win, tx - 2, ty + 0, 5, 1, wm->black, wm );
		    win_draw_box( win, tx - 1, ty - 1, 3, 1, wm->black, wm );
		    win_draw_box( win, tx - 0, ty - 2, 1, 1, wm->black, wm );
		}
		else
		if( win->name[ 0 ] == 0x12 )
		{
		    //DOWN:
		    win_draw_box( win, tx - 3, ty - 2, 7, 1, wm->black, wm );
		    win_draw_box( win, tx - 2, ty - 1, 5, 1, wm->black, wm );
		    win_draw_box( win, tx - 1, ty - 0, 3, 1, wm->black, wm );
		    win_draw_box( win, tx - 0, ty + 1, 1, 1, wm->black, wm );
		}
		else
		if( win->name[ 0 ] == 0x13 )
		{
		    //LEFT:
		    win_draw_box( win, tx + 1, ty - 3, 1, 7, wm->black, wm );
		    win_draw_box( win, tx + 0, ty - 2, 1, 5, wm->black, wm );
		    win_draw_box( win, tx - 1, ty - 1, 1, 3, wm->black, wm );
		    win_draw_box( win, tx - 2, ty - 0, 1, 1, wm->black, wm );
		}
		else
		if( win->name[ 0 ] == 0x14 )
		{
		    //RIGHT:
		    win_draw_box( win, tx - 2, ty - 3, 1, 7, wm->black, wm );
		    win_draw_box( win, tx - 1, ty - 2, 1, 5, wm->black, wm );
		    win_draw_box( win, tx - 0, ty - 1, 1, 3, wm->black, wm );
		    win_draw_box( win, tx + 1, ty - 0, 1, 1, wm->black, wm );
		}
		else
		{
		    tx = ( win->xsize - string_size( win, win->name, wm ) ) / 2;
		    ty = ( win->ysize - char_y_size( win, wm ) ) / 2;
		    win_draw_string( win, win->name, tx, ty + data->pushed, wm->black, col, wm );
		}
	    }
	    //Draw border:
	    if( data->flat )
	    {
		win_draw_frame( win, 0, 0, win->xsize, win->ysize, col, wm );
	    }
	    else
	    {
		win_draw_frame3d( win, 0, 0, win->xsize, win->ysize, col, 1 | ( 2 << 8 ), wm );
	    }
	    retval = 1;
	    win_draw_unlock( win, wm );
	    break;
    }
    return retval;
}

int CREATE_NUMBERED_LIST = 0;

struct wlist_data
{
    WINDOWPTR this_window;
    list_data *list;
    int prev_selected;
    WINDOWPTR scrollbar;
    int last_action;	//0 - none; 1 - up/down buttons; 2 - mouse click; 3 - enter
    int numbered;
    int numbered_size;
};

int list_scrollbar_handler( void *user_data, WINDOWPTR win, window_manager *wm )
{
    wlist_data *data = (wlist_data*)user_data;
    data->list->start_item = scrollbar_get_value( win, wm );
    if( data->list->start_item < 0 ) data->list->start_item = 0;
    draw_window( data->this_window, wm );
    return 0;
}

int list_handler( sundog_event *evt, window_manager *wm )
{
    if( window_handler_check_data( evt, wm ) ) return 0;
    int retval = 0;
    WINDOWPTR win = evt->win;
    wlist_data *data = (wlist_data*)win->data;
    COLOR col, col2;
    int i, yp;
    int rx = evt->x - win->screen_x;
    int ry = evt->y - win->screen_y;
    int xoffset = 1;
    int yoffset = 2;
    int ychar_size = char_y_size( win, wm );
    int ychars = ( win->ysize - yoffset - 1 ) / ychar_size;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( wlist_data );
	    break;
	case EVT_AFTERCREATE:
	    data->this_window = win;
	    data->list = (list_data*)MEM_NEW( HEAP_DYNAMIC, sizeof( list_data ) );
	    list_init( data->list );
	    data->prev_selected = -999;
	    //SCROLLBAR:
	    CREATE_VERTICAL_SCROLLBAR = 1;
	    CREATE_FLAT_SCROLLBAR = 1;
	    //data->scrollbar = new_window( "scroll", 1, 1, 1, 1, wm->colors[ 10 ], win, scrollbar_handler, wm );
	    data->scrollbar = new_window( "scroll", 1, 1, 1, 1, win->color, win, scrollbar_handler, wm );
	    set_window_controller( data->scrollbar, 0, wm, CPERC, 100, CSUB, 1, CEND );
	    set_window_controller( data->scrollbar, 1, wm, 1, CEND );
	    set_window_controller( data->scrollbar, 2, wm, CPERC, 100, CSUB, SCROLLBAR_SIZE, CEND );
	    set_window_controller( data->scrollbar, 3, wm, CPERC, 100, CSUB, 1, CEND );
	    set_handler( data->scrollbar, list_scrollbar_handler, data, wm );
	    data->numbered = CREATE_NUMBERED_LIST;
	    if( data->numbered )
	    {
		data->numbered_size = char_x_size( win, '9', wm ) * 2 + 2;
	    }
	    CREATE_NUMBERED_LIST = 0;
	    retval = 1;
	    break;
	case EVT_BUTTONDOWN:
	    {
		data->last_action = 0;
		int updown = 0;
		int step = 0;
		if( evt->key == KEY_ESCAPE ) { data->last_action = 4; updown = 0; step = 0; }
		if( evt->key == KEY_ENTER ||
		    evt->key == ' ' ) { data->last_action = 2; updown = 0; step = 0; }
		if( evt->key == KEY_UP ) { data->last_action = 1; updown = 1; step = 1; }
		if( evt->key == KEY_PAGEUP ) { data->last_action = 1; updown = 1; step = ychars; }
		if( evt->key == KEY_DOWN ) { data->last_action = 1; updown = 2; step = 1; }
		if( evt->key == KEY_PAGEDOWN ) { data->last_action = 1; updown = 2; step = ychars; }
		if( updown == 1 )
		{
		    data->list->selected_item -= step;
		    if( data->list->selected_item < 0 ) data->list->selected_item = 0;
		    if( data->list->selected_item < data->list->start_item )
			data->list->start_item = data->list->selected_item;
		    draw_window( win, wm );
		}
		if( updown == 2 )
		{
		    data->list->selected_item += step;
		    if( data->list->selected_item >= data->list->items_num ) 
			data->list->selected_item = data->list->items_num - 1;
		    if( data->list->selected_item >= data->list->start_item + ychars )
			data->list->start_item = data->list->selected_item - ychars + 1;
		    if( data->list->start_item < 0 ) data->list->start_item = 0;
		    draw_window( win, wm );
		}
		win->action_result = data->list->selected_item;
		if( win->action_handler )
		    win->action_handler( win->handler_data, win, wm );
	    }
	    retval = 1;
	    break;
	case EVT_MOUSEDOUBLECLICK:
	    data->last_action = 2;
	    win->action_result = data->list->selected_item;
	    if( win->action_handler )
		win->action_handler( win->handler_data, win, wm );
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONDOWN:
	case EVT_MOUSEMOVE:
	    if( evt->key == MOUSE_BUTTON_LEFT )
	    {
		data->list->selected_item = ( ry - yoffset ) / ychar_size + data->list->start_item;
		if( data->list->selected_item >= data->list->items_num ) 
		    data->list->selected_item = data->list->items_num - 1;
		if( data->prev_selected != data->list->selected_item || evt->type == EVT_MOUSEBUTTONDOWN )
		{
		    //Control parameters:
		    //If lower win bound:
		    if( data->list->selected_item < data->list->start_item )
			data->list->start_item = data->list->selected_item;
		    //If higher win bound:
		    if( data->list->selected_item >= data->list->start_item + ychars )
			data->list->start_item = data->list->selected_item - ychars + 1;
		    if( data->list->start_item < 0 ) data->list->start_item = 0;
		    draw_window( win, wm );
		}
		data->prev_selected = data->list->selected_item;
	    }
	    if( evt->key == MOUSE_BUTTON_SCROLLUP )
	    {
		data->list->start_item -= 2;
		if( data->list->start_item < 0 ) data->list->start_item = 0;
		draw_window( win, wm );
	    }
	    if( evt->key == MOUSE_BUTTON_SCROLLDOWN )
	    {
		data->list->start_item += 2;
		if( data->list->start_item >= data->list->items_num - ychars ) 
		    data->list->start_item = data->list->items_num - ychars;
		if( data->list->start_item < 0 ) data->list->start_item = 0;
		draw_window( win, wm );
	    }
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONUP:
	    if( evt->key == MOUSE_BUTTON_LEFT )
	    {
		data->last_action = 3;
		win->action_result = data->list->selected_item;
		if( data->list->selected_item >= 0 && data->list->selected_item < data->list->items_num )
		    if( win->action_handler )
			win->action_handler( win->handler_data, win, wm );
	    }
	    retval = 1;
	    break;
	case EVT_DRAW:
	    win_draw_lock( win, wm );
	    //Set scrollbar parameters:
	    {
		int page_size = ychars;
		int max_val = data->list->items_num - page_size;
		if( max_val < 0 ) max_val = 0;
		scrollbar_set_parameters( data->scrollbar, data->list->start_item, max_val, page_size, 1, wm );
	    }
	    //Fill window:
	    col2 = col = win->color;
	    //win_draw_box( win, 1, 1, win->xsize-2, win->ysize-2, col, wm );
	    //Draw items:
	    yp = yoffset;
	    if( yp )
		win_draw_box( win, 1, 1, win->xsize - 2, yp - 1, win->color, wm );
	    for( i = data->list->start_item; i < data->list->items_num; i++ )
	    {
		COLOR fc = wm->black;
		if( list_get_attr( i, data->list ) == 1 ) fc = blend( wm->black, win->color, 130 );
		if( data->list->selected_item == i )
		{
		    fc = COLORMASK;
		    col2 = blend( col, wm->black, 128 );
		}
		else
		    col2 = col;
		if( data->numbered )
		{
		    //Draw a number:
		    UTF8_CHAR num_str[ 8 ];
		    int_to_string( i, num_str );
		    COLOR bc = blend( col2, wm->black, 16 );
		    win_draw_box( win, xoffset, yp, data->numbered_size, char_y_size( win, wm ), bc, wm );
		    win_draw_string( win, num_str, xoffset, yp, fc, bc, wm );
		    win_draw_string( win, list_get_item( i, data->list ), xoffset + data->numbered_size, yp, fc, bc, wm );
		    int len = string_size( win, list_get_item( i, data->list ), wm );
		    if( len + xoffset + data->numbered_size < win->xsize - 1 )
			win_draw_box( win, len + xoffset + data->numbered_size, yp, win->xsize - ( len + xoffset ) - 1, char_y_size( win, wm ), col2, wm );
		}
		else
		{
		    win_draw_string( win, list_get_item( i, data->list ), xoffset, yp, fc, col2, wm );
		    int len = string_size( win, list_get_item( i, data->list ), wm );
		    if( len + xoffset < win->xsize - 1 )
			win_draw_box( win, len + xoffset, yp, win->xsize - ( len + xoffset ) - 1, char_y_size( win, wm ), col2, wm );
		}
		yp += char_y_size( win, wm );
	    }
	    if( yp < win->ysize - 1 )
		win_draw_box( win, 1, yp, win->xsize - 2, win->ysize - yp - 1, win->color, wm );
	    //Draw border:
	    win_draw_frame( win, 0, 0, win->xsize, win->ysize, blend( col, win->parent->color, 128 ), wm );
	    retval = 1;
	    win_draw_unlock( win, wm );
	    break;
	case EVT_BEFORECLOSE:
	    if( data->list ) { list_close( data->list ); mem_free( data->list ); }
	    retval = 1;
	    break;
    }
    return retval;
}

list_data *list_get_data( WINDOWPTR win, window_manager *wm )
{
    if( win )
    {
	wlist_data *data = (wlist_data*)win->data;
	return data->list;
    }
    return 0;
}

int list_get_last_action( WINDOWPTR win, window_manager *wm )
{
    if( win )
    {
	wlist_data *data = (wlist_data*)win->data;
	return data->last_action;
    }
    return 0;
}

void list_select_item( WINDOWPTR win, int item_num, window_manager *wm )
{
    if( win )
    {
	int yoffset = 2;
	int ychar_size = char_y_size( win, wm );
	int ychars = ( win->ysize - yoffset - 1 ) / ychar_size;

	wlist_data *data = (wlist_data*)win->data;
	list_data *ldata = (list_data*)data->list;

	if( item_num >= 0 && item_num < ldata->items_num )
	{
	    ldata->selected_item = item_num;
	    ldata->start_item = item_num - ychars / 2;
	    if( ldata->start_item + ychars >= ldata->items_num ) ldata->start_item = ldata->items_num - ychars;
	    if( ldata->start_item < 0 ) ldata->start_item = 0;
	}
    }
}

int CREATE_VERTICAL_SCROLLBAR = 0;
int CREATE_REVERSE_SCROLLBAR = 0;
int CREATE_COMPACT_SCROLLBAR = 0;
int CREATE_FLAT_SCROLLBAR = 0;
const UTF8_CHAR text_up[ 2 ] = { 0x11, 0 };
const UTF8_CHAR text_down[ 2 ] = { 0x12, 0 };
const UTF8_CHAR text_left[ 2 ] = { 0x13, 0 };
const UTF8_CHAR text_right[ 2 ] = { 0x14, 0 };

struct scrollbar_data
{
    WINDOWPTR this_window;
    WINDOWPTR but1;
    WINDOWPTR but2;
    int vert;
    int rev;
    int compact_mode;
    int flat;
    UTF8_CHAR *name;
    int max_value;
    int page_size;
    int step_size;
    int cur;
    int bar_selected;
    int drag_start;
    int drag_start_val;
    int show_offset;

    int pos;
    int bar_size;
    float one_pixel_size;
    int move_region;

    WINDOWPTR prev_focus_win;
};

int scrollbar_dec_button( void *user_data, WINDOWPTR win, window_manager *wm )
{
    scrollbar_data *data = (scrollbar_data*)user_data;
    data->cur -= data->step_size;
    if( data->cur < 0 ) data->cur = 0;
    if( data->cur > data->max_value ) data->cur = data->max_value;
    draw_window( data->this_window, wm );
    if( data->this_window->action_handler )
	data->this_window->action_handler( data->this_window->handler_data, data->this_window, wm );
    return 0;
}

int scrollbar_inc_button( void *user_data, WINDOWPTR win, window_manager *wm )
{
    scrollbar_data *data = (scrollbar_data*)user_data;
    data->cur += data->step_size;
    if( data->cur < 0 ) data->cur = 0;
    if( data->cur > data->max_value ) data->cur = data->max_value;
    draw_window( data->this_window, wm );
    if( data->this_window->action_handler )
	data->this_window->action_handler( data->this_window->handler_data, data->this_window, wm );
    return 0;
}

int scrollbar_handler( sundog_event *evt, window_manager *wm )
{
    if( window_handler_check_data( evt, wm ) ) return 0;
    int retval = 0;
    WINDOWPTR win = evt->win;
    scrollbar_data *data = (scrollbar_data*)win->data;
    int rx = evt->x - win->screen_x;
    int ry = evt->y - win->screen_y;
    int button_size = SCROLLBAR_SIZE;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( scrollbar_data );
	    break;
	case EVT_AFTERCREATE:
	    data->this_window = win;
	    data->flat = CREATE_FLAT_SCROLLBAR;
	    if( CREATE_VERTICAL_SCROLLBAR )
	    {
		data->vert = 1;
		CREATE_BUTTON_WITH_AUTOREPEAT = 1;
		CREATE_FLAT_BUTTON = data->flat;
		data->but1 = new_window( text_up, 0, 0, win->xsize, button_size, win->color, win, button_handler, wm );
		CREATE_BUTTON_WITH_AUTOREPEAT = 1;
		CREATE_FLAT_BUTTON = data->flat;
		data->but2 = new_window( text_down, 0, win->ysize - button_size, win->xsize, button_size, win->color, win, button_handler, wm );
		set_window_controller( data->but1, 0, wm, CPERC, 0, CEND );
		set_window_controller( data->but1, 1, wm, CPERC, 0, CEND );
		set_window_controller( data->but1, 2, wm, CPERC, 100, CEND );
		set_window_controller( data->but1, 3, wm, button_size, CEND );
		set_window_controller( data->but2, 0, wm, CPERC, 0, CEND );
		set_window_controller( data->but2, 1, wm, CPERC, 100, CEND );
		set_window_controller( data->but2, 2, wm, CPERC, 100, CEND );
		set_window_controller( data->but2, 3, wm, CPERC, 100, CSUB, button_size, CEND );
		if( CREATE_REVERSE_SCROLLBAR )
		{
		    set_handler( data->but1, scrollbar_inc_button, data, wm );
		    set_handler( data->but2, scrollbar_dec_button, data, wm );
		}
		else
		{
		    set_handler( data->but1, scrollbar_dec_button, data, wm );
		    set_handler( data->but2, scrollbar_inc_button, data, wm );
		}
	    }
	    else
	    {
		data->vert = 0;
		CREATE_BUTTON_WITH_AUTOREPEAT = 1;
		CREATE_FLAT_BUTTON = data->flat;
		data->but1 = new_window( text_right, 0, 0, button_size, win->ysize, win->color, win, button_handler, wm );
		CREATE_BUTTON_WITH_AUTOREPEAT = 1;
		CREATE_FLAT_BUTTON = data->flat;
		data->but2 = new_window( text_left, win->xsize - button_size, 0, button_size, win->ysize, win->color, win, button_handler, wm );
		set_window_controller( data->but1, 0, wm, CPERC, 100, CEND );
		set_window_controller( data->but1, 1, wm, CPERC, 0, CEND );
		set_window_controller( data->but1, 2, wm, CPERC, 100, CSUB, button_size, CEND );
		set_window_controller( data->but1, 3, wm, CPERC, 100, CEND );
		set_window_controller( data->but2, 0, wm, CPERC, 0, CEND );
		set_window_controller( data->but2, 1, wm, CPERC, 0, CEND );
		set_window_controller( data->but2, 2, wm, button_size, CEND );
		set_window_controller( data->but2, 3, wm, CPERC, 100, CEND );
		if( CREATE_REVERSE_SCROLLBAR )
		{
		    set_handler( data->but1, scrollbar_dec_button, data, wm );
		    set_handler( data->but2, scrollbar_inc_button, data, wm );
		}
		else
		{
		    set_handler( data->but1, scrollbar_inc_button, data, wm );
		    set_handler( data->but2, scrollbar_dec_button, data, wm );
		}
	    }
	    data->rev = CREATE_REVERSE_SCROLLBAR;
	    data->compact_mode = CREATE_COMPACT_SCROLLBAR;
	    data->name = 0;
	    data->cur = 0;
	    data->max_value = 0;
	    data->page_size = 0;
	    data->step_size = 1;
	    data->bar_selected = 0;
	    data->show_offset = 0;
	    CREATE_VERTICAL_SCROLLBAR = 0;
	    CREATE_REVERSE_SCROLLBAR = 0;
	    CREATE_COMPACT_SCROLLBAR = 0;
	    CREATE_FLAT_SCROLLBAR = 0;
	    retval = 1;
	    break;
	case EVT_BEFORECLOSE:
	    if( data->name )
	    {
		mem_free( data->name );
		data->name = 0;
	    }
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONDOWN:
	case EVT_MOUSEMOVE:
	    {
		int working_area = 0;
		if( data->vert )
		    working_area = win->ysize - button_size * 2;
		else
		    working_area = win->xsize - button_size * 2;
		
		int value_changed = 1;

		if( data->compact_mode == 0 )
		{
		    //Normal mode:
		    if( evt->type == EVT_MOUSEBUTTONDOWN && 
			evt->key == MOUSE_BUTTON_LEFT &&
			rx >= 0 && rx < win->xsize && ry >= 0 && ry < win->ysize )
		    {
			if( data->vert )
			{
			    if( ry >= button_size + data->pos && ry < button_size + data->pos + data->bar_size )
			    {
				data->bar_selected = 1;
				data->drag_start = ry;
				data->drag_start_val = data->cur;
			    }
			    int page = data->page_size;
			    if( data->rev ) page = -page;
			    if( ry < button_size + data->pos )
				data->cur -= page;
			    if( ry >= button_size + data->pos + data->bar_size )
				data->cur += page;
			}
			else
			{
			    if( rx >= button_size + data->pos && rx < button_size + data->pos + data->bar_size )
			    {
				data->bar_selected = 1;
				data->drag_start = rx;
				data->drag_start_val = data->cur;
			    }
			    int page = data->page_size;
			    if( data->rev ) page = -page;
			    if( rx < button_size + data->pos )
				data->cur -= page;
			    if( rx >= button_size + data->pos + data->bar_size )
				data->cur += page;
			}
		    }
		    else
		    {
			//Move:
			if( data->bar_selected )
			{
			    int d = 0;
			    if( data->vert ) 
				d = ry - data->drag_start;
			    else
				d = rx - data->drag_start;
			    if( data->rev )
				data->cur = data->drag_start_val - (long)( (float)d * data->one_pixel_size );
			    else
				data->cur = data->drag_start_val + (long)( (float)d * data->one_pixel_size );
			}
		    }
		}
		else
		{
		    //Compact mode:
		    if( evt->key == MOUSE_BUTTON_LEFT )
		    {
			if( evt->type == EVT_MOUSEBUTTONDOWN &&
			    rx >= 0 && ry >= 0 &&
			    rx < win->xsize && ry < win->ysize )
			{
			    data->bar_selected = 1;
			}
			if( data->bar_selected == 1 )
			{
			    int new_val = ( ( ( ( rx - button_size ) << 11 ) / ( working_area - 1 ) ) * data->max_value ) >> 11;
			    if( data->cur == new_val ) value_changed = 0;
			    data->cur = new_val;
			}
			else
			{
			    value_changed = 0;
			}
		    }
		}
		//Bounds control:
		if( data->cur < 0 ) data->cur = 0;
		if( data->cur > data->max_value ) data->cur = data->max_value;
		//User handler:
		if( win->action_handler && value_changed )
		    win->action_handler( win->handler_data, win, wm );
		//Redraw it:
		draw_window( win, wm );
	    }
	    retval = 1;
	    break;
	case EVT_FOCUS:
	    data->prev_focus_win = wm->prev_focus_win;
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONUP:
	    set_focus_win( data->prev_focus_win, wm );
	    data->bar_selected = 0;
	    draw_window( win, wm );
	    //User handler:
	    if( win->action_handler )
	        win->action_handler( win->handler_data, win, wm );
	    retval = 1;
	    break;
	case EVT_DRAW:
	    win_draw_lock( win, wm );
	    {
		int working_area = 0;
		if( data->vert )
		    working_area = win->ysize - button_size * 2;
		else
		    working_area = win->xsize - button_size * 2;

		if( data->compact_mode == 0 )
		{
		    //Normal mode:
		    if( data->max_value == 0 || data->page_size == 0 )
		    {
			data->bar_size = working_area;
			data->pos = 0;
		    }
		    else
		    {
			//Calculate move-region (in pixels)
			float ppage;
			if( data->max_value + 1 == 0 ) 
			    ppage = 1.0F;
			else
			    ppage = (float)data->page_size / (float)( data->max_value + 1 );
			if( ppage == 0 ) ppage = 1.0F;
			data->move_region = (long)( (float)working_area / ( ppage + 1.0F ) );
			if( data->move_region == 0 ) data->move_region = 1;

			//Caclulate slider size (in pixels)
			data->bar_size = working_area - data->move_region;

			//Calculate one pixel size
			data->one_pixel_size = (float)( data->max_value + 1 ) / (float)data->move_region;

			//Calculate slider position (in pixels)
			data->pos = (long)( ( (float)data->cur / (float)data->max_value ) * (float)data->move_region );

			if( data->bar_size < 2 ) data->bar_size = 2;
			if( data->rev )
			    data->pos = ( working_area - data->pos ) - data->bar_size;

			if( data->pos + data->bar_size > working_area )
			{
			    data->bar_size -= ( data->pos + data->bar_size ) - working_area;
			}
		    }

		    COLOR bar_color;
		    COLOR bg_color;
		    if( data->flat == 0 )
		    {
			bar_color = win->color;
			bg_color = wm->scroll_background_color;
			if( data->bar_selected ) bar_color = PUSHED_COLOR( bar_color );
		    }
		    else
		    {
			bar_color = blend( win->color, wm->black, 40 );
			bg_color = win->color;
			if( data->bar_selected ) bar_color = PUSHED_COLOR( bar_color );
		    }
		    if( data->vert )
		    {
			win_draw_box( win, 0, button_size, win->xsize, button_size + data->pos, bg_color, wm );
			win_draw_box( win, 0, button_size + data->pos + data->bar_size, win->xsize, working_area - ( data->pos + data->bar_size ), bg_color, wm );
			if( data->flat == 0 )
			{
			    win_draw_box( win, 1, button_size + data->pos + 1, win->xsize - 2, data->bar_size - 2, bar_color, wm );
			    win_draw_frame3d( win, 0, button_size + data->pos, win->xsize, data->bar_size, bar_color, 1 + ( 2<<8 ), wm );
			}
			else
			{
			    win_draw_box( win, 0, button_size + data->pos, win->xsize, data->bar_size, bar_color, wm );
			}
		    }
		    else
		    {
			win_draw_box( win, button_size, 0, button_size + data->pos, win->ysize, bg_color, wm );
			win_draw_box( win, button_size + data->pos + data->bar_size, 0, working_area - ( data->pos + data->bar_size ), win->ysize, bg_color, wm );
			if( data->flat == 0 )
			{
			    win_draw_box( win, button_size + data->pos + 1, 1, data->bar_size - 2, win->ysize - 2, bar_color, wm );
			    win_draw_frame3d( win, button_size + data->pos, 0, data->bar_size, win->ysize, bar_color, 1 + ( 2<<8 ), wm );
			}
			else
			{
			    win_draw_box( win, button_size + data->pos, 0, data->bar_size, win->ysize, bar_color, wm );
			}
		    }
		}
		else
		{
		    //Compact mode:
		    wbd_lock( win, wm );

		    UTF8_CHAR *str = data->name;
		    UTF8_CHAR num_str[ 32 ];
		    int_to_string( data->cur + data->show_offset, num_str );
		    int start_x = button_size;
		    COLOR bgcolor = wm->colors[ 9 ];
		    draw_box( start_x, 0, working_area, win->ysize, bgcolor, wm );

		    COLOR ccolor = wm->white;

		    wm->cur_font_color = wm->white;
		    wm->cur_font_draw_bgcolor = 0;
		    wm->cur_transparency = 256;

		    //Draw level:
		    int xc = 0;
		    if( data->max_value != 0 )
			xc = ( ( working_area - 2 ) * ( ( data->cur << 10 ) / data->max_value ) ) >> 10;
		    //draw_box( start_x + 1, 1, xc, win->ysize - 2, wm->selection_color, wm ); //wm->colors[ 11 ], wm );
		    draw_box( start_x + 1, 1, xc, win->ysize - 2, wm->colors[ 11 ], wm );
		    draw_box( start_x + 1 + xc, 1, 1, win->ysize - 2, wm->colors[ 14 ], wm );

		    if( str )
		    {
			wm->cur_font_color = wm->black;
			wm->cur_transparency = 50;
			draw_string( str, start_x + 1 + 1, ( win->ysize - wbd_char_y_size( wm ) ) / 2, wm );
			wm->cur_font_color = wm->white;
			wm->cur_transparency = 256;
			draw_string( str, start_x + 1, ( win->ysize - wbd_char_y_size( wm ) ) / 2, wm );
		    }

		    wm->cur_font_color = wm->white;
		    int num_str_size = wbd_string_size( num_str, wm ) + 2;
		    int num_x = start_x + working_area - num_str_size;
		    bgcolor = wm->colors[ 5 ];
		    wm->cur_transparency = 128;
		    draw_box( num_x, 0, num_str_size, win->ysize, bgcolor, wm );
		    wm->cur_transparency = 100;
		    draw_box( num_x - 1, 0, 1, win->ysize, bgcolor, wm );
		    wm->cur_transparency = 64;
		    draw_box( num_x - 2, 0, 1, win->ysize, bgcolor, wm );
		    wm->cur_transparency = 32;
		    draw_box( num_x - 3, 0, 1, win->ysize, bgcolor, wm );
		    wm->cur_transparency = 16;
		    draw_box( num_x - 4, 0, 1, win->ysize, bgcolor, wm );
		    num_str_size -= 1;
		    wm->cur_transparency = 60;
		    wm->cur_font_color = wm->black;
		    draw_string( num_str, start_x + working_area - num_str_size + 1, ( win->ysize - wbd_char_y_size( wm ) ) / 2, wm );
		    wm->cur_transparency = 256;
		    wm->cur_font_color = wm->white;
		    draw_string( num_str, start_x + working_area - num_str_size, ( win->ysize - wbd_char_y_size( wm ) ) / 2, wm );

		    wbd_draw( wm );
		    wbd_unlock( wm );
		}
	    }
	    win_draw_unlock( win, wm );
	    retval = 1;
	    break;
    }
    return retval;
}

void scrollbar_set_parameters( WINDOWPTR win, int cur, int max_value, int page_size, int step_size, window_manager *wm )
{
    if( win )
    {
	scrollbar_data *data = (scrollbar_data*)win->data;
	if( data )
	{
	    data->cur = cur;
	    data->max_value = max_value;
	    data->page_size = page_size;
	    data->step_size = step_size;
	}
    }
}

void scrollbar_set_value( WINDOWPTR win, int val, window_manager *wm )
{
    if( win )
    {
	scrollbar_data *data = (scrollbar_data*)win->data;
	if( data )
	{
	    data->cur = val;
	    if( data->cur < 0 ) data->cur = 0;
	    if( data->cur > data->max_value ) data->cur = data->max_value;
	}
    }
}

int scrollbar_get_value( WINDOWPTR win, window_manager *wm )
{
    if( win )
    {
	scrollbar_data *data = (scrollbar_data*)win->data;
	if( data )
	{
	    return data->cur;
	}
    }
    return 0;
}

void scrollbar_set_name( WINDOWPTR win, const UTF8_CHAR *name, window_manager *wm )
{
    if( win )
    {
	scrollbar_data *data = (scrollbar_data*)win->data;
	if( data )
	{
	    if( data->name == 0 )
	    {
		data->name = (UTF8_CHAR*)MEM_NEW( HEAP_DYNAMIC, mem_strlen( name ) + 1 );
	    }
	    else
	    {
		if( mem_strlen( name ) + 1 > mem_get_size( data->name ) )
		{
		    data->name = (UTF8_CHAR*)mem_resize( data->name, mem_strlen( name ) + 1 );
		}
	    }
	    mem_copy( data->name, name, mem_strlen( name ) + 1 );
	}
    }
}

void scrollbar_set_showing_offset( WINDOWPTR win, int offset, window_manager *wm )
{
    if( win )
    {
	scrollbar_data *data = (scrollbar_data*)win->data;
	if( data )
	{
	    data->show_offset = offset;
	}
    }
}

struct kbd_data
{
    int mx, my;
    int selected_key;
    int engrus;
};

//Can't add 0 in a last cell of this array :( Something with GCC?
const UTF8_CHAR *kbd_text1[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "|", "BCK", "#" };
int    kbd_key1[] = { '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '|', KEY_BACKSPACE };
int  kbd_texts1[] = { 1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   3 };

const UTF8_CHAR *kbd_text2[] = { "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", " ", "ENTER", "#" };
int    kbd_key2[] = { 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', ' ', KEY_ENTER };
#ifdef KBD_RUS
const UTF8_CHAR *kbd_text2r[] ={ "�", "�", "�", "�", "�", "�", "�", "�", "�", "�", "�", "ENTER", "#" };
int    kbd_key2r[] ={ '�', '�', '�', '�', '�', '�', '�', '�', '�', '�', '�', KEY_ENTER };
#endif
int  kbd_texts2[] = { 1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   5 };

#ifdef KBD_RUS
const UTF8_CHAR *kbd_text3[] = { "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", " ", "ENG/RUS", "#" };
int    kbd_key3[] = { 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', ' ', 1 };
const UTF8_CHAR *kbd_text3r[] ={ "�", "�", "�", "�", "�", "�", "�", "�", "�", "�", "�", "ENG/RUS", "#" };
int    kbd_key3r[] ={ '�', '�', '�', '�', '�', '�', '�', '�', '�', '�', '�', 1 };
#else
const UTF8_CHAR *kbd_text3[] = { "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", " ", "HIDE", "#" };
int    kbd_key3[] = { 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', ' ', 2 };
#endif
int  kbd_texts3[] = { 1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   5 };

const UTF8_CHAR *kbd_text4[] = { "Z", "X", "C", "V", "B", "N", "M", ",", ".", "/", " ", "SPACE", "#" };
int    kbd_key4[] = { 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', ' ', ' ' };
#ifdef KBD_RUS
const UTF8_CHAR *kbd_text4r[] ={ "�", "�", "�", "�", "�", "�", "�", "�", "�", "�", " ", "SPACE", "#" };
int    kbd_key4r[] ={ '�', '�', '�', '�', '�', '�', '�', '�', '�', '�', ' ', ' ' };
#endif
int  kbd_texts4[] = { 1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   5 };

void kbd_draw_line( const UTF8_CHAR **text, int *size, int *key_codes, int y, int mx, int my, kbd_data *data, WINDOWPTR win, window_manager *wm )
{
    int cell_xsize = ( win->xsize << 15 ) / 16;
    int cell_ysize = win->ysize / 4;
    int p = 0;
    int x = 0;
    y += ( win->ysize - ( cell_ysize * 4 ) ) / 2;
    while( 1 )
    {
	const UTF8_CHAR *str = text[ p ];
	if( str[ 0 ] == '#' ) break;
	int button_size = cell_xsize * size[ p ];
	int button_size2 = ( ( x + button_size ) >> 15 ) - ( x >> 15 );
	int str_size = string_size( win, str, wm );
	COLOR back_color = win->color;
	if( mx >= (x>>15) && mx < (x>>15) + button_size2 &&
	    my >= y && my < y + cell_ysize )
	{
	    data->selected_key = (unsigned)key_codes[ p ];
	    back_color = blend( wm->white, wm->green, 128 );
	    win_draw_box( win, x>>15, y, button_size2 - 1, cell_ysize - 1, back_color, wm );
	}
	win_draw_string( 
	    win, 
	    str, 
	    ( x >> 15 ) + ( button_size2 - str_size ) / 2, 
	    y + ( cell_ysize - char_y_size( win, wm ) ) / 2,
	    wm->black,
	    back_color, 
	    wm );
	win_draw_frame3d( win, x >> 15, y, button_size2, cell_ysize, blend( back_color, wm->black, 40 ), 1, wm );
	//win_draw_frame( win, x >> 15, y, button_size2 - 1, cell_ysize - 1, blend( back_color, wm->white, 100 ), wm );
	x += button_size;
	p++;
    }
}

int keyboard_handler( sundog_event *evt, window_manager *wm )
{ 
    if( window_handler_check_data( evt, wm ) ) return 0;
    int retval = 0;
    WINDOWPTR win = evt->win;
    kbd_data *data = (kbd_data*)win->data;
    int cell_xsize = win->xsize / 16;
    int cell_ysize = win->ysize / 4;
    int rx = evt->x - win->screen_x;
    int ry = evt->y - win->screen_y;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( kbd_data );
	    break;
	case EVT_AFTERCREATE:
	    data->mx = -1;
	    data->my = -1;
	    data->engrus = 0;
	    retval = 1;
	    break;
	case EVT_DRAW:
	    win_draw_lock( win, wm );
	    win_draw_box( win, 0, 0, win->xsize, win->ysize, win->color, wm );
	    data->selected_key = 0;
	    kbd_draw_line( kbd_text1, kbd_texts1, kbd_key1, 0, data->mx, data->my, data, win, wm );
	    if( data->engrus == 0 )
	    {
		kbd_draw_line( kbd_text2, kbd_texts2, kbd_key2, cell_ysize, data->mx, data->my, data, win, wm );
		kbd_draw_line( kbd_text3, kbd_texts3, kbd_key3, cell_ysize * 2, data->mx, data->my, data, win, wm );
		kbd_draw_line( kbd_text4, kbd_texts4, kbd_key4, cell_ysize * 3, data->mx, data->my, data, win, wm );
	    }
	    else
	    {
#ifdef KBD_RUS
		kbd_draw_line( kbd_text2r, kbd_texts2, kbd_key2r, cell_ysize, data->mx, data->my, data, win, wm );
		kbd_draw_line( kbd_text3r, kbd_texts3, kbd_key3r, cell_ysize * 2, data->mx, data->my, data, win, wm );
		kbd_draw_line( kbd_text4r, kbd_texts4, kbd_key4r, cell_ysize * 3, data->mx, data->my, data, win, wm );
#endif
	    }
	    win_draw_unlock( win, wm );
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONDOWN:
	    data->mx = rx;
	    data->my = ry;
	    draw_window( win, wm );
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONUP:
	    if( data->selected_key )
	    {
		if( data->selected_key == 1 )
		{
		    data->engrus ^= 1;
		    data->mx = -1;
		    data->my = -1;
		    draw_window( win, wm );
		}
		else if( data->selected_key == 2 )
		{
		    //Hide keyboard:
		    if( win->parent && (void*)win->parent->win_handler == (void*)decorator_handler )
		    {
			//hide decorator:
			hide_window( win->parent, wm );
		    }
		    else
		    {
			//hide single keyboard:
			hide_window( win, wm );
		    }
		    recalc_regions( wm );
		    draw_window( wm->root_win, wm );
		}
		else
		{
		    send_event( wm->focus_win, EVT_BUTTONDOWN, 0, 0, 0, data->selected_key, 0, 1024, 0, wm );
		    send_event( wm->focus_win, EVT_BUTTONUP, 0, 0, 0, data->selected_key, 0, 1024, 0, wm );
		    data->mx = -1;
		    data->my = -1;
		    draw_window( win, wm );
		}
	    }
	    data->mx = -1;
	    data->my = -1;
	    retval = 1;
	    break;
    }
    return retval;
}

const UTF8_CHAR *FILES_PROPS = 0;
const UTF8_CHAR *FILES_MASK = 0;
UTF8_CHAR *FILES_RESULTED_FILENAME = 0;

struct files_data
{
    WINDOWPTR this_window;
    WINDOWPTR go_up_button;
    WINDOWPTR disk_buttons[ 32 ];
    WINDOWPTR path;
    WINDOWPTR name;
    WINDOWPTR files;
    WINDOWPTR ok_button;
    WINDOWPTR cancel_button;
    const UTF8_CHAR *mask;
    const UTF8_CHAR *props_file; //File with dialog properties
    UTF8_CHAR *prop_path[ 32 ];
    int prop_cur_file[ 32 ];
    int	prop_cur_disk;
    UTF8_CHAR *resulted_filename;
    int first_time;
};

int files_ok_button_handler( void *user_data, WINDOWPTR win, window_manager *wm );
int files_cancel_button_handler( void *user_data, WINDOWPTR win, window_manager *wm );
int files_name_handler( void *user_data, WINDOWPTR win, window_manager *wm );
void files_refresh_list( WINDOWPTR win, window_manager *wm );
void files_save_props( WINDOWPTR win, window_manager *wm );

int files_list_handler( void *user_data, WINDOWPTR win, window_manager *wm )
{
    wlist_data *wdata = (wlist_data*)win->data;
    files_data *data = (files_data*)user_data;
    list_data *ldata = list_get_data( data->files, wm );
    if( ldata && wdata->last_action )
    {
	if( wdata->last_action == 4 )
	{
	    //ESCAPE KEY:
	    files_cancel_button_handler( data, data->ok_button, wm );
	    return 0;
	}
	UTF8_CHAR *item = list_get_item( list_get_selected_num( ldata ), ldata );
	int subdir = 0;
	if( item )
	{
	    if( wdata->last_action == 1 )
	    {
		//UP/DOWN KEY:
		text_set_text( data->name, item, wm );
	    }
	    if( wdata->last_action == 2 )
	    {
		//ENTER/SPACE KEY:
		if( list_get_attr( list_get_selected_num( ldata ), ldata ) == 1 )
		{
		    //It's a dir:
		    subdir = 1;
		}
		else
		{
		    //Select file:
		    files_ok_button_handler( data, data->ok_button, wm );
		    return 0;
		}
	    }
	    if( wdata->last_action == 3 )
	    {
		//MOUSE CLICK:
		if( list_get_attr( list_get_selected_num( ldata ), ldata ) == 1 )
		{
		    //It's a dir:
		    subdir = 1;
		}
		else
		{
		    text_set_text( data->name, item, wm );
		}
	    }
	    if( subdir )
	    {
		//Go to the subdir:
		UTF8_CHAR *path = data->prop_path[ data->prop_cur_disk ];
		if( path )
		    path = (UTF8_CHAR*)mem_resize( path, mem_strlen( path ) + mem_strlen( item ) + 2 );
		else
		{
		    path = (UTF8_CHAR*)MEM_NEW( HEAP_DYNAMIC, mem_strlen( item ) + 2 );
		    path[ 0 ] = 0;
		}
		mem_strcat( path, item );
		mem_strcat( path, "/" );
		data->prop_path[ data->prop_cur_disk ] = path;
		data->prop_cur_file[ data->prop_cur_disk ] = -1;
		text_set_text( data->path, path, wm );
		files_refresh_list( data->this_window, wm );
		draw_window( data->this_window, wm );
	    }
	}
	data->prop_cur_file[ data->prop_cur_disk ] = ldata->selected_item;
    }
    return 0;
}

int files_disk_button_handler( void *user_data, WINDOWPTR win, window_manager *wm )
{
    files_data *data = (files_data*)user_data;
    
    if( win == data->go_up_button )
    {
	//Go to parent dir:
	UTF8_CHAR *path = data->prop_path[ data->prop_cur_disk ];
	if( path )
	{
	    int p;
	    for( p = mem_strlen( path ) - 2; p >= 0; p-- )
	    {
		if( path[ p ] == '/' ) { path[ p + 1 ] = 0; break; }
	    }
	    if( p < 0 ) path[ 0 ] = 0;
	}
	data->prop_cur_file[ data->prop_cur_disk ] = -1;
    }
    else
    {
	for( int a = 0; a < disks; a++ )
	{
	    data->disk_buttons[ a ]->color = wm->button_color;
	    if( data->disk_buttons[ a ] == win )
		data->prop_cur_disk = a;
	}
	//Show selected disk:
#ifdef GRAYSCALE
        data->disk_buttons[ data->prop_cur_disk ]->color = wm->white;
#else
	data->disk_buttons[ data->prop_cur_disk ]->color = wm->selection_color;
#endif
    }

    //Set new filelist:
    files_refresh_list( data->this_window, wm );
    //Set new path:
    if( data->prop_path[ data->prop_cur_disk ] )
	text_set_text( data->path, data->prop_path[ data->prop_cur_disk ], wm );
    else
	text_set_text( data->path, "", wm );

    //Save props:
    files_save_props( data->this_window, wm );
    draw_window( data->this_window, wm );
    return 0;
}

int files_ok_button_handler( void *user_data, WINDOWPTR win, window_manager *wm )
{
    files_data *data = (files_data*)user_data;

    files_name_handler( data, data->name, wm ); //Add file extension (if need).

    if( data->resulted_filename )
    {
	UTF8_CHAR *res = data->resulted_filename;
	res[ 0 ] = 0;
	mem_strcat( res, get_disk_name( data->prop_cur_disk ) );
	if( data->prop_path[ data->prop_cur_disk ] )
	    mem_strcat( res, data->prop_path[ data->prop_cur_disk ] );
	UTF8_CHAR *fname = text_get_text( data->name, wm );
	if( fname )
	    mem_strcat( res, fname );
    }
    remove_window( data->this_window, wm );
    recalc_regions( wm );
    draw_window( wm->root_win, wm );
    return 0;
}

int files_cancel_button_handler( void *user_data, WINDOWPTR win, window_manager *wm )
{
    files_data *data = (files_data*)user_data;
    if( data->resulted_filename )
    {
	UTF8_CHAR *res = data->resulted_filename;
	res[ 0 ] = 0;
    }
    remove_window( data->this_window, wm );
    recalc_regions( wm );
    draw_window( wm->root_win, wm );
    return 0;
}

int files_name_handler( void *user_data, WINDOWPTR win, window_manager *wm )
{
    files_data *data = (files_data*)user_data;
    UTF8_CHAR *name = text_get_text( data->name, wm );
    if( name && data->mask )
    {
	int a = 0;
	for( a = mem_strlen( data->mask ) - 1; a >= 0; a-- ) 
	{
	    if( data->mask[ a ] == '/' )
	    {
		//More than one mask. Can't make substitution
		return 0;
	    }
	}
	for( a = mem_strlen( name ) - 1; a >= 0; a-- )
	    if( name[ a ] == '.' ) break;
	if( a < 0 ) 
	{
	    //Dot not found:
	    UTF8_CHAR *new_name = (UTF8_CHAR*)MEM_NEW( HEAP_DYNAMIC, mem_strlen( name ) + 10 );
	    new_name[ 0 ] = 0;
	    mem_strcat( new_name, name );
	    a = mem_strlen( name );
	    new_name[ a ] = '.';
	    a++;
	    int p = 0;
	    for(;;)
	    {
		new_name[ a ] = data->mask[ p ];
		if( new_name[ a ] == '/' ||
		    new_name[ a ] == 0 )
		{
		    new_name[ a ] = 0;
		    break;
		}
		p++;
		a++;
	    }
	    text_set_text( data->name, new_name, wm );
	    draw_window( data->name, wm );
	    mem_free( new_name );
	}
	else
	{
	    //Dot found:
	    UTF8_CHAR *new_name = (UTF8_CHAR*)MEM_NEW( HEAP_DYNAMIC, mem_strlen( name ) + 10 );
	    new_name[ 0 ] = 0;
	    mem_strcat( new_name, name );
	    a++;
	    int p = 0;
	    for(;;)
	    {
		new_name[ a ] = data->mask[ p ];
		if( new_name[ a ] == '/' ||
		    new_name[ a ] == 0 )
		{
		    new_name[ a ] = 0;
		    break;
		}
		p++;
		a++;
	    }
	    text_set_text( data->name, new_name, wm );
	    draw_window( data->name, wm );
	    mem_free( new_name );
	}
    }
    return 0;
}

void files_refresh_list( WINDOWPTR win, window_manager *wm )
{
    files_data *data = (files_data*)win->data;
    list_data *ldata = list_get_data( data->files, wm );
    list_close( ldata );
    list_init( ldata );
    find_struct fs;
    UTF8_CHAR *disk_name = get_disk_name( data->prop_cur_disk );
    UTF8_CHAR *path = data->prop_path[ data->prop_cur_disk ];
    UTF8_CHAR *res = (UTF8_CHAR*)MEM_NEW( HEAP_DYNAMIC, mem_strlen( disk_name ) + mem_strlen( path ) + 1 );
    res[ 0 ] = 0;
    mem_strcat( res, disk_name );
    mem_strcat( res, path );
    fs.start_dir = res;
    fs.mask = data->mask;
    if( find_first( &fs ) )
    {
	if( fs.name[ 0 ] != '.' )
	{
	    list_add_item( fs.name, fs.type, ldata );
	}
    }
    while( find_next( &fs ) )
    {
	if( fs.name[ 0 ] != '.' )
	{
	    list_add_item( fs.name, fs.type, ldata );
	}
    }
    find_close( &fs );
    mem_free( res );
    list_sort( ldata );

    //Set file num:
    list_select_item( data->files, data->prop_cur_file[ data->prop_cur_disk ], wm );
}

void files_load_string( V3_FILE f, UTF8_CHAR *dest )
{
    for( int p = 0; p < MAX_DIR_LEN; p++ )
    {
	if( v3_eof( f ) ) { dest[ p ] = 0; break; }
	dest[ p ] = v3_getc( f );
	if( dest[ p ] == 0 ) break;
    }
}

int files_get_disk_num( UTF8_CHAR *name )
{
    for( int a = 0; a < disks; a++ )
    {
	if( mem_strcmp( name, get_disk_name( a ) ) == 0 ) return a;
    }
    return -1;
}

void files_load_props( WINDOWPTR win, window_manager *wm )
{
    if( win )
    {
	files_data *data = (files_data*)win->data;
	if( data->props_file )
	{
	    V3_FILE f = v3_open( data->props_file, "rb" );
	    if( f )
	    {
		UTF8_CHAR *temp_str = (UTF8_CHAR*)MEM_NEW( HEAP_DYNAMIC, 4096 );

		//Current disk:
		files_load_string( f, temp_str );
		int d = files_get_disk_num( temp_str );
		if( d == -1 ) 
		    data->prop_cur_disk = get_current_disk();
		else
		    data->prop_cur_disk = d;

		//Disks info:
		for( int a = 0; a < 32; a++ )
		{
		    if( v3_eof( f ) ) break;
		    //Disk name:
		    files_load_string( f, temp_str );
		    int d = files_get_disk_num( temp_str );
		    if( d != -1 )
		    {
			//Path:
			files_load_string( f, temp_str );
			if( data->prop_path[ d ] )
			{
			    //Remove old path:
			    mem_free( data->prop_path[ d ] );
			    data->prop_path[ d ] = 0;
			}
			if( temp_str[ 0 ] )
			{
			    //Set new path:
			    data->prop_path[ d ] = (UTF8_CHAR*)MEM_NEW( HEAP_DYNAMIC, mem_strlen( temp_str ) + 1 );
			    mem_copy( data->prop_path[ d ], temp_str, mem_strlen( temp_str ) + 1 );
			}
		    }
		    int file_num;
		    v3_read( &file_num, 4, 1, f );
		    if( d != -1 )
			data->prop_cur_file[ d ] = file_num;
		}
		
		mem_free( temp_str );
		v3_close( f );
	    }
	    else
	    {
		//File not found
		for( int a = 0; a < 32; a++ )
		{
		    data->prop_path[ a ] = 0;
		    data->prop_cur_file[ a ] = 0;
		}
		//Set current disk:
		data->prop_cur_disk = get_current_disk();
		//Set current path:
		UTF8_CHAR *cdir = get_current_path();
		if( cdir[ 1 ] == ':' && cdir[ 2 ] == '/' )
		    cdir += 3;
		else if( cdir[ 1 ] == ':' && cdir[ 2 ] != '/' )
		    cdir += 2;
		data->prop_path[ data->prop_cur_disk ] = (UTF8_CHAR*)MEM_NEW( HEAP_DYNAMIC, mem_strlen( cdir ) + 1 );
		mem_copy( data->prop_path[ data->prop_cur_disk ], cdir, mem_strlen( cdir ) + 1 );
	    }
	}
	//Refresh list:
	files_refresh_list( win, wm );
    }
}

void files_save_props( WINDOWPTR win, window_manager *wm )
{
    if( win )
    {
	files_data *data = (files_data*)win->data;
	if( data->props_file )
	{
	    V3_FILE f = v3_open( data->props_file, "wb" );
	    if( f )
	    {
		//Save current disk:
		UTF8_CHAR *path;
		UTF8_CHAR *disk_name = get_disk_name( data->prop_cur_disk );
	        v3_write( disk_name, 1, mem_strlen( disk_name ) + 1, f );
		//Save other properties:
		for( int a = 0; a < disks; a++ )
		{
		    //Disk name:
		    disk_name = get_disk_name( a );
		    v3_write( disk_name, 1, mem_strlen( disk_name ) + 1, f );
		    //Current path:
		    if( data->prop_path[ a ] == 0 )
			v3_putc( 0, f );
		    else
		    {
			path = data->prop_path[ a ];
			v3_write( path, 1, mem_strlen( path ) + 1, f );
		    }
		    //Selected file num:
		    v3_write( &data->prop_cur_file[ a ], 4, 1, f );
		}
		v3_close( f );
	    }
	}
    }
}

int files_handler( sundog_event *evt, window_manager *wm )
{
    if( window_handler_check_data( evt, wm ) ) return 0;
    int retval = 0;
    WINDOWPTR win = evt->win;
    files_data *data = (files_data*)win->data;
    int a, b;
    int but_xsize = 30;
    int but_ysize = BUTTON_YSIZE( win, wm );
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( files_data );
	    break;
	case EVT_AFTERCREATE:
	    //DATA INIT:
	    get_disks();
	    for( a = 0; a < 32; a ++ )
	    {
		data->prop_path[ a ] = 0;
		data->prop_cur_file[ a ] = 0;
	    }
	    data->prop_cur_disk = get_current_disk();
	    data->props_file = FILES_PROPS;
	    data->mask = FILES_MASK;
	    data->resulted_filename = FILES_RESULTED_FILENAME;
	    data->this_window = win;
	    data->first_time = 1;

	    //DISKS:
	    for( a = 0; a < 32; a++ ) data->disk_buttons[ a ] = 0;
	    data->go_up_button = new_window( 
		"..", 
		INTERELEMENT_SPACE, INTERELEMENT_SPACE, but_xsize, but_ysize - 4, 
		wm->button_color,
		win,
		button_handler, 
		wm );
	    set_handler( data->go_up_button, files_disk_button_handler, data, wm );
	    for( a = 0; a < disks; a++ )
	    {
		data->disk_buttons[ a ] = new_window( 
		    get_disk_name( a ), 
		    INTERELEMENT_SPACE + ( but_xsize + INTERELEMENT_SPACE ) * ( a + 1 ), INTERELEMENT_SPACE, but_xsize, but_ysize - 4, 
		    wm->button_color,
		    win,
		    button_handler, 
		    wm );
		set_handler( data->disk_buttons[ a ], files_disk_button_handler, data, wm );
	    }

	    //DIRECTORY:
	    a = string_size( win, "#####", wm ) + 4;
	    b = INTERELEMENT_SPACE + but_ysize - 4 + INTERELEMENT_SPACE;
	    new_window( "Path:", INTERELEMENT_SPACE, b, a, char_y_size( win, wm ) + 4, 0, win, label_handler, wm );
	    data->path = new_window( "pathn", 0, 0, 1, 1, wm->text_background, win, text_handler, wm );
	    set_window_controller( data->path, 0, wm, INTERELEMENT_SPACE + a, CEND );
	    set_window_controller( data->path, 1, wm, b, CEND );
	    set_window_controller( data->path, 2, wm, CPERC, 100, CSUB, INTERELEMENT_SPACE, CEND );
	    set_window_controller( data->path, 3, wm, b + char_y_size( win, wm ) + 4, CEND );

	    //FILENAME:
	    b += char_y_size( win, wm ) + 4 + INTERELEMENT_SPACE;
	    new_window( "Name:", INTERELEMENT_SPACE, b, string_size( win, "Name:" , wm ) + 4, char_y_size( win, wm ) + 4, 0, win, label_handler, wm );
	    data->name = new_window( "namen", 0, 0, 1, 1, wm->text_background, win, text_handler, wm );
	    set_handler( data->name, files_name_handler, data, wm );
	    set_window_controller( data->name, 0, wm, INTERELEMENT_SPACE + a, CEND );
	    set_window_controller( data->name, 1, wm, b, CEND );
	    set_window_controller( data->name, 2, wm, CPERC, 100, CSUB, INTERELEMENT_SPACE, CEND );
	    set_window_controller( data->name, 3, wm, b + char_y_size( win, wm ) + 4, CEND );

	    //FILES:
	    b += char_y_size( win, wm ) + 4 + INTERELEMENT_SPACE;
	    data->files = new_window( "files", 0, b, 100, 100, wm->list_background, win, list_handler, wm );
	    set_handler( data->files, files_list_handler, data, wm );
	    set_window_controller( data->files, 0, wm, CPERC, 0, CADD, INTERELEMENT_SPACE, CEND );
	    set_window_controller( data->files, 1, wm, b, CEND );
	    set_window_controller( data->files, 2, wm, CPERC, 100, CSUB, INTERELEMENT_SPACE, CEND );
	    set_window_controller( data->files, 3, wm, CPERC, 100, CSUB, but_ysize + INTERELEMENT_SPACE * 2, CEND );

	    //BUTTONS:
	    data->ok_button = new_window( "OK", 0, 0, 1, 1, wm->button_color, win, button_handler, wm );
	    data->cancel_button = new_window( "Cancel", 0, 0, 1, 1, wm->button_color, win, button_handler, wm );
	    set_handler( data->ok_button, files_ok_button_handler, data, wm );
	    set_handler( data->cancel_button, files_cancel_button_handler, data, wm );
	    set_window_controller( data->ok_button, 0, wm, INTERELEMENT_SPACE, CEND );
	    set_window_controller( data->ok_button, 1, wm, CPERC, 100, CSUB, INTERELEMENT_SPACE, CEND );
	    set_window_controller( data->ok_button, 2, wm, INTERELEMENT_SPACE + BUTTON_XSIZE( win, wm ), CEND );
	    set_window_controller( data->ok_button, 3, wm, CPERC, 100, CSUB, but_ysize + INTERELEMENT_SPACE, CEND );
	    set_window_controller( data->cancel_button, 0, wm, INTERELEMENT_SPACE + BUTTON_XSIZE( win, wm ) + 1, CEND );
	    set_window_controller( data->cancel_button, 1, wm, CPERC, 100, CSUB, INTERELEMENT_SPACE, CEND );
	    set_window_controller( data->cancel_button, 2, wm, INTERELEMENT_SPACE + BUTTON_XSIZE( win, wm ) + 1 + BUTTON_XSIZE( win, wm ), CEND );
	    set_window_controller( data->cancel_button, 3, wm, CPERC, 100, CSUB, but_ysize + INTERELEMENT_SPACE, CEND );

	    //LOAD LAST PROPERTIES:
	    recalc_regions( wm ); //for getting calculated size of file's list
    	    files_load_props( win, wm );
	    if( data->prop_path[ data->prop_cur_disk ] )
		text_set_text( data->path, data->prop_path[ data->prop_cur_disk ], wm );
	    if( data->prop_cur_file >= 0 )
	    {
		list_data *ldata = list_get_data( data->files, wm );
		UTF8_CHAR *item = list_get_item( list_get_selected_num( ldata ), ldata );//Set file num:
		list_select_item( data->files, data->prop_cur_file[ data->prop_cur_disk ], wm );
		if( item ) 
		    text_set_text( data->name, item, wm );
	    }

	    //SHOW CURRENT DISK:
#ifdef GRAYSCALE
	    data->disk_buttons[ data->prop_cur_disk ]->color = wm->white;
#else
	    data->disk_buttons[ data->prop_cur_disk ]->color = wm->selection_color;
#endif

	    //SET FOCUS:
	    set_focus_win( data->files, wm );

	    FILES_PROPS = 0;
	    FILES_MASK = 0;
	    FILES_RESULTED_FILENAME = 0;
	    retval = 1;
	    break;
	case EVT_BEFORECLOSE:
	    files_save_props( win, wm );
	    for( a = 0; a < 32; a++ )
	    {
		if( data->prop_path[ a ] ) mem_free( data->prop_path[ a ] );
	    }
	    break;
	case EVT_DRAW:
	    win_draw_lock( win, wm );
	    win_draw_box( win, 0, 0, win->xsize, win->ysize, win->color, wm );
	    if( data->first_time )
	    {
		//Set file num:
		list_select_item( data->files, data->prop_cur_file[ data->prop_cur_disk ], wm );
		data->first_time = 0;
	    }
	    win_draw_unlock( win, wm );
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONDOWN:
	case EVT_MOUSEMOVE:
	case EVT_MOUSEBUTTONUP:
	    retval = 1;
	    break;
    }
    return retval;
}

const UTF8_CHAR *DIALOG_BUTTONS_TEXT = 0;
const UTF8_CHAR *DIALOG_TEXT = 0;
int *DIALOG_RESULT = 0;

struct dialog_data
{
    WINDOWPTR this_window;
    const UTF8_CHAR *text;
    int *result;
    UTF8_CHAR **buttons_text;
    WINDOWPTR *buttons;
    int buttons_num;
};

int dialog_button_handler( void *user_data, WINDOWPTR win, window_manager *wm )
{
    dialog_data *data = (dialog_data*)user_data;
    if( data->result ) 
    {
	int i;
	for( i = 0; i < data->buttons_num; i++ )
	{
	    if( data->buttons[ i ] == win )
	    {
		*data->result = i;
		break;
	    }
	}
	if( i == data->buttons_num )
	{
	    *data->result = -1;
	}
    }
    remove_window( data->this_window, wm );
    recalc_regions( wm );
    draw_window( wm->root_win, wm );
    return 0;
}

int dialog_handler( sundog_event *evt, window_manager *wm )
{
    if( window_handler_check_data( evt, wm ) ) return 0;
    int retval = 0;
    WINDOWPTR win = evt->win;
    dialog_data *data = (dialog_data*)win->data;
    int but_ysize = BUTTON_YSIZE( win, wm );
    int but_xsize = BUTTON_XSIZE( win, wm );
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( dialog_data );
	    break;
	case EVT_AFTERCREATE:
	    {
		data->this_window = win;
		data->text = DIALOG_TEXT;
		data->result = DIALOG_RESULT;
		
		data->buttons_text = 0;
		if( DIALOG_BUTTONS_TEXT )
		{
		    int buttons_num = 1;
		    int i, p, len;
		    int slen = mem_strlen( DIALOG_BUTTONS_TEXT );
		    for( i = 0; i < slen; i++ )
		    {
			if( DIALOG_BUTTONS_TEXT[ i ] == ';' )
			    buttons_num++;
		    }
		    data->buttons_text = (UTF8_CHAR**)MEM_NEW( HEAP_DYNAMIC, sizeof( UTF8_CHAR* ) * buttons_num );
		    data->buttons = (WINDOWPTR*)MEM_NEW( HEAP_DYNAMIC, sizeof( WINDOWPTR ) * buttons_num );
		    data->buttons_num = buttons_num;
		    mem_set( data->buttons_text, sizeof( UTF8_CHAR* ) * buttons_num, 0 );
		    int word_start = 0;
		    for( i = 0; i < buttons_num; i++ )
		    {
			len = 0;
			while( 1 )
			{
			    if( DIALOG_BUTTONS_TEXT[ word_start + len ] == 0 || DIALOG_BUTTONS_TEXT[ word_start + len ] == ';' )
			    {
				break;
			    }
			    len++;			    
			}
			if( len > 0 )
			{
			    data->buttons_text[ i ] = (UTF8_CHAR*)MEM_NEW( HEAP_DYNAMIC, len + 1 );
			    for( p = word_start; p < word_start + len; p++ )
			    {
				data->buttons_text[ i ][ p - word_start ] = DIALOG_BUTTONS_TEXT[ p ];
			    }
			    data->buttons_text[ i ][ p - word_start ] = 0;
			}
			word_start += len + 1;
			if( word_start >= slen ) break;
		    }
		    
		    //Create buttons:
		    if( buttons_num > 2 )
		    {
			but_xsize = ( win->xsize - INTERELEMENT_SPACE * 2 ) / buttons_num - 1;
		    }
		    int xoff = 0;
		    for( i = 0; i < buttons_num; i++ )
		    {
			data->buttons[ i ] = new_window( data->buttons_text[ i ], 0, 0, 10, 10, wm->button_color, win, button_handler, wm );
			set_handler( data->buttons[ i ], dialog_button_handler, data, wm );
			set_window_controller( data->buttons[ i ], 0, wm, xoff + INTERELEMENT_SPACE, CEND );
			set_window_controller( data->buttons[ i ], 1, wm, CPERC, 100, CSUB, INTERELEMENT_SPACE, CEND );
			set_window_controller( data->buttons[ i ], 2, wm, xoff + INTERELEMENT_SPACE + but_xsize, CEND );
			set_window_controller( data->buttons[ i ], 3, wm, CPERC, 100, CSUB, but_ysize + INTERELEMENT_SPACE, CEND );
			xoff += but_xsize + 1;
		    }
		}
		
		DIALOG_BUTTONS_TEXT = 0;
		DIALOG_TEXT = 0;
		DIALOG_RESULT = 0;

		//SET FOCUS:
		set_focus_win( win, wm );
	    }
	    retval = 1;
	    break;
	case EVT_BEFORECLOSE:
	    if( data->buttons_text )
	    {
		for( int i = 0; i < data->buttons_num; i++ )
		{
		    if( data->buttons_text[ i ] )
		    {
			mem_free( data->buttons_text[ i ] );
		    }
		}
		mem_free( data->buttons_text );
	    }
	    if( data->buttons )
	    {
		mem_free( data->buttons );
	    }
	    retval = 1;
	    break;
	case EVT_DRAW:
	    win_draw_lock( win, wm );
	    win_draw_box( win, 0, 0, win->xsize, win->ysize, win->color, wm );
	    if( data->text )
		win_draw_string( win, data->text, INTERELEMENT_SPACE, INTERELEMENT_SPACE, wm->black, win->color, wm );
	    win_draw_unlock( win, wm );
	    retval = 1;
	    break;
	case EVT_BUTTONDOWN:
	    if( evt->key == KEY_ESCAPE )
	    {
		dialog_button_handler( data, 0, wm );
		retval = 1;
	    }
	    if( evt->key == 'y' || evt->key == KEY_ENTER )
	    {
		if( data->buttons_num < 3 )
		{
		    dialog_button_handler( data, data->buttons[ 0 ], wm );
		    retval = 1;
		}
	    }
	    if( evt->key == 'n' )
	    {
		if( data->buttons_num < 3 )
		{
		    dialog_button_handler( data, data->buttons[ 1 ], wm );
		    retval = 1;
		}
	    }
	    break;
	case EVT_MOUSEBUTTONDOWN:
	case EVT_MOUSEMOVE:
	case EVT_MOUSEBUTTONUP:
	    retval = 1;
	    break;
    }
    return retval;
}

struct popup_data
{
    WINDOWPTR this_window;
    UTF8_CHAR *text;
    int lines[ 128 ];
    int lines_num;
    int current_selected;
    WINDOWPTR prev_focus;
    WINDOWPTR level;
    int *save_level_to;
    int *exit_flag;
    int *result;
};

const UTF8_CHAR *CREATE_TEXT_POPUP = 0;
int CREATE_LEVEL_POPUP = 0;
int *SAVE_LEVEL_TO = 0;
int *SAVE_POPUP_EXIT_TO = 0;
int *SAVE_POPUP_RESULT_TO = 0;

int popup_level_handler( void *user_data, WINDOWPTR win, window_manager *wm )
{
    popup_data *data = (popup_data*)user_data;
    scrollbar_data *sdata = (scrollbar_data*)win->data;

    if( sdata->bar_selected == 0 )
    {
	win->action_result = scrollbar_get_value( win, wm );
	if( data->save_level_to )
	    *data->save_level_to = win->action_result;
	remove_window( data->this_window, wm );
	recalc_regions( wm );
	draw_window( wm->root_win, wm );
    }
    
    return 0;
}

int popup_handler( sundog_event *evt, window_manager *wm )
{
    if( window_handler_check_data( evt, wm ) ) return 0;
    int retval = 0;
    WINDOWPTR win = evt->win;
    popup_data *data = (popup_data*)win->data;
    int popup_border = 3;
    int rx = evt->x - win->screen_x;
    int ry = evt->y - win->screen_y;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( popup_data );
	    break;
	case EVT_AFTERCREATE:
	    {
		//Set focus:
		data->prev_focus = wm->focus_win;

		//SET FOCUS:
		set_focus_win( win, wm );

		//Data init:
		data->this_window = win;
	    	data->text = 0;
		data->level = 0;
		data->save_level_to = 0;
		data->current_selected = -1;
		win->action_result = -1;

		//Set window size:
		int xsize = 0;
		int ysize = 0;
		int x = win->x;
		int y = win->y;
		if( win->name && win->name[ 0 ] != 0 )
		{
		    xsize = string_size( win, win->name, wm );
		    ysize = char_y_size( win, wm ) + popup_border;
		}
		if( CREATE_TEXT_POPUP )
		{
		    //Text popup:
		    data->text = (UTF8_CHAR*)MEM_NEW( HEAP_DYNAMIC, mem_strlen( CREATE_TEXT_POPUP ) + 1 );
		    mem_copy( data->text, CREATE_TEXT_POPUP, mem_strlen( CREATE_TEXT_POPUP ) + 1 );
		    int lines = 1;
		    int max_x_size = 1;
		    int cur_x_size = 0;
		    data->lines[ 0 ] = 0;
		    int nl_code = 0;
		    int text_size = mem_strlen( data->text );
		    for( int i = 0; i < text_size; i++ )
		    {
			if( data->text[ i ] == 0xA ) { data->text[ i ] = 0; lines++; cur_x_size = 0; nl_code = 1; }
			else
			{
			    if( data->text[ i ] != 0xA && data->text[ i ] != 0xD ) 
			    {
				if( nl_code == 1 ) data->lines[ lines - 1 ] = i;
				nl_code = 0;
				cur_x_size += char_x_size( win, data->text[ i ], wm );
				if( cur_x_size > max_x_size ) max_x_size = cur_x_size;
			    }
			    else
			    {
				data->text[ i ] = 0;
				nl_code = 1;
			    }
			}
		    }
		    data->lines_num = lines;
		    if( max_x_size > xsize ) xsize = max_x_size;
		    ysize += lines * char_y_size( win, wm );
		}
		if( CREATE_LEVEL_POPUP )
		{
		    data->save_level_to = SAVE_LEVEL_TO;
		    if( SCROLLBAR_SIZE > xsize ) xsize = SCROLLBAR_SIZE;
		    CREATE_VERTICAL_SCROLLBAR = 1;
		    data->level = new_window( 
			"Level", 
			popup_border,
			popup_border + ysize,
			SCROLLBAR_SIZE,
			128,
			win->color,
			win,
			scrollbar_handler, 
			wm );
		    set_handler( data->level, popup_level_handler, data, wm );
		    int start_val = 0;
		    if( data->save_level_to ) start_val = *data->save_level_to;
		    scrollbar_set_parameters( data->level, start_val, CREATE_LEVEL_POPUP, CREATE_LEVEL_POPUP / 4, 1, wm );
		    ysize += 128;
		}
		xsize += popup_border * 2;
		ysize += popup_border * 2;

		//Control window position:
		if( x + xsize > win->parent->xsize && x > 0 ) x -= ( x + xsize ) - win->parent->xsize;
		if( y + ysize > win->parent->ysize && y > 0 ) y -= ( y + ysize ) - win->parent->ysize;
		if( x < 0 ) x = 0;
		if( y < 0 ) y = 0;

		win->x = x;
		win->y = y;
		win->xsize = xsize;
		win->ysize = ysize;

		data->exit_flag = SAVE_POPUP_EXIT_TO;
		data->result = SAVE_POPUP_RESULT_TO;

		CREATE_TEXT_POPUP = 0;
		CREATE_LEVEL_POPUP = 0;
		SAVE_LEVEL_TO = 0;
		SAVE_POPUP_EXIT_TO = 0;
		SAVE_POPUP_RESULT_TO = 0;
    	    
		retval = 1;
	    }
	    break;
	case EVT_BEFORECLOSE:
	    if( data->text ) mem_free( data->text );
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONDOWN:
	case EVT_MOUSEMOVE:
	    {
		int cur_y = popup_border;
		if( win->name && win->name[ 0 ] != 0 )
		{
		    cur_y += char_y_size( win, wm ) + popup_border;
		}
		if( data->text && data->lines_num > 0 )
		{
		    data->current_selected = ( ry - cur_y ) / char_y_size( win, wm );
		}
		//If out of bounds:
		if( rx < 0 || rx >= win->xsize ||
		    ry < cur_y || ry >= win->ysize )
		    data->current_selected = -1;
	    }
	    draw_window( win, wm );
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONUP:
	    if( data->current_selected >= 0 && evt->key == MOUSE_BUTTON_LEFT )
	    {
		//Successful selection:
		//win->action_result = data->current_selected;
		if( data->result )
		    *data->result = data->current_selected;
		set_focus_win( data->prev_focus, wm ); // -> EVT_UNFOCUS -> remove_window()
	    }
	    retval = 1;
	    break;
	case EVT_UNFOCUS:
	    if( data->exit_flag ) 
		*data->exit_flag = 1;
	    remove_window( win, wm );
	    recalc_regions( wm );
	    draw_window( wm->root_win, wm );
	    retval = 1;
	    break;
	case EVT_DRAW:
	    win_draw_lock( win, wm );
	    {
		int cur_y = popup_border;
		win_draw_box( win, 1, 1, win->xsize - 2, win->ysize - 2, win->color, wm );

		//Draw name:
		if( win->name && win->name[ 0 ] != 0 )
		{
		    win_draw_box( win, popup_border - 1, cur_y - 1, win->xsize - popup_border * 2 + 2, char_y_size( win, wm ) + 2, blend( win->color, wm->black, 100 ), wm );
		    win_draw_string( win, win->name, popup_border, cur_y, wm->white, blend( win->color, wm->black, 100 ), wm );
		    cur_y += char_y_size( win, wm ) + popup_border;
		}

		//Draw text:
		if( data->text && data->lines_num > 0 )
		{
		    for( int i = 0; i < data->lines_num; i++ )
		    {
			if( i == data->current_selected )
			{
			    win_draw_box( win, popup_border - 1, cur_y, win->xsize - popup_border * 2 + 2, char_y_size( win, wm ), wm->black, wm );
			    win_draw_string( win, data->text + data->lines[ i ], popup_border, cur_y, wm->white, wm->black, wm );
			}
			else
			{
			    win_draw_string( win, data->text + data->lines[ i ], popup_border, cur_y, wm->black, win->color, wm );
			}
			cur_y += char_y_size( win, wm );
		    }
		}

		win_draw_frame( win, 0, 0, win->xsize, win->ysize, wm->black, wm );

		retval = 1;
	    }
	    win_draw_unlock( win, wm );
	    break;
    }
    return retval;
}

int popup_menu( const UTF8_CHAR* name, const UTF8_CHAR* items, int levels, int x, int y, COLOR color, window_manager *wm )
{
    int exit_flag = 0;
    int result = -1;
    CREATE_TEXT_POPUP = items;
    CREATE_LEVEL_POPUP = levels;
    SAVE_POPUP_EXIT_TO = &exit_flag;
    SAVE_POPUP_RESULT_TO = &result;
    WINDOWPTR win = new_window( name, x, y, 1, 1, color, wm->root_win, popup_handler, wm );
    show_window( win, wm );
    recalc_regions( wm );
    draw_window( wm->root_win, wm );
    while( 1 )
    {
	sundog_event evt;
	EVENT_LOOP_BEGIN( &evt, wm );
	if( EVENT_LOOP_END( wm ) ) break;
	if( exit_flag || win->visible == 0 ) break;
    }
    return result; //win->action_result;
}
