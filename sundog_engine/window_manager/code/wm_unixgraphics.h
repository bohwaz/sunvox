/*
    wm_unixgraphics.h. Platform-dependent module : Unix OpenGL + XWindows
    This file is part of the SunDog engine.
    Copyright (C) 2002 - 2009 Alex Zolotov <nightradio@gmail.com>
*/

#ifndef __WINMANAGER_UNIX_GL__
#define __WINMANAGER_UNIX_GL__

//#################################
//## DEVICE DEPENDENT FUNCTIONS: ##
//#################################

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>   	//for getenv()
#include <sys/time.h> 	//timeval struct
#include <sched.h>	//for sched_yield()

#ifdef FRAMEBUFFER
    COLORPTR framebuffer;
#endif

window_manager *current_wm;

XSetWindowAttributes swa;
Colormap        cmap;
XEvent          event;
int             dummy;
int		xscreen;
int		depth = 0;
int 		auto_repeat = 0;

int g_mod = 0;

#ifdef OPENGL
    GLXContext cx;
    static int snglBuf[] = { GLX_RGBA, GLX_DEPTH_SIZE, 16, GLX_STENCIL_SIZE, 8, None };
    static int dblBuf[] = { GLX_RGBA, GLX_DEPTH_SIZE, 16, GLX_DOUBLEBUFFER, GLX_STENCIL_SIZE, 8, None };
    XVisualInfo *vi;
    #include "wm_opengl.h"
#else
    Visual *vi;
#endif

void small_pause( long milliseconds )
{
    XFlush( current_wm->dpy );
    timeval t;
    t.tv_sec = 0;
    t.tv_usec = (long) milliseconds * 1000;
    select( 0 + 1, 0, 0, 0, &t );
}

ulong local_colors[ 32 * 32 * 32 ];
uchar *temp_bitmap = 0;
XImage *last_img = 0;

int g_flags = 0;

