/*
    wm_win32.h. Platform-dependent module : Win32
    This file is part of the SunDog engine.
    Copyright (C) 2002 - 2009 Alex Zolotov <nightradio@gmail.com>
*/

#ifndef __WINMANAGER_WIN32__
#define __WINMANAGER_WIN32__

#include <windows.h>
#include "win_res.h" //(IDI_ICON1) Must be in your project

const char *className = "SunDogEngine";
const UTF8_CHAR *windowName = "SunDogEngine_win32";
char windowName_ansi[ 256 ];
HGLRC hGLRC;
WNDCLASS wndClass;
HWND hWnd = 0;
int win_flags = 0;

int g_mod = 0;

#ifdef OPENGL
    #include <GL/gl.h>
    #include "wm_opengl.h"
#endif

window_manager *current_wm;

//#################################
//## DEVICE DEPENDENT FUNCTIONS: ##
//#################################

void SetupPixelFormat( HDC hDC );
LRESULT APIENTRY WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );
int Win32CreateWindow( HINSTANCE hCurrentInst, HINSTANCE hPreviousInst, LPSTR lpszCmdLine, int nCmdShow );
#ifdef DIRECTDRAW
    #include "wm_directx.h"
#endif

#ifdef FRAMEBUFFER
    COLORPTR framebuffer = 0;
#endif

int device_start( const UTF8_CHAR *windowname, int xsize, int ysize, int flags, window_manager *wm )
{
    int retval = 0;

    if( windowname ) windowName = windowname;
    current_wm = wm;
    win_flags = flags;
	
#ifdef GDI
    wm->gdi_bitmap_info[ 0 ] = 888;
    if( profile_get_int_value( KEY_SCREENX, 0 ) != -1 ) 
	xsize = profile_get_int_value( KEY_SCREENX, 0 );
    if( profile_get_int_value( KEY_SCREENY, 0 ) != -1 ) 
	ysize = profile_get_int_value( KEY_SCREENY, 0 );
#endif
#ifdef DIRECTDRAW
    if( profile_get_int_value( KEY_SCREENX, 0 ) != -1 ) 
	xsize = profile_get_int_value( KEY_SCREENX, 0 );
    if( profile_get_int_value( KEY_SCREENY, 0 ) != -1 ) 
	ysize = profile_get_int_value( KEY_SCREENY, 0 );
    fix_fullscreen_resolution( &xsize, &ysize, wm );
#endif
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

    wm->screen_xsize = xsize;
    wm->screen_ysize = ysize;
#ifdef OPENGL
    wm->real_window_width = xsize;
    wm->real_window_height = ysize;
#endif

    if( Win32CreateWindow( wm->hCurrentInst, wm->hPreviousInst, wm->lpszCmdLine, wm->nCmdShow ) ) //create main window
	return 1; //Error

    //Create framebuffer:
#ifdef FRAMEBUFFER
    #ifdef DIRECTDRAW
	framebuffer = 0;
    #else
	framebuffer = (COLORPTR)MEM_NEW( HEAP_DYNAMIC, wm->screen_xsize * wm->screen_ysize * COLORLEN );
	wm->fb_xpitch = 1;
	wm->fb_ypitch = wm->screen_xsize;
    #endif
#endif

    return retval;
}

void device_end( window_manager *wm )
{
#ifdef OPENGL
    wglMakeCurrent( current_wm->hdc, 0 );
    wglDeleteContext( hGLRC );
#endif

#ifdef DIRECTX
    dd_close();
#endif

#ifdef FRAMEBUFFER
#ifndef DIRECTDRAW
    mem_free( framebuffer );
#endif
#endif

    DestroyWindow( hWnd );
}

long device_event_handler( window_manager *wm )
{
    MSG msg;

    while( PeekMessage( &msg, NULL, 0, 0, PM_NOREMOVE ) ) 
    {
	if( GetMessage( &msg, NULL, 0, 0 ) == 0 ) return 1; //Exit
	TranslateMessage( &msg );
	DispatchMessage( &msg );
    }
    Sleep( 1 );

    return 0;
}

