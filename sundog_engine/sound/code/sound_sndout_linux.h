#define LINUX_ALSA
//#define LINUX_OSS

//################################
//## LINUX:                     ##
//################################

#include <pthread.h>

int g_buffer_size = DEFAULT_BUFFER_SIZE;
int g_frame_size = 0;
int dsp;
pthread_t pth;
volatile int g_sound_thread_exit_request = 0;

#ifdef LINUX_OSS

#include <linux/soundcard.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#endif //...LINUX_OSS

#ifdef LINUX_ALSA

#include <alsa/asoundlib.h>

snd_pcm_t *g_alsa_playback_handle = 0;
int xrun_recovery( snd_pcm_t *handle, int err )
{
    if( err == -EPIPE ) 
    { //under-run:
	printf( "ALSA EPIPE (underrun)\n" );
	err = snd_pcm_prepare( handle );
	if( err < 0 )
	    printf( "ALSA Can't recovery from underrun, prepare failed: %s\n", snd_strerror( err ) );
	return 0;
    }
    else if( err == -ESTRPIPE )
    {
	printf( "ALSA ESTRPIPE\n" );
	while( ( err = snd_pcm_resume( handle ) ) == -EAGAIN )
	    sleep( 1 ); //wait until the suspend flag is released
	if( err < 0 )
	{
	    err = snd_pcm_prepare( handle );
	    if( err < 0 )
		 printf( "ALSA Can't recovery from suspend, prepare failed: %s\n", snd_strerror( err ) );
	}
	return 0;
    }
    return err;
}

#endif //...LINUX_ALSA

