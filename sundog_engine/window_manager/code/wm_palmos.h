/*
    wm_palmos.h. Platform-dependent module : PalmOS
    This file is part of the SunDog engine.
    Copyright (C) 2002 - 2009 Alex Zolotov <nightradio@gmail.com>
*/

#ifndef __WINMANAGER_PALMOS__
#define __WINMANAGER_PALMOS__

#include "time/timemanager.h"
#include <PalmOS.h>
#include <PenInputMgr.h>
#include "palm_functions.h"

//#################################
//## DEVICE DEPENDENT FUNCTIONS: ##
//#################################

void wait( void )
{
    ticks_t cur_ticks = time_ticks();
    int counter = 0;
    while( time_ticks() < cur_ticks + ( time_ticks_per_second() / 2 ) )
    {
	device_event_handler( 0 );
	counter++;
	if( ( counter & 0xFFFF ) == 0 ) dprint( "MAIN: waiting...\n" );
    }
}

#ifdef FRAMEBUFFER
    BitmapType    *g_screen_low;  //Low density bitmap
    BitmapTypeV3  *g_screen;      //High density bitmap
    COLORPTR framebuffer = 0;
#endif

Boolean ApplicationHandleEvent( EventPtr event )
{
    Boolean handled = false;
    FormPtr pForm;

    switch( BSwap16( event->eType ) )
    {
	case frmLoadEvent:
	    unsigned short *ptr = (unsigned short*)event;
	    pForm = FrmInitForm( BSwap16( ptr[ 4 ] ) );
	    FrmSetActiveForm( pForm );
	    FrmSetEventHandler( pForm, (Boolean (*)(EventType*))g_form_handler );
	    handled = true;
	    break;
    }
    return handled;
}

void check_screen_buffer( window_manager *wm )
{
#ifdef DIRECTDRAW
    long xscreen = 0;
    long yscreen = 0;
    WinScreenGetAttribute( winScreenWidth, (UInt32*)&xscreen );
    WinScreenGetAttribute( winScreenHeight, (UInt32*)&yscreen );
    framebuffer = (COLORPTR)BmpGetBits( WinGetBitmap( WinGetDisplayWindow() ) );
    wm->fb_xpitch = 1;
    wm->fb_ypitch = xscreen;
#endif
}

void check_screen_size( window_manager *wm )
{
    if( g_new_screen_size[ 2 ] != 0 &&
	g_new_screen_size[ 3 ] != 0 )
    {
	wm->screen_xsize = g_new_screen_size[ 2 ];
	wm->screen_ysize = g_new_screen_size[ 3 ];
#ifndef PALMLOWRES
	wm->screen_xsize *= 2;
	wm->screen_ysize *= 2;
#endif
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
    	    send_event( wm->root_win, EVT_SCREENRESIZE, EVT_FLAG_AC, 0, 0, 0, 0, 1024, 0, wm );
	}
	g_new_screen_size[ 2 ] = 0;
	g_new_screen_size[ 3 ] = 0;
	//May be real screen size was changed too?
	check_screen_buffer( wm );
    }
}

