#include <PalmOS.h>
#include "VFSMgr.h"
#include "PceNativeCall.h"
#include "palm_functions.h"

// local definition of the emulation state structure
void *g_form_handler;
long *g_new_screen_size;
EmulStateType *emulStatePtr;
Call68KFuncType *call68KFuncPtr;
unsigned char args_stack[ 4 * 32 ];
unsigned char args_ptr = 0;

// =================
// PalmOS functions:
// =================

Char *StrCat( Char *dst, const Char *src )
{
    CALL P4(dst) P4(src) return (Char*)TRAPP(sysTrapStrCat)
}

UInt16 StrLen( const Char *src )
{
    CALL P4(src) return (UInt16)TRAP(sysTrapStrLen)
}

Int16 StrCompare( const Char *s1, const Char *s2 )
{
    CALL P4(s1) P4(s2) return (UInt16)TRAP(sysTrapStrCompare)
}

Err MemSemaphoreReserve( Boolean writeAccess )
{
    CALL P1(writeAccess) return (Err)TRAP(sysTrapMemSemaphoreReserve)
}

Err MemSemaphoreRelease( Boolean writeAccess )
{
    CALL P1(writeAccess) return (Err)TRAP(sysTrapMemSemaphoreRelease)
}

UInt16 MemHeapID( UInt16 cardNo, UInt16 heapIndex )
{
    CALL P2(cardNo) P2(heapIndex) return (UInt16)TRAP(sysTrapMemHeapID)
}

Err MemMove( void *dstP, const void *sP, Int32 numBytes )
{
    CALL P4(dstP) P4(sP) P4(numBytes) return (Err)TRAP(sysTrapMemMove)
}

Err MemSet( void *dstP, Int32 numBytes, UInt8 value )
{
    CALL P4(dstP) P4(numBytes) P1(value) return (Err)TRAP(sysTrapMemSet)
}

Int16 MemCmp ( const void* s1, const void* s2, Int32 numBytes )
{
    CALL P4(s1) P4(s2) P4(numBytes) return (Int16)TRAP(sysTrapMemCmp)
}

Err MemChunkFree( MemPtr chunkDataP )
{
    CALL P4(chunkDataP) return (Err)TRAP(sysTrapMemChunkFree)
}

MemPtr MemChunkNew( UInt16 heapID, UInt32 size, UInt16 attr )
{
    CALL P2(heapID) P4(size) P2(attr) return (MemPtr)TRAPP(sysTrapMemChunkNew)
}

MemHandle MemHandleNew( UInt32 size )
{
    CALL P4(size) return (MemHandle)TRAPP(sysTrapMemHandleNew)
}

Err MemHandleFree( MemHandle h )
{
    CALL P4(h) return (Err)TRAP(sysTrapMemHandleFree)
}

UInt16 MemHandleLockCount( MemHandle h )
{
    CALL P4(h) return (UInt16)TRAP(sysTrapMemHandleLockCount)
}

MemPtr MemHandleLock( MemHandle h )
{
    CALL P4(h) return (MemPtr)TRAPP(sysTrapMemHandleLock)
}

Err	MemHandleUnlock( MemHandle h )
{
    CALL P4(h) return (Err)TRAP(sysTrapMemHandleUnlock)
}

UInt32 MemHandleSize( MemHandle h )
{
    CALL P4(h) return (UInt32)TRAP(sysTrapMemHandleSize)
}

MemPtr MemPtrNew( UInt32 size ) 
{
    CALL P4(size) return (MemPtr)TRAPP(sysTrapMemPtrNew)
}

UInt16 MemPtrHeapID( MemPtr p )
{
    CALL P4(p) return (UInt16)TRAP(sysTrapMemPtrHeapID)
}

// =================
// VFS :
// =================

Err VFSVolumeEnumerate( UInt16 *volRefNumP, UInt32 *volIteratorP )
{
    *volIteratorP = BSwap32( *volIteratorP ); // <->
    CALL P4(volRefNumP) P4(volIteratorP) 
    emulStatePtr->regData[2] = vfsTrapVolumeEnumerate;
    Err retval = (Err)TRAP(0xA348 & (0x00000FFF))
    if( volIteratorP ) *volIteratorP = BSwap32( *volIteratorP ); // <->
    if( volRefNumP ) *volRefNumP = BSwap16( *volRefNumP ); // <-
    return retval;
}

