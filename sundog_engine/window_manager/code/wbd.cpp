/*
    wbd.cpp. WBD (Window Buffer Draw) functions
    This file is part of the SunDog engine.
    Copyright (C) 2002 - 2009 Alex Zolotov <nightradio@gmail.com>
*/

//How to use it in a window handler:
//EVT_DRAW:	
//  win_draw_lock( win, wm );
//  wbd_lock( win, wm );
//  ...
//  wbd_draw( wm );
//  wbd_unlock( wm );
//  win_draw_unlock( win, wm );

#include "../../main/user_code.h"
#include "../../core/debug.h"
#include "../../utils/utils.h"
#include "../wmanager.h"

extern sundog_image *g_font0_img;
extern sundog_image *g_font1_img;

void wbd_lock( WINDOWPTR win, window_manager *wm )
{
#if defined(OPENGL) && !defined(FRAMEBUFFER)
    if( win )
    {
	glEnable( GL_STENCIL_TEST ); //Enable using the stencil buffer
	glColorMask( 0, 0, 0, 0 ); //Disable drawing colors to the screen
	glStencilFunc( GL_ALWAYS, 1, 1 ); //Make the stencil test always pass
	//Make pixels in the stencil buffer be set to 1 when the stencil test passes:
	glStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
	//Draw the window:
	if( win->reg->numRects )
        {
            for( int r = 0; r < win->reg->numRects; r++ )
            {
                int rx1 = win->reg->rects[ r ].left;
                int rx2 = win->reg->rects[ r ].right;
                int ry1 = win->reg->rects[ r ].top;
                int ry2 = win->reg->rects[ r ].bottom;
		glBegin( GL_POLYGON );
    		glVertex2i( rx1, ry1 );
    		glVertex2i( rx2, ry1 );
    		glVertex2i( rx2, ry2 );
    		glVertex2i( rx1, ry2 );
    		glEnd();
	    }
	}
	glColorMask( 1, 1, 1, 1 ); //Enable drawing colors to the screen
	//Make the stencil test pass only when the pixel is 1 in the stencil buffer:
	glStencilFunc( GL_EQUAL, 1, 1 );
	glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP ); //Make the stencil buffer not change
    }
#endif
    if( win )
    {
	if( wm->g_screen == 0 )
	{
	    wm->g_screen = (COLORPTR)MEM_NEW( HEAP_DYNAMIC, wm->screen_xsize * wm->screen_ysize * COLORLEN );
	}
	else
	{
    	    int real_size = mem_get_size( wm->g_screen ) / COLORLEN;
    	    int new_size = wm->screen_xsize * wm->screen_ysize;
    	    if( new_size > real_size )
    	    {
    		//Resize:
    		wm->g_screen = (COLORPTR)mem_resize( wm->g_screen, ( new_size + ( new_size / 4 ) ) * COLORLEN );
	    }
	}
	wm->cur_window = win;
	if( win->xsize == 0 ||
	    win->ysize == 0 )
	{
	    //Wrong window size:
	    wm->cur_window = 0;
	}
    }
}

void wbd_unlock( window_manager *wm )
{
#if defined(OPENGL) && !defined(FRAMEBUFFER)
    if( wm->cur_window )
    {
	glEnable( GL_STENCIL_TEST ); //Enable using the stencil buffer
	glColorMask( 0, 0, 0, 0 ); //Disable drawing colors to the screen
	glStencilFunc( GL_ALWAYS, 0, 1 ); //Make the stencil test always pass
	//Make pixels in the stencil buffer be set to 1 when the stencil test passes:
	glStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
	//Draw the window:
	WINDOWPTR win = wm->cur_window;
	if( win->reg->numRects )
        {
            for( int r = 0; r < win->reg->numRects; r++ )
            {
                int rx1 = win->reg->rects[ r ].left;
                int rx2 = win->reg->rects[ r ].right;
                int ry1 = win->reg->rects[ r ].top;
                int ry2 = win->reg->rects[ r ].bottom;
		glBegin( GL_POLYGON );
    		glVertex2i( rx1, ry1 );
    		glVertex2i( rx2, ry1 );
    		glVertex2i( rx2, ry2 );
    		glVertex2i( rx1, ry2 );
    		glEnd();
	    }
	}
	glColorMask( 1, 1, 1, 1 ); //Enable drawing colors to the screen
	glDisable( GL_STENCIL_TEST ); 
    }
#endif
    wm->cur_window = 0;
}

