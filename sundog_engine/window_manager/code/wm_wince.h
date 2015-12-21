/*
    wm_wince.h. Platform-dependent module : WindowsCE
    This file is part of the SunDog engine.
    Copyright (C) 2002 - 2009 Alex Zolotov <nightradio@gmail.com>
*/

#ifndef __WINMANAGER_WINCE__
#define __WINMANAGER_WINCE__

#include <windows.h>
#include <wingdi.h>
#include <aygshell.h>
#include "win_res.h" //(IDI_ICON1) Must be defined in your project

enum
{
    VIDEODRIVER_NONE = 0,
    VIDEODRIVER_GAPI,
    VIDEODRIVER_RAW,
    VIDEODRIVER_DDRAW,
    VIDEODRIVER_GDI
};

#define GETRAWFRAMEBUFFER   0x00020001
#define FORMAT_565 1
#define FORMAT_555 2
#define FORMAT_OTHER 3
typedef struct _RawFrameBufferInfo
{
   WORD wFormat;
   WORD wBPP;
   VOID *pFramePointer;
   int  cxStride;
   int  cyStride;
   int  cxPixels;
   int  cyPixels;
} RawFrameBufferInfo;

WCHAR *className = L"SunDogEngine";
const UTF8_CHAR *windowName = "SunDogEngine_winCE";
HGLRC hGLRC;
WNDCLASS wndClass;
HWND hWnd = 0;
int win_flags = 0;

HANDLE systemIdleTimerThread = 0;

int g_mod = 0;

window_manager *current_wm;

//#################################
//## DEVICE DEPENDENT FUNCTIONS: ##
//#################################

void SetupPixelFormat( HDC hDC );
LRESULT APIENTRY WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );
int Win32CreateWindow( HINSTANCE hCurrentInst, HINSTANCE hPreviousInst, LPSTR lpszCmdLine, int nCmdShow );

#ifdef FRAMEBUFFER
    COLORPTR framebuffer = 0;
#endif

#include "gx_loader.h"
tGXOpenDisplay GXOpenDisplay = 0;
tGXCloseDisplay GXCloseDisplay = 0;
tGXBeginDraw GXBeginDraw = 0;
tGXEndDraw GXEndDraw = 0;
tGXOpenInput GXOpenInput = 0;
tGXCloseInput GXCloseInput = 0;
tGXGetDisplayProperties GXGetDisplayProperties = 0;
tGXGetDefaultKeys GXGetDefaultKeys = 0;
tGXSuspend GXSuspend = 0;
tGXResume GXResume = 0;
tGXSetViewport GXSetViewport = 0;
GXDisplayProperties gx_dp;

typedef void (*tSystemIdleTimerReset)( void );
tSystemIdleTimerReset SD_SystemIdleTimerReset = 0;

DWORD WINAPI SystemIdleTimerProc( LPVOID lpParameter )
{
    while( 1 )
    {
	Sleep( 1000 * 10 );
	if( SD_SystemIdleTimerReset )
	    SD_SystemIdleTimerReset();
    }
    ExitThread( 0 );
    return 0;
}

