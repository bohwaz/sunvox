/*
    image.cpp. Functions for working with images
    This file is part of the SunDog engine.
    Copyright (C) 2002 - 2009 Alex Zolotov <nightradio@gmail.com>
*/

#include "../../core/debug.h"
#include "../../utils/utils.h"
#include "../wmanager.h"

sundog_image* new_image( 
    int xsize, 
    int ysize, 
    void *src,
    int src_x,
    int src_y,
    int src_xsize,
    int src_ysize,
    int flags,
    window_manager *wm )
{
    sundog_image* img = 0;
    
    img = (sundog_image*)MEM_NEW( HEAP_DYNAMIC, sizeof( sundog_image ) );
    if( img )
    {
	img->xsize = xsize;
	img->ysize = ysize;
	img->flags = flags;
	img->backcolor = COLORMASK;
	img->color = COLORMASK;
	img->transp = 255;
#if defined(OPENGL) && !defined(FRAMEBUFFER)
	int b = 1;
	while( 1 )
	{
	    if( b >= xsize )
	    {
		img->int_xsize = b;
		break;
	    }
	    b <<= 1;
	}
	b = 1;
	while( 1 )
	{
	    if( b >= ysize )
	    {
		img->int_ysize = b;
		break;
	    }
	    b <<= 1;
	}
#else
	img->int_xsize = xsize;
	img->int_ysize = ysize;
#endif
	if( flags & IMAGE_STATIC_SOURCE )
	{
	    img->data = src;
	}
	else
	{
	    if( flags & IMAGE_NATIVE_RGB )
	    {
		img->data = MEM_NEW( HEAP_DYNAMIC, img->int_xsize * img->int_ysize * COLORLEN );
		mem_set( img->data, img->int_xsize * img->int_ysize * COLORLEN, 0 );
		COLORPTR data = (COLORPTR)img->data;
	        COLORPTR psrc = (COLORPTR)src;
	        for( int y = 0; y < ysize; y++ )
	        {
		    int ptr = y * img->int_xsize;
		    int src_ptr = ( y + src_y ) * src_xsize + src_x;
		    for( int x = 0; x < xsize; x++ )
		    {
		        data[ ptr ] = psrc[ src_ptr ];
			ptr++;
			src_ptr++;
		    }
		}
	    }
	    if( flags & IMAGE_ALPHA8 )
	    {
		img->data = MEM_NEW( HEAP_DYNAMIC, img->int_xsize * img->int_ysize );
		mem_set( img->data, img->int_xsize * img->int_ysize, 0 );
	        uchar* data = (uchar*)img->data;
	        uchar* csrc = (uchar*)src;
	        for( int y = 0; y < ysize; y++ )
	        {
		    int ptr = y * img->int_xsize;
		    int src_ptr = ( y + src_y ) * src_xsize + src_x;
	    	    for( int x = 0; x < xsize; x++ )
		    {
		        data[ ptr ] = csrc[ src_ptr ];
		        ptr++;
		        src_ptr++;
		    }
		}
	    }
	}
#if defined(OPENGL) && !defined(FRAMEBUFFER)
	if( img && img->data )
	{
	    unsigned int texture_id;
	    glGenTextures( 1, &texture_id );
	    img->gl_texture_id = texture_id;
	    glBindTexture( GL_TEXTURE_2D, texture_id );
	    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	    if( flags & IMAGE_NATIVE_RGB )
	    {
		glTexImage2D(
    		    GL_TEXTURE_2D,
    		    0,
    		    4,
    		    img->int_xsize, img->int_ysize,
    		    0,
    		    GL_RGBA,
    		    GL_UNSIGNED_BYTE,
    		    img->data );
	    }
	    else if( flags & IMAGE_ALPHA8 )
	    {
		glTexImage2D(
    		    GL_TEXTURE_2D,
    		    0,
    		    GL_ALPHA8,
    		    img->int_xsize, img->int_ysize,
    		    0,
    		    GL_ALPHA,
    		    GL_UNSIGNED_BYTE,
    		    img->data );
	    }
	}
#endif
    }
    
    return img;
}

void remove_image( sundog_image *img )
{
    if( img )
    {
#if defined(OPENGL) && !defined(FRAMEBUFFER)
	glDeleteTextures( 1, &img->gl_texture_id );
#endif
	if( img->data && !( img->flags & IMAGE_STATIC_SOURCE ) )
	{
	    mem_free( img->data );
	}
	mem_free( img );
    }
}