int device_start( const UTF8_CHAR *windowname, int xsize, int ysize, int flags, window_manager *wm )
{
    int retval = 0;

    for( int lc = 0; lc < 32 * 32 * 32; lc++ ) local_colors[ lc ] = 0xFFFFFFFF;

    g_flags = flags;

    current_wm = wm;

#if defined(OPENGL) && defined(FRAMEBUFFER)
    xsize = 512;
    ysize = 512;
#endif
#if defined(OPENGL) && !defined(FRAMEBUFFER)
    if( profile_get_int_value( KEY_SCREENX, 0 ) != -1 ) 
	xsize = profile_get_int_value( KEY_SCREENX, 0 );
    if( profile_get_int_value( KEY_SCREENY, 0 ) != -1 ) 
	ysize = profile_get_int_value( KEY_SCREENY, 0 );
#endif
#ifdef X11
    if( profile_get_int_value( KEY_SCREENX, 0 ) != -1 ) 
	xsize = profile_get_int_value( KEY_SCREENX, 0 );
    if( profile_get_int_value( KEY_SCREENY, 0 ) != -1 ) 
	ysize = profile_get_int_value( KEY_SCREENY, 0 );
#endif

    wm->screen_xsize = xsize;
    wm->screen_ysize = ysize;
#ifdef OPENGL
    wm->real_window_width = xsize;
    wm->real_window_height = ysize;
#endif

    /*** open a connection to the X server ***/
    const char *name;
    if( ( name = getenv( "DISPLAY" ) ) == NULL )
	name = ":0";
    wm->dpy = XOpenDisplay( name );
    if( wm->dpy == NULL ) 
    {
    	dprint( "could not open display\n" );
    }
#ifndef OPENGL
    //Simple X11 init (not GLX) :
    xscreen = XDefaultScreen( wm->dpy );
    vi = XDefaultVisual( wm->dpy, xscreen ); wm->win_visual = vi; if( !vi ) dprint( "XDefaultVisual error\n" );
    depth = XDefaultDepth( wm->dpy, xscreen ); wm->win_depth = depth; if( !depth ) dprint( "XDefaultDepth error\n" );
    temp_bitmap = (uchar*)malloc( 2000000 * ( depth / 8 ) );
    dprint( "depth = %d\n", depth );
    wm->cmap = cmap = XDefaultColormap( wm->dpy, xscreen );
    swa.colormap = cmap;
    swa.border_pixel = 0;
    swa.event_mask = 
	( 0
	| ExposureMask 
	| ButtonPressMask 
	| ButtonReleaseMask 
	| PointerMotionMask 
	| StructureNotifyMask 
	| KeyPressMask 
	| KeyReleaseMask 
        | FocusChangeMask 
	| StructureNotifyMask
	);
    wm->win = XCreateWindow( wm->dpy, XDefaultRootWindow( wm->dpy ), 0, 0, xsize, ysize, 0, CopyFromParent, InputOutput, vi, CWBorderPixel | CWColormap | CWEventMask, &swa );
    XStoreName( wm->dpy, wm->win, windowname );

    wm->win_gc = XDefaultGC( wm->dpy, xscreen ); if( !wm->win_gc ) dprint( "XDefaultGC error\n" );
#endif //!OPENGL

#ifdef OPENGL
    /********************************/
    /*** Initialize GLX routines  ***/
    /********************************/

    /*** make sure OpenGL's GLX extension supported ***/
    if( !glXQueryExtension( wm->dpy, &dummy, &dummy ) ) 
    {
    	dprint( "X server has no OpenGL GLX extension\n" );
    }

    /*** find an appropriate visual ***/
    /* find an OpenGL-capable RGB visual with depth buffer */
    vi = 0;
    if( wm->gl_double_buffer )
    {
	vi = glXChooseVisual( wm->dpy, DefaultScreen( wm->dpy ), dblBuf );
    }
    if( vi == 0 ) 
    {
    	vi = glXChooseVisual( wm->dpy, DefaultScreen( wm->dpy ), snglBuf );
	if( vi == NULL ) 
	{
	    dprint( "no RGB visual with depth buffer\n" );
	}
	wm->gl_double_buffer = 0;
    }

    /*** create an OpenGL rendering context  ***/
    /* create an OpenGL rendering context */
    cx = glXCreateContext( wm->dpy, vi, /* no sharing of display lists */ None, /* direct rendering if possible */ GL_TRUE );
    if( cx == NULL ) 
    {
	dprint( "could not create rendering context\n" );
    }

    /*** create an X window with the selected visual ***/
    /* create an X colormap since probably not using default visual */
    cmap = XCreateColormap( wm->dpy, RootWindow( wm->dpy, vi->screen ), vi->visual, AllocNone );
    swa.colormap = cmap;
    swa.border_pixel = 0;
    swa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | StructureNotifyMask | KeyPressMask | KeyReleaseMask | FocusChangeMask | StructureNotifyMask;
    wm->win = XCreateWindow( wm->dpy, RootWindow(wm->dpy, vi->screen), 0, 0, xsize, ysize, 0, vi->depth, InputOutput, vi->visual, CWBorderPixel | CWColormap | CWEventMask, &swa );
    XSetStandardProperties( wm->dpy, wm->win, windowname, windowname, None, wm->argv, wm->argc, NULL );
#endif //OPENGL

    /*** request the X window to be displayed on the screen ***/
    XMapWindow( wm->dpy, wm->win );
    
#ifdef OPENGL
    /*** bind the rendering context to the window ***/
    glXMakeCurrent( wm->dpy, wm->win, cx );
    gl_init( wm );
    gl_resize( wm );
#endif

    XkbSetDetectableAutoRepeat( wm->dpy, 1, &auto_repeat );
    
    //XGrabKeyboard( wm->dpy, wm->win, 1, GrabModeAsync, GrabModeAsync, CurrentTime );
    //XGrabKey( wm->dpy, 0, ControlMask, wm->win, 1, 1, GrabModeSync );
    
    //Create framebuffer:
#ifdef FRAMEBUFFER
    framebuffer = (COLORPTR)MEM_NEW( HEAP_DYNAMIC, wm->screen_xsize * wm->screen_ysize * COLORLEN );
    wm->fb_xpitch = 1;
    wm->fb_ypitch = wm->screen_xsize;
#endif

    return retval;
}

void device_end( window_manager *wm )
{
    int temp;

    dprint( "device_end(): stage 1\n" );
    XkbSetDetectableAutoRepeat( wm->dpy, auto_repeat, &temp );
#ifndef OPENGL
    dprint( "device_end(): stage 2\n" );
    if( last_img ) XDestroyImage( last_img );
    last_img = 0;
    temp_bitmap = 0;
#endif
    dprint( "device_end(): stage 3\n" );
    XDestroyWindow( wm->dpy, wm->win );
    dprint( "device_end(): stage 4\n" );
    XCloseDisplay( wm->dpy );

#ifdef FRAMEBUFFER
    mem_free( framebuffer );
#endif
}

