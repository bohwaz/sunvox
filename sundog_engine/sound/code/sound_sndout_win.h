#ifdef WIN
    #define WIN_DSOUND	1
#endif
#ifdef WINCE
    #define WIN_MMSOUND	1
#endif

//################################
//## WINDOWS DIRECT SOUND       ##
//################################

int g_buffer_size = DEFAULT_BUFFER_SIZE;
int g_frame_size = 0;
void *g_sound_buffer = 0;

#ifdef WIN_DSOUND
#include "dsound.h"
#define NUMEVENTS 2
LPDIRECTSOUND               lpds;
DSBUFFERDESC                dsbdesc;
LPDIRECTSOUNDBUFFER         lpdsb = 0;
LPDIRECTSOUNDBUFFER         lpdsbPrimary;
LPDIRECTSOUNDNOTIFY         lpdsNotify;
WAVEFORMATEX                *pwfx;
HMMIO                       hmmio;
MMCKINFO                    mmckinfoData, mmckinfoParent;
DSBPOSITIONNOTIFY           rgdsbpn[ NUMEVENTS ];
HANDLE                      rghEvent[ NUMEVENTS ];

//Sound thread:
HANDLE sound_thread;
SECURITY_ATTRIBUTES atr;
bool StreamToBuffer( ulong dwPos )
{
    LONG            lNumToWrite;
    DWORD           dwStartOfs;
    VOID            *lpvPtr1, *lpvPtr2;
    DWORD           dwBytes1, dwBytes2;
    static DWORD    dwStopNextTime = 0xFFFF;

    g_snd.out_time = time_ticks() + ( ( ( ( g_buffer_size << 15 ) / g_snd.freq ) * time_ticks_per_second() ) >> 15 );

    if( dwStopNextTime == dwPos )   // All data has been played
    {
	lpdsb->Stop();
	dwStopNextTime = 0xFFFF;
	return TRUE;
    }

    if( dwStopNextTime != 0xFFFF )  // No more to stream, but keep
		                    // playing to end of data
        return TRUE;

    if( dwPos == 0 )
	dwStartOfs = rgdsbpn[ 1 ].dwOffset;
    else
	dwStartOfs = rgdsbpn[ 0 ].dwOffset;

    lNumToWrite = (LONG) rgdsbpn[ dwPos ].dwOffset - dwStartOfs;
    if( lNumToWrite < 0 ) lNumToWrite += dsbdesc.dwBufferBytes;

    IDirectSoundBuffer_Lock( 
	lpdsb,
        dwStartOfs,       // Offset of lock start
        lNumToWrite,      // Number of bytes to lock
        &lpvPtr1,         // Address of lock start
        &dwBytes1,        // Count of bytes locked
        &lpvPtr2,         // Address of wrap around
        &dwBytes2,        // Count of wrap around bytes
        0 );              // Flags

    //Write data to the locked buffer:
    g_snd.out_frames = dwBytes1 / ( 2 * g_snd.channels );
    if( g_snd.mode & SOUND_MODE_INT16 )
    {
	g_snd.out_buffer = lpvPtr1;
	main_sound_output_callback( &g_snd );
    }
    if( g_snd.mode & SOUND_MODE_FLOAT32 )
    {
	g_snd.out_buffer = g_sound_buffer;
	main_sound_output_callback( &g_snd );
	float *fb = (float*)g_sound_buffer;
	signed short *sb = (signed short*)lpvPtr1;
	for( unsigned int i = 0; i < dwBytes1 / 2; i++ )
	{
	    FLOAT32_TO_INT16( sb[ i ], fb[ i ] );
	}
    }

    IDirectSoundBuffer_Unlock( lpdsb, lpvPtr1, dwBytes1, lpvPtr2, dwBytes2 );

    return TRUE;
}

volatile int g_dsound_need_to_close_thread = 0;
volatile int g_dsound_thread_closed = 0;

unsigned long __stdcall sound_callback( void *par )
{
    int last_evt = 99;
    while( g_dsound_need_to_close_thread == 0 )
    {
	DWORD dwEvt = MsgWaitForMultipleObjects(
	    NUMEVENTS, // How many possible events
	    rghEvent, // Location of handles
	    FALSE, // Wait for all?
	    200, // How long to wait
	    QS_ALLINPUT ); // Any message is an event

	dwEvt -= WAIT_OBJECT_0;

	// If the event was set by the buffer, there's input
	// to process. 

	if( dwEvt < NUMEVENTS && dwEvt != last_evt ) 
	{
	    if( lpdsb )	StreamToBuffer( dwEvt ); // copy data to output stream
	    last_evt = dwEvt;
	}
    }
    g_dsound_thread_closed = 1;
    return 0;
}

LPGUID guids[ 128 ];
int guids_num = 0;