Err VFSFileOpen( UInt16 volRefNum, const Char *pathNameP,UInt16 openMode, FileRef *fileRefP )
{
    CALL P2(volRefNum) P4(pathNameP) P2(openMode) P4(fileRefP)
    emulStatePtr->regData[2] = vfsTrapFileOpen;
    Err retval = (Err)TRAP(0xA348 & (0x00000FFF))
    if( fileRefP ) *fileRefP = (FileRef)BSwap32( *fileRefP ); // <-
    return retval;
}

Err VFSFileClose( FileRef fileRef )
{
    CALL P4(fileRef)
    emulStatePtr->regData[2] = vfsTrapFileClose;
    return (Err)TRAP(0xA348 & (0x00000FFF))
}

Err VFSFileCreate( UInt16 volRefNum, const Char *pathNameP )
{
    CALL P2(volRefNum) P4(pathNameP)
    emulStatePtr->regData[2] = vfsTrapFileCreate;
    return (Err)TRAP(0xA348 & (0x00000FFF))
}

Err VFSFileReadData( FileRef fileRef, UInt32 numBytes, void *bufBaseP, UInt32 offset, UInt32 *numBytesReadP )
{
    CALL P4(fileRef) P4(numBytes) P4(bufBaseP) P4(offset) P4(numBytesReadP)
    emulStatePtr->regData[2] = vfsTrapFileReadData;
    Err retval = (Err)TRAP(0xA348 & (0x00000FFF))
    if( numBytesReadP ) *numBytesReadP = BSwap32( *numBytesReadP ); // <-
    return retval;
}

Err VFSFileRead( FileRef fileRef, UInt32 numBytes, void *bufP, UInt32 *numBytesReadP )
{
    CALL P4(fileRef) P4(numBytes) P4(bufP) P4(numBytesReadP)
    emulStatePtr->regData[2] = vfsTrapFileRead;
    Err retval = (Err)TRAP(0xA348 & (0x00000FFF))
    if( numBytesReadP ) *numBytesReadP = BSwap32( *numBytesReadP ); // <-
    return retval;
}

Err VFSFileWrite( FileRef fileRef, UInt32 numBytes, const void *dataP, UInt32 *numBytesWrittenP )
{
    CALL P4(fileRef) P4(numBytes) P4(dataP) P4(numBytesWrittenP)
    emulStatePtr->regData[2] = vfsTrapFileWrite;
    Err retval = (Err)TRAP(0xA348 & (0x00000FFF))
    if( numBytesWrittenP ) *numBytesWrittenP = BSwap32( *numBytesWrittenP ); // <-
    return retval;
}

Err VFSFileDelete( UInt16 volRefNum, const Char *pathNameP )
{
    CALL P2(volRefNum) P4(pathNameP)
    emulStatePtr->regData[2] = vfsTrapFileDelete;
    return (Err)TRAP(0xA348 & (0x00000FFF))
}

Err VFSFileRename( UInt16 volRefNum, const Char *pathNameP, const Char *newNameP )
{
    CALL P2(volRefNum) P4(pathNameP) P4(newNameP)
    emulStatePtr->regData[2] = vfsTrapFileRename;
    return (Err)TRAP(0xA348 & (0x00000FFF))
}

Err VFSFileSeek( FileRef fileRef, FileOrigin origin, Int32 offset )
{
    CALL P4(fileRef) P2(origin) P4(offset)
    emulStatePtr->regData[2] = vfsTrapFileSeek;
    return (Err)TRAP(0xA348 & (0x00000FFF))
}

Err VFSFileEOF( FileRef fileRef )
{
    CALL P4(fileRef)
    emulStatePtr->regData[2] = vfsTrapFileEOF;
    return (Err)TRAP(0xA348 & (0x00000FFF))
}

Err VFSFileTell( FileRef fileRef, UInt32 *filePosP )
{
    CALL P4(fileRef) P4(filePosP)
    emulStatePtr->regData[2] = vfsTrapFileTell;
    Err retval = (Err)TRAP(0xA348 & (0x00000FFF))
    if( filePosP ) *filePosP = BSwap32( *filePosP ); // <-
    return retval;
}

