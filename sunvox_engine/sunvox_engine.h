#ifndef __SUNVOX_ENGINE__
#define __SUNVOX_ENGINE__

#include "psynth/psynth_net.h"

/*
Timeline:
size of one BEAT = 24 ticks;
number of ticks in one note = speed;
*/

#define MAX_OCTAVES		10
#define MAX_NOTES		( MAX_OCTAVES * 12 )
#define MAX_PLAYING_PATS	64
#define MAX_USER_COMMANDS	64
#define MAX_PATTERN_CHANNELS	8

extern int ( *g_synths [] )( PSYTEXX_SYNTH_PARAMETERS );
extern int g_synths_num;

#define NOTECMD_ALL_NOTES_OFF	    129 /* notes of all synths off */
#define NOTECMD_CLEAN_SYNTHS	    130 /* stop and clean all synths */
#define NOTECMD_STOP		    131
#define NOTECMD_PLAY		    132
struct sunvox_note
{
    uchar	    note;	//0 - nothing; 1..127 - note num; 128 - note off; 129, 130... - see NOTECMD_xxx defines
    uchar	    vel;	//Velocity 1..129; 0 - default
    uchar	    synth;	//0 - nothing; 1..255 - synth number
    uchar	    nothing;
    uint16	    ctl;	//CCXX. CC - number of controller. XX - std effect
    uint16	    ctl_val;	//Value of controller
};

struct sunvox_pattern
{
    sunvox_note	    *data;
    int		    data_xsize; //May be != channels
    int		    data_ysize; //May be != lines

    int		    channels;
    int		    lines;	//xsize
    int		    ysize;

    uint16	    icon[ 16 ]; //Pattern icon 16x16
    uchar	    fg[ 3 ];	//Foreground color
    uchar	    bg[ 3 ];	//Background color
};

#define SUNVOX_PATTERN_FLAG_CLONE	1
#define SUNVOX_PATTERN_FLAG_SELECTED	2
#define SUNVOX_PATTERN_FLAG_RESERVED	4   /*Used in timeline editor (clone patterns)*/

struct sunvox_pattern_info
{
    int		    flags;
    int		    parent_num;
    int		    x, y;
    int		    start_x, start_y;				//Used in timeline editor, while user drag pattern
    int		    cur_time_ptr;
    int		    prev_time_ptr;
    uchar	    channel_status[ MAX_PATTERN_CHANNELS ];	//0..127 - number of internal synth channel; bit 7 - channel was stoped by "note off"
    uchar	    channel_synth[ MAX_PATTERN_CHANNELS ];	//Synth number (for noteON/noteOFF) for each channel
    int		    std_eff_ptr;				//Pointer to structure in std_eff array
};

#define EFF_FLAG_TONE_PORTA			1
#define EFF_FLAG_ARPEGGIO_IN_PREVIOUS_TICK	2

struct sunvox_std_eff
{
    int		    cur_period;
    int16	    target_period;
    uint16	    porta_speed;
    char	    flags;
    int16	    cur_vel;
    int16	    vel_speed;
    uint16	    arpeggio;
};

#ifdef PALMOS
    #define SUNVOX_F_BUFFER_SIZE	16
    #define SUNVOX_F_BUFFERS		1
    #define SUNVOX_F_BUFFERS_MASK	( SUNVOX_F_BUFFERS - 1 )
#else
    #define SUNVOX_F_BUFFER_SIZE	64
    #define SUNVOX_F_BUFFERS		4
    #define SUNVOX_F_BUFFERS_MASK	( SUNVOX_F_BUFFERS - 1 )
#endif

struct sunvox_engine
{
    int		    initialized;

    volatile int    playing;
    ticks_t	    start_time; //Time in system ticks
    ticks_t	    cur_time; //...
    int		    single_pattern_play; //Number of pattern or -1.
    int		    stop_at_the_end_of_song;
    int		    end_of_song;

    int		    tick_counter;   //From 0 to tick_size << 8
    int		    time_counter;   //Number of current line
    int		    speed_counter; 

    int		    bpm; //Beats per minute (One beat = 4 lines on speed 6)
    int		    speed; //Ticks per line

    UTF8_CHAR	    *song_name;

    int		    *sorted_pats;
    int		    sorted_pats_num;
    int		    cur_playing_pats[ MAX_PLAYING_PATS ]; //Numbers of patterns in sorted table
    int		    temp_pats[ MAX_PLAYING_PATS ];
    int		    last_sort_pat;  //Number of last pattern (in sorted table) with x less then cursor position
    
    int		    song_lines; //Song length (number of lines). Calculated in sunvox_sort_patterns()

    unsigned int    song_len; //Song length (in samples). Calculated in play(), play_from_beginning()

