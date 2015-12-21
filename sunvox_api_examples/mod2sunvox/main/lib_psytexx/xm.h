#ifndef __XM__
#define __XM__

#include "core/core.h"
#include "filesystem/v3nus_fs.h"

//PsyTexx XM player. Modification for SunDog engine

#ifdef NONPALM
    //Max channels (virtual) in XM:
    #define MAX_CHANNELS	48
    #define SUBCHANNELS		4
    #define MAX_REAL_CHANNELS	( MAX_CHANNELS * SUBCHANNELS )
    //For realtime keyboard playing:
    #define CHANNELS_IN_BUFFER	4 
    //Pixel scope size for each channel:
    #define SCOPE_SIZE		256
#else
    //Max channels (virtual) in XM:
    #define MAX_CHANNELS	32
    #define SUBCHANNELS		1
    #define MAX_REAL_CHANNELS	( MAX_CHANNELS * SUBCHANNELS )
    //For realtime keyboard playing:
    #define CHANNELS_IN_BUFFER	4
    //Pixel scope size for each channel:
    #define SCOPE_SIZE		1
#endif
#define ENV_TICKS         512

#define XM_PREC (long)16	    //sample pointer (fixed point) precision 
#define INTERP_PREC ( XM_PREC - 1 ) //sample interpolation precision 

#define SIGNED_SUB64( v1, v1_p, v2, v2_p ) \
{ v1 -= v2; v1_p -= v2_p; \
if( v1_p < 0 ) { v1--; v1_p += ( (long)1 << XM_PREC ); } }

#define SIGNED_ADD64( v1, v1_p, v2, v2_p ) \
{ v1 += v2; v1_p += v2_p; \
if( v1_p > ( (long)1 << XM_PREC ) - 1 ) { v1++; v1_p -= ( (long)1 << XM_PREC ); } }

struct xmnote
{
        uchar   n;
        uchar   inst;
        uchar   vol;
        uchar   fx;
	uchar   par;

	uchar   n1;
	uchar   n2;
	uchar   n3;
};

struct pattern
{
	ulong   header_length;  //*
	uchar   reserved5;
	uchar   reserved6;
	uint16  rows;           //*
	uint16  data_size;
	uint16  reserved8;
	
	uint16  channels;
	uint16  real_rows;
	uint16  real_channels;
	
	xmnote  *pattern_data;
};

struct sample
{
        ulong        length;
        ulong        reppnt;
        ulong        replen;
        uchar        volume;
        signed char  finetune;
	uchar        type;
	uchar        panning;
	signed char  relative_note;
	uchar        reserved2;
        uchar        name[22];
	
	signed short *data;  //Sample data
};

#define EXT_INST_BYTES	    3
#define INST_FLAG_NOTE_OFF  1	//Note dead = note off

struct instrument
{
	//Offset in XM file is 0:
	ulong   instrument_size;   
	uchar   name[22];
	uchar   type;
	uint16  samples_num;       

	//Offset in XM file is 29:
	//>>> Standart info block:
	ulong   sample_header_size;
	uchar   sample_number[96];
	uint16  volume_points[24];
	uint16  panning_points[24];
	uchar   volume_points_num;
	uchar   panning_points_num;
	uchar   vol_sustain;
	uchar   vol_loop_start;
	uchar   vol_loop_end;	
	uchar   pan_sustain;
	uchar   pan_loop_start;
	uchar   pan_loop_end;
	uchar   volume_type;
	uchar   panning_type;
	uchar   vibrato_type;
	uchar   vibrato_sweep;
	uchar   vibrato_depth;
	uchar   vibrato_rate;
	uint16  volume_fadeout;
	//>>> End of standart info block. Size of this block is 212 bytes

	//Offset in XM file is 241:
	//Additional 2 bytes (empty in standart XM file):
	uchar        volume;	    //[for PsyTexx only]
        signed char  finetune;	    //[for PsyTexx only]
	
	//Offset in XM file is 243:
	//EXT_INST_BYTES starts here:
	uchar   panning;	    //[for PsyTexx only]
	signed char  relative_note; //[for PsyTexx only]
	uchar	flags;		    //[for PsyTexx only]
	
	//System use data: (not in XM file)
	uint16  volume_env[ ENV_TICKS ];  //each value is 0..1023
	uint16  panning_env[ ENV_TICKS ];
	
	sample  *samples[16];
};

struct module
{
	uchar   id_text[17];
        uchar   name[20];
	uchar   reserved1;
	uchar   tracker_name[20];
	uint16  version;
	ulong   header_size;
	uint16  length;
	uint16  restart_position;
	uint16  channels; //Number of virtual (song) channels
	uint16  patterns_num;
	uint16  instruments_num;
	uint16  freq_table;
	uint16  tempo;
	uint16  bpm;
	uchar   patterntable[256];
	
	pattern    *patterns[256];
	instrument *instruments[128];

