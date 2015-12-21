#ifndef __PSYTEXXSYNTHNET__
#define __PSYTEXXSYNTHNET__

//Use it in your synths host

#include "psynth.h"

#define PSYNTH_OUTPUT_CHANNELS	    2
#define PSYNTH_INPUT_CHANNELS	    2
#ifdef PALMOS
    #define PSYNTH_BUFFER_SIZE	    1024
#else
    #define PSYNTH_BUFFER_SIZE	    4096
#endif

//How to use:
//1) psynth_render_clear() - clear buffers (output & external instruments) and "rendered" flags;
//2) Render external synths to buffers;
//3) Render external simple samplers to OUTPUT.channels_in directly (adding);
//4) Full net rendering (buffer size = user defined; not more then MAIN_BUFFER_SIZE).
//5) Ok. All rendered sound is in the OUTPUT synth (item 0, input channels).

#define PSYNTH_FLAG_CREATE_SYNTHS	1

void psynth_init( int flags, int freq, psynth_net *pnet );
void psynth_close( psynth_net *pnet );
void psynth_clear( psynth_net *pnet );
void psynth_render_clear( int size, psynth_net *pnet );
int psynth_add_synth(  
    int (*synth)(
        PSYTEXX_SYNTH_PARAMETERS
    ), 
    const UTF8_CHAR *name, 
    int flags, 
    int x, 
    int y, 
    int instr_num, 
    psynth_net *pnet );
void psynth_synth_setup_finished( int snum, psynth_net *pnet );
void psynth_remove_synth( int snum, psynth_net *pnet );
int psynth_get_synth_by_name( UTF8_CHAR *name, psynth_net *pnet );
void psynth_make_link( int out, int in, psynth_net *pnet ); //out.link = in; Example: out = OUT; in = SYNTH
int psynth_remove_link( int out, int in, psynth_net *pnet );
void psynth_cpu_usage_clean( psynth_net *pnet );
void psynth_cpu_usage_recalc( psynth_net *pnet );
void psynth_render( int start_item, int buf_size, psynth_net *pnet );

#endif
