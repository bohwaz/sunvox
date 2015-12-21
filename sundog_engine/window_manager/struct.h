#ifndef __WINDOWMANAGER_STRUCT__
#define __WINDOWMANAGER_STRUCT__

#include "../core/core.h"
#include "../memory/memory.h"
#include "../filesystem/v3nus_fs.h"
#include "../time/timemanager.h"
#include "regions/device.h" //Part of the Nano-X project (functions for working with regions)

#define EVENTS	32 //must be 2/4/8/16/32...
#define TIMERS	4

#define WM_INLINE inline

#if defined(WIN) || defined(WINCE)
    #include <windows.h>
#endif

#ifdef UNIX
    #include <pthread.h>
#endif

#ifdef OPENGL
    #include <GL/gl.h>
    #include <GL/glu.h>
#ifdef UNIX
    #include <GL/glx.h>
    #include <X11/keysym.h>
    #include <X11/XKBlib.h>
#endif
#endif

#ifdef X11
    #include <X11/Xlib.h>
    #include <X11/Xutil.h>
    #include <X11/keysym.h>
    #include <X11/XKBlib.h>
    #include <stdlib.h>
#endif

#if defined(DIRECTDRAW) && defined(UNIX)
    #include "SDL/SDL.h"
#endif

#if defined(DIRECTDRAW) && defined(WIN)
    #include "ddraw.h"
#endif

#ifdef COLOR8BITS
    #define COLOR     uchar
    #define COLORPTR  uchar*
    #define COLORLEN  (long)1
    #define COLORBITS 8
    #define COLORMASK 0xFF
    #define CLEARCOLOR 0
#endif
#ifdef COLOR16BITS
    #define COLOR     uint16
    #define COLORPTR  uint16*
    #define COLORLEN  (long)2
    #define COLORBITS 16
    #define COLORMASK 0xFFFF
    #define CLEARCOLOR 0
#endif
#ifdef COLOR32BITS
    #define COLOR     ulong
    #define COLORPTR  ulong*
    #define COLORLEN  (long)4
    #define COLORBITS 32
    #define COLORMASK 0xFFFFFFFF
    #define CLEARCOLOR 0xFF000000
#endif

#define CW 				COLORMASK
#define CB 				CLEARCOLOR

#define WIN_INIT_FLAG_SCALABLE	    	1
#define WIN_INIT_FLAG_NOBORDER	    	2
#define WIN_INIT_FLAG_FULL_CPU_USAGE	4
#define WIN_INIT_FLAG_SCREENFLIP	8

enum 
{
    EVT_NULL = 0,
    EVT_GETDATASIZE,
    EVT_AFTERCREATE,
    EVT_BEFORECLOSE,
    EVT_DRAW,
    EVT_FOCUS,
    EVT_UNFOCUS, //When user click on other window
    EVT_MOUSEDOUBLECLICK,
    EVT_MOUSEBUTTONDOWN,
    EVT_MOUSEBUTTONUP,
    EVT_BUTTONDOWN,
    EVT_BUTTONUP,
    EVT_MOUSEMOVE,
    EVT_SCREENRESIZE, //After main screen resize
    EVT_SCREENFOCUS,
    EVT_SCREENUNFOCUS,
    EVT_QUIT
};

//Event flags:
#define EVT_FLAG_AC 			1 /* send event to children */
#define EVT_FLAG_SHIFT 			2
#define EVT_FLAG_CTRL 			4
#define EVT_FLAG_ALT 			8
#define EVT_FLAG_MODS			( EVT_FLAG_SHIFT | EVT_FLAG_CTRL | EVT_FLAG_ALT )

//Mouse buttons:
#define MOUSE_BUTTON_LEFT 		1
#define MOUSE_BUTTON_MIDDLE 		2
#define MOUSE_BUTTON_RIGHT 		4
#define MOUSE_BUTTON_SCROLLUP 		8
#define MOUSE_BUTTON_SCROLLDOWN 	16

//Virtual key codes (ASCII):
//(scancode and unicode may be set for each virtual key)
// 00      : empty;
// 01 - 1F : control codes;
// 20 - 7E : standart ASCII symbols (all letters are small - from 61);
// 7F      : not defined;
// 80 - xx : additional key codes
#define KEY_BACKSPACE   		0x08
#define KEY_TAB         		0x09
#define KEY_ENTER       		0x0D
#define KEY_ESCAPE      		0x1B
#define KEY_SPACE       		0x20