void wbd_draw( window_manager *wm )
{
#if defined(OPENGL) && !defined(FRAMEBUFFER)
#else
    WINDOWPTR win = wm->cur_window;
    sundog_image img;
    img.xsize = wm->screen_xsize;
    img.ysize = wm->screen_ysize;
    img.flags = IMAGE_NATIVE_RGB | IMAGE_STATIC_SOURCE;
    img.data = wm->g_screen;
    win_draw_bitmap_ext( win, 0, 0, win->xsize, win->ysize, win->screen_x, win->screen_y, &img, wm );
#endif
}

void clear_screen( window_manager *wm )
{
#if defined(OPENGL) && !defined(FRAMEBUFFER)
    int rx1 = wm->cur_window->screen_x;
    int ry1 = wm->cur_window->screen_y;
    int rx2 = wm->cur_window->screen_x + wm->cur_window->xsize;
    int ry2 = wm->cur_window->screen_y + wm->cur_window->ysize;
    glColor4ub( 0, 0, 0, 255 );
    glBegin( GL_POLYGON );
    glVertex2i( rx1, ry1 );
    glVertex2i( rx2, ry1 );
    glVertex2i( rx2, ry2 );
    glVertex2i( rx1, ry2 );
    glEnd();
#endif
    draw_box( 0, 0, wm->cur_window->xsize, wm->cur_window->ysize, wm->black, wm );
}

#define bottom 1
#define top 2
#define left 4
#define right 8
int make_code( int x, int y, int clip_x, int clip_y )
{
    int code = 0;
    if( y >= clip_y ) code = bottom;
    else if( y < 0 ) code = top;
    if( x >= clip_x ) code += right;
    else if( x < 0 ) code += left;
    return code;
}