BOOL CALLBACK DSEnumCallback (
    LPGUID GUID,
    LPCSTR Description,
    LPCSTR Module,
    VOID *Context
)
{
    dprint( "Found sound device %d: %s\n", guids_num, Description );
    guids[ guids_num ] = GUID;
    guids_num++;
    return 1;
}
#endif

//################################
//## WINDOWS MMSOUND            ##
//################################

#ifdef WIN_MMSOUND

#define USE_THREAD
#define MAX_BUFFERS	2

HWAVEOUT		g_waveOutStream = 0;
HANDLE			g_waveOutThread = 0;
DWORD			g_waveOutThreadID = 0;
volatile int		g_waveOutExitRequest = 0;
WAVEHDR			g_outBuffersHdr[ MAX_BUFFERS ];

void SendBuffer( WAVEHDR *waveHdr )
{
    g_snd.out_time = time_ticks() + ( ( ( ( g_buffer_size << 15 ) / g_snd.freq ) * time_ticks_per_second() ) >> 15 );
    g_snd.out_buffer = waveHdr->lpData;
    g_snd.out_frames = waveHdr->dwBufferLength / 4;
    main_sound_output_callback( &g_snd );
    MMRESULT mres = waveOutWrite( g_waveOutStream, waveHdr, sizeof( WAVEHDR ) );
    if( mres != MMSYSERR_NOERROR )
    {
        dprint( "ERROR in waveOutWrite: %d\n", mres );
    }
}

DWORD WINAPI WaveOutThreadProc( LPVOID lpParameter )
{
    while( g_waveOutExitRequest == 0 )
    {
	MSG msg;
	WAVEHDR *waveHdr = NULL;
        while( PeekMessage( &msg, 0, 0, 0, PM_REMOVE ) )
        {
    	    switch( msg.message )
            {
        	case MM_WOM_DONE:
            	    waveHdr = (WAVEHDR*)msg.lParam;
		    SendBuffer( waveHdr );
		    break;
	    }
	}
	Sleep( 5 );
    }
    g_waveOutExitRequest = 0;
    return 0;
}

#ifndef USE_THREAD
ulong WaveOut( ulong hwo, unsigned short uMsg, ulong dwInstance, ulong dwParam1, ulong dwParam2 )
{
    #error WaveOut function not implemented.
    if( uMsg == WOM_DONE ) 
    {
	//...
    }
    return 0;
}
#endif

#endif //...WIN_MMSOUND

//################################
//## MAIN FUNCTIONS:            ##
//################################

