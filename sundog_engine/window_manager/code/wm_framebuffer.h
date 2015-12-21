#define FB_OFFSET		    wm->fb_offset
#define FB_XPITCH		    wm->fb_xpitch
#define FB_YPITCH		    wm->fb_ypitch

#define LINE_PREC 20

void device_draw_line( int x1, int y1, int x2, int y2, COLOR color, window_manager *wm )
{
    if( !wm->screen_is_active ) return;

#ifdef WINCE
    if( wm->vd == VIDEODRIVER_GDI )
    {
	color = ( color & 31 ) | ( ( color & ~63 ) >> 1 ); //RGB565 to RGB555
    }
#endif

    int fb_xpitch = FB_XPITCH;
    int fb_ypitch = FB_YPITCH;

    framebuffer[ FB_OFFSET + y1 * fb_ypitch + x1 * fb_xpitch ] = color;
    framebuffer[ FB_OFFSET + y2 * fb_ypitch + x2 * fb_xpitch ] = color;

    int len_x = x2 - x1; if( len_x < 0 ) len_x = -len_x;
    int len_y = y2 - y1; if( len_y < 0 ) len_y = -len_y;
    if( len_x ) len_x++;
    if( len_y ) len_y++;
    int xdir; if( x2 - x1 >= 0 ) xdir = fb_xpitch; else xdir = -fb_xpitch;
    int ydir; if( y2 - y1 >= 0 ) ydir = fb_ypitch; else ydir = -fb_ypitch;
    COLORPTR ptr = framebuffer + FB_OFFSET + y1 * fb_ypitch + x1 * fb_xpitch;
    int delta;
    int v = 0, old_v = 0;
    if( len_x > len_y )
    {
	//Horisontal:
	if( len_x != 0 )
	    delta = ( len_y << LINE_PREC ) / len_x;
	else
	    delta = 0;
	for( int a = 0; a < len_x; a++ )
	{
	    *ptr = color;
	    old_v = v;
	    v += delta;
	    ptr += xdir;
	    if( ( old_v >> LINE_PREC ) != ( v >> LINE_PREC ) ) 
		ptr += ydir;
	}
    }
    else
    {
	//Vertical:
	if( len_y != 0 ) 
	    delta = ( len_x << LINE_PREC ) / len_y;
	else
	    delta = 0;
	for( int a = 0; a < len_y; a++ )
	{
	    *ptr = color;
	    old_v = v;
	    v += delta;
	    ptr += ydir;
	    if( ( old_v >> LINE_PREC ) != ( v >> LINE_PREC ) ) 
		ptr += xdir;
	}
    }
}

void device_draw_box( int x, int y, int xsize, int ysize, COLOR color, window_manager *wm )
{
    if( !wm->screen_is_active ) return;

#ifdef WINCE
    if( wm->vd == VIDEODRIVER_GDI )
    {
	color = ( color & 31 ) | ( ( color & ~63 ) >> 1 ); //RGB565 to RGB555
    }
#endif

    int fb_xpitch = FB_XPITCH;
    int fb_ypitch = FB_YPITCH;

    COLORPTR ptr = framebuffer + FB_OFFSET + y * fb_ypitch + x * fb_xpitch;
    if( fb_xpitch == 1 )
    {
#if defined(COLOR16BITS) || defined(COLOR8BITS)
    /*
	if( ( (ulong)ptr & 3 ) == 0 && ( x & 3 ) == 0 && ( xsize & 3 ) == 0 )
	{
	    ulong *lptr = (ulong*)ptr;
	    ulong lxsize = ( xsize * COLORLEN ) / 4;
	    for( int cy = 0; cy < ysize; cy++ )
	    {
		ulong *lsize = lptr + lxsize;
		while( lptr < lsize ) *lptr++ = color;
		ptr += fb_ypitch - xsize;
	    }
	}
	else
    */
#endif
	int add = fb_ypitch - xsize;
	for( int cy = 0; cy < ysize; cy++ )
	{
	    COLORPTR size = ptr + xsize;
	    while( ptr < size ) *ptr++ = color;
	    ptr += add;
	}
    }
    else
    {
	int add = fb_ypitch - ( xsize * fb_xpitch );
	for( int cy = 0; cy < ysize; cy++ )
	{
	    for( int cx = 0; cx < xsize; cx++ )
	    {
		*ptr = color;
		ptr += fb_xpitch;
	    }
	    ptr += add;
	}
    }
}

