//Main sound header

#ifndef __SOUND__
#define __SOUND__

#include "core/core.h"
#include "time/timemanager.h"

//Structures and defines:

#define SOUND_MODE_INT16	1
#define SOUND_MODE_FLOAT32	2

#define FLOAT32_TO_INT16( res, val ) \
{ \
    float temp = val; \
    temp *= 32767; \
    if( temp > 32767 ) \
	temp = 32767; \
    if( temp < -32767 ) \
	temp = -32767; \
    res = (signed short)temp; \
}

enum 
{
    STATUS_STOP = 0,
    STATUS_PLAY,
};

struct sound_struct
{
    volatile int	status;		    //Current playing status
    volatile int	need_to_stop;	    //Set it to 1 if you want to stop sound stream
    volatile int	stream_stoped;	    //If stream really stoped
    volatile int	stop_counter;

    void		*user_data;	    //Data for user defined render_piece_of_sound()

    int			mode;
    int			freq;
    int			channels;

    //Parameters for output callback:
    void 		*out_buffer;
    long 		out_frames; //buffer size in frames
    ticks_t 		out_time;

    //Flags:
    volatile int 	main_sound_callback_working;
};

//Variables:

extern sound_struct g_snd;

//Functions:

void main_sound_output_callback( sound_struct *ss );

extern int render_piece_of_sound( sound_struct *ss );

int sound_stream_init( int mode, int freq, int channels );
void sound_stream_play( void );
void sound_stream_stop( void );
void sound_stream_close( void );

#endif