Err VFSFileSize( FileRef fileRef, UInt32 *fileSizeP )
{
    CALL P4(fileRef) P4(fileSizeP)
    emulStatePtr->regData[2] = vfsTrapFileSize;
    Err retval = (Err)TRAP(0xA348 & (0x00000FFF))
    if( fileSizeP ) *fileSizeP = BSwap32( *fileSizeP ); // <-
    return retval;
}

Err VFSDirEntryEnumerate( FileRef dirRef, UInt32 *dirEntryIteratorP, FileInfoType *infoP )
{
    *dirEntryIteratorP = BSwap32( *dirEntryIteratorP ); // <->
    infoP->attributes = BSwap32( infoP->attributes ); // <->
    infoP->nameP = (Char*)BSwap32( infoP->nameP );    // <->
    infoP->nameBufLen = BSwap16( infoP->nameBufLen ); // <->
    CALL P4(dirRef) P4(dirEntryIteratorP) P4(infoP)
    emulStatePtr->regData[2] = vfsTrapDirEntryEnumerate;
    Err retval = (Err)TRAP(0xA348 & (0x00000FFF))
    *dirEntryIteratorP = BSwap32( *dirEntryIteratorP ); // <->
    infoP->attributes = BSwap32( infoP->attributes ); // <->
    infoP->nameP = (Char*)BSwap32( infoP->nameP );    // <->
    infoP->nameBufLen = BSwap16( infoP->nameBufLen ); // <->
    return retval;
}

// =================
// DATABASES :
// =================

Err DmDatabaseInfo(
    UInt16 cardNo, LocalID dbID, Char *nameP,
    UInt16 *attributesP, UInt16 *versionP, UInt32 *crDateP,
    UInt32 *	modDateP, UInt32 *bckUpDateP,
    UInt32 *	modNumP, LocalID *appInfoIDP,
    LocalID *sortInfoIDP, UInt32 *typeP,
    UInt32 *creatorP )
{
    CALL P2(cardNo) P4(dbID) P4(nameP) P4(attributesP) P4(versionP)
    P4(crDateP) P4(modDateP) P4(bckUpDateP) P4(modNumP) P4(appInfoIDP)
    P4(sortInfoIDP) P4(typeP) P4(creatorP)
    Err retval = (Err)TRAP(sysTrapDmDatabaseInfo)
    if( attributesP ) *attributesP = BSwap16( *attributesP );
    if( versionP ) *versionP = BSwap16( *versionP );
    if( crDateP ) *crDateP = BSwap32( *crDateP );
    if( modNumP ) *modNumP = BSwap32( *modNumP );
    if( appInfoIDP ) *appInfoIDP = (LocalID)BSwap32( *appInfoIDP );
    if( sortInfoIDP ) *sortInfoIDP = (LocalID)BSwap32( *sortInfoIDP );
    if( typeP ) *typeP = BSwap32( *typeP );
    if( creatorP ) *creatorP = BSwap32( *creatorP );
    return retval;
}

MemHandle DmGetResource( DmResType type, DmResID resID )
{
    CALL P4(type) P2(resID)
    return (MemHandle)TRAPP(sysTrapDmGetResource)
}

Err DmRecordInfo( 
    DmOpenRef dbP, UInt16 index,
    UInt16 *attrP, UInt32 *uniqueIDP, LocalID *chunkIDP )
{
    CALL P4(dbP) P2(index) P4(attrP) P4(uniqueIDP) P4(chunkIDP)
    Err retval = (Err)TRAP(sysTrapDmRecordInfo)
    if( attrP )     *attrP = BSwap16( *attrP );                // <-
    if( uniqueIDP ) *uniqueIDP = BSwap32( *uniqueIDP );        // <-
    if( chunkIDP )  *chunkIDP = (LocalID)BSwap32( *chunkIDP ); // <-
    return retval;
}

DmOpenRef DmOpenDatabase( UInt16 cardNo, LocalID dbID, UInt16 mode )
{
    CALL P2(cardNo) P4(dbID) P2(mode)
    return (DmOpenRef)TRAPP(sysTrapDmOpenDatabase)
}

Err DmCreateDatabase( UInt16 cardNo, const Char *nameP, UInt32 creator, UInt32 type, Boolean resDB )
{
    CALL P2(cardNo) P4(nameP) P4(creator) P4(type) P1(resDB)
    return (Err)TRAP(sysTrapDmCreateDatabase)
}