void device_draw_bitmap( 
    int dest_x, int dest_y, 
    int dest_xs, int dest_ys,
    int src_x, int src_y,
    sundog_image *img,
    window_manager *wm )
{
    if( !wm->screen_is_active ) return;

    int fb_xpitch = FB_XPITCH;
    int fb_ypitch = FB_YPITCH;

    COLORPTR data = (COLORPTR)img->data; //Source
    COLORPTR ptr = framebuffer + FB_OFFSET + dest_y * fb_ypitch + dest_x * fb_xpitch; //Destination

    //Add offset:
    int bp = src_y * img->xsize + src_x;
    data += bp;
    
    //Draw:
    int data_add = img->xsize - dest_xs;
#ifdef WINCE
    if( wm->vd == VIDEODRIVER_GDI )
    {
	COLOR color;
	if( fb_xpitch == 1 )
	{
	    int ptr_add = fb_ypitch - ( dest_xs * fb_xpitch );
	    for( int cy = 0; cy < dest_ys; cy++ )
	    {
		COLORPTR size = ptr + dest_xs;
		while( ptr < size ) 
		{
		    color = *data++;
		    color = ( color & 31 ) | ( ( color & ~63 ) >> 1 ); //RGB565 to RGB555
		    *ptr++ = color;
		}
		ptr += ptr_add;
		data += data_add;
	    }
	}
	else
	{
	    for( int cy = 0; cy < dest_ys; cy++ )
	    {
		int ptr_add = fb_ypitch - ( dest_xs * fb_xpitch );
		int cx = dest_xs + 1;
		while( --cx )
		{
		    color = *data;
		    color = ( color & 31 ) | ( ( color & ~63 ) >> 1 ); //RGB565 to RGB555
		    *ptr = color;
		    ptr += fb_xpitch;
		    data++;
		}
		ptr += ptr_add;
		data += data_add;
	    }
	}
    }
    else
#endif
    {
    
    if( fb_xpitch == 1 )
    {
	int ptr_add = fb_ypitch - ( dest_xs * fb_xpitch );
	for( int cy = 0; cy < dest_ys; cy++ )
	{
	    COLORPTR size = ptr + dest_xs;
	    while( ptr < size ) *ptr++ = *data++;
	    ptr += ptr_add;
	    data += data_add;
	}
    }
    else
    {
	for( int cy = 0; cy < dest_ys; cy++ )
	{
	    int ptr_add = fb_ypitch - ( dest_xs * fb_xpitch );
	    int cx = dest_xs + 1;
	    while( --cx )
	    {
		*ptr = *data;
		ptr += fb_xpitch;
		data++;
	    }
	    ptr += ptr_add;
	    data += data_add;
	}
    }
    
    }
}

void device_redraw_framebuffer( window_manager *wm )
{
#ifdef OPENGL
    //OpenGL: 
    /* draw polygon with the screen */
    glBindTexture( GL_TEXTURE_2D, 1 );
#ifdef COLOR8BITS
    glTexImage2D( 
	GL_TEXTURE_2D,
        0,
        3,
        wm->screen_xsize, wm->screen_ysize,
        0,
        GL_LUMINANCE,
        GL_UNSIGNED_BYTE,
        framebuffer );
#endif
#ifdef COLOR16BITS
    glTexImage2D( 
	GL_TEXTURE_2D,
        0,
        3,
        wm->screen_xsize, wm->screen_ysize,
        0,
        GL_LUMINANCE,
        GL_UNSIGNED_SHORT,
        framebuffer );
#endif
#ifdef COLOR32BITS
    glTexImage2D( 
	GL_TEXTURE_2D,
        0,
        4,
        wm->screen_xsize, wm->screen_ysize,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        framebuffer );
#endif
    glDisable( GL_DEPTH_TEST );
    glBegin( GL_POLYGON );
    glTexCoord2f( 0, 0 );glVertex3f( -1, 1, 0 );
    glTexCoord2f( 1, 0 );glVertex3f( 1, 1, 0 );
    glTexCoord2f( 1, 1 );glVertex3f( 1, -1, 0 );
    glTexCoord2f( 0, 1 );glVertex3f( -1, -1, 0 );
    glEnd();
    glEnable( GL_DEPTH_TEST );
#ifdef WIN
    SwapBuffers( wm->hdc );
#endif
#ifdef UNIX
    #if defined(X11) || defined(OPENGL)
	XSync( wm->dpy, 0 );
	glFlush();
	if( wm->doubleBuffer ) glXSwapBuffers( wm->dpy, wm->win );
    #endif
#endif //UNIX
#endif //OPENGL

#if defined(UNIX) && defined(DIRECTDRAW)
    if( wm->screen_lock_counter == 0 )
    {
	if( wm->screen_changed )
	    SDL_UpdateRect( wm->sdl_screen, 0, 0, 0, 0 );
	wm->screen_changed = 0;
    }
    else
    {
	dprint( "Can't redraw the framebuffer: screen locked!\n" );
    }
#endif

#if defined(WINCE) && defined(FRAMEBUFFER)
    //WinCE virtual framebuffer:
    if( wm->vd != VIDEODRIVER_GDI ) return;
    if( framebuffer == 0 ) return;
    if( wm->screen_changed == 0 ) return;
    wm->screen_changed = 0;
    int src_xsize = wm->screen_xsize;
    int src_ysize = wm->screen_ysize;
    if( wm->screen_flipped )
    {
	int ttt = src_xsize;
	src_xsize = src_ysize;
	src_ysize = ttt;
    }
    BITMAPINFO bi;
    memset( &bi, 0, sizeof( BITMAPINFO ) );
    bi.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
    bi.bmiHeader.biWidth = src_xsize;
    bi.bmiHeader.biHeight = -src_ysize;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = COLORBITS;
    bi.bmiHeader.biCompression = BI_RGB;
    StretchDIBits(
        wm->hdc,
        0, // Destination top left hand corner X Position
        0, // Destination top left hand corner Y Position
        GetSystemMetrics( SM_CXSCREEN ), // Destinations Width
        GetSystemMetrics( SM_CYSCREEN ), // Desitnations height
        0, // Source low left hand corner's X Position
        0, // Source low left hand corner's Y Position
        src_xsize,
        src_ysize,
        framebuffer, // Source's data
	&bi, // Bitmap Info
        DIB_RGB_COLORS,
	SRCCOPY );
#endif

#ifdef PALMOS
    //PalmOS:
#ifndef DIRECTDRAW
#ifdef PALMLOWRES
    WinDrawBitmap( (BitmapType*)g_screen_low, 0, 0 );
#else
    WinDrawBitmap( (BitmapType*)g_screen, 0, 0 );
#endif
#endif
#endif
}