#ifdef OPENGL
void SetupPixelFormat( HDC hDC, window_manager *wm )
{
    if( hDC == 0 ) return;

    unsigned int flags = 
	( 0
        | PFD_SUPPORT_OPENGL
        | PFD_DRAW_TO_WINDOW
	);
    if( wm->gl_double_buffer )
	flags |= PFD_DOUBLEBUFFER;

    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),  /* size */
        1,                              /* version */
	flags,
        PFD_TYPE_RGBA,                  /* color type */
        16,                             /* prefered color depth */
        0, 0, 0, 0, 0, 0,               /* color bits (ignored) */
        0,                              /* no alpha buffer */
        0,                              /* alpha bits (ignored) */
        0,                              /* no accumulation buffer */
        0, 0, 0, 0,                     /* accum bits (ignored) */
        16,                             /* depth buffer */
        8,                              /* stencil buffer */
        0,                              /* no auxiliary buffers */
        PFD_MAIN_PLANE,                 /* main layer */
        0,                              /* reserved */
        0, 0, 0,                        /* no layer, visible, damage masks */
    };
    int pixelFormat;

    pixelFormat = ChoosePixelFormat( hDC, &pfd );
    if( pixelFormat == 0 ) 
    {
        MessageBox( WindowFromDC( hDC ), "ChoosePixelFormat failed.", "Error", MB_ICONERROR | MB_OK );
        exit( 1 );
    }

    if( SetPixelFormat( hDC, pixelFormat, &pfd ) != TRUE ) 
    {
        MessageBox( WindowFromDC( hDC ), "SetPixelFormat failed.", "Error", MB_ICONERROR | MB_OK );
        exit( 1 );
    }
}
#endif

uint16 win_key_to_sundog_key( uint16 vk )
{
    switch( vk )
    {
	case VK_BACK: return KEY_BACKSPACE; break;
	case VK_TAB: return KEY_TAB; break;
	case VK_RETURN: return KEY_ENTER; break;
	case VK_ESCAPE: return KEY_ESCAPE; break;
	
	case VK_F1: return KEY_F1; break;
	case VK_F2: return KEY_F2; break;
	case VK_F3: return KEY_F3; break;
	case VK_F4: return KEY_F4; break;
	case VK_F5: return KEY_F5; break;
	case VK_F6: return KEY_F6; break;
	case VK_F7: return KEY_F7; break;
	case VK_F8: return KEY_F8; break;
	case VK_F9: return KEY_F9; break;
	case VK_F10: return KEY_F10; break;
	case VK_F11: return KEY_F11; break;
	case VK_F12: return KEY_F12; break;
	case VK_UP: return KEY_UP; break;
	case VK_DOWN: return KEY_DOWN; break;
	case VK_LEFT: return KEY_LEFT; break;
	case VK_RIGHT: return KEY_RIGHT; break;
	case VK_INSERT: return KEY_INSERT; break;
	case VK_DELETE: return KEY_DELETE; break;
	case VK_HOME: return KEY_HOME; break;
	case VK_END: return KEY_END; break;
	case 33: return KEY_PAGEUP; break;
	case 34: return KEY_PAGEDOWN; break;
	case VK_CAPITAL: return KEY_CAPS; break;
	case VK_SHIFT: return KEY_SHIFT; break;
	case VK_CONTROL: return KEY_CTRL; break;

	case 189: return '-'; break;
	case 187: return '='; break;
	case 219: return '['; break;
	case 221: return ']'; break;
	case 186: return ';'; break;
	case 222: return 0x27; break; // '
	case 188: return ','; break;
	case 190: return '.'; break;
	case 191: return '/'; break;
	case 220: return 0x5C; break; // |
	case 192: return '`'; break;
    }
    
    if( vk == 0x20 ) return vk;
    if( vk >= 0x30 && vk <= 0x39 ) return vk; //Numbers
    if( vk >= 0x41 && vk <= 0x5A ) return vk; //Letters (capital)
    if( vk >= 0x61 && vk <= 0x7A ) return vk; //Letters (small)
    
    return 0;
}