int device_start( const UTF8_CHAR *windowname, int xsize, int ysize, int flags, window_manager *wm )
{
    int retval = 0;

    //Open window:
    dprint( "MAIN: device start\n" );

    dprint( "MAIN: open form\n" );
    FrmGotoForm( 8888 );
    wait();

#ifdef PALMLOWRES
    xsize = 160;
    ysize = 160;
#else
    xsize = 320;
    ysize = 320;
#endif
#ifdef DIRECTDRAW
    long xscreen = 0;
    long yscreen = 0;
    WinScreenGetAttribute( winScreenWidth, (UInt32*)&xscreen );
    WinScreenGetAttribute( winScreenHeight, (UInt32*)&yscreen );
    xsize = xscreen;
    ysize = yscreen;
#endif
    wm->screen_xsize = xsize;
    wm->screen_ysize = ysize;
    dprint( "MAIN: device start finished\n" );

    //Set screen-mode and palette:
    RGBColorType pal[ 256 ];
    ulong x, y, depth;
    long a;
    x = xsize; y = ysize; depth = COLORBITS; 
    WinScreenMode( winScreenModeSet, &x, &y, &depth, 0 );  
#ifdef GRAYSCALE
    for( a = 0; a < 256; a++ )
    {
	pal[ a ].r = a;
	pal[ a ].g = a;
	pal[ a ].b = a;
	pal[ a ].index = a;
    }
#else
    for( a = 0; a < 256; a++ )
    {
	pal[ a ].r = ( (a<<5)&224 );
	pal[ a ].g = ( (a<<2)&224 );
	pal[ a ].b = (a&192);
	pal[ a ].index = a;
	if( pal[ a ].r ) pal[ a ].r |= 0x1F;
	if( pal[ a ].g ) pal[ a ].g |= 0x1F;
	if( pal[ a ].b ) pal[ a ].b |= 0x3F;
    }
#endif
    WinPalette( winPaletteSet, 0, 256, pal );

#ifdef FRAMEBUFFER
    //Create framebuffer:
#ifndef DIRECTDRAW
    uint16 bmp_err;
    g_screen_low = BmpCreate( wm->screen_xsize, wm->screen_xsize, COLORBITS, 0, &bmp_err );
    framebuffer = (COLORPTR)BmpGetBits( g_screen_low );
    #ifndef PALMLOWRES
	g_screen = BmpCreateBitmapV3( g_screen_low, kDensityDouble, framebuffer, 0 );
    #endif
#else
    framebuffer = (COLORPTR)BmpGetBits( WinGetBitmap( WinGetDisplayWindow() ) );
#endif
    wm->fb_xpitch = 1;
    wm->fb_ypitch = wm->screen_xsize;
#endif

    check_screen_size( wm );

    return retval;
}

int prev_move_x = -999;
int prev_move_y = -999;

void device_end( window_manager *wm )
{
    //Close window:
    dprint( "MAIN: device end\n" );

#ifdef FRAMEBUFFER
    //Close screenbuffer:
    BmpDelete( g_screen_low );
    #ifndef PALMLOWRES
	BmpDelete( (BitmapType*)g_screen );
    #endif
#endif

    dprint( "MAIN: close all forms\n" );
    FrmCloseAllForms();
    wait();
}

long old_chrr;