//Additional key codes:
enum 
{
    KEY_F1 = 0x80,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
    KEY_F11,
    KEY_F12,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_INSERT,
    KEY_DELETE,
    KEY_HOME,
    KEY_END,
    KEY_PAGEUP,
    KEY_PAGEDOWN,
    KEY_CAPS,
    KEY_SHIFT,
    KEY_CTRL,
    KEY_ALT,
};

//Scroll flags:
#define SF_LEFT         		1
#define SF_RIGHT        		2
#define SF_UP           		4
#define SF_DOWN         		8
#define SF_STATICWIN    		16

#define WINDOWPTR			sundog_window*
#define WINDOW				sundog_window

#define DECOR_BORDER_SIZE		wm->decor_border_size
#define DECOR_HEADER_SIZE		wm->decor_header_size
#define DECOR_FLAG_CENTERED		1
#define DECOR_FLAG_CHECK_SIZE		2

#define SCROLLBAR_SIZE	    		wm->scrollbar_size
#define BUTTON_YSIZE( win, wm )		( char_y_size( win, wm ) + 8 + 4 )
#define BUTTON_XSIZE( win, wm )  	wm->button_xsize
#define INTERELEMENT_SPACE  		2

#define PUSHED_COLOR( orig ) 		blend( orig, wm->black, 44 )

#define DOUBLE_CLICK_PERIOD 		200

#define CWIN		    		30000
#define CX1		    		30001
#define CX2		    		30002
#define CY1		    		30003
#define CY2		    		30004
#define CSUB		    		30005
#define CADD		    		30006
#define CPERC		    		30007
#define CBACKVAL0	    		30008
#define CBACKVAL1	    		30009
#define CMAXVAL		    		30010
#define CMINVAL		    		30011
#define CPUTR0		    		30012
#define CGETR0		    		30013
#define CEND		    		30014

#define IMAGE_NATIVE_RGB		1
#define IMAGE_ALPHA8			2
#define IMAGE_STATIC_SOURCE		4

struct sundog_image
{
    int			xsize;
    int			ysize;
    int			int_xsize; //internal xsize
    int			int_ysize; //internal ysize
    void		*data;
    COLOR		color;
    COLOR		backcolor;
    uchar		transp; //0..255
    unsigned int	gl_texture_id;
    int			flags;
};

#define WIN_FLAG_ALWAYS_INVISIBLE	1
#define WIN_FLAG_ALWAYS_ON_TOP		2
#define WIN_FLAG_ALWAYS_UNFOCUSED	4
#define WIN_FLAG_TRASH			8

struct sundog_window
{
    int		    	visible;
    uint16	    	flags;
    uint16	    	id;
    const UTF8_CHAR    	*name; //window name
    int16	    	x, y; //x/y (relative)
    int16	    	screen_x, screen_y; //real x/y
    int16	    	xsize, ysize; //window size
    COLOR	    	color;
    int	    		font;
    MWCLIPREGION    	*reg;
    WINDOWPTR	    	parent;
    WINDOWPTR	    	*childs;
    int		    	childs_num;
    int		    	(*win_handler)( void*, void* ); //window handler: (*event, *window_manager)
    void	    	*data;

    int		    	*x1com; //Controllers of window coordinates (special commands)
    int		    	*y1com;
    int		    	*x2com;
    int		    	*y2com;
    int16	    	controllers_defined;
    int16	    	controllers_calculated;

    int		    	(*action_handler)( void*, void*, void* );
    int		    	action_result;
    void	    	*handler_data;

    ticks_t	    	click_time;
};

//SunDog event handling:
// EVENT 
//   |
// user_event_handler()
//   | (if not handled OR all_childs)
// window 
//   | (if not handled OR all_childs)
// window childs
//   | (if not handled)
// handler_of_unhandled_events()

struct sundog_event
{
    uint16          	type; //event type
    uint16	    	flags;
    ticks_t	    	time;
    WINDOWPTR	    	win;
    int16           	x;
    int16           	y;
    uint16	    	key; //virtual key code: standart ASCII (0..127) and additional (see KEY_xxx defines)
    uint16	    	scancode; //device dependent
    uint16	    	pressure; //key pressure (0..1024)
    uint16	    	reserved;
    UTF32_CHAR	    	unicode; //unicode if possible, or zero
};

struct window_manager;
struct sundog_timer;

struct sundog_timer
{
    void            	(*handler)( void*, sundog_timer*, window_manager* );
    void	    	*data;
    ticks_t	    	deadline;
    ticks_t	    	delay;
};

struct window_manager
{
    int		    	flags;
    