int device_start( const char *windowname, int xsize, int ysize, int flags, window_manager *wm )
{
    int retval = 0;
    FARPROC proc;

    if( windowname ) windowName = windowname;
    current_wm = wm;
    win_flags = flags;

    wm->gdi_bitmap_info[ 0 ] = 888;
	
    //Get OS version:
    OSVERSIONINFO ver;
    GetVersionEx( &ver );
    wm->os_version = ver.dwMajorVersion;

    UTF8_CHAR *vd_str = profile_get_str_value( KEY_VIDEODRIVER, 0 );
    wm->vd = VIDEODRIVER_GAPI;
    if( vd_str )
    {
	dprint( "Videodriver: %s\n", vd_str );
	if( mem_strcmp( vd_str, "raw" ) == 0 )
	    wm->vd = VIDEODRIVER_RAW;
	if( mem_strcmp( vd_str, "gdi" ) == 0 )
	    wm->vd = VIDEODRIVER_GDI;
    }
    else
    {
	dprint( "No videodriver selected. Default: gapi\n" );
    }

    //Get real screen resolution:
    xsize = GetSystemMetrics( SM_CXSCREEN );
    ysize = GetSystemMetrics( SM_CYSCREEN );
    if( xsize > 320 && ysize > 320 )
	wm->hires = 1;
    else
	wm->hires = 0;

    if( flags & WIN_INIT_FLAG_SCREENFLIP ) wm->screen_flipped = 1;
    if( ysize > xsize ) wm->screen_flipped ^= 1;

    //Get user defined resolution:
    if( profile_get_int_value( KEY_SCREENX, 0 ) != -1 ) xsize = profile_get_int_value( KEY_SCREENX, 0 );
    if( profile_get_int_value( KEY_SCREENY, 0 ) != -1 ) ysize = profile_get_int_value( KEY_SCREENY, 0 );

    //Save it:
    wm->screen_xsize = xsize;
    wm->screen_ysize = ysize;
    
    if( wm->vd == VIDEODRIVER_GAPI )
    {
	HMODULE gapiLibrary;
	FARPROC proc;
	gapiLibrary = LoadLibrary( TEXT( "gx.dll" ) );
	if( gapiLibrary == 0 ) 
	{
	    MessageBox( hWnd, L"GX not found", L"Error", MB_OK );
	    dprint( "ERROR: GX.dll not found\n" );
	    return 1;
	}
	IMPORT( gapiLibrary, proc, tGXOpenDisplay, "?GXOpenDisplay@@YAHPAUHWND__@@K@Z", GXOpenDisplay );
	IMPORT( gapiLibrary, proc, tGXGetDisplayProperties, "?GXGetDisplayProperties@@YA?AUGXDisplayProperties@@XZ", GXGetDisplayProperties );
	IMPORT( gapiLibrary, proc, tGXOpenInput,"?GXOpenInput@@YAHXZ", GXOpenInput );
	IMPORT( gapiLibrary, proc, tGXCloseDisplay, "?GXCloseDisplay@@YAHXZ", GXCloseDisplay );
	IMPORT( gapiLibrary, proc, tGXBeginDraw, "?GXBeginDraw@@YAPAXXZ", GXBeginDraw );
	IMPORT( gapiLibrary, proc, tGXEndDraw, "?GXEndDraw@@YAHXZ", GXEndDraw );
	IMPORT( gapiLibrary, proc, tGXCloseInput,"?GXCloseInput@@YAHXZ", GXCloseInput );
	IMPORT( gapiLibrary, proc, tGXSetViewport,"?GXSetViewport@@YAHKKKK@Z", GXSetViewport );
	IMPORT( gapiLibrary, proc, tGXSuspend, "?GXSuspend@@YAHXZ", GXSuspend );
	IMPORT( gapiLibrary, proc, tGXResume, "?GXResume@@YAHXZ", GXResume );
	if( GXOpenDisplay == 0 ) retval = 1;
	if( GXGetDisplayProperties == 0 ) retval = 2;
	if( GXOpenInput == 0 ) retval = 3;
	if( GXCloseDisplay == 0 ) retval = 4;
	if( GXBeginDraw == 0 ) retval = 5;
	if( GXEndDraw == 0 ) retval = 6;
	if( GXCloseInput == 0 ) retval = 7;
	if( GXSuspend == 0 ) retval = 8;
	if( GXResume == 0 ) retval = 9;
	if( retval )
	{
	    MessageBox( hWnd, L"Some GX functions not found", L"Error", MB_OK );
	    dprint( "ERROR: some GX functions not found (%d)\n", retval );
	    return retval;
	}
    }

    Win32CreateWindow( wm->hCurrentInst, wm->hPreviousInst, (char*)wm->lpszCmdLine, wm->nCmdShow ); //create main window

    if( wm->vd == VIDEODRIVER_GAPI )
    {
	wm->gx_suspended = 0;
	if( GXOpenDisplay( hWnd, GX_FULLSCREEN ) == 0 )
	{
	    MessageBox( hWnd, L"Can't open GAPI display", L"Error", MB_OK );
	    dprint( "ERROR: Can't open GAPI display\n" );
	    return 1;
	}
	else
        {
	    gx_dp = GXGetDisplayProperties();
	    dprint( "GAPI Width: %d\n", gx_dp.cxWidth );
	    dprint( "GAPI Height: %d\n", gx_dp.cyHeight );
	    {
		//Screen resolution must be less or equal to GAPI resolution:
		if( wm->screen_xsize >= gx_dp.cxWidth )
		    wm->screen_xsize = gx_dp.cxWidth;
		if( wm->screen_ysize >= gx_dp.cyHeight )
		    wm->screen_ysize = gx_dp.cyHeight;
	    }
	    dprint( "GAPI cbxPitch: %d\n", gx_dp.cbxPitch );
	    dprint( "GAPI cbyPitch: %d\n", gx_dp.cbyPitch );
	    dprint( "GAPI cBPP: %d\n", gx_dp.cBPP );
	    if( gx_dp.ffFormat & kfLandscape ) dprint( "kfLandscape\n" );
	    if( gx_dp.ffFormat & kfPalette ) dprint( "kfPalette\n" );
	    if( gx_dp.ffFormat & kfDirect ) dprint( "kfDirect\n" );
	    if( gx_dp.ffFormat & kfDirect555 ) dprint( "kfDirect555\n" );
	    if( gx_dp.ffFormat & kfDirect565 ) dprint( "kfDirect565\n" );
	    if( gx_dp.ffFormat & kfDirect888 ) dprint( "kfDirect888\n" );
	    if( gx_dp.ffFormat & kfDirect444 ) dprint( "kfDirect444\n" );
	    if( gx_dp.ffFormat & kfDirectInverted ) dprint( "kfDirectInverted\n" );
	    wm->fb_xpitch = gx_dp.cbxPitch;
	    wm->fb_ypitch = gx_dp.cbyPitch;
	    wm->fb_xpitch /= COLORLEN;
	    wm->fb_ypitch /= COLORLEN;
    	    GXSetViewport( 0, gx_dp.cyHeight, 0, 0 );
	    GXOpenInput();
	}
    }
    else if( wm->vd == VIDEODRIVER_RAW )
    {
	RawFrameBufferInfo *rfb = (RawFrameBufferInfo*)wm->rfb;
	HDC hdc = GetDC( NULL );
	int rv = ExtEscape( hdc, GETRAWFRAMEBUFFER, 0, NULL, sizeof( RawFrameBufferInfo ), (char *)rfb );
	ReleaseDC( NULL, hdc );
	if( rv < 0 )
	{
	    MessageBox( hWnd, L"Can't open raw framebuffer", L"Error", MB_OK );
	    dprint( "ERROR: Can't open raw framebuffer\n" );
	    return rv;
	}
	dprint( "RFB wFormat:\n" );
	if( rfb->wFormat == FORMAT_565 ) dprint( "FORMAT_565\n" );
	else if( rfb->wFormat == FORMAT_555 ) dprint( "FORMAT_555\n" );
	else 
	{
	    dprint( "%d\n", rfb->wFormat );
	    MessageBox( hWnd, L"Can't open raw framebuffer", L"Error", MB_OK );
	    return 1;
	}
	dprint( "RFB wBPP: %d\n", rfb->wBPP );
	dprint( "RFB cxStride: %d\n", rfb->cxStride );
	dprint( "RFB cyStride: %d\n", rfb->cyStride );
	dprint( "RFB cxPixels: %d\n", rfb->cxPixels );
	dprint( "RFB cyPixels: %d\n", rfb->cyPixels );
	wm->fb_xpitch = rfb->cxStride;
	wm->fb_ypitch = rfb->cyStride;
	wm->fb_xpitch /= COLORLEN;
	wm->fb_ypitch /= COLORLEN;
    }
    else if( wm->vd == VIDEODRIVER_GDI )
    {
	//Create virtual framebuffer:
	if( wm->hires )
	{
	    wm->screen_xsize /= 2;
	    wm->screen_ysize /= 2;
	}
	framebuffer = (COLORPTR)MEM_NEW( HEAP_DYNAMIC, wm->screen_xsize * wm->screen_ysize * COLORLEN );
	wm->fb_xpitch = 1;
	wm->fb_ypitch = wm->screen_xsize;
    }

    if( wm->screen_flipped )
    {
	//Flip the screen:
	wm->fb_offset = wm->fb_ypitch * ( wm->screen_ysize - 1 );
	int ttt = wm->fb_ypitch;
	wm->fb_ypitch = wm->fb_xpitch;
	wm->fb_xpitch = -ttt;
	ttt = wm->screen_xsize; wm->screen_xsize = wm->screen_ysize; wm->screen_ysize = ttt;
    }

    HMODULE coreLibrary;
    coreLibrary = LoadLibrary( TEXT( "coredll.dll" ) );
    if( coreLibrary == 0 ) 
    {
	dprint( "ERROR: coredll.dll not found\n" );
    }
    else
    {
	IMPORT( coreLibrary, proc, tSystemIdleTimerReset, "SystemIdleTimerReset", SD_SystemIdleTimerReset );
	if( SD_SystemIdleTimerReset == 0 )
	    dprint( "SystemIdleTimerReset() not found\n" );
    }
    systemIdleTimerThread = (HANDLE)CreateThread( NULL, 0, SystemIdleTimerProc, NULL, 0, NULL );
    
    return retval;
}