Err DmDeleteDatabase( UInt16 cardNo, LocalID dbID )
{
    CALL P2(cardNo) P4(dbID)
    return (Err)TRAP(sysTrapDmDeleteDatabase)
}

LocalID DmFindDatabase( UInt16 cardNo, const Char *nameP )
{
    CALL P2(cardNo) P4(nameP)
    return (LocalID)TRAP(sysTrapDmFindDatabase)
}

Err DmGetNextDatabaseByTypeCreator(
    Boolean newSearch, DmSearchStatePtr stateInfoP,
    UInt32	type, UInt32 creator, Boolean onlyLatestVers, 
    UInt16 *cardNoP, LocalID *dbIDP )
{
    CALL P1(newSearch) P4(stateInfoP) P4(type) P4(creator) P1(onlyLatestVers)
    P4(cardNoP) P4(dbIDP)
    Err retval = (Err)TRAP(sysTrapDmGetNextDatabaseByTypeCreator)
    if( cardNoP ) *cardNoP = BSwap16( *cardNoP );      // <-
    if( dbIDP )   *dbIDP = (LocalID)BSwap32( *dbIDP ); // <-
    return retval;
}

Err DmCloseDatabase( DmOpenRef dbP ) 
{
    CALL P4(dbP)
    return (Err)TRAP(sysTrapDmCloseDatabase)
}

UInt16 DmNumRecords( DmOpenRef dbP )
{
    CALL P4(dbP)
    return (UInt16)TRAP(sysTrapDmNumRecords)
}

MemHandle DmNewRecord( DmOpenRef dbP, UInt16 *atP, UInt32 size )
{
    *atP = BSwap16( *atP ); // <->
    CALL P4(dbP) P4(atP) P4(size)
    MemHandle retval = (MemHandle)TRAPP(sysTrapDmNewRecord)
    *atP = BSwap16( *atP ); // <->
    return retval;
}

MemHandle DmGetRecord( DmOpenRef dbP, UInt16 index )
{
    CALL P4(dbP) P2(index)
    return (MemHandle)TRAPP(sysTrapDmGetRecord)
}

MemHandle DmResizeRecord( DmOpenRef dbP, UInt16 index, UInt32 newSize )
{
    CALL P4(dbP) P2(index) P4(newSize)
    return (MemHandle)TRAPP(sysTrapDmResizeRecord)
}

Err DmReleaseRecord( DmOpenRef dbP, UInt16 index, Boolean dirty )
{
    CALL P4(dbP) P2(index) P1(dirty)
    return (Err)TRAP(sysTrapDmReleaseRecord)
}

Err DmWrite( void *recordP, UInt32 offset, const void *srcP, UInt32 bytes )
{
    CALL P4(recordP) P4(offset) P4(srcP) P4(bytes)
    return (Err)TRAP(sysTrapDmWrite)
}

Err DmWriteCheck( void *recordP, UInt32 offset, UInt32 bytes )
{
    CALL P4(recordP) P4(offset) P4(bytes)
    return (Err)TRAP(sysTrapDmWriteCheck)
}

// =================
// SOUND :
// =================

Err SndStreamCreate(
    SndStreamRef *channel, 
    SndStreamMode mode,
    UInt32 samplerate,
    SndSampleType type,
    SndStreamWidth width,
    SndStreamBufferCallback func,
    void *userdata,
    UInt32 buffsize,
    Boolean armNative )
{
    CALL P4(channel) P1(mode) P4(samplerate) P2(type) P1(width) P4(func) P4(userdata) P4(buffsize) P1(armNative)
    Err retval = (Err)TRAP(sysTrapSndStreamCreate)
    *channel = (SndStreamRef)BSwap32( *channel ); // <-
    return retval;
}

Err SndStreamDelete( SndStreamRef channel )
{
    CALL P4(channel)
    return (Err)TRAP(sysTrapSndStreamDelete)
}

Err SndStreamStart( SndStreamRef channel )
{
    CALL P4(channel)
    return (Err)TRAP(sysTrapSndStreamStart)
}

Err SndStreamPause( SndStreamRef channel, Boolean pause )
{
    CALL P4(channel) P1(pause)
    return (Err)TRAP(sysTrapSndStreamPause)
}

Err SndStreamStop( SndStreamRef channel )
{
    CALL P4(channel)
    return (Err)TRAP(sysTrapSndStreamStop)
}