int device_sound_stream_init( int mode, int freq, int channels )
{
    if( profile_get_int_value( KEY_SOUNDBUFFER, 0 ) != -1 ) 
	g_buffer_size = profile_get_int_value( KEY_SOUNDBUFFER, 0 );

    if( mode & SOUND_MODE_INT16 )
    {
	g_frame_size = 2 * channels;
    }
    if( mode & SOUND_MODE_FLOAT32 )
    {
	g_frame_size = 4 * channels;
	g_sound_buffer = malloc( g_buffer_size * g_frame_size * 2 );
    }	

#ifdef WIN_DSOUND
    HWND hWnd = GetForegroundWindow();
    if( hWnd == NULL )
    {
        hWnd = GetDesktopWindow();
    }
    LPVOID EnumContext = 0;
    DirectSoundEnumerate( DSEnumCallback, EnumContext );
    if FAILED( DirectSoundCreate( 0, &lpds, NULL ) )
    {
        MessageBox( hWnd, "DSound: DirectSoundCreate error", "Error", MB_OK );
        return 1;
    }
    if FAILED( IDirectSound_SetCooperativeLevel(
	lpds, hWnd, DSSCL_PRIORITY ) )
    {
	MessageBox( hWnd, "DSound: SetCooperativeLevel error", "Error", MB_OK );
	return 1;
    }

    WAVEFORMATEX wfx;
    memset( &wfx, 0, sizeof( WAVEFORMATEX ) ); 
    wfx.wFormatTag = WAVE_FORMAT_PCM; 
    wfx.nChannels = channels; 
    wfx.nSamplesPerSec = freq; 
    wfx.wBitsPerSample = 16; 
    wfx.nBlockAlign = wfx.wBitsPerSample / 8 * wfx.nChannels;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    pwfx = &wfx;

    // Create primary buffer:

    /*

    DSBUFFERDESC buf_desc;
    ZeroMemory( &buf_desc, sizeof( DSBUFFERDESC ) );
    buf_desc.dwSize        = sizeof( DSBUFFERDESC );
    buf_desc.dwFlags       = DSBCAPS_PRIMARYBUFFER;
    buf_desc.dwBufferBytes = 0;
    buf_desc.lpwfxFormat   = NULL;
    if FAILED( lpds->CreateSoundBuffer(
		&buf_desc, &lpdsbPrimary, NULL ) )
    {
	MessageBox( hWnd, "DSound: Create primary buffer error", "Error", MB_OK );
	return;
    }
    if FAILED( lpdsbPrimary->SetFormat( &wfx ) )
    {
	MessageBox( hWnd, "DSound: Can't set format of the primary buffer", "Error", MB_OK );
	return;
    }
    lpdsbPrimary->Release();

    */

    // Secondary buffer:
	
    memset( &dsbdesc, 0, sizeof( DSBUFFERDESC ) ); 
    dsbdesc.dwSize = sizeof( DSBUFFERDESC ); 
    dsbdesc.dwFlags = 
	( 0
        | DSBCAPS_GETCURRENTPOSITION2 // Always a good idea
        | DSBCAPS_GLOBALFOCUS // Allows background playing
        | DSBCAPS_CTRLPOSITIONNOTIFY // Needed for notification
	);
 
    // The size of the buffer is arbitrary, but should be at least
    // two seconds, to keep data writes well ahead of the play
    // position.
 
    dsbdesc.dwBufferBytes = g_buffer_size * 2 * channels * 2;
    dsbdesc.lpwfxFormat = pwfx;

    if FAILED( IDirectSound_CreateSoundBuffer(
               lpds, &dsbdesc, &lpdsb, NULL ) )
    {
	MessageBox( hWnd,"DSound: Create secondary buffer error","Error",MB_OK);
	return 1;
    }

    //Create buffer events:
	
    for( int i = 0; i < NUMEVENTS; i++ )
    {
        rghEvent[ i ] = CreateEvent( NULL, FALSE, FALSE, NULL );
        if( NULL == rghEvent[ i ] ) 
	{
	    MessageBox( hWnd,"DSound: Create event error","Error",MB_OK);
	    return 1;
	}
    }
    rgdsbpn[ 0 ].dwOffset = 0;
    rgdsbpn[ 0 ].hEventNotify = rghEvent[ 0 ];
    rgdsbpn[ 1 ].dwOffset = ( dsbdesc.dwBufferBytes / 2 );
    rgdsbpn[ 1 ].hEventNotify = rghEvent[ 1 ];
	
    if FAILED( lpdsb->QueryInterface( IID_IDirectSoundNotify, (VOID **)&lpdsNotify ) )
    {
    	MessageBox( hWnd,"DSound: QueryInterface error","Error",MB_OK);
    	return 1;
    }
 
    if FAILED( IDirectSoundNotify_SetNotificationPositions(
             lpdsNotify, NUMEVENTS, rgdsbpn ) )
    {
        IDirectSoundNotify_Release( lpdsNotify );
	MessageBox( hWnd, "DSound: SetNotificationPositions error"," Error", MB_OK );
	return 1;
    }

    IDirectSoundBuffer_Play( lpdsb, 0, 0, DSBPLAY_LOOPING );

    //Create main thread:
    atr.nLength = sizeof(atr);
    atr.lpSecurityDescriptor = 0;
    atr.bInheritHandle = 0;
    sound_thread = CreateThread( &atr, 1024 * 64, &sound_callback, &g_snd, 0, 0 );
    SetThreadPriority( sound_thread, THREAD_PRIORITY_TIME_CRITICAL );
#endif

#ifdef WIN_MMSOUND
    g_waveOutExitRequest = 0;

    int soundDevices = waveOutGetNumDevs();
    if( soundDevices == 0 ) 
	{ dprint( "ERROR: No sound devices :(\n" ); return 1; } //No sound devices
    dprint( "SOUND: Number of sound devices: %d\n", soundDevices );
    
    int ourDevice;
    int dev = -1;
    for( ourDevice = 0; ourDevice < soundDevices; ourDevice++ )
    {
    	WAVEOUTCAPS deviceCaps;
	waveOutGetDevCaps( ourDevice, &deviceCaps, sizeof( deviceCaps ) );
	if( deviceCaps.dwFormats & WAVE_FORMAT_4S16 ) 
	{
	    dev = ourDevice;
	    break;
	}
    }
    if( dev == -1 )
    {
	dprint( "ERROR: Can't find compatible sound device\n" );
	return 1;
    }
    dprint( "SOUND: Dev: %d. Sound freq: %d\n", dev, freq );

    WAVEFORMATEX waveFormat;
    waveFormat.wFormatTag = WAVE_FORMAT_PCM;
    waveFormat.nChannels = channels;
    waveFormat.nSamplesPerSec = freq;
    waveFormat.nAvgBytesPerSec = freq * 4;
    waveFormat.nBlockAlign = 4;
    waveFormat.wBitsPerSample = 16;
    waveFormat.cbSize = 0;
#ifdef USE_THREAD
    g_waveOutThread = (HANDLE)CreateThread( NULL, 1024 * 64, WaveOutThreadProc, NULL, 0, &g_waveOutThreadID );
    SetThreadPriority( g_waveOutThread, THREAD_PRIORITY_TIME_CRITICAL );
    MMRESULT mres = waveOutOpen( &g_waveOutStream, dev, &waveFormat, (ulong)g_waveOutThreadID, 0, CALLBACK_THREAD );
#else
    MMRESULT mres = waveOutOpen( &g_waveOutStream, dev, &waveFormat, (ulong)&WaveOut, 0, CALLBACK_FUNCTION );
#endif
    if( mres != MMSYSERR_NOERROR )
    {
	dprint( "ERROR: Can't open sound device (%d)\n", mres );
	switch( mres )
	{
	    case MMSYSERR_INVALHANDLE: dprint( "ERROR: MMSYSERR_INVALHANDLE\n" ); break;
	    case MMSYSERR_BADDEVICEID: dprint( "ERROR: MMSYSERR_BADDEVICEID\n" ); break;
	    case MMSYSERR_NODRIVER: dprint( "ERROR: MMSYSERR_NODRIVER\n" ); break;
	    case MMSYSERR_NOMEM: dprint( "ERROR: MMSYSERR_NOMEM\n" ); break;
	    case WAVERR_BADFORMAT: dprint( "ERROR: WAVERR_BADFORMAT\n" ); break;
	    case WAVERR_SYNC: dprint( "ERROR: WAVERR_SYNC\n" ); break;
	}
	return 1;
    }
    dprint( "SOUND: waveout device opened\n" );

    for( int b = 0; b < MAX_BUFFERS; b++ )
    {
        ZeroMemory( &g_outBuffersHdr[ b ], sizeof( WAVEHDR ) );
        g_outBuffersHdr[ b ].lpData = (char *)malloc( g_buffer_size * 2 * channels );
        g_outBuffersHdr[ b ].dwBufferLength = g_buffer_size * 2 * channels;
	mres = waveOutPrepareHeader( g_waveOutStream, &g_outBuffersHdr[ b ], sizeof( WAVEHDR ) );
	if( mres != MMSYSERR_NOERROR )
	{
	    dprint( "ERROR: Can't prepare %d waveout header (%d)\n", b, mres );
	}
	SendBuffer( &g_outBuffersHdr[ b ] );
    }
#endif
    
    return 0;
}

