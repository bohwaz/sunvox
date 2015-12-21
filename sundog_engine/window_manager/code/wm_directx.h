/*
    wm_directx.h. Platform-dependent module : DirectDraw (Win32)
    This file is part of the SunDog engine.
    Copyright (C) 2002 - 2009 Alex Zolotov <nightradio@gmail.com>
*/

#ifndef __WINMANAGER_DIRECTX__
#define __WINMANAGER_DIRECTX__

#include "ddraw.h"

LPDIRECTDRAW lpDD; // DirectDraw object
LPDIRECTDRAWSURFACE lpDDSPrimary; // DirectDraw primary surface
LPDIRECTDRAWSURFACE lpDDSBack; // DirectDraw back surface
LPDIRECTDRAWSURFACE lpDDSOne;
LPDIRECTDRAWCLIPPER lpClipper;
HRESULT ddrval;

DDSURFACEDESC g_sd;
int g_primary_locked = 0;

extern COLORPTR framebuffer;

int dd_init( int fullscreen )
{
    DDSURFACEDESC ddsd;
    DDSCAPS ddscaps;
    HRESULT ddrval;

    ddrval = DirectDrawCreate( NULL, &lpDD, NULL );
    if( ddrval != DD_OK ) 
    {
	MessageBox( hWnd, "DirectDrawCreate error", "Error", MB_OK );
	return 1;
    }
    if( fullscreen )
	ddrval = lpDD->SetCooperativeLevel( hWnd, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN );
    else
	ddrval = lpDD->SetCooperativeLevel( hWnd, DDSCL_NORMAL );
    if( ddrval != DD_OK ) 
    {
	MessageBox( hWnd, "SetCooperativeLevel error", "Error", MB_OK );
	return 1;
    }
    if( fullscreen )
    {
	ddrval = lpDD->SetDisplayMode( current_wm->screen_xsize, current_wm->screen_ysize, COLORBITS ); 
	if( ddrval != DD_OK ) 
	{ 
	    MessageBox( hWnd, "SetDisplayMode error", "Error", MB_OK );
	    return 1;
	}
    }

    // Create the primary surface with 1 back buffer
    memset( &ddsd, 0, sizeof(ddsd) );
    ddsd.dwSize = sizeof( ddsd );
    ddsd.dwFlags = DDSD_CAPS;// | DDSD_BACKBUFFERCOUNT;
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;// | DDSCAPS_FLIP | DDSCAPS_COMPLEX;
    //ddsd.dwBackBufferCount = 1;

    ddrval = lpDD->CreateSurface( &ddsd, &lpDDSPrimary, NULL ); 
    if( ddrval != DD_OK ) 
    { 
	switch( ddrval )
	{
	    case DDERR_UNSUPPORTEDMODE:
	    MessageBox( hWnd,"CreateSurface DDERR_UNSUPPORTEDMODE","Error",MB_OK);
	    break;
	    case DDERR_PRIMARYSURFACEALREADYEXISTS:
	    MessageBox( hWnd,"CreateSurface DDERR_PRIMARYSURFACEALREADYEXISTS ","Error",MB_OK);
	    break;
	    case DDERR_OUTOFVIDEOMEMORY:
	    MessageBox( hWnd,"CreateSurface DDERR_OUTOFVIDEOMEMORY  ","Error",MB_OK);
	    break;
	    case DDERR_OUTOFMEMORY:
	    MessageBox( hWnd,"CreateSurface DDERR_OUTOFMEMORY","Error",MB_OK);
	    break;
	    case DDERR_NOZBUFFERHW:
	    MessageBox( hWnd,"CreateSurface DDERR_NOZBUFFERHW","Error",MB_OK);
	    break;
	    case DDERR_NOOVERLAYHW:
	    MessageBox( hWnd,"CreateSurface DDERR_NOOVERLAYHW","Error",MB_OK);
	    break;
	    case DDERR_NOMIPMAPHW:
	    MessageBox( hWnd,"CreateSurface DDERR_NOMIPMAPHW","Error",MB_OK);
	    break;
	    case DDERR_NOEXCLUSIVEMODE:
	    MessageBox( hWnd,"CreateSurface DDERR_NOEXCLUSIVEMODE","Error",MB_OK);
	    break;
	    case DDERR_NOEMULATION:
	    MessageBox( hWnd,"CreateSurface DDERR_NOEMULATION","Error",MB_OK);
	    break;
	    case DDERR_NODIRECTDRAWHW:
	    MessageBox( hWnd,"CreateSurface DDERR_NODIRECTDRAWHW","Error",MB_OK);
	    break;
	    case DDERR_NOCOOPERATIVELEVELSET:
	    MessageBox( hWnd,"CreateSurface DDERR_NOCOOPERATIVELEVELSET","Error",MB_OK);
	    break;
	    case DDERR_NOALPHAHW:
	    MessageBox( hWnd,"CreateSurface DDERR_NOALPHAHW","Error",MB_OK);
	    break;
	    case DDERR_INVALIDPIXELFORMAT:
	    MessageBox( hWnd,"CreateSurface DDERR_INVALIDPIXELFORMAT","Error",MB_OK);
	    break;
	    case DDERR_INVALIDPARAMS:
	    MessageBox( hWnd,"CreateSurface DDERR_INVALIDPARAMS","Error",MB_OK);
	    break;
	    case DDERR_INVALIDOBJECT:
	    MessageBox( hWnd,"CreateSurface DDERR_INVALIDOBJECT","Error",MB_OK);
	    break;
	    case DDERR_INVALIDCAPS:
	    MessageBox( hWnd,"CreateSurface DDERR_INVALIDCAPS","Error",MB_OK);
	    break;
	    case DDERR_INCOMPATIBLEPRIMARY:
	    MessageBox( hWnd,"CreateSurface DDERR_INCOMPATIBLEPRIMARY","Error",MB_OK);
	    break;
	}
        return 1;
    } 

    if( fullscreen == 0 )
    {
	// Create a clipper to ensure that our drawing stays inside our window
	LPDIRECTDRAWCLIPPER lpClipper;
	ddrval = lpDD->CreateClipper( 0, &lpClipper, NULL );
	if( ddrval != DD_OK )
	{
	    MessageBox( hWnd, "Error in CreateClipper", "Error", MB_OK );
	    return 1;
	}

	// setting it to our hwnd gives the clipper the coordinates from our window
	ddrval = lpClipper->SetHWnd( 0, hWnd );
	if( ddrval != DD_OK )
	{
	    MessageBox( hWnd, "Error in SetHWnd", "Error", MB_OK );
	    return 1;
	}

	// attach the clipper to the primary surface
	ddrval = lpDDSPrimary->SetClipper( lpClipper );
	if( ddrval != DD_OK )
	{
	    MessageBox( hWnd, "Error in SetClipper", "Error", MB_OK );
	    return 1;
	}
    }

    /*ddscaps.dwCaps = DDSCAPS_BACKBUFFER; 
    ddrval = lpDDSPrimary->GetAttachedSurface( &ddscaps, &lpDDSBack );
    if( ddrval != DD_OK )
    { 
	MessageBox( hWnd, "GetAttachedSurface error", "Error", MB_OK );
	return 1;
    }*/

    //current_wm->lpDDSBack = lpDDSBack;
    current_wm->lpDDSPrimary = lpDDSPrimary;

    memset( &g_sd, 0, sizeof( DDSURFACEDESC ) );
    g_sd.dwSize = sizeof( g_sd );
    /*if( lpDDSPrimary )
    {
	lpDDSPrimary->Lock( 0, &g_sd, DDLOCK_SURFACEMEMORYPTR  | DDLOCK_WAIT, 0 );
	g_lPitch = g_sd.lPitch / COLORLEN;
	framebuffer = (COLORPTR)g_sd.lpSurface;
	g_primary_locked = 1;
    }*/
    g_primary_locked = 0;
    framebuffer = 0;

    return 0;
}

int dd_close()
{
    if( lpDD != NULL ) 
    { 
        if( lpDDSPrimary != NULL ) 
        { 
	    if( g_primary_locked )
	    {
		lpDDSPrimary->Unlock( g_sd.lpSurface );
		g_primary_locked = 0;
	    }
            lpDDSPrimary->Release(); 
            lpDDSPrimary = NULL; 
        } 
        lpDD->Release(); 
        lpDD = NULL; 
    }
    return 0;
}

#endif