	//Real channel number = virt_chan_num * SUBCHANNELS + real_channel_num[ virt_chan_num ];
	char    real_channel_num[ MAX_CHANNELS ];
};

struct channel
{
	ulong   enable;
	ulong   recordable;
	ulong   paused;
	long    lvalue;  //For the scope drawing
	long    rvalue;  //For the scope drawing
	long    temp_lvalue;
	long    temp_rvalue;
	long    scope[ SCOPE_SIZE ]; //Normal values are -127...127
    
	sample  *smp;
	instrument *ins;
	long    note;    //current note
	long    period;
        ulong   delta;   //integer part
	ulong   delta_p; //part after fixed point (16 bits)
        long    ticks;   //integer part
	long    ticks_p; //part after fixed point (16 bits)
	ulong   back;    //0: ticks+=delta;  1: ticks-=delta
	
	ulong   v_pos;   //volume envelope position
	ulong   p_pos;   //panning envelope position
	ulong   sustain;
	long    fadeout; //fadeout volume (0..65536). it's 65536 after the note has been triggered
	long    vib_pos; //-32..31 - autovibrato position
	long    cur_frame;//0..vibrato_sweep - current frame for autovibrato

	long    vol;     //0..64 - start volumes (level 1)
	long    vol_after_tremolo; //0..64 - volume after tremolo
	long    pan;     //0..255 - current panning (level 1) (from worknote() function)
	//for volume interpolation (in the envelope() function):
	long    env_start; //first envelope frame flag (after envelope reset)
	long    vol_step;  //current interpolation step (from xx to 0)
	long    l_delta;   //0..1024<<12 - delta for left (12 because 12bits = 4096 - max step)
	long    r_delta;   //0..1024<<12 - delta for right
	long    l_cur;     //0..1024<<12 - current volumes:
	long    r_cur;     //0..1024<<12 
	long    l_old;     //0..1024 - previous volume
	long    r_old;     //0..1024 - previous volume

	//for frequency interpolation (in the envelope() function):
	long    cur_period;  //current period
	long    period_delta;//period delta
	long    old_period;  //period from previous tick

	//effects:
	long    v_down;  //volume down speed
	long    v_up;    //volume up speed
	long    pan_down;//panning down speed
	long    pan_up;  //panning up speed
	
	long    tremolo_type;      //tremolo type: 0 - sine; 1 - ramp down; 2 - square; 3 - random
	long    tremolo_pos;       //-32..31 - tremolo position
	long    tremolo_speed;     //1..15
	long    tremolo_depth;     //1..15
	long    old_tremolo_speed; //previous tremolo speed
	long    old_tremolo_depth; //previous tremolo depth
	long    old_tremolo;       //flag for tremolo control (in effects() function)
	
	long    p_speed;    //period speed
	long    p_period;   //target period
	long    tone_porta; //tone porta flag (porta effect ON/OFF)
	
	long    arpeggio_periods[4]; //periods (deltas) for arpeggio effect
	long    arpeggio_counter;    //0..2
	channel *arpeggio_main_channel;//Parent channel
	
	long    vibrato_type;  //vibrato type: 0 - sine; 1 - ramp down; 2 - square; 3 - random
	long    vibrato_pos;   //-32..31 - vibrato position
	long    vibrato_speed; //1..15
	long    vibrato_depth; //1..15
	long    old_speed;     //previous vibrato speed
	long    old_depth;     //previous vibrato depth
	long    new_period;    //new period after vibrato
	
	long    retrig_rate;   //0..15          - retrigger sample parameter:  E9x effect 
	long    retrig_count;  //0..retrig_rate - retrigger sample counter
	long    note_cut;      //0..15          - length of note (in ticks):   ECx effect
	long    note_delay;    //0..15          - delay length (in ticks + 1): EDx effect
	xmnote  *note_pointer; //note pointer for EDx effect
	
	//old parameters for effects:
	long    old_p_speed;   //old period speed (tone porta)
	long    old_p_speed2;  //old period speed (porta up/down)
	long    old_ticks;     //for 9xx
	long    old_fine_up;   //for E1x
	long    old_fine_down; //for E2x
	long    old_slide_up;  //for EAx
	long    old_slide_down;//for EBx
	long    old_vol_up;    //for Axx
	long    old_vol_down;  //for Axx

	int     ch_effect_freq;
	int     freq_ptr;
	long    freq_r;
	long    freq_l;
	int     ch_effect_bits;
};


enum {
    XM_STATUS_STOP = 0,
    XM_STATUS_PLAY,
    XM_STATUS_PPLAY,
    XM_STATUS_PLAYLIST
};


struct xm_struct
{
    module     *song;
    channel    *channels[ MAX_REAL_CHANNELS ]; //Real channels

    long       *buffer;
    long       compression_volume; //0..32768
    
    long       status;           //Current playing status
    long       song_finished;    //Current song finished - we must to load new song (playlist mode)
    long       counter;          //Number of current buffer