void device_end( window_manager *wm )
{
    if( systemIdleTimerThread )
	CloseHandle( systemIdleTimerThread );

    if( wm->vd == VIDEODRIVER_GDI && framebuffer )
    {
	mem_free( framebuffer );
    }

    SHFullScreen( hWnd, SHFS_SHOWTASKBAR | SHFS_SHOWSTARTICON | SHFS_SHOWSIPBUTTON );

    DestroyWindow( hWnd );
    Sleep( 500 );
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

void SetupPixelFormat( HDC hDC )
{
    if( hDC == 0 ) return;
}

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
	case VK_UP: if( current_wm->screen_flipped == 0 ) return KEY_UP; else return KEY_RIGHT; break;
	case VK_DOWN: if( current_wm->screen_flipped == 0 ) return KEY_DOWN; else return KEY_LEFT; break;
	case VK_LEFT: if( current_wm->screen_flipped == 0 ) return KEY_LEFT; else return KEY_UP; break;
	case VK_RIGHT: if( current_wm->screen_flipped == 0 ) return KEY_RIGHT; else return KEY_DOWN; break;
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

#define GET_WINDOW_COORDINATES \
    /*Real coordinates -> window_manager coordinates*/\
    x = lParam & 0xFFFF;\
    y = lParam>>16; \
    if( current_wm->vd == VIDEODRIVER_GAPI || current_wm->vd == VIDEODRIVER_GDI ) \
    { \
	if( current_wm->hires ) \
	{ \
	    x /= 2; \
	    y /= 2; \
	} \
    } \
    if( current_wm->screen_flipped ) \
    { \
	int ttt = x; \
	x = ( current_wm->screen_xsize - 1 ) - y;\
	y = ttt; \
    }

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

	case WM_DESTROY:
	    if( current_wm->vd == VIDEODRIVER_GAPI )
	    {
		GXCloseInput();
		GXCloseDisplay();
	    }
	    PostQuitMessage( 0 );
	    break;

	case WM_SIZE:
#ifndef FRAMEBUFFER
	    if( LOWORD(lParam) != 0 && HIWORD(lParam) != 0 )
	    {
		current_wm->screen_xsize = (int) LOWORD(lParam);
		current_wm->screen_ysize = (int) HIWORD(lParam);
		send_event( current_wm->root_win, EVT_SCREENRESIZE, EVT_FLAG_AC, 0, 0, 0, 0, 1024, 0, current_wm );
	    }
#endif
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

	case WM_DEVICECHANGE:
	    if( wParam == 0x8000 ) //DBT_DEVICEARRIVAL
	    {
		//It may be device power ON:
		send_event( current_wm->root_win, EVT_DRAW, EVT_FLAG_AC, 0, 0, 0, 0, 1024, 0, current_wm );
	    }
	    break;

	case WM_KILLFOCUS:
	    if( current_wm->vd == VIDEODRIVER_GAPI )
	    {
		if( current_wm->screen_lock_counter > 0 )
		    GXEndDraw();
		GXSuspend();
	    }
    	    current_wm->gx_suspended = 1;
	    send_event( current_wm->root_win, EVT_SCREENUNFOCUS, EVT_FLAG_AC, 0, 0, 0, 0, 1024, 0, current_wm );
	    break;
	case WM_SETFOCUS:
	    if( current_wm->gx_suspended )
	    {
		if( current_wm->vd == VIDEODRIVER_GAPI )
		{
		    if( current_wm->screen_lock_counter > 0 )
		    {
			framebuffer = (COLORPTR)GXBeginDraw();
		    }
		    GXResume();
		}
    		current_wm->gx_suspended = 0;
	    }
	    //Hide taskbar and another system windows:
	    SHFullScreen( hWnd, SHFS_HIDETASKBAR | SHFS_HIDESTARTICON | SHFS_HIDESIPBUTTON );
	    //Redraw all:
	    send_event( current_wm->root_win, EVT_SCREENFOCUS, EVT_FLAG_AC, 0, 0, 0, 0, 1024, 0, current_wm );
	    send_event( current_wm->root_win, EVT_DRAW, EVT_FLAG_AC, 0, 0, 0, 0, 1024, 0, current_wm );
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

    //Register window class:
    wndClass.style = CS_HREDRAW | CS_VREDRAW;
    wndClass.lpfnWndProc = WndProc;
    wndClass.cbClsExtra = 0;
    wndClass.cbWndExtra = 0;
    wndClass.hInstance = hCurrentInst;
    wndClass.hIcon = LoadIcon( hCurrentInst, MAKEINTRESOURCE( IDI_ICON1 ) );
    wndClass.hCursor = LoadCursor( NULL, IDC_ARROW );
    wndClass.hbrBackground = (HBRUSH)GetStockObject( BLACK_BRUSH );
    wndClass.lpszMenuName = NULL;
    wndClass.lpszClassName = className;
    RegisterClass( &wndClass );

    //Create window
    RECT Rect;
#ifndef FRAMEBUFFER
    Rect.top = 0;
    Rect.bottom = current_wm->screen_ysize;
    Rect.left = 0;
    Rect.right = current_wm->screen_xsize;
    AdjustWindowRectEx( &Rect, WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX, 0, 0 );
    if( win_flags & WIN_INIT_FLAG_SCALABLE )
    {
	hWnd = CreateWindow(
	    className, windowName_utf16,
	    WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
	    ( GetSystemMetrics(SM_CXSCREEN) - (Rect.right - Rect.left) ) / 2, 
	    ( GetSystemMetrics(SM_CYSCREEN) - (Rect.bottom - Rect.top) ) / 2, 
	    Rect.right - Rect.left, Rect.bottom - Rect.top,
	    NULL, NULL, hCurrentInst, NULL
	);
    }
    else
    {
	hWnd = CreateWindow(
	    className, windowName_utf16,
	    WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
	    ( GetSystemMetrics(SM_CXSCREEN) - (Rect.right - Rect.left) ) / 2, 
	    ( GetSystemMetrics(SM_CYSCREEN) - (Rect.bottom - Rect.top) ) / 2, 
	    Rect.right - Rect.left, Rect.bottom - Rect.top,
	    NULL, NULL, hCurrentInst, NULL
	);
    }
#else
    hWnd = CreateWindow(
	className, (const WCHAR*)windowName_utf16,
	WS_VISIBLE,
	0, 0,
	GetSystemMetrics( SM_CXSCREEN ), GetSystemMetrics( SM_CYSCREEN ),
	NULL, NULL, hCurrentInst, NULL
    );
#endif //FRAMEBUFFER

    //Hide taskbar and another system windows:
    SHFullScreen( hWnd, SHFS_HIDETASKBAR | SHFS_HIDESTARTICON | SHFS_HIDESIPBUTTON );

    //Display the window:
    ShowWindow( hWnd, nCmdShow );
    UpdateWindow( hWnd );

    //Get DC:
    current_wm->hdc = GetDC( hWnd );

    return 0;
}