void draw_line( int x1, int y1, int x2, int y2, COLOR color, window_manager *wm )
{
    x1 += wm->cur_window->screen_x;
    y1 += wm->cur_window->screen_y;
    x2 += wm->cur_window->screen_x;
    y2 += wm->cur_window->screen_y;

#if defined(OPENGL) && !defined(FRAMEBUFFER)
    unsigned int transp = wm->cur_transparency;
    if( transp > 255 ) transp = 255;
    glColor4ub( red( color ), green( color ), blue( color ), transp );

    glBegin( GL_LINES );

    glVertex2i( x1, y1 );
    glVertex2i( x2, y2 );

    glVertex2i( x1, y1 );
    glVertex2i( x1 + 1, y1 );

    glVertex2i( x2, y2 );
    glVertex2i( x2 + 1, y2 );

    glEnd();
#else

    int screen_xsize = wm->screen_xsize;
    int screen_ysize = wm->screen_ysize;
    COLORPTR g_screen = wm->g_screen;

    if( x1 == x2 )
    {
	if( x1 >= 0 && x1 < screen_xsize )
	{
	    if( y1 > y2 ) { int temp = y1; y1 = y2; y2 = temp; }
	    if( y1 < 0 ) y1 = 0;
	    if( y1 >= screen_ysize ) return;
	    if( y2 < 0 ) return;
	    if( y2 >= screen_ysize ) y2 = screen_ysize - 1;
	    int ptr = y1 * screen_xsize + x1;
	    int transp = wm->cur_transparency;
	    if( transp >= 256 )
	    {
		for( y1; y1 <= y2; y1++ )
		{
		    g_screen[ ptr ] = color;
		    ptr += screen_xsize;
		}
	    }
	    else
	    {
		for( y1; y1 <= y2; y1++ )
		{
		    g_screen[ ptr ] = blend( g_screen[ ptr ], color, transp );
		    ptr += screen_xsize;
		}
	    }
	}
	return;
    }
    if( y1 == y2 )
    {
	if( y1 >= 0 && y1 < screen_ysize )
	{
	    if( x1 > x2 ) { int temp = x1; x1 = x2; x2 = temp; }
	    if( x1 < 0 ) x1 = 0;
	    if( x1 >= screen_xsize ) return;
	    if( x2 < 0 ) return;
	    if( x2 >= screen_xsize ) x2 = screen_xsize - 1;
	    int ptr = y1 * screen_xsize + x1;
	    int transp = wm->cur_transparency;
	    if( transp >= 256 )
	    {
		for( x1; x1 <= x2; x1++ )
		{
		    g_screen[ ptr ] = color;
		    ptr ++;
		}
	    }
	    else
	    {
		for( x1; x1 <= x2; x1++ )
		{
		    g_screen[ ptr ] = blend( g_screen[ ptr ], color, transp );
		    ptr ++;
		}
	    }
	}
	return;
    }

    //Cohen-Sutherland line clipping algorithm:
    int code0;
    int code1;
    int out_code;
    int x, y;
    code0 = make_code( x1, y1, screen_xsize, screen_ysize );
    code1 = make_code( x2, y2, screen_xsize, screen_ysize );
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

	    if( out_code & bottom )
	    {
		//Clip the line to the bottom of the viewport
		y = screen_ysize - 1;
		x = x1 + ( x2 - x1 ) * ( y - y1 ) / ( y2 - y1 );
	    }
	    else 
	    if( out_code & top )
	    {
		y = 0;
		x = x1 + ( x2 - x1 ) * ( y - y1 ) / ( y2 - y1 );
	    }
	    else
	    if( out_code & right )
	    {
		x = screen_xsize - 1;
		y = y1 + ( y2 - y1 ) * ( x - x1 ) / ( x2 - x1 );
	    }
	    else
	    if( out_code & left )
	    {
		x = 0;
		y = y1 + ( y2 - y1 ) * ( x - x1 ) / ( x2 - x1 );
	    }

	    if( out_code == code0 )
	    { //Modify the first coordinate 
		x1 = x; y1 = y;
		code0 = make_code( x1, y1, screen_xsize, screen_ysize );
	    }
	    else
	    { //Modify the second coordinate
		x2 = x; y2 = y;
		code1 = make_code( x2, y2, screen_xsize, screen_ysize );
	    }
	}
    }

    //Draw line:
    int len_x = x2 - x1; if( len_x < 0 ) len_x = -len_x;
    int len_y = y2 - y1; if( len_y < 0 ) len_y = -len_y;
    int ptr = y1 * screen_xsize + x1;
    int delta;
    int v = 0, old_v = 0;
    int transp = wm->cur_transparency;
    if( len_x > len_y )
    {
	//Horisontal:
	if( len_x != 0 )
	    delta = ( len_y << 10 ) / len_x;
	else
	    delta = 0;
	if( transp >= 256 )
	    for( int a = 0; a <= len_x; a++ )
	    {
		g_screen[ ptr ] = color;
		old_v = v;
		v += delta;
		if( x2 - x1 > 0 ) ptr++; else ptr--;
		if( ( old_v >> 10 ) != ( v >> 10 ) ) 
		{
		    if( y2 - y1 > 0 )
			ptr += screen_xsize;
		    else
			ptr -= screen_xsize;
		}
	    }
	else
	    for( int a = 0; a <= len_x; a++ )
	    {
		g_screen[ ptr ] = blend( g_screen[ ptr ], color, transp );
		old_v = v;
		v += delta;
		if( x2 - x1 > 0 ) ptr++; else ptr--;
		if( ( old_v >> 10 ) != ( v >> 10 ) ) 
		{
		    if( y2 - y1 > 0 )
			ptr += screen_xsize;
		    else
			ptr -= screen_xsize;
		}
	    }
    }
    else
    {
	//Vertical:
	if( len_y != 0 ) 
	    delta = ( len_x << 10 ) / len_y;
	else
	    delta = 0;
	if( transp >= 256 )
	    for( int a = 0; a <= len_y; a++ )
	    {
		g_screen[ ptr ] = color;
		old_v = v;
		v += delta;
		if( y2 - y1 > 0 ) 
		    ptr += screen_xsize;
		else
		    ptr -= screen_xsize;
		if( ( old_v >> 10 ) != ( v >> 10 ) ) 
		{
		    if( x2 - x1 > 0 ) ptr++; else ptr--;
		}
	    }
	else
	    for( int a = 0; a <= len_y; a++ )
	    {
		g_screen[ ptr ] = blend( g_screen[ ptr ], color, transp );
		old_v = v;
		v += delta;
		if( y2 - y1 > 0 ) 
		    ptr += screen_xsize;
		else
		    ptr -= screen_xsize;
		if( ( old_v >> 10 ) != ( v >> 10 ) ) 
		{
		    if( x2 - x1 > 0 ) ptr++; else ptr--;
		}
	    }
    }