    long       freq;             //sound frequency
    ulong      *g_linear_tab;      //linear frequency table
    uint16     *stereo_tab;      //stereo (volumes) table
    uchar      *g_vibrato_tab;     //sinus table for vibrato effect
    
    long       chans;            //global number of channels
    
    long       sample_int;       //sample interpolation ON/OFF
    long       freq_int;         //frequency interpolation ON/OFF
    
    long       global_volume;    //64 = normal (1.0)

    long       bpm;              //beats (4 lines) per minute (using for onetick calculate only)
    long       speed;            //speed (ticks per line)
    long       sp;               //dinamic value
    long       onetick;          //one tick size.  one pattern line has 1/2/3/4... ticks
    long       tick_number;      //current tick number (it is 0 after new line start)
    long       patternticks;     //current position (from 0 to one tick)
    long       one_subtick;      //one subtick. 1 tick = 64 subticks   ** for frequency interpolation **
    long       subtick_count;    //from 0 to subtick                   ** for frequency interpolation **
    
    long       tablepos;         //position in pat-table
    long       patternpos;       //position in current pattern
    long       jump_flag;        //flag for jump effects
    long       loop_start;       //loop pattern start
    long       loop_count;       //loop pattern counter;
    
    //Realtime note playing:
    long       cur_channel;      //Current real channel number
    char       channel_busy[ MAX_REAL_CHANNELS ];
    //For real-time playing. Notes from this buffer will be copied to main channels
    channel    ch_buf[ CHANNELS_IN_BUFFER ];
    int        ch_channels[ CHANNELS_IN_BUFFER ];
    int        ch_read_ptr;
    int        ch_write_ptr;
};

//Main functions:

void xm_init( xm_struct *xm );
void xm_close( xm_struct *xm );
void xm_set_volume( int volume, xm_struct *xm );

ulong read_long( V3_FILE f );
uint16 read_int( V3_FILE f );

void xm_pat_rewind(xm_struct *xm); //Rewind to the pattern start
void xm_rewind(xm_struct *xm);     //Rewind to the song start
void clear_struct(xm_struct *xm);
void create_envelope(uint16 *src, uint16 points, uint16 *dest);
void load_module( char *filename, xm_struct *xm );
int mod_load( V3_FILE f, xm_struct *xm );
int xm_load( V3_FILE f, xm_struct *xm );
int xm_save( char *filename, xm_struct *xm );

//Channels:

void clear_channels(xm_struct *xm);
void clean_channels(xm_struct *xm);
void new_channels( long number_of_channels, xm_struct *xm );
long play_note( long note_num, long instr_num, long pressure, xm_struct *xm );
void stop_note( long channel_num, xm_struct *xm );

//Instruments:

void new_instrument( uint16 num, 
                     char *name, 
                     uint16 samples, 
                     xm_struct *xm );
void clear_instrument( uint16 num, xm_struct *xm );
void clear_instruments( xm_struct *xm );
void clear_comments( xm_struct *xm );
void save_instrument( uint16 num, char *filename, xm_struct *xm );
void load_instrument( uint16 num, char *filename, xm_struct *xm );
void load_wav_instrument( uint16 num, FILE *f, char *name, xm_struct *xm );
void load_xi_instrument( uint16 num, FILE *f, xm_struct *xm );

//Patterns:

void new_pattern( uint16 num,
                  uint16 rows,
		  uint16 channels,
		  xm_struct *xm );
void resize_pattern( uint16 num,
                     uint16 rows,
	             uint16 channels,
		     xm_struct *xm );
void clear_pattern( uint16 num, xm_struct *xm );
void clear_patterns( xm_struct *xm );
void clean_pattern( uint16 num, xm_struct *xm );

//Playing:

int xm_callback(void*,long,void*);
void worknote(xmnote*, long, long, xm_struct*);
void envelope(long, xm_struct*);
void effects(channel*, xm_struct*);

//Samples:

void new_sample(uint16 num, 
                uint16 ins_num, 
                const char *name,
		long length,      /*length in bytes*/
		long type,
                xm_struct *xm);
void clear_sample(uint16 num, uint16 ins_num, xm_struct *xm);
void bytes2frames( sample *smp, xm_struct *xm ); //Convert sample length from bytes to frames
void frames2bytes( sample *smp, xm_struct *xm ); //Convert sample length from frames to bytes (before XM-saving)

//Song:

void close_song(xm_struct *xm);
void clear_song(xm_struct *xm);
void new_song(xm_struct *xm);
void create_silent_song(xm_struct *xm);

void set_bpm( long bpm, xm_struct *xm );
void set_speed( long speed, xm_struct *xm );

//Tables:

extern ulong g_linear_tab[768];
extern uchar g_vibrato_tab[256];
extern uint16 stereo_tab[256*2]; //l,r, l,r, l,r, ...  min/max: 0/1024

void tables_init();

#endif