#define VK_APP_LAUNCH1       0xC1
#define VK_APP_LAUNCH2       0xC2
#define VK_APP_LAUNCH3       0xC3
#define VK_APP_LAUNCH4       0xC4
#define VK_APP_LAUNCH5       0xC5
#define VK_APP_LAUNCH6       0xC6
#define VK_APP_LAUNCH7       0xC7
#define VK_APP_LAUNCH8       0xC8
#define VK_APP_LAUNCH9       0xC9
#define VK_APP_LAUNCH10      0xCA
#define VK_APP_LAUNCH11      0xCB
#define VK_APP_LAUNCH12      0xCC
#define VK_APP_LAUNCH13      0xCD
#define VK_APP_LAUNCH14      0xCE
#define VK_APP_LAUNCH15      0xCF

void device_screen_unlock( WINDOWPTR win, window_manager *wm )
{
    if( wm->screen_lock_counter == 1 && wm->gx_suspended == 0 )
    {
	if( current_wm->vd == VIDEODRIVER_GAPI )
	{
	    if( framebuffer )
	    {
		GXEndDraw();
	    }
	}
    }

    if( wm->screen_lock_counter > 0 )
    {
	wm->screen_lock_counter--;
    }

    if( wm->gx_suspended == 0 && wm->screen_lock_counter > 0 )
	wm->screen_is_active = 1;
    else
	wm->screen_is_active = 0;
}