#endif
}

void draw_frame( int x, int y, int x_size, int y_size, COLOR color, window_manager *wm )
{
    draw_line( x, y, x + x_size, y, color, wm );
    draw_line( x + x_size, y, x + x_size, y + y_size, color, wm );
    draw_line( x + x_size, y + y_size, x, y + y_size, color, wm );
    draw_line( x, y + y_size, x, y, color, wm );
}

void draw_box( int x, int y, int x_size, int y_size, COLOR color, window_manager *wm )
{
    x += wm->cur_window->screen_x;
    y += wm->cur_window->screen_y;

    if( x < 0 ) { x_size += x; x = 0; }
    if( y < 0 ) { y_size += y; y = 0; }
    if( x + x_size <= 0 ) return;
    if( y + y_size <= 0 ) return;

    int screen_xsize = wm->screen_xsize;
    int screen_ysize = wm->screen_ysize;

    if( x + x_size > screen_xsize ) x_size -= x + x_size - screen_xsize;
    if( y + y_size > screen_ysize ) y_size -= y + y_size - screen_ysize;
    if( x >= screen_xsize ) return;
    if( y >= screen_ysize ) return;
    if( x_size < 0 ) return;
    if( y_size < 0 ) return;

#if defined(OPENGL) && !defined(FRAMEBUFFER)
    unsigned int transp = wm->cur_transparency;
    if( transp > 255 ) transp = 255;
    glColor4ub( red( color ), green( color ), blue( color ), transp );

    glBegin( GL_QUADS );
    glVertex2i( x, y );
    glVertex2i( x + x_size, y );
    glVertex2i( x + x_size, y + y_size );
    glVertex2i( x, y + y_size );
    glEnd();
#else

    COLORPTR ptr = wm->g_screen + y * screen_xsize + x;
    int transp = wm->cur_transparency;
    int add = screen_xsize - x_size;
    if( transp >= 255 )
	for( int cy = 0; cy < y_size; cy++ )
	{
	    COLORPTR size = ptr + x_size;
	    while( ptr < size ) *ptr++ = color;
	    ptr += add;
	}
    else
	for( int cy = 0; cy < y_size; cy++ )
	{
	    COLORPTR size = ptr + x_size;
	    while( ptr < size ) *ptr++ = blend( *ptr, color, transp );
	    ptr += add;
	}
#endif
}