long device_event_handler( window_manager *wm )
{
    EventType event;
    UInt16 error;
    long chrr,x,y;
    char appkey_down = 0;
    char keyup = 0;
    char device_wakeup = 0;
    int key;

    EvtGetEvent( &event, 0 );

    if( wm )
    {
	keyup = 1;
	//Native ARM mode:
	UInt16 *ptr = (UInt16*)&event;
	chrr = BSwap16( ptr[ 4 ] );
	x = BSwap16( ptr[ 2 ] );
	y = BSwap16( ptr[ 3 ] );
	switch( BSwap16( event.eType ) ) 
	{
#ifdef PALMLOWRES
	    case penDownEvent: send_event( 0, EVT_MOUSEBUTTONDOWN, 0, x, y, MOUSE_BUTTON_LEFT, 0, 1024, 0, wm ); break;
	    case penMoveEvent: 
		if( x != prev_move_x || y != prev_move_y )
		    send_event( 0, EVT_MOUSEMOVE, 0, x, y, MOUSE_BUTTON_LEFT, 0, 1024, 0, wm );
		prev_move_x = x;
		prev_move_y = y;
		break;
	    case penUpEvent:   send_event( 0, EVT_MOUSEBUTTONUP, 0, x, y, MOUSE_BUTTON_LEFT, 0, 1024, 0, wm ); break;
#else
	    case penDownEvent: send_event( 0, EVT_MOUSEBUTTONDOWN, 0, x << 1, y << 1, MOUSE_BUTTON_LEFT, 0, 1024, 0, wm ); break;
	    case penMoveEvent: 
		if( x != prev_move_x || y != prev_move_y )
		    send_event( 0, EVT_MOUSEMOVE, 0, x << 1, y << 1, MOUSE_BUTTON_LEFT, 0, 1024, 0, wm );
		prev_move_x = x;
		prev_move_y = y;
		break;
	    case penUpEvent:   send_event( 0, EVT_MOUSEBUTTONUP, 0, x << 1, y << 1, MOUSE_BUTTON_LEFT, 0, 1024, 0, wm ); break;
#endif
	    case keyDownEvent:
		keyup = 0;
	    //keyUpEvent:
	    case 0x4000:
		if( chrr == vchrLateWakeup ) device_wakeup = 1;
		if( chrr >= 11 && chrr <= 12 ) appkey_down = 1;
		if( chrr >= 516 && chrr <= 519 ) appkey_down = 1;
		key = 0;
		switch( chrr )
		{
		    case 0x1E: key = 0; break; //KEY_UP; break;
		    case 0x1F: key = 0; break; //KEY_DOWN; break;
		    case 0x1C: key = 0; break; //KEY_LEFT; break;
		    case 0x1D: key = 0; break; //KEY_RIGHT; break;
		    case 0x08: key = KEY_BACKSPACE; break;
		    case 0x0A: key = KEY_ENTER; break;
		    case 0x0B: key = 0; break; //KEY_UP; break;
		    case 0x0C: key = 0; break; //KEY_DOWN; break;
		    default: key = chrr; if( key > 255 ) key = 0; break;
		}
		if( appkey_down == 0 &&
		    key != 0 )
		{
		    if( !keyup )
			send_event( 0, EVT_BUTTONDOWN, 0, 0, 0, key, chrr, 1024, 0, wm );
		    else
			send_event( 0, EVT_BUTTONUP, 0, 0, 0, key, chrr, 1024, 0, wm );
		}
		break;
	}
    
	chrr = KeyCurrentState();
	if( chrr & 0x2 && !( old_chrr & 0x2 ) ) { send_event( 0, EVT_BUTTONDOWN, 0, 0, 0, KEY_UP, 0, 1024, 0, wm ); }
	if( !( chrr & 0x2 ) && old_chrr & 0x2 ) { send_event( 0, EVT_BUTTONUP, 0, 0, 0, KEY_UP, 0, 1024, 0, wm ); }
	if( chrr & 0x4 && !( old_chrr & 0x4 ) ) { send_event( 0, EVT_BUTTONDOWN, 0, 0, 0, KEY_DOWN, 0, 1024, 0, wm ); }
	if( !( chrr & 0x4 ) && old_chrr & 0x4 ) { send_event( 0, EVT_BUTTONUP, 0, 0, 0, KEY_DOWN, 0, 1024, 0, wm ); }
	if( chrr & 0x1000000 && !( old_chrr & 0x1000000 ) ) { send_event( 0, EVT_BUTTONDOWN, 0, 0, 0, KEY_LEFT, 0, 1024, 0, wm ); }
	if( !( chrr & 0x1000000 ) && old_chrr & 0x1000000 ) { send_event( 0, EVT_BUTTONUP, 0, 0, 0, KEY_LEFT, 0, 1024, 0, wm ); }
	if( chrr & 0x2000000 && !( old_chrr & 0x2000000 ) ) { send_event( 0, EVT_BUTTONDOWN, 0, 0, 0, KEY_RIGHT, 0, 1024, 0, wm ); }
	if( !( chrr & 0x2000000 ) && old_chrr & 0x2000000 ) { send_event( 0, EVT_BUTTONUP, 0, 0, 0, KEY_RIGHT, 0, 1024, 0, wm ); }
	if( chrr & 0x4000000 && !( old_chrr & 0x4000000 ) ) { send_event( 0, EVT_BUTTONDOWN, 0, 0, 0, KEY_SPACE, 0, 1024, 0, wm ); }
	if( !( chrr & 0x4000000 ) && old_chrr & 0x4000000 ) { send_event( 0, EVT_BUTTONUP, 0, 0, 0, KEY_SPACE, 0, 1024, 0, wm ); }
	old_chrr = chrr;

    } //if( wm )

    if( appkey_down == 0 )
    {
	if( !SysHandleEvent( &event ) && 
	    !MenuHandleEvent( 0, &event, &error ) &&
	    !ApplicationHandleEvent( &event ) )
	{
	    FrmDispatchEvent( &event );
	}
    }

    if( BSwap16( event.eType ) == appStopEvent ) 
	if( wm ) send_event( 0, EVT_QUIT, 0, 0, 0, KEY_ESCAPE, 0, 1024, 0, wm );

    if( wm )
        check_screen_size( wm );
	
    if( device_wakeup )
    {
	check_screen_buffer( wm );
	send_event( wm->root_win, EVT_DRAW, EVT_FLAG_AC, 0, 0, 0, 0, 1024, 0, wm );
    }

    return 0;
}