void device_screen_lock( WINDOWPTR win, window_manager *wm )
{
    if( wm->screen_lock_counter == 0 && wm->gx_suspended == 0 )
    {
	if( current_wm->vd == VIDEODRIVER_GAPI )
	{
	    framebuffer = (COLORPTR)GXBeginDraw();
	}
	else if( current_wm->vd == VIDEODRIVER_RAW )
	{
	    RawFrameBufferInfo *rfb = (RawFrameBufferInfo*)wm->rfb;
	    framebuffer = (COLORPTR)rfb->pFramePointer;
	}
    }
    wm->screen_lock_counter++;
    
    if( wm->gx_suspended == 0 && wm->screen_lock_counter > 0 )
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
    int new_src_y;
    if( wm->os_version > 4 )
    {
	new_src_y = src_ys - ( src_y + dest_ys );
    }
    else
    {
	new_src_y = src_y;
    }
    SetDIBitsToDevice(  
	wm->hdc,
	dest_x, // Destination top left hand corner X Position
	dest_y,	// Destination top left hand corner Y Position
	dest_xs, // Destinations Width
	dest_ys, // Desitnations height
	src_x, // Source low left hand corner's X Position
	new_src_y, // Source low left hand corner's Y Position
	0,
	src_ys,
	data, // Source's data
	(BITMAPINFO*)wm->gdi_bitmap_info, // Bitmap Info
	DIB_RGB_COLORS );
}

#endif

//#################################
//#################################
//#################################

#endif