    long            	events_count; //number of events to execute
    long	    	current_event_num;
    sundog_event    	events[ EVENTS ];
    sundog_mutex    	events_mutex;

    WINDOWPTR	    	*trash;

    ulong	    	window_counter; //for ID calculation

    WINDOWPTR	    	root_win;

    int			creg0; //temp register for window's controller calculation

    sundog_timer    	timers[ TIMERS ];
    int		    	timers_num;

    int             	screen_xsize;
    int             	screen_ysize;
    int			screen_flipped;

    int		    	screen_lock_counter;
    int	            	screen_is_active;
    int			screen_changed;
    
    int			default_font;

    COLOR	    	colors[ 16 ]; //System palette
    COLOR	    	white;
    COLOR	    	black;
    COLOR	    	yellow;
    COLOR	    	green;
    COLOR	    	red;
    COLOR	    	dialog_color;
    COLOR	    	decorator_color;
    COLOR	    	decorator_border;
    COLOR	    	button_color;
    COLOR	    	menu_color;
    COLOR	    	selection_color;
    COLOR	    	text_background;
    COLOR	    	list_background;
    COLOR	    	scroll_color;
    COLOR	    	scroll_background_color;

    //Standart window decoration properties: =============================================
    int		    	decor_border_size;
    int		    	decor_header_size;
    int		    	scrollbar_size;
    int		    	button_xsize;
    int		    	gui_size_increment;
    //====================================================================================
    					  
    long            	wm_initialized;
    
    long	    	exit_request;
    long	    	exit_code;

    //WBD (Window Buffer Draw): ==========================================================
    WINDOWPTR		cur_window;
    COLOR           	*g_screen; //graphics screen pointer
    COLOR           	cur_font_color;
    bool            	cur_font_draw_bgcolor; 
    COLOR           	cur_font_bgcolor;
    int		    	cur_font_zoom; //256 - normal; 512 - x2
    int		    	cur_transparency;
    //====================================================================================

    int		    	pen_x;
    int		    	pen_y;
    WINDOWPTR	    	focus_win;
    WINDOWPTR	    	prev_focus_win;
    uint16	    	focus_win_id;
    uint16	    	prev_focus_win_id;
    WINDOWPTR	    	last_unfocused_window;

    WINDOWPTR	    	handler_of_unhandled_events;

    int             	argc; //minimum - 1
    UTF8_CHAR       	**argv; // "program name"; "arg1"; "arg2"; ...

    //DEVICE DEPENDENT PART:

    int		    	fb_xpitch; //framebuffer xpitch
    int		    	fb_ypitch; //...
    int		    	fb_offset; //...
#ifdef OPENGL
    int	       		gl_double_buffer;
#ifdef UNIX
#endif //UNIX
    int		    	real_window_width;
    int		    	real_window_height;
#endif //OPENGL
#if defined(DIRECTDRAW) && defined(WIN)
    LPDIRECTDRAWSURFACE	lpDDSPrimary; //DirectDraw primary surface (for win32)
    LPDIRECTDRAWSURFACE	lpDDSBack; //DirectDraw back surface (for win32)
#endif
#ifdef GDI
    ulong           	gdi_bitmap_info[ 512 ]; //GDI bitmap info (for win32)
#endif
#ifdef WIN
    HDC	            	hdc; //graphics content handler
    HWND	    	hwnd;
    HINSTANCE       	hCurrentInst;
    HINSTANCE       	hPreviousInst;
    LPSTR           	lpszCmdLine;
    int             	nCmdShow;
#endif
#ifdef WINCE
    int 		hires;
    int			vd; //video-driver
    char		rfb[ 32 ]; //raw framebuffer info
    HDC             	hdc; //graphics content handler
    HINSTANCE       	hCurrentInst;
    HINSTANCE       	hPreviousInst;
    LPWSTR          	lpszCmdLine;
    int             	nCmdShow;
    int		    	os_version;
    int		    	gx_suspended;
#endif
#ifdef UNIX
    pthread_mutex_t 	sdl_lock_mutex;
#ifdef DIRECTDRAW
    SDL_Surface     	*sdl_screen;
#endif
#ifdef X11
    Display         	*dpy;
    Colormap	    	cmap;
    Window          	win;
    Visual          	*win_visual;
    GC              	win_gc;
    XImage          	*win_img;
    XImage          	*win_img_back_pattern;
    char            	*win_img_back_pattern_data;
    long	    	win_img_depth;
    char            	*win_buffer;
    int	            	win_depth;
#endif //X11
#endif //UNIX
};

#endif
