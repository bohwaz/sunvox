/*
    wm_opengl.h. Platform-dependent module : OpenGL functions
    This file is part of the SunDog engine.
    Copyright (C) 2002 - 2009 Alex Zolotov <nightradio@gmail.com>
*/

#ifndef __WINMANAGER_OPENGL__
#define __WINMANAGER_OPENGL__

//#################################
//## DEVICE DEPENDENT FUNCTIONS: ##
//#################################

void gl_init( window_manager *wm )
{
    /*if( wm->gl_double_buffer )
    {
	glDrawBuffer( GL_BACK );
    }*/
    
    glMatrixMode( GL_MODELVIEW );

    glClearDepth( 1.0f );
    glClearStencil( 0 );
    glDepthFunc( GL_LESS );
    glDisable( GL_DEPTH_TEST );
    glEnable( GL_BLEND );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
}

void gl_resize( window_manager *wm )
{
    //Set viewport to cover the window:
    glViewport( 0, 0, wm->real_window_width, wm->real_window_height );
    glLoadIdentity();
    glOrtho( 0, wm->real_window_width, wm->real_window_height, 0, -1, 1 );
    glTranslatef( 0.375, 0.375, 0 );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
}

#if defined(OPENGL) && !defined(FRAMEBUFFER)

void device_draw_line( int x1, int y1, int x2, int y2, COLOR color, window_manager *wm )
{
    glColor4ub( red( color ), green( color ), blue( color ), 255 );

    glBegin( GL_LINES );

    glVertex2i( x1, y1 );
    glVertex2i( x2, y2 );

    glVertex2i( x1, y1 );
    glVertex2i( x1 + 1, y1 );

    glVertex2i( x2, y2 );
    glVertex2i( x2 + 1, y2 );

    glEnd();
}

void device_draw_box( int x, int y, int xsize, int ysize, COLOR color, window_manager *wm )
{
    glColor4ub( red( color ), green( color ), blue( color ), 255 );
    glBegin( GL_POLYGON );
    glVertex2i( x, y );
    glVertex2i( x + xsize, y );
    glVertex2i( x + xsize, y + ysize );
    glVertex2i( x, y + ysize );
    glEnd();
}

void device_draw_bitmap( 
    int dest_x, int dest_y, 
    int dest_xs, int dest_ys,
    int src_x, int src_y,
    sundog_image *data,
    window_manager *wm )
{
    float tx = (float)src_x / (float)data->int_xsize;
    float ty = (float)src_y / (float)data->int_ysize;
    float tx2 = (float)( src_x + dest_xs ) / (float)data->int_xsize;
    float ty2 = (float)( src_y + dest_ys ) / (float)data->int_ysize;
    uchar transp = data->transp;
    glEnable( GL_TEXTURE_2D );
    glColor4ub( red( data->color ), green( data->color ), blue( data->color ), transp );
    glBindTexture( GL_TEXTURE_2D, data->gl_texture_id );
    glBegin( GL_QUADS );
    glTexCoord2f( tx, ty );	glVertex2i( dest_x, dest_y );
    glTexCoord2f( tx2, ty );	glVertex2i( dest_x + dest_xs, dest_y );
    glTexCoord2f( tx2, ty2 );	glVertex2i( dest_x + dest_xs, dest_y + dest_ys );
    glTexCoord2f( tx, ty2 );	glVertex2i( dest_x, dest_y + dest_ys );
    glEnd();
    glDisable( GL_TEXTURE_2D );
}

void device_redraw_framebuffer( window_manager *wm ) 
{
#ifdef WIN
    glFlush();
    SwapBuffers( wm->hdc );
#endif
#ifdef UNIX
    XSync( wm->dpy, 0 );
    glFlush();
    glXSwapBuffers( wm->dpy, wm->win );
#endif
    /*if( wm->gl_double_buffer )
    {
        glReadBuffer( GL_BACK ); 
        glDrawBuffer( GL_FRONT );
	glCopyPixels( 0, 0, wm->screen_xsize, wm->screen_ysize, GL_COLOR );
        glDrawBuffer( GL_BACK );
    }*/
}	

#endif

//#################################
//#################################
//#################################

#endif