void device_sound_stream_play( void )
{
}

void device_sound_stream_stop( void )
{
#ifdef WIN_DSOUND
    if( lpdsb )
    {
	g_snd.stream_stoped = 0;
	g_snd.need_to_stop = 1;
	while( g_snd.stream_stoped == 0 ) {} //Waiting for stoping
    }
#endif

#ifdef WIN_MMSOUND
    dprint( "SOUND: req. for stop...\n" );
    if( g_waveOutStream )
    {
	g_snd.stream_stoped = 0;
	g_snd.need_to_stop = 1;
	while( g_snd.stream_stoped == 0 ) { Sleep( 1 ); } //Waiting for stoping
    }
    dprint( "SOUND: req. for stop... ok\n" );
#endif
}

void device_sound_stream_close( void )
{
#ifdef WIN_DSOUND
    g_dsound_need_to_close_thread = 1;
    while( g_dsound_thread_closed == 0 ) {};
    CloseHandle( sound_thread );
    if( lpdsb )
    {
        if( lpdsNotify )
	    lpdsNotify->Release();
	if( lpds )
	    lpds->Release();
    }
#endif

#ifdef WIN_MMSOUND
    g_waveOutExitRequest = 1;
    while( g_waveOutExitRequest ) { Sleep( 1 ); } //Waiting for thread stop
    CloseHandle( g_waveOutThread );
    dprint( "SOUND: CloseHandle (soundThread) ok\n" );
        
    MMRESULT mres;
    mres = waveOutReset( g_waveOutStream );
    if( mres != MMSYSERR_NOERROR )
    {
        dprint( "ERROR in waveOutReset (%d)\n", mres );
    }
    
    for( int b = 0; b < MAX_BUFFERS; b++ )
    {
	mres = waveOutUnprepareHeader( g_waveOutStream, &g_outBuffersHdr[ b ], sizeof( WAVEHDR ) );
	if( mres != MMSYSERR_NOERROR )
	{
	    dprint( "ERROR: Can't unprepare waveout header %d waveout header (%d)\n", b, mres );
	}
        free( g_outBuffersHdr[ b ].lpData );
    }

    mres = waveOutClose( g_waveOutStream );
    if( mres != MMSYSERR_NOERROR ) 
	dprint( "ERROR in waveOutClose: %d\n", mres );
    else
	dprint( "SOUND: waveOutClose ok\n" );
#endif

    if( g_sound_buffer ) 
	free( g_sound_buffer );
    g_sound_buffer = 0;
}