#if defined(OPENGL) && defined(FRAMEBUFFER)
#define GET_WINDOW_COORDINATES \
    /*Real coordinates -> window_manager coordinates*/\
    x = lParam & 0xFFFF;\
    y = lParam>>16;\
    x = ( x << 11 ) / current_wm->real_window_width; x *= current_wm->screen_xsize; x >>= 11;\
    y = ( y << 11 ) / current_wm->real_window_height; y *= current_wm->screen_ysize; y >>= 11;
#else
#define GET_WINDOW_COORDINATES \
    /*Real coordinates -> window_manager coordinates*/\
    x = lParam & 0xFFFF;\
    y = lParam>>16;
#endif

LRESULT APIENTRY
WndProc(
    HWND hWnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    long x, y, d;
    short d2;
    int key;
    POINT point;

    switch( message )
    {
	case WM_CREATE:
	    break;

	case WM_CLOSE:
	    send_event( 0, EVT_QUIT, 0, 0, 0, 0, 0, 1024, 0, current_wm );
	    break;

	case WM_SIZE:
	    if( LOWORD(lParam) != 0 && HIWORD(lParam) != 0 )
	    {
#if defined(OPENGL) && defined(FRAMEBUFFER)
		// track window size changes
		if( hGLRC ) 
		{
		    current_wm->real_window_width = (int) LOWORD(lParam);
		    current_wm->real_window_height = (int) HIWORD(lParam);
            	    gl_resize( current_wm );
		    return 0;
		}
#endif
#if defined(OPENGL) && !defined(FRAMEBUFFER)
		// track window size changes
		if( hGLRC ) 
		{
		    current_wm->real_window_width = (int) LOWORD(lParam);
		    current_wm->real_window_height = (int) HIWORD(lParam);
		    current_wm->screen_xsize = (int) LOWORD(lParam);
		    current_wm->screen_ysize = (int) HIWORD(lParam);
		    send_event( current_wm->root_win, EVT_SCREENRESIZE, EVT_FLAG_AC, 0, 0, 0, 0, 1024, 0, current_wm );
            	    gl_resize( current_wm );
		    return 0;
		}
#endif
#ifdef GDI
		current_wm->screen_xsize = (int) LOWORD(lParam);
		current_wm->screen_ysize = (int) HIWORD(lParam);
		send_event( current_wm->root_win, EVT_SCREENRESIZE, EVT_FLAG_AC, 0, 0, 0, 0, 1024, 0, current_wm );
#endif
	    }
	    break;

	case WM_PAINT:
	    {
		PAINTSTRUCT gdi_ps;
		BeginPaint( hWnd, &gdi_ps );
		EndPaint( hWnd, &gdi_ps );
		send_event( current_wm->root_win, EVT_DRAW, EVT_FLAG_AC, 0, 0, 0, 0, 1024, 0, current_wm );
		return 0;
	    }
	    break;
	case 0x020A: //WM_MOUSEWHEEL
	    GET_WINDOW_COORDINATES;
	    point.x = x;
	    point.y = y;
	    ScreenToClient( hWnd, &point );
	    x = point.x;
	    y = point.y;
	    key = 0;
	    d = (unsigned long)wParam >> 16;
	    d2 = (short)d;
	    if( d2 < 0 ) key = MOUSE_BUTTON_SCROLLDOWN;
	    if( d2 > 0 ) key = MOUSE_BUTTON_SCROLLUP;
	    send_event( 0, EVT_MOUSEBUTTONDOWN, g_mod, x, y, key, 0, 1024, 0, current_wm );
	    break;
	case WM_LBUTTONDOWN:
	    GET_WINDOW_COORDINATES;
	    send_event( 0, EVT_MOUSEBUTTONDOWN, g_mod, x, y, MOUSE_BUTTON_LEFT, 0, 1024, 0, current_wm );
	    break;
	case WM_LBUTTONUP:
	    GET_WINDOW_COORDINATES;
	    send_event( 0, EVT_MOUSEBUTTONUP, g_mod, x, y, MOUSE_BUTTON_LEFT, 0, 1024, 0, current_wm );
	    break;
	case WM_MBUTTONDOWN:
	    GET_WINDOW_COORDINATES;
	    send_event( 0, EVT_MOUSEBUTTONDOWN, g_mod, x, y, MOUSE_BUTTON_MIDDLE, 0, 1024, 0, current_wm );
	    break;
	case WM_MBUTTONUP:
	    GET_WINDOW_COORDINATES;
	    send_event( 0, EVT_MOUSEBUTTONUP, g_mod, x, y, MOUSE_BUTTON_MIDDLE, 0, 1024, 0, current_wm );
	    break;
	case WM_RBUTTONDOWN:
	    GET_WINDOW_COORDINATES;
	    send_event( 0, EVT_MOUSEBUTTONDOWN, g_mod, x, y, MOUSE_BUTTON_RIGHT, 0, 1024, 0, current_wm );
	    break;
	case WM_RBUTTONUP:
	    GET_WINDOW_COORDINATES;
	    send_event( 0, EVT_MOUSEBUTTONUP, g_mod, x, y, MOUSE_BUTTON_RIGHT, 0, 1024, 0, current_wm );
	    break;
	case WM_MOUSEMOVE:
	    GET_WINDOW_COORDINATES;
	    key = 0;
	    if( wParam & MK_LBUTTON ) key |= MOUSE_BUTTON_LEFT;
	    if( wParam & MK_MBUTTON ) key |= MOUSE_BUTTON_MIDDLE;
	    if( wParam & MK_RBUTTON ) key |= MOUSE_BUTTON_RIGHT;
	    send_event( 0, EVT_MOUSEMOVE, g_mod, x, y, key, 0, 1024, 0, current_wm );
	    break;
	case WM_KEYDOWN:
	    if( wParam == VK_SHIFT )
		g_mod |= EVT_FLAG_SHIFT;
	    if( wParam == VK_CONTROL )
		g_mod |= EVT_FLAG_CTRL;
	    key = win_key_to_sundog_key( wParam );
	    if( key ) 
	    {
		send_event( 0, EVT_BUTTONDOWN, g_mod, 0, 0, key, ( lParam >> 16 ) & 511, 1024, 0, current_wm );
	    }
	    break;
	case WM_KEYUP:
	    if( wParam == VK_SHIFT )
		g_mod &= ~EVT_FLAG_SHIFT;
	    if( wParam == VK_CONTROL )
		g_mod &= ~EVT_FLAG_CTRL;
	    key = win_key_to_sundog_key( wParam );
	    if( key ) 
	    {
		send_event( 0, EVT_BUTTONUP, g_mod, 0, 0, key, ( lParam >> 16 ) & 511, 1024, 0, current_wm );
	    }
	    break;

	default:
	    return DefWindowProc( hWnd, message, wParam, lParam );
	    break;
    }

    return 0;
}