    //For standart effects handling:
    sunvox_std_eff  std_eff[ MAX_PLAYING_PATS * MAX_PATTERN_CHANNELS ];
    char	    std_eff_busy[ MAX_PLAYING_PATS ];
    //Pattern can be placed in different cells of cur_playing_pats[] array during its life-cycle
    //(see step 3 in the sunvox_render_piece_of_sound() function),
    //but at the same time this pattern must be linked to one unique cell in the std_eff[] array.

    sunvox_pattern  *copy_pats;
    sunvox_pattern_info	*copy_pats_info;
    int		    copy_pats_num;

    sunvox_pattern  **pats;
    sunvox_pattern_info	*pats_info;
    int		    pats_num;
    psynth_net	    *net;

    sunvox_note	    user_commands[ MAX_USER_COMMANDS ];
    int		    user_commands_channel_num[ MAX_USER_COMMANDS ];
    int		    user_rp;
    int		    user_wp;
    sunvox_pattern_info	user_pat_info;

    //Visualization frames: (scope drawing, realtime song pointers ...)
    uchar	    f_volumes_l[ SUNVOX_F_BUFFER_SIZE * SUNVOX_F_BUFFERS ]; //Volume levels
    uchar	    f_volumes_r[ SUNVOX_F_BUFFER_SIZE * SUNVOX_F_BUFFERS ]; //Volume levels
#ifdef SUNVOX_F_SYNTHS
    int		    f_synth_volumes[ SUNVOX_F_BUFFER_SIZE * SUNVOX_F_BUFFERS * 256 ];
    int		    f_synth_prev_volume[ 256 ];
#endif
    int		    f_lines[ SUNVOX_F_BUFFER_SIZE * SUNVOX_F_BUFFERS ];	//Numbers of sunvox lines
    ulong	    f_buffer_size[ SUNVOX_F_BUFFERS ];
    ticks_t	    f_buffer_start_time[ SUNVOX_F_BUFFERS ]; //In system ticks
    int		    f_current_buffer;
};

extern sunvox_engine *g_sunvox_engine;
extern int g_cancel_export_to_wav;

#define SUNVOX_FLAG_CREATE_PATTERN	1
#define SUNVOX_FLAG_CREATE_SYNTHS	2

void sunvox_engine_init( int flags, sunvox_engine *s );
void sunvox_engine_close( sunvox_engine *s );

void sunvox_load_song( const UTF8_CHAR *name, sunvox_engine *s );
void sunvox_save_song( const UTF8_CHAR *name, sunvox_engine *s );
int sunvox_load_synth( int x, int y, const UTF8_CHAR *name, sunvox_engine *s );
void sunvox_save_synth( int synth_id, const UTF8_CHAR *name, sunvox_engine *s );
unsigned int sunvox_get_song_length( sunvox_engine *s );
void sunvox_export_to_wav( const UTF8_CHAR *name, int mode, void (*status_handler)( void*, ulong ), void *status_data, sunvox_engine *s );

void sunvox_sort_patterns( sunvox_engine *s );

int sunvox_new_pattern( int lines, int channels, int x, int y, sunvox_engine *s );
int sunvox_new_pattern_clone( int pat_num, int x, int y, sunvox_engine *s );
void sunvox_copy_selected_patterns( sunvox_engine *s );
void sunvox_paste_patterns( int x, int y, sunvox_engine *s );
void sunvox_remove_pattern( int pat_num, sunvox_engine *s );
void sunvox_print_patterns( sunvox_engine *s );
void sunvox_optimize_patterns( sunvox_engine *s );
void sunvox_pattern_set_number_of_channels( int pat_num, int cnum, sunvox_engine *s );
void sunvox_pattern_set_number_of_lines( int pat_num, int lnum, sunvox_engine *s );

void clean_std_effects_for_playing_pattern( int pat_num, sunvox_engine *s );
int sunvox_get_playing_status( sunvox_engine *s );
void sunvox_play( sunvox_engine *s );
void sunvox_play_from_beginning( sunvox_engine *s );
void sunvox_rewind( int t, sunvox_engine *s );
void sunvox_stop( sunvox_engine *s );
void sunvox_send_user_command( sunvox_note *snote, int channel_num, sunvox_engine *s );

//Buffer types:
// 0 - 16 bits (int)
// 1 - 32 bits (float)
void sunvox_render_piece_of_sound( 
    int buffer_type,
    void *buffer,
    int frames,
    int channels, 
    int freq, 
    ticks_t out_time,
    sunvox_engine *s );

#define SUNVOX_F_CHANNEL_VOL0 		0
#define SUNVOX_F_CHANNEL_VOL1 		1
#define SUNVOX_F_CHANNEL_LINES 		8
#define SUNVOX_F_CHANNEL_SYNTH_VOL	16

int sunvox_frames_get_value( int channel, sunvox_engine *s );

#endif