int draw_char( UTF32_CHAR c, int x, int y, window_manager *wm )
{
    if( c < 0x10 ) return 0; //Control symbols

    WINDOWPTR win = wm->cur_window;

    x += win->screen_x;
    y += win->screen_y;

    int char_xsize_zoom = wbd_char_x_size( c, wm );

    //Bounds control:
    if( x < 0 ) return char_xsize_zoom;
    if( y < 0 ) return char_xsize_zoom;
    if( x > wm->screen_xsize - char_xsize_zoom ) return char_xsize_zoom;
    if( x >= win->screen_x + win->xsize ) return char_xsize_zoom;

    //Convert from UTF32 to WIN1251:
    utf32_to_win1251( c, c );

    uchar *font;
    int char_xsize;
    int char_ysize;
    if( win->font == 0 )
    {
        font = g_font0;
	char_xsize = 8;
	char_ysize = 8;
    }
    else
    {
        font = g_font1;
	char_xsize = font[ 1 + c ];
	char_ysize = font[ 0 ];
    }

#if defined(OPENGL) && !defined(FRAMEBUFFER)
    int char_ysize_zoom = wbd_char_y_size( wm );

    sundog_image *font_img;
    if( win->font == 0 )
        font_img = g_font0_img;
    else
        font_img = g_font1_img;

    unsigned int transp = wm->cur_transparency;
    if( transp > 255 ) transp = 255;
    font_img->transp = transp;

    if( wm->cur_font_draw_bgcolor )
    {
	font_img->color = wm->cur_font_bgcolor;
	device_draw_bitmap(
	    x, y,
	    char_xsize_zoom, char_ysize_zoom,
	    0, 255 * char_ysize,
	    font_img,
	    wm );
    }

    font_img->color = wm->cur_font_color;
    device_draw_bitmap(
	x, y,
	char_xsize_zoom, char_ysize_zoom,
	0, c * char_ysize,
	font_img,
	wm );
#else

    int transp = wm->cur_transparency;
    int screen_xsize = wm->screen_xsize;
    int screen_ysize = wm->screen_ysize;
    COLORPTR g_screen = wm->g_screen;
    COLOR cur_font_color = wm->cur_font_color;
    COLOR cur_font_bgcolor = wm->cur_font_bgcolor;
    int cur_font_draw_bgcolor = wm->cur_font_draw_bgcolor;

    int p = c * char_ysize;
    int sp = y * screen_xsize + x;
    
    font += 257;

    if( transp >= 255 )
    {
	if( wm->cur_font_zoom < 512 )
	{
	    //y clipping
	    if( y + char_ysize > screen_ysize ) 
	    {
		char_ysize -= ( y + char_ysize ) - screen_ysize;
		if( char_ysize <= 0 ) return char_xsize_zoom;
	    }
	    int add = screen_xsize - char_xsize;
	    for( int cy = 0; cy < char_ysize; cy++ )
	    {
		int fpart = font[ p ];
		for( int cx = 0; cx < char_xsize; cx++ )
		{
		    if( fpart & 128 ) 
			g_screen[ sp ] = cur_font_color;
		    else 
			if( cur_font_draw_bgcolor ) g_screen[ sp ] = cur_font_bgcolor;
		    fpart <<= 1;
		    sp++;
		}
		sp += add;
		p++;
	    }
	}
	else
	{
	    //y clipping
	    if( y + char_ysize * 2 > screen_ysize ) 
	    {
		char_ysize -= ( ( y + char_ysize * 2 ) - screen_ysize ) / 2;
		if( char_ysize <= 0 ) return char_xsize_zoom;
	    }
	    int add = screen_xsize * 2 - char_xsize * 2;
	    for( int cy = 0; cy < char_ysize; cy++ )
	    {
		int fpart = font[ p ];
		for( int cx = 0; cx < char_xsize; cx++ )
		{
		    if( fpart & 128 ) 
		    {
			g_screen[ sp ] = cur_font_color;
			g_screen[ sp+1 ] = cur_font_color;
			g_screen[ sp+screen_xsize ] = cur_font_color;
			g_screen[ sp+screen_xsize+1 ] = cur_font_color;
		    }
		    else if( cur_font_draw_bgcolor ) 
		    {
			g_screen[ sp ] = cur_font_bgcolor;
			g_screen[ sp+1 ] = cur_font_bgcolor;
			g_screen[ sp+screen_xsize ] = cur_font_bgcolor;
			g_screen[ sp+screen_xsize+1 ] = cur_font_bgcolor;
		    }
		    fpart <<= 1;
		    sp += 2;
		}
		sp += add;
		p++;
	    }
	}
    }
    else
    {
	if( wm->cur_font_zoom < 512 )
	{
	    //y clipping
	    if( y + char_ysize > screen_ysize ) 
	    {
		char_ysize -= ( y + char_ysize ) - screen_ysize;
		if( char_ysize <= 0 ) return char_xsize_zoom;
	    }
	    int add = screen_xsize - char_xsize;
	    for( int cy = 0; cy < char_ysize; cy++ )
	    {
		int fpart = font[ p ];
		for( int cx = 0; cx < char_xsize; cx++ )
		{
		    if( fpart & 128 ) 
			g_screen[ sp ] = blend( g_screen[ sp ], cur_font_color, transp );
		    else 
			if( cur_font_draw_bgcolor ) g_screen[ sp ] = blend( g_screen[ sp ], cur_font_bgcolor, transp );
		    fpart <<= 1;
		    sp++;
		}
		sp += add;
		p++;
	    }
	}
	else
	{
	    //y clipping
	    if( y + char_ysize * 2 > screen_ysize ) 
	    {
		char_ysize -= ( ( y + char_ysize * 2 ) - screen_ysize ) / 2;
		if( char_ysize <= 0 ) return char_xsize_zoom;
	    }
	    int add = screen_xsize * 2 - char_xsize * 2;
	    for( int cy = 0; cy < char_ysize; cy++ )
	    {
		int fpart = font[ p ];
		for( int cx = 0; cx < char_xsize; cx++ )
		{
		    if( fpart & 128 ) 
		    {
			g_screen[ sp ] = blend( g_screen[ sp ], cur_font_color, transp );
			g_screen[ sp+1 ] = blend( g_screen[ sp+1 ], cur_font_color, transp );
			g_screen[ sp+screen_xsize ] = blend( g_screen[ sp+screen_xsize ], cur_font_color, transp );
			g_screen[ sp+screen_xsize+1 ] = blend( g_screen[ sp+screen_xsize+1 ], cur_font_color, transp );
		    }
		    else if( cur_font_draw_bgcolor ) 
		    {
			g_screen[ sp ] = blend( g_screen[ sp ], cur_font_bgcolor, transp );
			g_screen[ sp+1 ] = blend( g_screen[ sp+1 ], cur_font_bgcolor, transp );
			g_screen[ sp+screen_xsize ] = blend( g_screen[ sp+screen_xsize ], cur_font_bgcolor, transp );
			g_screen[ sp+screen_xsize+1 ] = blend( g_screen[ sp+screen_xsize+1 ], cur_font_bgcolor, transp );
		    }
		    fpart <<= 1;
		    sp += 2;
		}
		sp += add;
		p++;
	    }
	}
    }
#endif
    return char_xsize_zoom;
}