#ifdef OPENGL
#define GET_WINDOW_COORDINATES \
    /*Real coordinates -> window_manager coordinates*/\
    x = winX;\
    y = winY;\
    x = ( x << 11 ) / wm->screen_xsize; x *= current_wm->screen_xsize; x >>= 11;\
    y = ( y << 11 ) / wm->screen_ysize; y *= current_wm->screen_ysize; y >>= 11;
#else
#define GET_WINDOW_COORDINATES \
    /*Real coordinates -> window_manager coordinates*/\
    x = winX;\
    y = winY;
#endif

int g_mod_state = 0;

long device_event_handler( window_manager *wm )
{
    //if( wm->events_count == 0 ) XPeekEvent( wm->dpy, &event ); //Waiting for the next event
    int old_screen_x, old_screen_y;
    int old_char_x, old_char_y;
    int winX, winY;
    int x, y, window_num;
    int key = 0;
    KeySym sym;
    XSizeHints size_hints;
    int pend = XPending( wm->dpy );
    if( pend )
    {
	XNextEvent( wm->dpy, &event );
        switch( event.type ) 
	{
	    case FocusIn:
		//All keyboard event must sending to the PsyTexx window only:
		//XGrabKeyboard( wm->dpy, wm->win, 1, GrabModeAsync, GrabModeAsync, CurrentTime ); 
		break;

	    case FocusOut:
		//XUngrabKeyboard( wm->dpy, CurrentTime );
		break;
		
	    case MapNotify:
	    case Expose:
		#ifndef OPENGL
		//draw_screen( wm );
		#endif
		//dprint( "%d %d\n", event.xexpose.x, event.xexpose.height );
		send_event( current_wm->root_win, EVT_DRAW, EVT_FLAG_AC, 0, 0, 0, 0, 1024, 0, current_wm );
		break;
	
	    case DestroyNotify:
		send_event( 0, EVT_QUIT, 0, 0, 0, 0, 0, 1024, 0, wm );
		//wm->exit_request = 1;
		break;
		
	    case ConfigureNotify:
		#ifndef OPENGL
		if( !( g_flags & WIN_INIT_FLAG_SCALABLE ) )
		{
		    size_hints.flags = PMinSize;
		    size_hints.min_width = wm->screen_xsize;
		    size_hints.min_height = wm->screen_ysize;
		    size_hints.max_width = wm->screen_xsize;
		    size_hints.max_height = wm->screen_ysize;
		    XSetNormalHints( wm->dpy, wm->win, &size_hints );
		}
		#endif
		if( g_flags & WIN_INIT_FLAG_SCALABLE )
		{
		    if( wm->screen_xsize != event.xconfigure.width ||
			wm->screen_ysize != event.xconfigure.height )
		    {
			wm->screen_xsize = event.xconfigure.width;
    	    		wm->screen_ysize = event.xconfigure.height;
			send_event( current_wm->root_win, EVT_SCREENRESIZE, 0, 0, 0, 0, 0, 1024, 0, current_wm );
		    }
		}
		#ifdef OPENGL
		current_wm->real_window_width = wm->screen_xsize;
		current_wm->real_window_height = wm->screen_ysize;
		gl_resize( current_wm );
		#else
		#endif
		break;
		
	    case MotionNotify:
		//if( pend >= 2 ) break;
		if( event.xmotion.window != wm->win ) break;
		winX = event.xmotion.x;
		winY = event.xmotion.y;
		GET_WINDOW_COORDINATES;
		key = 0;
		if( event.xmotion.state & Button1Mask )
                    key |= MOUSE_BUTTON_LEFT;
                if( event.xmotion.state & Button2Mask )
                    key |= MOUSE_BUTTON_MIDDLE;
                if( event.xmotion.state & Button3Mask )
                    key |= MOUSE_BUTTON_RIGHT;
		send_event( 0, EVT_MOUSEMOVE, g_mod, x, y, key, 0, 1024, 0, current_wm );
		break;
		
	    case ButtonPress:
	    case ButtonRelease:
		if( event.xmotion.window != wm->win ) break;
		winX = event.xbutton.x;
		winY = event.xbutton.y;
		GET_WINDOW_COORDINATES;
		/* Get pressed button */
	    	if( event.xbutton.button == 1 )
		    key = MOUSE_BUTTON_LEFT;
		else if( event.xbutton.button == 2 )
		    key = MOUSE_BUTTON_MIDDLE;
		else if( event.xbutton.button == 3 )
		    key = MOUSE_BUTTON_RIGHT;
		else if( ( event.xbutton.button == 4 || event.xbutton.button == 5 ) && event.type == ButtonPress )
		{
		    if( event.xbutton.button == 4 ) 
			key = MOUSE_BUTTON_SCROLLUP;
		    else
			key = MOUSE_BUTTON_SCROLLDOWN;
		}
		if( event.type == ButtonPress )
		    send_event( 0, EVT_MOUSEBUTTONDOWN, g_mod, x, y, key, 0, 1024, 0, current_wm );
		else
		    send_event( 0, EVT_MOUSEBUTTONUP, g_mod, x, y, key, 0, 1024, 0, current_wm );
		break;
		
	    case KeyPress:
	    case KeyRelease:
		sym = XKeycodeToKeysym( wm->dpy, event.xkey.keycode, 0 );
		key = 0;
		if( sym == NoSymbol || sym == 0 ) break;
		if( sym >= 0x20 && sym <= 0x7E ) key = sym;
		switch( sym )
		{
		    case XK_F1: key = KEY_F1; break;
		    case XK_F2: key = KEY_F2; break;
		    case XK_F3: key = KEY_F3; break;
		    case XK_F4: key = KEY_F4; break;
		    case XK_F5: key = KEY_F5; break;
		    case XK_F6: key = KEY_F6; break;
		    case XK_F7: key = KEY_F7; break;
		    case XK_F8: key = KEY_F8; break;
		    case XK_F9: key = KEY_F9; break;
		    case XK_F10: key = KEY_F10; break;
		    case XK_F11: key = KEY_F11; break;
		    case XK_F12: key = KEY_F12; break;
		    case XK_BackSpace: key = KEY_BACKSPACE; break;
		    case XK_Tab: key = KEY_TAB; break;
		    case XK_Return: key = KEY_ENTER; break;
		    case XK_Escape: key = KEY_ESCAPE; break;
		    case XK_Left: key = KEY_LEFT; break;
		    case XK_Right: key = KEY_RIGHT; break;
		    case XK_Up: key = KEY_UP; break;
		    case XK_Down: key = KEY_DOWN; break;
		    case XK_Home: key = KEY_HOME; break;
		    case XK_End: key = KEY_END; break;
		    case XK_Page_Up: key = KEY_PAGEUP; break;
		    case XK_Page_Down: key = KEY_PAGEDOWN; break;
		    case XK_Delete: key = KEY_DELETE; break;
		    case XK_Insert: key = KEY_INSERT; break;
		    case XK_Caps_Lock: key = KEY_CAPS; break;
		    case XK_Shift_L: 
		    case XK_Shift_R:
			key = KEY_SHIFT;
			if( event.type == KeyPress )
			    g_mod |= EVT_FLAG_SHIFT;
			else
			    g_mod &= ~EVT_FLAG_SHIFT;
			break;
		    case XK_Control_L: 
		    case XK_Control_R:
			key = KEY_CTRL;
			if( event.type == KeyPress )
			    g_mod |= EVT_FLAG_CTRL;
			else
			    g_mod &= ~EVT_FLAG_CTRL;
			break;
		    case XK_Alt_L: 
		    case XK_Alt_R:
			key = KEY_ALT;
			if( event.type == KeyPress )
			    g_mod |= EVT_FLAG_ALT;
			else
			    g_mod &= ~EVT_FLAG_ALT;
			break;
		}
		if( key )
		{
		    if( event.type == KeyPress )
		    {
			send_event( 0, EVT_BUTTONDOWN, g_mod, 0, 0, key, 0, 1024, 0, current_wm );
		    }
		    else
		    {
			send_event( 0, EVT_BUTTONUP, g_mod, 0, 0, key, 0, 1024, 0, current_wm );
		    }
		}
		break;
	}
    } 
    else
    {
	//There are no X11 events
	if( wm->events_count == 0 ) 
	{
	    //And no WM events
	    if( wm->flags & WIN_INIT_FLAG_FULL_CPU_USAGE )
	    {
		sched_yield();
	    }
	    else
	    {
		small_pause( 1 );
	    }
	}
    }
    if( wm->exit_request ) return 1;
    return 0;
}