void *sound_thread( void *arg )
{
    sound_struct *ss = (sound_struct*)arg;
    char buf[ g_buffer_size * g_frame_size ];
    long len;
    volatile int err;
    for(;;) 
    {
	len = g_buffer_size;
	ss->out_buffer = buf;
	ss->out_frames = len;
	ss->out_time = time_ticks() + ( ( ( ( g_buffer_size << 15 ) / ss->freq ) * time_ticks_per_second() ) >> 15 );
	main_sound_output_callback( ss );
#ifdef LINUX_OSS
	if( dsp >= 0 && g_sound_thread_exit_request == 0 )
	{
	    if( g_snd.mode & SOUND_MODE_FLOAT32 )
	    {
		float *fb = (float*)buf;
		signed short *sb = (signed short*)buf;
		for( int i = 0; i < g_buffer_size * g_snd.channels; i++ )
		    FLOAT32_TO_INT16( sb[ i ], fb[ i ] );
	    }
	    err = write( dsp, buf, len * 4 ); 
#endif //...LINUX_OSS
#ifdef LINUX_ALSA
	if( g_alsa_playback_handle != 0 && g_sound_thread_exit_request == 0 ) 
	{
	    if( g_snd.mode & SOUND_MODE_FLOAT32 )
	    {
		float *fb = (float*)buf;
		signed short *sb = (signed short*)buf;
		for( int i = 0; i < g_buffer_size * g_snd.channels; i++ )
		    FLOAT32_TO_INT16( sb[ i ], fb[ i ] );
	    }
	    /* //THIS CODE NOT WORKING ON SOME CARDS//
	    char *buf_ptr = buf;
	    int sent = 0;
	    int frames_to_deliver;
	    while( len > 0 )
	    {
		//Wait till the interface is ready for data, or 1 second has elapsed:
		err = snd_pcm_wait( g_alsa_playback_handle, 1000 );
		if( err < 0 ) 
		{
		    printf( "ALSA snd_pcm_wait error: %s\n", snd_strerror( err ) );
		    err = xrun_recovery( g_alsa_playback_handle, err );
		    if( err < 0 )
		    {
			printf( "ALSA snd_pcm_wait error: %s\n", snd_strerror( err ) );
			goto sound_thread_exit;
		    }
		}
	    
		//Find out how much space is available for playback data:
		frames_to_deliver = snd_pcm_avail_update( g_alsa_playback_handle );
		if( frames_to_deliver < 0 )
		{
		    if( frames_to_deliver == -EPIPE ) 
		    {
			printf( "ALSA an xrun occured\n" );
			break;
		    }
		    else
		    {
			printf( "ALSA unknown avail update return value (%d)\n", frames_to_deliver );
			break;
		    }
		}
	    
		frames_to_deliver = frames_to_deliver > len ? len : frames_to_deliver;
		
		//printf( "len: %d [%d]\n", len, frames_to_deliver );
    		sent = snd_pcm_writei( g_alsa_playback_handle, buf_ptr, frames_to_deliver );
	    
		if( sent != frames_to_deliver )
		{
		    printf( "ALSA playback callback failed\n" );
		}
		
		len -= frames_to_deliver;
		buf_ptr += frames_to_deliver * g_frame_size;
	    }
	    */
	    char *buf_ptr = (char*)buf;
	    int err;
	    while( len > 0 ) 
	    {
		err = snd_pcm_writei( g_alsa_playback_handle, buf_ptr, len );
		if( err == -EAGAIN )
                    continue;
		if( err < 0 ) 
		{
		    printf( "ALSA snd_pcm_writei error: %s\n", snd_strerror( err ) );
		    err = xrun_recovery( g_alsa_playback_handle, err );
		    if( err < 0 )
		    {
			printf( "ALSA xrun_recovery error: %s\n", snd_strerror( err ) );
			goto sound_thread_exit;
		    }
		}
		else
		{
		    len -= err;
		    buf_ptr += err * g_frame_size;
		}
	    }
#endif //...LINUX_ALSA
#ifdef OUTPUT_TO_FILE
	    fwrite( buf, 1, len * g_frame_size, g_audio_output );
#endif
	} 
	else 
	{
	    break;
	}
    }
sound_thread_exit:
    g_sound_thread_exit_request = 0;
    pthread_exit( 0 );
    return 0;
}

//################################
//## MAIN FUNCTIONS:            ##
//################################

int device_sound_stream_init( int mode, int freq, int channels )
{
    if( profile_get_int_value( KEY_SOUNDBUFFER, 0 ) != -1 ) 
	g_buffer_size = profile_get_int_value( KEY_SOUNDBUFFER, 0 );

    if( mode & SOUND_MODE_INT16 )
	g_frame_size = 2 * channels;
    if( mode & SOUND_MODE_FLOAT32 )
	g_frame_size = 4 * channels;

#ifdef LINUX_OSS
    if( channels != 2 )
    {
	dprint( "OSS ERROR: channels must be 2\n" );
	return 1;
    }
    //Start first time:
    int temp;
    dsp = -1;
    int adev = profile_get_str_value( KEY_AUDIODEVICE, 0 );
    if( adev != -1 )
    {
	char *ts = (char*)adev;
	dsp = open ( ts, O_WRONLY, 0 );
    }
    if( dsp == -1 )
    {
	dsp = open ( "/dev/dsp", O_WRONLY, 0 );
	if( dsp == -1 )
	    dsp = open ( "/dev/.static/dev/dsp", O_WRONLY, 0 );
    }
    if( dsp == -1 )
    {
        dprint( "OSS ERROR: Can't open sound device\n" );
        return 1;
    }
    temp = 1;
    ioctl( dsp, SNDCTL_DSP_STEREO, &temp );
    g_snd.channels = channels;
    temp = 16;
    ioctl( dsp, SNDCTL_DSP_SAMPLESIZE, &temp );
    temp = freq;
    ioctl( dsp, SNDCTL_DSP_SPEED, &temp );
    temp = 16 << 16 | 8;
    ioctl( dsp, SNDCTL_DSP_SETFRAGMENT, &temp );
    ioctl( dsp, SNDCTL_DSP_GETBLKSIZE, &temp );
    
    //Create sound thread:
    if( pthread_create( &pth, NULL, sound_thread, &g_snd ) != 0 )
    {
        dprint( "OSS ERROR: Can't create sound thread!\n" );
        return 1;
    }
#endif //...LINUX_OSS
#ifdef LINUX_ALSA
    int err;
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_sw_params_t *sw_params;

    snd_pcm_hw_params_malloc( &hw_params );
    snd_pcm_sw_params_malloc( &sw_params );

    if( profile_get_str_value( KEY_AUDIODEVICE, 0 ) )
    {
	err = snd_pcm_open( &g_alsa_playback_handle, profile_get_str_value( KEY_AUDIODEVICE, 0 ), SND_PCM_STREAM_PLAYBACK, 0 );
    }
    else
    {
	err = snd_pcm_open( &g_alsa_playback_handle, "plughw:0,0", SND_PCM_STREAM_PLAYBACK, 0 );
    }
    if( err < 0 ) 
    {
	dprint( "ALSA ERROR: Can't open audio device: %s\n", snd_strerror( err ) );
	return 1;
    }
    
    err = snd_pcm_hw_params_any( g_alsa_playback_handle, hw_params );
    if( err < 0 ) 
    {
	dprint( "ALSA ERROR: Can't initialize hardware parameter structure: %s\n", snd_strerror( err ) );
	return 1;
    }
    err = snd_pcm_hw_params_set_access( g_alsa_playback_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED );
    if( err < 0 ) 
    {
	dprint( "ALSA ERROR: Can't set access type: %s\n", snd_strerror( err ) );
	return 1;
    }
    const UTF8_CHAR *sample_format = "";
    err = snd_pcm_hw_params_set_format( g_alsa_playback_handle, hw_params, SND_PCM_FORMAT_S16_LE );
    /*if( mode & SOUND_MODE_INT16 )
    {
	sample_format = "INT16";
	err = snd_pcm_hw_params_set_format( g_alsa_playback_handle, hw_params, SND_PCM_FORMAT_S16_LE );
    }
    if( mode & SOUND_MODE_FLOAT32 )
    {
	sample_format = "FLOAT32";
	err = snd_pcm_hw_params_set_format( g_alsa_playback_handle, hw_params, SND_PCM_FORMAT_FLOAT_LE );
    }*/
    if( err < 0 ) 
    {
	dprint( "ALSA ERROR: Can't set sample format %s: %s\n", sample_format, snd_strerror( err ) );
	return 1;
    }
    err = snd_pcm_hw_params_set_rate_near( g_alsa_playback_handle, hw_params, (unsigned int*)&freq, 0 );
    if( err < 0 ) 
    {
	dprint( "ALSA ERROR: Can't set sample rate: %s\n", snd_strerror( err ) );
	return 1;
    }
    dprint( "ALSA Sample rate: %d\n", freq );
    err = snd_pcm_hw_params_set_channels( g_alsa_playback_handle, hw_params, channels );
    if( err < 0 ) 
    {
	dprint( "ALSA ERROR: Can't set channel count: %s\n", snd_strerror( err ) );
	return 1;
    }
    snd_pcm_uframes_t frames;
    if( profile_get_int_value( KEY_SOUNDBUFFER, 0 ) != -1 )
	frames = profile_get_int_value( KEY_SOUNDBUFFER, 0 );
    else
	frames = DEFAULT_BUFFER_SIZE;
    err = snd_pcm_hw_params_set_buffer_size_near( g_alsa_playback_handle, hw_params, &frames );
    if( err < 0 ) 
    {
	dprint( "ALSA ERROR: Can't set buffer size: %s\n", snd_strerror( err ) );
	return 1;
    }
    snd_pcm_hw_params_get_buffer_size( hw_params, &frames );
    dprint( "ALSA Buffer size: %d samples\n", frames );
    err = snd_pcm_hw_params( g_alsa_playback_handle, hw_params );
    if( err < 0 ) 
    {
	dprint( "ALSA ERROR: Can't set parameters: %s\n", snd_strerror( err ) );
	return 1;
    }

    //snd_pcm_sw_params_current( g_alsa_playback_handle, sw_params );
    //snd_pcm_sw_params_set_start_threshold( g_alsa_playback_handle, sw_params, 0 );
    //snd_pcm_sw_params_set_avail_min( g_alsa_playback_handle, sw_params, frames / 8 );
    //snd_pcm_sw_params( g_alsa_playback_handle, sw_params );

    snd_pcm_hw_params_free( hw_params );
    snd_pcm_sw_params_free( sw_params );
    
    err = snd_pcm_prepare( g_alsa_playback_handle );
    if( err < 0 ) 
    {
	dprint( "ALSA ERROR: Can't prepare audio interface for use: %s\n", snd_strerror( err ) );
	return 1;
    }    
    //Create sound thread:
    if( pthread_create( &pth, NULL, sound_thread, &g_snd ) != 0 )
    {
        dprint( "ALSA ERROR: Can't create sound thread!\n" );
        return 1;
    }
#endif //...LINUX_ALSA

    return 0;
}

void device_sound_stream_play( void )
{
}

void device_sound_stream_stop( void )
{
    g_snd.stream_stoped = 0;
    g_snd.need_to_stop = 1;
    while( g_snd.stream_stoped == 0 ) {} //Waiting for stoping
}

void device_sound_stream_close( void )
{
#ifdef LINUX_OSS
    if( dsp >= 0 )
    {
	g_sound_thread_exit_request = 1;
	while( g_sound_thread_exit_request ) {}
	close( dsp );
	dsp = -1;
    }
#endif //...LINUX_OSS
#ifdef LINUX_ALSA
    if( g_alsa_playback_handle )
    {
	g_sound_thread_exit_request = 1;
	while( g_sound_thread_exit_request ) {}
	snd_pcm_close( g_alsa_playback_handle );
	g_alsa_playback_handle = 0;
    }
#endif //...LINUX_ALSA
}