Err SndStreamSetVolume( SndStreamRef channel, Int32 volume )
{
    CALL P4(channel) P4(volume)
    return (Err)TRAP(sysTrapSndStreamSetVolume)
}



// WINDOWS :



void WinDrawChars( const Char *chars, Int16 len, Coord x, Coord y )
{
    CALL P4(chars) P2(len) P2(x) P2(y) 
    TRAP(sysTrapWinDrawChars)
}

WinHandle WinGetDisplayWindow( void )
{
    CALL
    return (WinHandle)TRAPP(sysTrapWinGetDisplayWindow)
}

BitmapType *WinGetBitmap( WinHandle winHandle )
{
    CALL P4(winHandle)
    return (BitmapType*)TRAPP(sysTrapWinGetBitmap);
}

void WinGetBounds( WinHandle winH, RectangleType *rP )
{
    CALL P4(winH) P4(rP)
    TRAP(sysTrapWinGetBounds)
    rP->topLeft.x = BSwap16( rP->topLeft.x ); // <-
    rP->topLeft.y = BSwap16( rP->topLeft.y ); // <-
    rP->extent.x = BSwap16( rP->extent.x );   // <-
    rP->extent.y = BSwap16( rP->extent.y );   // <-
}

void WinSetBounds( WinHandle winHandle, const RectangleType *rP )
{
    RectangleType *r = (RectangleType*)rP;
    r->topLeft.x = BSwap16( r->topLeft.x ); // ->
    r->topLeft.y = BSwap16( r->topLeft.y ); // ->
    r->extent.x = BSwap16( r->extent.x );   // ->
    r->extent.y = BSwap16( r->extent.y );   // ->
    CALL P4(winHandle) P4(rP)
    TRAP(sysTrapWinSetBounds)
}

void WinSetClip( const RectangleType *rP )
{
    RectangleType *r = (RectangleType*)rP;
    r->topLeft.x = BSwap16( r->topLeft.x ); // ->
    r->topLeft.y = BSwap16( r->topLeft.y ); // ->
    r->extent.x = BSwap16( r->extent.x );   // ->
    r->extent.y = BSwap16( r->extent.y );   // ->
    CALL P4(rP)
    TRAP(sysTrapWinSetClip)
}

void WinDrawRectangle( const RectangleType *rP, UInt16 cornerDiam )
{
    RectangleType *r = (RectangleType*)rP;
    r->topLeft.x = BSwap16( r->topLeft.x ); // ->
    r->topLeft.y = BSwap16( r->topLeft.y ); // ->
    r->extent.x = BSwap16( r->extent.x );   // ->
    r->extent.y = BSwap16( r->extent.y );   // ->
    CALL P4(rP) P2(cornerDiam)
    TRAP(sysTrapWinDrawRectangle)
}

void WinDrawLine( Coord x1, Coord y1, Coord x2, Coord y2 )
{
    CALL P2(x1) P2(y1) P2(x2) P2(y2)
    TRAP(sysTrapWinDrawLine)
}

void WinDrawPixel( Coord x, Coord y )
{
    CALL P2(x) P2(y)
    TRAP(sysTrapWinDrawPixel)
}

Err WinGetPixelRGB( Coord x, Coord y, RGBColorType *rgbP )
{
    CALL P2(x) P2(y) P4(rgbP)
    TRAP(sysTrapWinGetPixelRGB)
}

void WinSetForeColorRGB( const RGBColorType *newRgbP, RGBColorType *prevRgbP )
{
    CALL P4(newRgbP) P4(prevRgbP)
    TRAP(sysTrapWinSetForeColorRGB)
}

UInt16 WinSetCoordinateSystem( UInt16 coordSys )
{
    CALL P2(coordSys)
    emulStatePtr->regData[2] = HDSelectorWinSetCoordinateSystem;
#ifdef sysTrapSysHighDensitySelector
    return (UInt16)TRAP(sysTrapSysHighDensitySelector & (0x00000FFF))
#else
    return (UInt16)TRAP(sysTrapHighDensityDispatch & (0x00000FFF))
#endif
}

#define WinSetWindowBounds(winH, rP)		(WinSetBounds((winH), (rP)))

