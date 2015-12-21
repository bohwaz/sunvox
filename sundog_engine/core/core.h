#ifndef __CORE__
#define __CORE__

#define SUNDOG_VERSION "SunDog Engine v1.5"
#define SUNDOG_DATE    __DATE__

//#define COLOR8BITS	//8 bit color
//#define COLOR16BITS	//16 bit color
//#define COLOR32BITS	//32 bit color
//#define RGB565	//Default for 16bit color
//#define RGB555	//16bit color in Windows GDI
//#define GRAYSCALE	//Used with COLOR8BITS for grayscale palette
//#define UNIX		//OS = some UNIX veriation (Linux, FreeBSD...)
//#define FREEBSD	//OS = FreeBSD
//#define LINUX		//OS = Linux
//#define WIN		//OS = Windows
//#define WINCE		//OS = WindowsCE
//#define PALMOS	//OS = PalmOS
//#define OSX		//OS = OSX
//#define X11		//X11 support
//#define OPENGL	//OpenGL support
//#define GDI		//GDI support
//#define DIRECTDRAW	//Win32: DirectDraw; PalmOS: direct write to screen; WinCE: GAPI
//#define FRAMEBUFFER	//Use framebuffer
//#define SLOWMODE	//For some slow devices
//#define NOSTORAGE	//PalmOS: do not use the Storage Heap and MemSemaphores
//#define PALMLOWRES	//PalmOS: low-density screen
//#define ONLY44100	//Sampling frequencies other then 44100 not allowed
//#define NODEBUG	//No debug messages
//#define SMALLCACHE
//#define SMALLMEMORY
//#define ARCH_ARM
//#define ARCH_X86

//Variations:
//WIN32:  COLOR32BITS + WIN [ + OPENGL / DIRECTDRAW / GDI + RGB555 / FRAMEBUFFER ]
//LINUX:  COLOR32BITS + LINUX [ + OPENGL / DIRECTDRAW / X11 / FRAMEBUFFER ]
//PALMOS: COLOR8BITS + PALMOS [ + NOSTORAGE / PALMLOWRES / SLOWMODE / DIRECTDRAW ]
//WINCE:  COLOR16BITS + WINCE [ + GDI + RGB555 / FRAMEBUFFER / DIRECTDRAW ]

//Examples:
//WIN32:  ARCH_X86, WIN, COLOR32BITS, GDI
//LINUX:  ARCH_X86, LINUX, COLOR32BITS, X11
//PALMOS: ARCH_ARM, PALMOS, COLOR8BITS, PALMLOWRES
//PALMOS: ARCH_ARM, PALMOS, COLOR8BITS, PALMLOWRES, FRAMEBUFFER
//PALMOS: ARCH_ARM, PALMOS, COLOR8BITS, DIRECTDRAW
//WINCE:  ARCH_ARM, WINCE, COLOR16BITS, GDI, RGB555
//WINCE:  ARCH_ARM, WINCE, COLOR16BITS, DIRECTDRAW

typedef unsigned char   uchar;
typedef unsigned short  uint16;
typedef signed short    int16;
typedef unsigned long   ulong;
typedef signed long     slong;

typedef char		UTF8_CHAR;
typedef unsigned short	UTF16_CHAR;
typedef unsigned long	UTF32_CHAR;

#ifdef DIRECTDRAW
    #define FRAMEBUFFER
#endif

#if defined(OSX) || defined(LINUX) || defined(FREEBSD)
    #define UNIX
#endif

#ifdef PALMOS
    #ifdef DIRECTDRAW
	#undef PALMLOWRES
    #endif
    #define ONLY44100
    #define SMALLMEMORY
    #define SMALLCACHE
#else
    #define NONPALM
#endif

#ifdef WINCE
    #define ONLY44100
    #define SMALLCACHE
#endif

#endif