int Win32CreateWindow( HINSTANCE hCurrentInst, HINSTANCE hPreviousInst, LPSTR lpszCmdLine, int nCmdShow )
{
    UTF16_CHAR windowName_utf16[ 256 ];
    utf8_to_utf16( windowName_utf16, 256, windowName );
    wcstombs( windowName_ansi, (const wchar_t*)windowName_utf16, 256 );

    /* register window class */
#ifdef DIRECTDRAW
    wndClass.style = CS_BYTEALIGNCLIENT;
#else
    wndClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
#endif
    wndClass.lpfnWndProc = WndProc;
    wndClass.cbClsExtra = 0;
    wndClass.cbWndExtra = 0;
    wndClass.hInstance = hCurrentInst;
    wndClass.hIcon = LoadIcon( hCurrentInst, (LPCTSTR)IDI_ICON1 );
    wndClass.hCursor = LoadCursor( NULL, IDC_ARROW );
    wndClass.hbrBackground = (HBRUSH)GetStockObject( BLACK_BRUSH );
    wndClass.lpszMenuName = NULL;
    wndClass.lpszClassName = className;
    RegisterClass( &wndClass );

    /* create window */
    RECT Rect;
    Rect.top = 0;
    Rect.bottom = current_wm->screen_ysize;
    Rect.left = 0;
    Rect.right = current_wm->screen_xsize;
    ulong WS = 0;
    ulong WS_EX = 0;
    if( win_flags & WIN_INIT_FLAG_SCALABLE )
	WS = WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    else
	WS = WS_CAPTION | WS_SYSMENU | WS_THICKFRAME;
    if( win_flags & WIN_INIT_FLAG_NOBORDER )
	WS = WS_POPUP;
#ifdef DIRECTDRAW
    int fullscreen = 1;
    //if( profile_get_int_value( KEY_FULLSCREEN, 0 ) != -1 ) fullscreen = 1;
    if( fullscreen )
    {
	WS = WS_POPUP;
	WS_EX = WS_EX_TOPMOST;
    }
#endif
    AdjustWindowRect( &Rect, WS, 0 );
    hWnd = CreateWindowEx(
	WS_EX,
        className, windowName_ansi,
        WS,
        ( GetSystemMetrics(SM_CXSCREEN) - (Rect.right - Rect.left) ) / 2, 
        ( GetSystemMetrics(SM_CYSCREEN) - (Rect.bottom - Rect.top) ) / 2, 
        Rect.right - Rect.left, Rect.bottom - Rect.top,
        NULL, NULL, hCurrentInst, NULL
    );
    /* display window */
    ShowWindow( hWnd, nCmdShow );
    UpdateWindow( hWnd );

    current_wm->hdc = GetDC( hWnd );
    current_wm->hwnd = hWnd;

#if defined(OPENGL) && defined(FRAMEBUFFER)
    /* initialize OpenGL framebuffer rendering */
    SetupPixelFormat( current_wm->hdc, current_wm );
    hGLRC = wglCreateContext( current_wm->hdc );
    wglMakeCurrent( current_wm->hdc, hGLRC );
    gl_init( current_wm );
    gl_resize( current_wm );
#endif
#if defined(OPENGL) && !defined(FRAMEBUFFER)
    /* initialize OpenGL accelerated rendering */
    SetupPixelFormat( current_wm->hdc, current_wm );
    hGLRC = wglCreateContext( current_wm->hdc );
    wglMakeCurrent( current_wm->hdc, hGLRC );
    gl_init( current_wm );
    gl_resize( current_wm );
#endif
#ifdef DIRECTDRAW
    if( dd_init( fullscreen ) ) 
	return 1; //Error
#endif

    return 0;
}