Err WinScreenGetAttribute( WinScreenAttrType selector, UInt32* attrP )
{
    CALL P1(selector) P4(attrP)
    emulStatePtr->regData[2] = HDSelectorWinScreenGetAttribute;
    Err retval;
#ifdef sysTrapSysHighDensitySelector
    retval = (Err)TRAP(sysTrapSysHighDensitySelector & (0x00000FFF))
#else
    retval = (Err)TRAP(sysTrapHighDensityDispatch & (0x00000FFF))
#endif
    *attrP = BSwap32( *attrP );		// <-
    return retval;
}

Err WinScreenMode(
    WinScreenModeOperation operation, 
    UInt32 *widthP,
    UInt32 *heightP, 
    UInt32 *depthP, 
    Boolean *enableColorP )
{
    *widthP = BSwap32( *widthP );   // <->
    *heightP = BSwap32( *heightP ); // <->
    *depthP = BSwap32( *depthP );   // <->
    CALL P1(operation) P4(widthP) P4(heightP) P4(depthP) P4(enableColorP)
    Err retval = (Err)TRAP(sysTrapWinScreenMode)
    *widthP = BSwap32( *widthP );   // <->
    *heightP = BSwap32( *heightP ); // <->
    *depthP = BSwap32( *depthP );   // <->
    return retval;
}

UInt8 *WinScreenLock( WinLockInitType initMode )
{
    CALL P1(initMode)
    return (UInt8*)TRAPP(sysTrapWinScreenLock);
}

void WinScreenUnlock( void )
{
    CALL
    TRAP(sysTrapWinScreenUnlock);
}

Err WinPalette( UInt8 operation, Int16 startIndex, UInt16 paletteEntries, RGBColorType *tableP )
{
    CALL P1(operation) P2(startIndex) P2(paletteEntries) P4(tableP)
    return (Err)TRAP(sysTrapWinPalette)
}

void WinDrawBitmap( BitmapPtr bitmapP, Coord x, Coord y )
{
    CALL P4(bitmapP) P2(x) P2(y)
    TRAP(sysTrapWinDrawBitmap)
}

// =================
// FORMS :
// =================

WinHandle FrmGetWindowHandle( const FormType *formP )
{
    CALL P4(formP)
    return (WinHandle)TRAPP(sysTrapFrmGetWindowHandle)
}

FormType *FrmGetActiveForm( void )
{
    CALL
    return (FormType*)TRAPP(sysTrapFrmGetActiveForm)
}

void FrmDrawForm( FormType *formP )
{
    CALL P4(formP)
    TRAP(sysTrapFrmDrawForm)
}

void FrmEraseForm( FormType *formP )
{
    CALL P4(formP)
    TRAP(sysTrapFrmEraseForm)
}

void FrmDeleteForm( FormType *formP )
{
    CALL P4(formP)
    TRAP(sysTrapFrmDeleteForm)
}

FormType *FrmInitForm( UInt16 rscID )
{
    CALL P2(rscID)
    return (FormType*)TRAPP(sysTrapFrmInitForm)
}

void FrmSetActiveForm( FormType *formP )
{
    CALL P4(formP)
    TRAP(sysTrapFrmSetActiveForm)
}

void FrmSetEventHandler( FormType *formP, FormEventHandlerType *handler )
{
    CALL P4(formP) P4(handler)
    TRAP(sysTrapFrmSetEventHandler)
}

void FrmGotoForm( UInt16 formId )
{
    CALL P2(formId)
    TRAP(sysTrapFrmGotoForm)
}

void FrmGetFormBounds( const FormType *formP, RectangleType *rP )
{
    CALL P4(formP) P4(rP)
    TRAP(sysTrapFrmGetFormBounds)
    rP->topLeft.x = BSwap16( rP->topLeft.x ); // ->
    rP->topLeft.y = BSwap16( rP->topLeft.y ); // ->
    rP->extent.x = BSwap16( rP->extent.x );   // ->
    rP->extent.y = BSwap16( rP->extent.y );   // ->
}

void FrmCloseAllForms( void )
{
    CALL
    TRAP(sysTrapFrmCloseAllForms)
}

// Events are in m68 format! You need convert it for using on the ARM

Boolean FrmDispatchEvent( EventType *eventP )
{
    CALL P4(eventP)
    return (Boolean)TRAP(sysTrapFrmDispatchEvent)
}

