#ifndef __GX_H__
#define __GX_H__

struct GXDisplayProperties 
{
    DWORD cxWidth;
    DWORD cyHeight;         // notice lack of 'th' in the word height.
    long cbxPitch;          // number of bytes to move right one x pixel - can be negative.
    long cbyPitch;          // number of bytes to move down one y pixel - can be negative.
    long cBPP;              // # of bits in each pixel
    DWORD ffFormat;         // format flags.
};

struct GXKeyList 
{
    short vkUp;             // key for up
    POINT ptUp;             // x,y position of key/button.  Not on screen but in screen coordinates.
    short vkDown;
    POINT ptDown;
    short vkLeft;
    POINT ptLeft;
    short vkRight;
    POINT ptRight;
    short vkA;
    POINT ptA;
    short vkB;
    POINT ptB;
    short vkC;
    POINT ptC;
    short vkStart;
    POINT ptStart;
};

typedef int (*tGXOpenDisplay)( HWND hWnd, DWORD dwFlags );
typedef int (*tGXCloseDisplay)( void );
typedef void* (*tGXBeginDraw)( void );
typedef int (*tGXEndDraw)( void );
typedef int (*tGXOpenInput)( void );
typedef int (*tGXCloseInput)( void );
typedef GXDisplayProperties (*tGXGetDisplayProperties)( void );
typedef GXKeyList (*tGXGetDefaultKeys)( int iOptions );
typedef int (*tGXSuspend)( void );
typedef int (*tGXResume)( void );
typedef int (*tGXSetViewport)( DWORD dwTop, DWORD dwHeight, DWORD dwReserved1, DWORD dwReserved2 );
typedef BOOL (*tGXIsDisplayDRAMBuffer)( void );

#define GX_FULLSCREEN   0x01        // for OpenDisplay()
#define GX_NORMALKEYS   0x02
#define GX_LANDSCAPEKEYS    0x03

#ifndef kfLandscape
    #define kfLandscape 0x8         // Screen is rotated 270 degrees
    #define kfPalette   0x10        // Pixel values are indexes into a palette
    #define kfDirect    0x20        // Pixel values contain actual level information
    #define kfDirect555 0x40        // 5 bits each for red, green and blue values in a pixel.
    #define kfDirect565 0x80        // 5 red bits, 6 green bits and 5 blue bits per pixel
    #define kfDirect888 0x100       // 8 bits each for red, green and blue values in a pixel.
    #define kfDirect444 0x200       // 4 red, 4 green, 4 blue
    #define kfDirectInverted 0x400
#endif

extern tGXOpenDisplay GXOpenDisplay;
extern tGXCloseDisplay GXCloseDisplay;
extern tGXBeginDraw GXBeginDraw;
extern tGXEndDraw GXEndDraw;
extern tGXOpenInput GXOpenInput;
extern tGXCloseInput GXCloseInput;
extern tGXGetDisplayProperties GXGetDisplayProperties;
extern tGXGetDefaultKeys GXGetDefaultKeys;
extern tGXSuspend GXSuspend;
extern tGXResume GXResume;
extern tGXSetViewport GXSetViewport;

// for importing entries from dll's..
#define IMPORT( Handle, Variable, Type, Function, Store ) \
    Variable = GetProcAddress(Handle, TEXT(Function)); \
    Store = (Type)Variable;
		 
#endif