void device_screen_lock( WINDOWPTR win, window_manager *wm )
{
    if( wm->screen_lock_counter == 0 )
    {
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
    }

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

#else

void device_redraw_framebuffer( window_manager *wm ) {}

void device_draw_line( int x1, int y1, int x2, int y2, COLOR color, window_manager *wm )
{
#ifndef PALMLOWRES
    WinSetCoordinateSystem( kCoordinatesNative );
#endif
    RGBColorType c;
    c.r = red( color );
    c.g = green( color );
    c.b = blue( color );
    WinSetForeColorRGB( &c, 0 );
    WinDrawLine( x1, y1, x2, y2 );
#ifndef PALMLOWRES
    WinSetCoordinateSystem( kCoordinatesStandard );
#endif
}

RectangleType box_rec;

void device_draw_box( int x, int y, int xsize, int ysize, COLOR color, window_manager *wm )
{
#ifndef PALMLOWRES
    WinSetCoordinateSystem( kCoordinatesNative );
#endif
    box_rec.topLeft.x = x;
    box_rec.topLeft.y = y;
    box_rec.extent.x = xsize;
    box_rec.extent.y = ysize;
    RGBColorType c;
    c.r = red( color );
    c.g = green( color );
    c.b = blue( color );
    WinSetForeColorRGB( &c, 0 );
    WinDrawRectangle( &box_rec, 0 );
#ifndef PALMLOWRES
    WinSetCoordinateSystem( kCoordinatesStandard );
#endif
}

uchar bmp[ 32 ] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 6, 5, 0, 0, 0, 0, 0 };
RectangleType bmp_rec;

void device_draw_bitmap( 
    int dest_x, int dest_y, 
    int dest_xs, int dest_ys,
    int src_x, int src_y,
    sundog_image *img,
    window_manager *wm )
{
    int src_xs = img->xsize;
    int src_ys = img->ysize;
    COLORPTR data = (COLORPTR)img->data;

    //Create bitmap:
    bmp[ 0 ] = ( src_xs >> 8 ) & 255;
    bmp[ 1 ] = src_xs & 255;
    bmp[ 2 ] = ( src_ys >> 8 ) & 255;
    bmp[ 3 ] = src_ys & 255;
    int linesize = src_xs * COLORLEN;
    bmp[ 4 ] = ( linesize >> 8 ) & 255;
    bmp[ 5 ] = linesize & 255;
    //Type:
    bmp[ 6 ] = 0x04;
    bmp[ 7 ] = 0x00;
    bmp[ 8 ] = COLORBITS;
    bmp[ 9 ] = 2; //Version;
    BitmapTypeV3 *bitmap;
#ifdef PALMLOWRES
    bitmap = BmpCreateBitmapV3( (BitmapType*)&bmp, kDensityLow, data, 0 );
#else
    bitmap = BmpCreateBitmapV3( (BitmapType*)&bmp, kDensityDouble, data, 0 );
#endif
    //Draw it:
    bmp_rec.topLeft.x = dest_x;
    bmp_rec.topLeft.y = dest_y;
    bmp_rec.extent.x = dest_xs;
    bmp_rec.extent.y = dest_ys;
#ifndef PALMLOWRES
    WinSetCoordinateSystem( kCoordinatesNative );
#endif
    WinSetClip( &bmp_rec );
    WinDrawBitmap( (BitmapType*)bitmap, dest_x - src_x, dest_y - src_y );
    bmp_rec.topLeft.x = 0;
    bmp_rec.topLeft.y = 0;
    bmp_rec.extent.x = wm->screen_xsize;
    bmp_rec.extent.y = wm->screen_ysize;
    WinSetClip( &bmp_rec );
#ifndef PALMLOWRES
    WinSetCoordinateSystem( kCoordinatesStandard );
#endif
    //Delete bitmap:
    BmpDelete( (BitmapType*)bitmap );
}

#endif

//#################################
//#################################
//#################################

#endif