FormType *FrmGetFormPtr( UInt16 formId )
{
    CALL P2(formId)
    return (FormType*)TRAPP(sysTrapFrmGetFormPtr)
}

// =================
// FEATURES :
// =================

Err FtrGet( UInt32 creator, UInt16 featureNum, UInt32 *valueP )
{
    CALL P4(creator) P2(featureNum) P4(valueP)
    Err retval = (Err)TRAP(sysTrapFtrGet)
    *valueP = BSwap32( *valueP ); // <-
    return retval;
}

// =================
// EVENTS :
// =================

void EvtGetEvent( EventType *event, Int32 timeout )
{
    CALL P4(event) P4(timeout)
    TRAP(sysTrapEvtGetEvent)
}

Boolean MenuHandleEvent( MenuBarType *menuP, EventType *event, UInt16 *error )
{
    CALL P4(menuP) P4(event) P4(error)
    Boolean retval = (Boolean)TRAP(sysTrapMenuHandleEvent)
    *error = BSwap16( *error ); // <-
    return retval;
}

Boolean	SysHandleEvent( EventPtr eventP )
{
    CALL P4(eventP)
    return (Boolean)TRAP(sysTrapSysHandleEvent)
}

// =================
// BITMAPS :
// =================

BitmapType* BmpCreate( Coord width, Coord height, UInt8 depth, ColorTableType * colortableP, UInt16 * error )
{
    CALL P2(width) P2(height) P1(depth) P4(colortableP) P4(error)
    BitmapType* retval = (BitmapType*)TRAPP(sysTrapBmpCreate)
    *error = BSwap16( *error ); // <-
    return retval;
}

void* BmpGetBits( BitmapType * bitmapP )
{
    CALL P4(bitmapP)
    return (void*)TRAPP(sysTrapBmpGetBits)
}

BitmapTypeV3* BmpCreateBitmapV3( const BitmapType* bitmapP, UInt16 density, const void* bitsP, const ColorTableType* colorTableP )
{
    CALL P4(bitmapP) P2(density) P4(bitsP) P4(colorTableP)
    emulStatePtr->regData[2] = HDSelectorBmpCreateBitmapV3;
#ifdef sysTrapSysHighDensitySelector
    return (BitmapTypeV3*)TRAPP(sysTrapSysHighDensitySelector)
#else
    return (BitmapTypeV3*)TRAPP(sysTrapHighDensityDispatch)
#endif
}

Err BmpDelete( BitmapType * bitmapP )
{
    CALL P4(bitmapP)
    return (Err)TRAP(sysTrapBmpDelete)
}

// =================
// TIME :
// =================

void TimSecondsToDateTime( UInt32 seconds, DateTimeType *dateTimeP )
{
    CALL P4(seconds) P4(dateTimeP)
    TRAP(sysTrapTimSecondsToDateTime)
    if( dateTimeP )
    {	
	dateTimeP->second = BSwap16( dateTimeP->second );  // <-
	dateTimeP->minute = BSwap16( dateTimeP->minute );  // <-
	dateTimeP->hour = BSwap16( dateTimeP->hour );      // <-
	dateTimeP->day = BSwap16( dateTimeP->day );        // <-
	dateTimeP->month = BSwap16( dateTimeP->month );    // <-
	dateTimeP->year = BSwap16( dateTimeP->year );      // <-
	dateTimeP->weekDay = BSwap16( dateTimeP->weekDay );// <-
    }
}

UInt16  SysTicksPerSecond( void )
{
    CALL
    return (UInt16)TRAP(sysTrapSysTicksPerSecond)
}

UInt32 TimGetTicks( void )
{
    CALL
    return (UInt32)TRAP(sysTrapTimGetTicks)
}

UInt32 TimGetSeconds( void )
{
    CALL
    return (UInt32)TRAP(sysTrapTimGetSeconds)
}

// =================
// SYSTEM :
// =================

unsigned long KeyCurrentState( void )
{
    CALL
    return (unsigned long)TRAP(sysTrapKeyCurrentState)
}

UInt16 	SysSetAutoOffTime( UInt16	seconds )
{
    CALL P2(seconds)
    return (UInt16)TRAP(sysTrapSysSetAutoOffTime)
}

Int16  SysRandom( Int32 newSeed )
{
    CALL P4(newSeed)
    return (UInt16)TRAP(sysTrapSysRandom)
}
