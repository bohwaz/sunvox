#ifndef __PALMTYPES_H__
#define __PALMTYPES_H__


/************************************************************
 * Environment configuration
 *************************************************************/
// <BuildDefaults.h> must be included here, rather than in <PalmOS.h>
// because they must be included for ALL builds.
// Not every build includes <PalmOS.h>.

// To override build options in a local component, include <BuildDefines.h>
// first, then define switches as need, and THEN include <PalmTypes.h>.
// This new mechanism supercedes the old "AppBuildRules.h" approach.
// More details available in <BuildDefaults.h>.
#include <BuildDefaults.h>


/************************************************************
 * Useful Macros 
 *************************************************************/
#if defined(__GNUC__) && defined(__UNIX__)		// used to be in <BuildRules.h>
	// Ensure that structure elements are 16-bit aligned
	// Other [host] development platforms may need this as well...
	#pragma pack(2)
#endif


/********************************************************************
 * Elementary data types
 ********************************************************************/
// Determine if we need to define our basic types or not
#ifndef	__TYPES__					// (Already defined in CW11)
#ifndef	__MACTYPES__				// (Already defined in CWPro3)
#define	__DEFINE_TYPES_	1
#endif
#endif


// Fixed size data types
typedef signed char		Int8;
typedef signed short		Int16;	
typedef signed long		Int32;

#if __DEFINE_TYPES_
typedef unsigned char	UInt8;
typedef unsigned short  UInt16;
typedef unsigned long   UInt32;
#endif


// Logical data types
#if __DEFINE_TYPES_						 
typedef unsigned char	Boolean;
#endif

typedef char				Char;
typedef UInt16				WChar;		// 'wide' int'l character type.

typedef UInt16				Err;

typedef UInt32				LocalID;		// local (card relative) chunk ID

typedef Int16 				Coord;		// screen/window coordinate


typedef void *				MemPtr;		// global pointer
typedef struct _opaque *MemHandle;	// global handle


#if __DEFINE_TYPES_						 
typedef Int32 (*ProcPtr)();
#endif


/************************************************************
 * Useful Macros 
 *************************************************************/

// The min() and max() macros which used to be defined here have been removed
// because they conflicted with facilities in C++.  If you need them, you
// should define them yourself, or see PalmUtils.h -- but please read the
// comments in that file before using it in your own projects.


#define OffsetOf(type, member)	((UInt32) &(((type *) 0)->member))




/************************************************************
 * Common constants
 *************************************************************/
#ifndef NULL
#define NULL	0
#endif	// NULL

#ifndef bitsInByte
#define bitsInByte	8
#endif	// bitsInByte


// Include the following typedefs if types.h wasn't read.
#if  __DEFINE_TYPES_
	#ifdef __MWERKS__
		#if !__option(bool)
			#ifndef true
				#define true			1
			#endif
			#ifndef false
				#define false			0
			#endif
		#endif
	#else
      #ifndef __cplusplus
		  #ifndef true
				enum {false, true};
		  #endif
	   #endif
	#endif
#endif /* __TYPES__ */




/************************************************************
 * Misc
 *************************************************************/

// Standardized infinite loop notation
// Use in place of while(1), while(true), while(!0), ...
#define loop_forever	for (;;)


// Include M68KHwr.h:
#if EMULATION_LEVEL == EMULATION_NONE
#if CPU_TYPE == CPU_68K
#include <M68KHwr.h>
//#pragma warn_no_side_effect on
#endif
#endif

/************************************************************
 * Metrowerks will substitute strlen and strcpy with inline
 * 68K assembly code.  Prevent this.
 *************************************************************/
 
#ifdef __MC68K__
#define _NO_FAST_STRING_INLINES_ 0
#endif


/************************************************************
 * Define whether or not we are direct linking, or going through
 *  traps.
 *
 * When eumulating we use directy linking.
 * When running under native mode, we use traps EXCEPT for the
 *   modules that actually install the routines into the trap table. 
 *   These modules will set the DIRECT_LINK define to 1
 *************************************************************/
#ifndef EMULATION_LEVEL
#error "This should not happen!"
#endif

#ifndef USE_TRAPS 
	#if EMULATION_LEVEL == EMULATION_NONE
		#define	USE_TRAPS 1						// use Pilot traps
	#else
		#define	USE_TRAPS 0						// direct link (Simulator)
	#endif
#endif


/********************************************************************
 * Palm OS System and Library trap macro definitions:
 ********************************************************************/

#define _DIRECT_CALL(table, vector)
#define _DIRECT_CALL_WITH_SELECTOR(table, vector, selector)
#define _DIRECT_CALL_WITH_16BIT_SELECTOR(table, vector, selector)

#ifndef _STRUCTURE_PICTURES

#define _SYSTEM_TABLE	15
#define _HAL_TABLE		15

#ifdef __GNUC__

 #if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95)

	#ifndef _Str
	#define _Str(X)  #X
	#endif

	#define _OS_CALL(table, vector)		/**/
	#define _OS_CALL_WITH_SELECTOR(table, vector, selector)	/**/

	#define _OS_CALL_WITH_16BIT_SELECTOR(table, vector, selector)	/**/

 #else

	#define _OS_CALL(table, vector)  /**/
	#define _OS_CALL_WITH_SELECTOR(table, vector, selector)
	#define _OS_CALL_WITH_16BIT_SELECTOR(table, vector, selector)

 #endif

#elif defined (__MWERKS__)	/* The equivalent in CodeWarrior syntax */

	#define _OS_CALL(table, vector) \
		= { 0x4E40 + table, vector }

	#define _OS_CALL_WITH_SELECTOR(table, vector, selector) \
		= { 0x7400 + selector, 0x4E40 + table, vector }

	#define _OS_CALL_WITH_16BIT_SELECTOR(table, vector, selector) \
		= { 0x3F3C, selector, 0x4E40 + table, vector, 0x544F }

#endif

#else

#define _SYSTEM_TABLE	"systrap"
#define _HAL_TABLE		"hal"

#define _OS_CALL(table, vector) /**/

#define _OS_CALL_WITH_SELECTOR(table, vector, selector) /**/

#define _OS_CALL_WITH_16BIT_SELECTOR(table, vector, selector) /**/

#endif


#if EMULATION_LEVEL != EMULATION_NONE

#define _HAL_API(kind)		_DIRECT##kind
#define _SYSTEM_API(kind)	_DIRECT##kind

#elif USE_TRAPS == 0

#define _HAL_API(kind)		_OS##kind
#define _SYSTEM_API(kind)	_DIRECT##kind

#else

#define _HAL_API(kind)		_OS##kind
#define _SYSTEM_API(kind)	_OS##kind

#endif


/************************************************************
 * Palm specific TRAP instruction numbers
 *************************************************************/
#define sysDbgBreakpointTrapNum		0		// For soft breakpoints		
#define sysDbgTrapNum					8		// For compiled breakpoints			
#define sysDispatchTrapNum				15		// Trap dispatcher


#define SYS_TRAP(trapNum)  /**/
	
#define ASM_SYS_TRAP(trapNum)	/**/
	

#endif //__PALMTYPES_H__
