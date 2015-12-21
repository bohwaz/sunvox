#ifndef __PALM_ARM_FUNCTIONS__
#define __PALM_ARM_FUNCTIONS__

#include "PceNativeCall.h"
#include "../../core/debug.h"

// Main structure for the ARM-main function
struct ARM_INFO
{
    void *GLOBALS;
    void *GOT;
    void *FORMHANDLER;
    long ID;
    long *new_screen_size;
};

#define BSwap16(n) (	((((unsigned long) n) <<8 ) & 0xFF00) | \
			((((unsigned long) n) >>8 ) & 0x00FF) )

#define BSwap32(n) (	((((unsigned long) n) << 24 ) & 0xFF000000) | \
			((((unsigned long) n) << 8 ) & 0x00FF0000) | \
			((((unsigned long) n) >> 8 ) & 0x0000FF00) | \
			((((unsigned long) n) >> 24 ) & 0x000000FF) )

// local definition of the emulation state structure
struct EmulStateType 
{
	UInt32 instr;
	UInt32 regData[8];
	UInt32 regAddress[8];
	UInt32 regPC;
};
extern void *g_form_handler;
extern long *g_new_screen_size;
extern EmulStateType *emulStatePtr;
extern Call68KFuncType *call68KFuncPtr;
extern unsigned char args_stack[ 4 * 32 ];
extern unsigned char args_ptr;

#define CALL_INIT \
emulStatePtr = (EmulStateType*)emulStateP; \
call68KFuncPtr = call68KFuncP;

#define CALL args_ptr = 0;

#define P1( par ) \
args_stack[ args_ptr ] = (unsigned char)par; args_ptr++; \
args_stack[ args_ptr ] = 0; args_ptr++;

#define P2( par ) \
args_stack[ args_ptr ] = (unsigned char)((unsigned short)par>>8); args_ptr++; \
args_stack[ args_ptr ] = (unsigned char)((unsigned short)par&255); args_ptr++;

#define P4( par ) \
args_stack[ args_ptr ] = (unsigned char)((unsigned long)par>>24); args_ptr++; \
args_stack[ args_ptr ] = (unsigned char)((unsigned long)par>>16); args_ptr++; \
args_stack[ args_ptr ] = (unsigned char)((unsigned long)par>>8); args_ptr++; \
args_stack[ args_ptr ] = (unsigned char)((unsigned long)par&255); args_ptr++;

#define TRAP( trap ) \
(call68KFuncPtr) (emulStatePtr,PceNativeTrapNo(trap),args_stack,args_ptr); 

#define TRAPP( trap ) \
(call68KFuncPtr) (emulStatePtr,PceNativeTrapNo(trap),args_stack,args_ptr|kPceNativeWantA0); 

#define	MemPtrFree(p) MemChunkFree(p)

#endif //__PALM_ARM_FUNCTIONS__