void draw_string( const UTF8_CHAR *str, int x, int y, window_manager *wm )
{
    //Bounds control:
    if( y < 0 ) return;
    if( x >= wm->cur_window->xsize ) return;

    int start_x = x;

    while( *str )
    {
	if( *str == 0xA ) 
	{ 
	    y += wbd_char_y_size( wm ); 
	    x = start_x; 
	    str++;
	}
	else
	{
	    UTF32_CHAR c32;
	    str += utf8_to_utf32_char( str, &c32 );
	    draw_char( c32, x, y, wm );
	    x += wbd_char_x_size( c32, wm );
	}
    }
}

void draw_pixel( int x, int y, COLOR color, window_manager *wm )
{
#if defined(OPENGL) && !defined(FRAMEBUFFER)
    draw_box( x, y, 1, 1, color, wm );
#else

    x += wm->cur_window->screen_x;
    y += wm->cur_window->screen_y;

    if( x >= 0 && x < wm->screen_xsize &&
	y >= 0 && y < wm->screen_ysize )
    {
	int ptr = y * wm->screen_xsize + x;
	if( wm->cur_transparency >= 255 )
	    wm->g_screen[ ptr ] = color;
	else
	    wm->g_screen[ ptr ] = blend( wm->g_screen[ ptr ], color, wm->cur_transparency );
    }
#endif
}