void device_find_color( XColor *col, COLOR color, window_manager *wm )
{
    int r = red( color );
    int g = green( color );
    int b = blue( color );
    int p = ( r >> 3 ) | ( ( g >> 3 ) << 5 ) | ( ( b >> 3 ) << 10 );
    if( local_colors[ p ] == 0xFFFFFFFF )
    {
	col->red = r << 8;
	col->green = g << 8;
	col->blue = b << 8;
	XAllocColor( wm->dpy, wm->cmap, col );
	local_colors[ p ] = col->pixel;
    }
    else col->pixel = local_colors[ p ];
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
}

void device_screen_lock( WINDOWPTR win, window_manager *wm )
{
    if( wm->screen_lock_counter == 0 )
    {
    }
    wm->screen_lock_counter++;
}

#ifndef OPENGL

#ifdef FRAMEBUFFER

#include "wm_framebuffer.h"

void device_screen_lock( window_manager *wm )
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

void device_screen_unlock( window_manager *wm )
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

#else

int last_img_real_item_size = 0;

void device_redraw_framebuffer( window_manager *wm ) {}	

void device_draw_bitmap( 
    int dest_x, int dest_y, 
    int dest_xs, int dest_ys,
    int src_x, int src_y,
    sundog_image *bitmap,
    window_manager *wm )
{
    int src_xs = bitmap->xsize;
    int src_ys = bitmap->ysize;
    COLORPTR data = (COLORPTR)bitmap->data;

    XImage *img;
    if( last_img )
    {
	img = last_img;
	img->width = src_xs;
	img->height = src_ys;
	img->bytes_per_line = last_img_real_item_size * src_xs;
    }
    else
    {
	img = XCreateImage( wm->dpy, wm->win_visual, wm->win_depth, ZPixmap, 
                            0, (char*)temp_bitmap, src_xs, src_ys, 8, 0 );
	last_img_real_item_size = img->bytes_per_line / src_xs;
	last_img = img;
    }
    if( last_img_real_item_size == COLORLEN )
    {
	mem_copy( temp_bitmap, data, src_xs * src_ys * COLORLEN );
    }
    else
    {
	uchar *dest = temp_bitmap;
	for( int a = 0; a < src_xs * src_ys; a++ )
	{
	    COLOR c = data[ a ];
	    int r = red( c );
	    int g = green( c );
	    int b = blue( c );
	    int res;
	    switch( last_img_real_item_size )
	    {
		case 1:
		*(dest++) = (uchar)( (r>>5) + ((g>>5)<<3) + (b>>6)<<6 ); break;
		case 2:
		res = (b>>3) + ((g>>2)<<5) + ((r>>3)<<11);
		*(dest++) = (uchar)(res & 255); *(dest++) = (uchar)(res >> 8); break;
		case 3:
		*(dest++) = (uchar)b; *(dest++) = (uchar)g; *(dest++) = (uchar)r; break;
		case 4:
		*(dest++) = (uchar)b; *(dest++) = (uchar)g; *(dest++) = (uchar)r; *(dest++) = 255; break;		
	    }
	}
    }
    switch( XPutImage( wm->dpy, wm->win, wm->win_gc, img, src_x, src_y, dest_x, dest_y, dest_xs, dest_ys ) )
    {
	case BadDrawable: dprint( "BadDrawable\n" ); break;
	case BadGC: dprint( "BadGC\n" ); break;
	case BadMatch: dprint( "BadMatch\n" ); break;
	case BadValue: dprint( "BadValue\n" ); break;
    }
}

void device_draw_line( int x1, int y1, int x2, int y2, COLOR color, window_manager *wm )
{
    XColor col;
    device_find_color( &col, color, wm );
    XSetForeground( wm->dpy, wm->win_gc, col.pixel );
    XDrawLine( wm->dpy, wm->win, wm->win_gc, x1, y1, x2, y2 );
}

void device_draw_box( int x, int y, int xsize, int ysize, COLOR color, window_manager *wm )
{
    XColor col;
    device_find_color( &col, color, wm );
    XSetForeground( wm->dpy, wm->win_gc, col.pixel );
    XFillRectangle( wm->dpy, wm->win, wm->win_gc, x, y, xsize, ysize );
}

#endif

#endif //!OPENGL

//#################################
//#################################
//#################################

#endif