void device_screen_unlock( WINDOWPTR win, window_manager *wm )
{
    if( wm->screen_lock_counter == 1 )
    {
#ifdef DIRECTDRAW
	if( g_primary_locked )
	{
	    lpDDSPrimary->Unlock( g_sd.lpSurface );
	    g_primary_locked = 0;
	    framebuffer = 0;
	}
#endif //DIRECTDRAW
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

void device_screen_lock( WINDOWPTR win, window_manager *wm )
{
    if( wm->screen_lock_counter == 0 )
    {
#ifdef DIRECTDRAW
	if( g_primary_locked == 0 )
	{
	    if( lpDDSPrimary )
	    {
		lpDDSPrimary->Lock( 0, &g_sd, DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAIT, 0 );
		wm->fb_ypitch = g_sd.lPitch / COLORLEN;
		framebuffer = (COLORPTR)g_sd.lpSurface;
		g_primary_locked = 1;
	    }
	}
#endif //DIRECTDRAW
    }
    wm->screen_lock_counter++;
    
    if( wm->screen_lock_counter > 0 ) 
	wm->screen_is_active = 1;
    else
	wm->screen_is_active = 0;
}

#ifdef FRAMEBUFFER

#include "wm_framebuffer.h"

#else

#ifdef GDI

void device_redraw_framebuffer( window_manager *wm ) 
{
}	

void device_draw_line( int x1, int y1, int x2, int y2, COLOR color, window_manager *wm )
{
    if( wm->hdc == 0 ) return;
    HPEN pen;
    pen = CreatePen( PS_SOLID, 1, RGB( red( color ), green( color ), blue( color ) ) );
    SelectObject( wm->hdc, pen );
    MoveToEx( wm->hdc, x1, y1, 0 );
    LineTo( wm->hdc, x2, y2 );
    SetPixel( wm->hdc, x2, y2, RGB( red( color ), green( color ), blue( color ) ) );
    DeleteObject( pen );
}

void device_draw_box( int x, int y, int xsize, int ysize, COLOR color, window_manager *wm )
{
    if( wm->hdc == 0 ) return;
    if( xsize == 1 && ysize == 1 )
    {
	SetPixel( wm->hdc, x, y, RGB( red( color ), green( color ), blue( color ) ) );
    }
    else
    {
	HPEN pen = CreatePen( PS_SOLID, 1, RGB( red( color ), green( color ), blue( color ) ) );
	HBRUSH brush = CreateSolidBrush( RGB( red( color ), green( color ), blue( color ) ) );
	SelectObject( wm->hdc, pen );
	SelectObject( wm->hdc, brush );
	Rectangle( wm->hdc, x, y, x + xsize, y + ysize );
	DeleteObject( brush );
	DeleteObject( pen );
    }
}

void device_draw_bitmap( 
    int dest_x, int dest_y, 
    int dest_xs, int dest_ys,
    int src_x, int src_y,
    sundog_image *img,
    window_manager *wm )
{
    if( wm->hdc == 0 ) return;
    BITMAPINFO *bi = (BITMAPINFO*)wm->gdi_bitmap_info;
    if( wm->gdi_bitmap_info[ 0 ] == 888 )
    {
	memset( bi, 0, sizeof( wm->gdi_bitmap_info ) );
	//Set 256 colors palette:
	int a;
#ifdef GRAYSCALE
	for( a = 0; a < 256; a++ ) 
	{ 
    	    bi->bmiColors[ a ].rgbRed = a; 
    	    bi->bmiColors[ a ].rgbGreen = a; 
    	    bi->bmiColors[ a ].rgbBlue = a; 
	}
#else
	for( a = 0; a < 256; a++ ) 
	{ 
    	    bi->bmiColors[ a ].rgbRed = (a<<5)&224; 
	    if( bi->bmiColors[ a ].rgbRed ) 
		bi->bmiColors[ a ].rgbRed |= 0x1F; 
	    bi->bmiColors[ a ].rgbReserved = 0;
	}
	for( a = 0; a < 256; a++ )
	{
	    bi->bmiColors[ a ].rgbGreen = (a<<2)&224; 
	    if( bi->bmiColors[ a ].rgbGreen ) 
		bi->bmiColors[ a ].rgbGreen |= 0x1F; 
	}
	for( a = 0; a < 256; a++ ) 
	{ 
	    bi->bmiColors[ a ].rgbBlue = (a&192);
	    if( bi->bmiColors[ a ].rgbBlue ) 
		bi->bmiColors[ a ].rgbBlue |= 0x3F; 
	}
#endif
    }
    int src_xs = img->xsize;
    int src_ys = img->ysize;
    COLORPTR data = (COLORPTR)img->data;
    bi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi->bmiHeader.biWidth = src_xs;
    bi->bmiHeader.biHeight = -src_ys;
    bi->bmiHeader.biPlanes = 1;
    bi->bmiHeader.biBitCount = COLORBITS;
    bi->bmiHeader.biCompression = BI_RGB;
    SetDIBitsToDevice(  
	wm->hdc,
	dest_x, // Destination top left hand corner X Position
	dest_y, // Destination top left hand corner Y Position
	dest_xs, // Destinations Width
	dest_ys, // Desitnations height
	src_x, // Source low left hand corner's X Position
	src_ys - ( src_y + dest_ys ), // Source low left hand corner's Y Position
	0,
	src_ys,
	data, // Source's data
	(BITMAPINFO*)wm->gdi_bitmap_info, // Bitmap Info
	DIB_RGB_COLORS );
}

#endif //GDI

#endif //not FRAMEBUFFER

//#################################
//#################################
//#################################

#endif