#define IMG_PREC 11

void draw_image_scaled( int x, int y, int xsize, int ysize, int xsize_crop, sundog_image *img, window_manager *wm )
{
    //Calc deltas:
    if( xsize_crop <= 0 ) return;
    if( xsize <= 0 ) return;
    if( ysize <= 0 ) return;
    int dx = ( img->xsize << IMG_PREC ) / xsize;
    int dy = ( img->ysize << IMG_PREC ) / ysize;
    
    int cx = 0;
    int cy = 0;

    WINDOWPTR win = wm->cur_window;
    COLORPTR g_screen = wm->g_screen;
    int screen_xsize = wm->screen_xsize;
    int screen_ysize = wm->screen_ysize;
    x += win->screen_x;
    y += win->screen_y;

    //Bounds control:
    if( x < 0 ) 
    { 
	xsize += x;
	if( xsize <= 0 ) return;
	cx = -x * dx;
	x = 0;
    }    
    if( y < 0 ) 
    { 
	ysize += y;
	if( ysize <= 0 ) return;
	cy = -y * dy;
	y = 0;
    }
    if( x + xsize > screen_xsize )
    {
	xsize -= ( x + xsize ) - screen_xsize;
	if( xsize <= 0 ) return;
    }
    if( y + ysize > screen_ysize )
    {
	ysize -= ( y + ysize ) - screen_ysize;
	if( ysize <= 0 ) return;
    }

    if( xsize > xsize_crop ) xsize = xsize_crop;
    
    int p = y * screen_xsize + x;
    int add = screen_xsize - xsize;
    if( img->flags & IMAGE_ALPHA8 )
    {
	uchar *img_data = (uchar*)img->data;
	COLOR color = img->color;
	for( int yy = 0; yy < ysize; yy++ )
	{
	    int cx2 = cx;
	    int img_ptr = ( cy >> IMG_PREC ) * img->xsize;
	    for( int xx = 0; xx < xsize; xx++ )
	    {
		int img_ptr2 = img_ptr + ( cx2 >> IMG_PREC );
		uchar v = img_data[ img_ptr2 ];
		if( v == 255 )
		    g_screen[ p ] = color;
		else if( v > 0 )
		    g_screen[ p ] = blend( g_screen[ p ], color, v );
		p++;
		cx2 += dx;
	    }
	    p += add;
	    cy += dy;
	}
    }
    else
    if( img->flags & IMAGE_NATIVE_RGB )
    {
	COLORPTR img_data = (COLORPTR)img->data;
	for( int yy = 0; yy < ysize; yy++ )
	{
	    int cx2 = cx;
	    int img_ptr = ( cy >> IMG_PREC ) * img->xsize;
	    for( int xx = 0; xx < xsize; xx++ )
	    {
		int img_ptr2 = img_ptr + ( cx2 >> IMG_PREC );
		g_screen[ p ] = img_data[ img_ptr2 ];
		p++;
		cx2 += dx;
	    }
	    p += add;
	    cy += dy;
	}
    }
}

int wbd_char_x_size( UTF32_CHAR c, window_manager *wm )
{
    if( c < 0x10 ) return 0;
    if( wm->cur_window->font == 0 )
    {
	return ( 8 * wm->cur_font_zoom ) / 256;
    }
    else
    {
	utf32_to_win1251( c, c );
	return ( (int)g_font1[ 1 + c ] * wm->cur_font_zoom ) / 256;
    }
}

int wbd_char_y_size( window_manager *wm )
{
    if( wm->cur_window->font == 0 )
    {
	return ( 8 * wm->cur_font_zoom ) / 256;
    }
    else
    {
	return ( (int)g_font1[ 0 ] * wm->cur_font_zoom ) / 256;
    }
}

int wbd_string_size( const UTF8_CHAR *str, window_manager *wm )
{
    int size = 0;
    while( *str )
    {
	UTF32_CHAR c32;
	str += utf8_to_utf32_char( str, &c32 );
	size += wbd_char_x_size( c32, wm );
    }    
    return size;
}
