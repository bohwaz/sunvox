/*
    psynth_net.cpp.
    This file is part of the SunVox engine.
    Copyright (C) 2002 - 2008 Alex Zolotov <nightradio@gmail.com>
*/

#include "core/debug.h"
#include "utils/utils.h"
#include "psynth_net.h"

#ifdef HIRES_TIMER
#include "time/timemanager.h"
#endif

extern int psynth_generator( PSYTEXX_SYNTH_PARAMETERS );
extern int psynth_echo( PSYTEXX_SYNTH_PARAMETERS );

//Global variables:
unsigned int denorm_rand_state = 1;

//Global tables:

//Linear table for frequency calculation:
ulong g_linear_tab[ 768 ] =
{
    535232, 534749, 534266, 533784, 533303, 532822, 532341, 531861,
    531381, 530902, 530423, 529944, 529466, 528988, 528511, 528034,
    527558, 527082, 526607, 526131, 525657, 525183, 524709, 524236,
    523763, 523290, 522818, 522346, 521875, 521404, 520934, 520464,
    519994, 519525, 519057, 518588, 518121, 517653, 517186, 516720,
    516253, 515788, 515322, 514858, 514393, 513929, 513465, 513002,
    512539, 512077, 511615, 511154, 510692, 510232, 509771, 509312,
    508852, 508393, 507934, 507476, 507018, 506561, 506104, 505647,
    505191, 504735, 504280, 503825, 503371, 502917, 502463, 502010,
    501557, 501104, 500652, 500201, 499749, 499298, 498848, 498398,
    497948, 497499, 497050, 496602, 496154, 495706, 495259, 494812,
    494366, 493920, 493474, 493029, 492585, 492140, 491696, 491253,
    490809, 490367, 489924, 489482, 489041, 488600, 488159, 487718,
    487278, 486839, 486400, 485961, 485522, 485084, 484647, 484210,
    483773, 483336, 482900, 482465, 482029, 481595, 481160, 480726,
    480292, 479859, 479426, 478994, 478562, 478130, 477699, 477268,
    476837, 476407, 475977, 475548, 475119, 474690, 474262, 473834,
    473407, 472979, 472553, 472126, 471701, 471275, 470850, 470425,
    470001, 469577, 469153, 468730, 468307, 467884, 467462, 467041,
    466619, 466198, 465778, 465358, 464938, 464518, 464099, 463681,
    463262, 462844, 462427, 462010, 461593, 461177, 460760, 460345,
    459930, 459515, 459100, 458686, 458272, 457859, 457446, 457033,
    456621, 456209, 455797, 455386, 454975, 454565, 454155, 453745,
    453336, 452927, 452518, 452110, 451702, 451294, 450887, 450481,
    450074, 449668, 449262, 448857, 448452, 448048, 447644, 447240,
    446836, 446433, 446030, 445628, 445226, 444824, 444423, 444022,
    443622, 443221, 442821, 442422, 442023, 441624, 441226, 440828,
    440430, 440033, 439636, 439239, 438843, 438447, 438051, 437656,
    437261, 436867, 436473, 436079, 435686, 435293, 434900, 434508,
    434116, 433724, 433333, 432942, 432551, 432161, 431771, 431382,
    430992, 430604, 430215, 429827, 429439, 429052, 428665, 428278,
    427892, 427506, 427120, 426735, 426350, 425965, 425581, 425197,
    424813, 424430, 424047, 423665, 423283, 422901, 422519, 422138,
    421757, 421377, 420997, 420617, 420237, 419858, 419479, 419101,
    418723, 418345, 417968, 417591, 417214, 416838, 416462, 416086,
    415711, 415336, 414961, 414586, 414212, 413839, 413465, 413092,
    412720, 412347, 411975, 411604, 411232, 410862, 410491, 410121,
    409751, 409381, 409012, 408643, 408274, 407906, 407538, 407170,
    406803, 406436, 406069, 405703, 405337, 404971, 404606, 404241,
    403876, 403512, 403148, 402784, 402421, 402058, 401695, 401333,
    400970, 400609, 400247, 399886, 399525, 399165, 398805, 398445,
    398086, 397727, 397368, 397009, 396651, 396293, 395936, 395579,
    395222, 394865, 394509, 394153, 393798, 393442, 393087, 392733,
    392378, 392024, 391671, 391317, 390964, 390612, 390259, 389907,
    389556, 389204, 388853, 388502, 388152, 387802, 387452, 387102,
    386753, 386404, 386056, 385707, 385359, 385012, 384664, 384317,
    383971, 383624, 383278, 382932, 382587, 382242, 381897, 381552,
    381208, 380864, 380521, 380177, 379834, 379492, 379149, 378807,
    378466, 378124, 377783, 377442, 377102, 376762, 376422, 376082,
    375743, 375404, 375065, 374727, 374389, 374051, 373714, 373377,
    373040, 372703, 372367, 372031, 371695, 371360, 371025, 370690,
    370356, 370022, 369688, 369355, 369021, 368688, 368356, 368023,
    367691, 367360, 367028, 366697, 366366, 366036, 365706, 365376,
    365046, 364717, 364388, 364059, 363731, 363403, 363075, 362747,
    362420, 362093, 361766, 361440, 361114, 360788, 360463, 360137,
    359813, 359488, 359164, 358840, 358516, 358193, 357869, 357547,
    357224, 356902, 356580, 356258, 355937, 355616, 355295, 354974,
    354654, 354334, 354014, 353695, 353376, 353057, 352739, 352420,
    352103, 351785, 351468, 351150, 350834, 350517, 350201, 349885,
    349569, 349254, 348939, 348624, 348310, 347995, 347682, 347368,
    347055, 346741, 346429, 346116, 345804, 345492, 345180, 344869,
    344558, 344247, 343936, 343626, 343316, 343006, 342697, 342388,
    342079, 341770, 341462, 341154, 340846, 340539, 340231, 339924,
    339618, 339311, 339005, 338700, 338394, 338089, 337784, 337479,
    337175, 336870, 336566, 336263, 335959, 335656, 335354, 335051,
    334749, 334447, 334145, 333844, 333542, 333242, 332941, 332641,
    332341, 332041, 331741, 331442, 331143, 330844, 330546, 330247,
    329950, 329652, 329355, 329057, 328761, 328464, 328168, 327872,
    327576, 327280, 326985, 326690, 326395, 326101, 325807, 325513,
    325219, 324926, 324633, 324340, 324047, 323755, 323463, 323171,
    322879, 322588, 322297, 322006, 321716, 321426, 321136, 320846,
    320557, 320267, 319978, 319690, 319401, 319113, 318825, 318538,
    318250, 317963, 317676, 317390, 317103, 316817, 316532, 316246,
    315961, 315676, 315391, 315106, 314822, 314538, 314254, 313971,
    313688, 313405, 313122, 312839, 312557, 312275, 311994, 311712,
    311431, 311150, 310869, 310589, 310309, 310029, 309749, 309470,
    309190, 308911, 308633, 308354, 308076, 307798, 307521, 307243,
    306966, 306689, 306412, 306136, 305860, 305584, 305308, 305033,
    304758, 304483, 304208, 303934, 303659, 303385, 303112, 302838,
    302565, 302292, 302019, 301747, 301475, 301203, 300931, 300660,
    300388, 300117, 299847, 299576, 299306, 299036, 298766, 298497,
    298227, 297958, 297689, 297421, 297153, 296884, 296617, 296349,
    296082, 295815, 295548, 295281, 295015, 294749, 294483, 294217,
    293952, 293686, 293421, 293157, 292892, 292628, 292364, 292100,
    291837, 291574, 291311, 291048, 290785, 290523, 290261, 289999,
    289737, 289476, 289215, 288954, 288693, 288433, 288173, 287913,
    287653, 287393, 287134, 286875, 286616, 286358, 286099, 285841,
    285583, 285326, 285068, 284811, 284554, 284298, 284041, 283785,
    283529, 283273, 283017, 282762, 282507, 282252, 281998, 281743,
    281489, 281235, 280981, 280728, 280475, 280222, 279969, 279716,
    279464, 279212, 278960, 278708, 278457, 278206, 277955, 277704,
    277453, 277203, 276953, 276703, 276453, 276204, 275955, 275706,
    275457, 275209, 274960, 274712, 274465, 274217, 273970, 273722,
    273476, 273229, 272982, 272736, 272490, 272244, 271999, 271753,
    271508, 271263, 271018, 270774, 270530, 270286, 270042, 269798,
    269555, 269312, 269069, 268826, 268583, 268341, 268099, 267857
};

//Half of sinus:
uchar g_vibrato_tab[ 256 ] =
{
    0, 3, 6, 9, 12, 15, 18, 21, 25, 28, 31, 34, 37, 40, 43, 46,
    49, 52, 56, 59, 62, 65, 68, 71, 74, 77, 80, 83, 86, 89, 92, 95, 
    97, 100, 103, 106, 109, 112, 115, 117, 120, 123, 126, 128, 131, 134, 136, 139, 
    142, 144, 147, 149, 152, 154, 157, 159, 162, 164, 167, 169, 171, 174, 176, 178, 
    180, 183, 185, 187, 189, 191, 193, 195, 197, 199, 201, 203, 205, 207, 209, 211, 
    212, 214, 216, 217, 219, 221, 222, 224, 225, 227, 228, 229, 231, 232, 233, 235, 
    236, 237, 238, 239, 240, 242, 243, 243, 244, 245, 246, 247, 248, 249, 249, 250, 
    251, 251, 252, 252, 253, 253, 254, 254, 254, 255, 255, 255, 255, 255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 254, 254, 254, 253, 253, 252, 252, 251, 
    251, 250, 249, 249, 248, 247, 246, 245, 245, 244, 243, 242, 241, 240, 238, 237, 
    236, 235, 234, 232, 231, 230, 228, 227, 225, 224, 222, 221, 219, 218, 216, 214, 
    213, 211, 209, 207, 205, 203, 201, 200, 198, 196, 194, 191, 189, 187, 185, 183, 
    181, 179, 176, 174, 172, 169, 167, 165, 162, 160, 157, 155, 152, 150, 147, 145, 
    142, 139, 137, 134, 131, 129, 126, 123, 120, 118, 115, 112, 109, 106, 104, 101, 
    98, 95, 92, 89, 86, 83, 80, 77, 74, 71, 68, 65, 62, 59, 56, 53, 
    50, 47, 44, 41, 37, 34, 31, 28, 25, 22, 19, 16, 12, 9, 6, 3
};

void psynth_init( int flags, int freq, psynth_net *pnet )
{
#ifndef NOPSYNTH
    mem_set( pnet, sizeof( psynth_net ), 0 ); //Clear main struct
    pnet->items = (psynth_net_item*)MEM_NEW( HEAP_DYNAMIC, sizeof( psynth_net_item ) * 4 );

    mem_set( pnet->items, sizeof( psynth_net_item ) * 4, 0 ); //Clear items
    pnet->items_num = 4;

    //Sound info:
    pnet->sampling_freq = freq;
    pnet->global_volume = 52;

    //Create OUTPUT:
    int out = psynth_add_synth( 0, "OUT", PSYNTH_FLAG_OUTPUT, 1024-128, 512, 0, pnet );
    if( flags & PSYNTH_FLAG_CREATE_SYNTHS )
    {
	//Create default synth with echo:
	int gen = psynth_add_synth( &psynth_generator, "Generator", PSYNTH_FLAG_EXISTS, 128, 512, 0, pnet );
	pnet->items[ gen ].ctls[ 7 ].ctl_val[ 0 ] = 0; //No sustain
	pnet->items[ gen ].ctls[ 4 ].ctl_val[ 0 ] = 512; //Long release
	psynth_synth_setup_finished( gen, pnet );
	int echo = psynth_add_synth( &psynth_echo, "Echo", PSYNTH_FLAG_EXISTS, 512, 512, 0, pnet );
	psynth_synth_setup_finished( echo, pnet );
	//Make default link GENERATOR -> ECHO -> OUTPUT:
	psynth_make_link( out, echo, pnet );
	psynth_make_link( echo, gen, pnet );
    }
#endif
}

void psynth_close( psynth_net *pnet )
{
#ifndef NOPSYNTH
    if( pnet )
    {
	//Remove items:
	if( pnet->items )
	{
	    for( int i = 0; i < pnet->items_num; i++ ) psynth_remove_synth( i, pnet );
	    mem_free( pnet->items );
	}
	//Remove main struct:
	mem_free( pnet );
    }
#endif
}

//Clear the net:
void psynth_clear( psynth_net *pnet )
{
#ifndef NOPSYNTH
    if( pnet )
    {
	//Remove items:
	if( pnet->items )
	{
	    for( int i = 1; i < pnet->items_num; i++ ) psynth_remove_synth( i, pnet );
	}
    }
#endif
}

//Clear buffers (output and external instruments) and "rendered" flags:
void psynth_render_clear( int size, psynth_net *pnet )
{
#ifndef NOPSYNTH
    if( pnet )
    {
        pnet->all_synths_muted = 0;

	if( pnet->items )
	{
	    for( int i = 0; i < pnet->items_num; i++ ) 
	    {
		//Clear "rendered" flags:
		pnet->items[ i ].flags &= ~PSYNTH_FLAG_RENDERED;

		//Check the "solo" flag:
		if( pnet->items[ i ].flags & PSYNTH_FLAG_SOLO )
		    pnet->all_synths_muted = 1;

		//Clear output:
		if( ( pnet->items[ i ].flags & PSYNTH_FLAG_EXISTS ) && ( pnet->items[ i ].flags & PSYNTH_FLAG_OUTPUT ) )
		{
		    for( int c = 0; c < PSYNTH_MAX_CHANNELS; c++ )
		    {
			STYPE *ch = pnet->items[ i ].channels_in[ c ];
			if( ch )
			{
			    for( int ss = pnet->items[ i ].in_empty[ c ]; ss < size; ss++ )
				ch[ ss ] = 0;
			}
			ch = pnet->items[ i ].channels_out[ c ];
			if( ch )
			{
			    for( int ss = pnet->items[ i ].out_empty[ c ]; ss < size; ss++ )
				ch[ ss ] = 0;
			}
			pnet->items[ i ].in_empty[ c ] = size;
			pnet->items[ i ].out_empty[ c ] = size;
		    }
		}
	    }
	    if( ( pnet->items[ 0 ].flags & PSYNTH_FLAG_EXISTS ) && ( pnet->items[ 0 ].flags & PSYNTH_FLAG_OUTPUT ) )
	    {
	    }
	}
    }
#endif
}

int psynth_add_synth(  
    int (*synth)(  
	PSYTEXX_SYNTH_PARAMETERS
    ), 
    const UTF8_CHAR *name, 
    int flags, 
    int x, 
    int y, 
    int instr_num, 
    psynth_net *pnet )
{
#ifndef NOPSYNTH
    if( pnet )
    {
	//Get free item:
	int i;
	for( i = 0; i < pnet->items_num; i++ )
	{
	    if( !( pnet->items[ i ].flags & PSYNTH_FLAG_EXISTS ) ) break;
	}
	if( i == pnet->items_num )
	{
	    //No free item:
	    pnet->items = (psynth_net_item*)mem_resize( pnet->items, sizeof( psynth_net_item ) * ( pnet->items_num + 4 ) );
	    i = pnet->items_num;
	    pnet->items_num += 4;
	}
	//Add new synth:
	mem_set( &pnet->items[ i ], sizeof( pnet->items[ i ] ), 0 );
	pnet->items[ i ].synth = synth;
	pnet->items[ i ].flags = PSYNTH_FLAG_EXISTS | flags;
	if( synth ) pnet->items[ i ].flags |= synth( 0, 0, 0, 0, 0, COMMAND_GET_FLAGS, 0 );
	pnet->items[ i ].x = x;
	pnet->items[ i ].y = y;
	pnet->items[ i ].instr_num = instr_num;
	if( name )
	    mem_strcat( pnet->items[ i ].item_name, name );
	else
	{
	    if( !( flags & PSYNTH_FLAG_OUTPUT ) )
	    {
		//Set name automatically:
	    	const UTF8_CHAR *synth_name = (const UTF8_CHAR*)synth( 0, 0, 0, 0, 0, COMMAND_GET_SYNTH_NAME, 0 );
		if( synth_name == 0 || synth_name[ 0 ] == 0 ) synth_name = "SYNTH";
		mem_strcat( pnet->items[ i ].item_name, synth_name );
		int num = 0;
		for( int a = 0; a < pnet->items_num; a++ )
		{
		    if( pnet->items[ a ].synth == synth &&
			pnet->items[ a ].name_counter > num ) num = pnet->items[ a ].name_counter;
		}
		num++;
		pnet->items[ i ].name_counter = num;
		if( num > 1 )
		{
		    UTF8_CHAR num_str[ 16 ];
		    int_to_string( num, num_str );
		    mem_strcat( pnet->items[ i ].item_name, num_str );
		}
	    }
	}
	//Data chunks init:
	pnet->items[ i ].chunks = 0;
	//Init it:
	int data_size = 0;
	if( synth )
	    data_size = synth( 0, i, 0, 0, 0, COMMAND_GET_DATA_SIZE, (void*)pnet );
	if( data_size )
	    pnet->items[ i ].data_ptr = MEM_NEW( HEAP_DYNAMIC, data_size );
	else
	    pnet->items[ i ].data_ptr = 0;
	if( synth )
	    synth( pnet->items[ i ].data_ptr, i, 0, 0, 0, COMMAND_INIT, (void*)pnet );
	pnet->items[ i ].flags |= PSYNTH_FLAG_INITIALIZED;
	pnet->items[ i ].input_channels = PSYNTH_INPUT_CHANNELS;
	pnet->items[ i ].output_channels = PSYNTH_OUTPUT_CHANNELS;
	if( synth )
	{
	    //Get number of in/out channels:
	    pnet->items[ i ].input_channels = synth( pnet->items[ i ].data_ptr, i, 0, 0, 0, COMMAND_GET_INPUTS_NUM, (void*)pnet );
	    pnet->items[ i ].output_channels = synth( pnet->items[ i ].data_ptr, i, 0, 0, 0, COMMAND_GET_OUTPUTS_NUM, (void*)pnet );
	}
	//Create buffers:
	int i2;
	for( i2 = 0; i2 < PSYNTH_MAX_CHANNELS; i2++ )
	{
	    pnet->items[ i ].channels_in[ i2 ] = 0;
	    pnet->items[ i ].channels_out[ i2 ] = 0;
	}
	int max_channels = pnet->items[ i ].output_channels;
	if( pnet->items[ i ].input_channels > max_channels ) max_channels = pnet->items[ i ].input_channels;
	for( i2 = 0; i2 < max_channels; i2++ )
	{
	    pnet->items[ i ].channels_in[ i2 ] = (STYPE*)MEM_NEW( HEAP_DYNAMIC, PSYNTH_BUFFER_SIZE * sizeof( STYPE ) );
	    mem_set( pnet->items[ i ].channels_in[ i2 ], PSYNTH_BUFFER_SIZE * sizeof( STYPE ), 0 );
	    pnet->items[ i ].in_empty[ i2 ] = PSYNTH_BUFFER_SIZE;
	    if( !( pnet->items[ i ].flags & PSYNTH_FLAG_OUTPUT ) )
	    {
		pnet->items[ i ].channels_out[ i2 ] = (STYPE*)MEM_NEW( HEAP_DYNAMIC, PSYNTH_BUFFER_SIZE * sizeof( STYPE ) );
		mem_set( pnet->items[ i ].channels_out[ i2 ], PSYNTH_BUFFER_SIZE * sizeof( STYPE ), 0 );
		pnet->items[ i ].out_empty[ i2 ] = PSYNTH_BUFFER_SIZE;
	    }
	}
	//Set empty input links:
	pnet->items[ i ].input_num = 0;
	pnet->items[ i ].input_links = 0;

	return i;
    }
#endif
    return -1;
}

void psynth_synth_setup_finished( int snum, psynth_net *pnet )
{
#ifndef NOPSYNTH
    if( snum >= 0 && snum < pnet->items_num )
    {
	if( pnet->items[ snum ].synth )
	    pnet->items[ snum ].synth( pnet->items[ snum ].data_ptr, snum, 0, 0, 0, COMMAND_SETUP_FINISHED, (void*)pnet );
    }
#endif
}

void psynth_remove_synth( int snum, psynth_net *pnet )
{
#ifndef NOPSYNTH
    if( snum >= 0 && snum < pnet->items_num )
    {
	if( pnet->items[ snum ].synth )
	    pnet->items[ snum ].synth( pnet->items[ snum ].data_ptr, snum, 0, 0, 0, COMMAND_CLOSE, (void*)pnet );
	//Remove synth data:
	if( pnet->items[ snum ].data_ptr )
	    mem_free( pnet->items[ snum ].data_ptr ); 
	pnet->items[ snum ].data_ptr = 0;
	//Remove data chunks:
	psynth_clear_chunks( snum, pnet );
	//Remove synth channels:
	int i;
	for( i = 0; i < PSYNTH_MAX_CHANNELS; i++ )
	{
	    if( pnet->items[ snum ].channels_in[ i ] ) 
	    {
		mem_free( pnet->items[ snum ].channels_in[ i ] );
		pnet->items[ snum ].channels_in[ i ] = 0;
	    }
	    if( pnet->items[ snum ].channels_out[ i ] ) 
	    {
		mem_free( pnet->items[ snum ].channels_out[ i ] );
		pnet->items[ snum ].channels_out[ i ] = 0;
	    }
	}
	//Remove synth links:
	if( pnet->items[ snum ].input_num && pnet->items[ snum ].input_links )
	{
	    mem_free( pnet->items[ snum ].input_links );
	    pnet->items[ snum ].input_links = 0;
	    pnet->items[ snum ].input_num = 0;
	}
	//Remove links from another items:
	for( i = 0; i < pnet->items_num; i++ )
	{
	    if( pnet->items[ i ].flags & PSYNTH_FLAG_EXISTS )
	    {
		if( pnet->items[ i ].input_num )
		{
		    for( int l = 0; l < pnet->items[ i ].input_num; l++ )
		    {
			if( pnet->items[ i ].input_links[ l ] == snum )
			    pnet->items[ i ].input_links[ l ] = -1;
		    }
		}
	    }
	}
	//Remove synth handler:
	pnet->items[ snum ].synth = 0;

	pnet->items[ snum ].flags = 0;
    }
#endif
}

int psynth_get_synth_by_name( UTF8_CHAR *name, psynth_net *pnet )
{
    int retval = -1;
    for( int i = 0; i < pnet->items_num; i++ )
    {
	if( mem_strcmp( pnet->items[ i ].item_name, name ) == 0 ) return i;
    }
    return retval;
}

//Make link: out.link = in
void psynth_make_link( int out, int in, psynth_net *pnet )
{
#ifndef NOPSYNTH
    if( psynth_remove_link( out, in, pnet ) == 1 ) return; //Remove link, if it already exists
    if( pnet->items_num )
    if( out >= 0 && out < pnet->items_num )
    if( in >= 0 && in < pnet->items_num )
    {
	int i = 0;
	//Create array with a links:
	if( pnet->items[ out ].input_num == 0 )
	{
	    pnet->items[ out ].input_links = (int*)MEM_NEW( HEAP_DYNAMIC, 4 * sizeof( int ) );
	    pnet->items[ out ].input_num = 4;
	    for( i = 0; i < pnet->items[ out ].input_num; i++ ) pnet->items[ out ].input_links[ i ] = -1;
	}
	//Find free link:
	for( i = 0; i < pnet->items[ out ].input_num; i++ )
	{
	    if( pnet->items[ out ].input_links[ i ] < 0 ) break;
	}
	if( i == pnet->items[ out ].input_num )
	{
	    //No free space for new link:
	    pnet->items[ out ].input_links = 
		(int*)mem_resize( pnet->items[ out ].input_links, 
		                ( pnet->items[ out ].input_num + 4 ) * sizeof( int ) );
	    for( i = pnet->items[ out ].input_num; i < pnet->items[ out ].input_num + 4; i++ ) 
		pnet->items[ out ].input_links[ i ] = -1;
	    i = pnet->items[ out ].input_num;
	    pnet->items[ out ].input_num += 4;
	}
	pnet->items[ out ].input_links[ i ] = in;
    }
#endif
}

//Remove link: out.link = in
int psynth_remove_link( int out, int in, psynth_net *pnet )
{
    int retval = 0;
#ifndef NOPSYNTH
    if( pnet->items_num )
    if( out >= 0 && out < pnet->items_num )
    if( in >= 0 && in < pnet->items_num )
    {
	//Find our link:
	for( int i = 0; i < pnet->items[ in ].input_num; i++ )
	    if( pnet->items[ in ].input_links[ i ] == out )
	    {
		pnet->items[ in ].input_links[ i ] = -1;
		retval = 1;
	    }
	for( int i = 0; i < pnet->items[ out ].input_num; i++ )
	    if( pnet->items[ out ].input_links[ i ] == in )
	    {
		pnet->items[ out ].input_links[ i ] = -1;
		retval = 1;
	    }
    }
#endif
    return retval;
}

void psynth_cpu_usage_clean( psynth_net *pnet )
{
#ifdef HIRES_TIMER
    for( int i = 0; i < pnet->items_num; i++ )
    {
	psynth_net_item *item = &pnet->items[ i ];
	item->cpu_usage_ticks = 0;
	item->cpu_usage_samples = 0;
    }
#endif
}

void psynth_cpu_usage_recalc( psynth_net *pnet )
{
#ifdef HIRES_TIMER
    double cpu_usage = 0;
    for( int i = 0; i < pnet->items_num; i++ )
    {
	psynth_net_item *item = &pnet->items[ i ];
	if( item->flags & PSYNTH_FLAG_EXISTS )
        {
	    double delta = (double)item->cpu_usage_ticks / (double)time_ticks_per_second_hires();
	    double len = (double)item->cpu_usage_samples / (double)pnet->sampling_freq;
	    if( len == 0 ) 
		delta = 0;
	    else
	        delta = ( delta / len ) * 100;
	    cpu_usage += delta;
	    item->cpu_usage = (int)delta;
	}
    }
    pnet->cpu_usage = (int)cpu_usage;
#endif
}

//(Start item must be 0)
void psynth_render( int start_item, int buf_size, psynth_net *pnet )
{
#ifndef NOPSYNTH
    psynth_net_item *item = &pnet->items[ start_item ];
    if( ( item->flags & PSYNTH_FLAG_EXISTS ) && 
	!( item->flags & PSYNTH_FLAG_RENDERED )
    )
    {
	int i, ch;
	int inp;
	psynth_net_item *in;
	if( start_item == 0 )
	{
	    //Current item is the main OUTPUT
	    for( inp = 0; inp < item->input_num; inp++ )
	    {
		//For each input:
		if( item->input_links[ inp ] >= 0 && item->input_links[ inp ] < pnet->items_num )
		{
		    in = &pnet->items[ item->input_links[ inp ] ];
		    if( !( in->flags & PSYNTH_FLAG_RENDERED ) )
		    {
			//This input is not rendered yet. Do it:
			psynth_render( item->input_links[ inp ], buf_size, pnet );
		    }
		    if( in->flags & PSYNTH_FLAG_RENDERED )
		    {
			//Add input to output:
			STYPE *prev_in_channel;
			int prev_ch;
			for( ch = 0; ch < item->output_channels; ch++ )
			{
			    STYPE *in_data = in->channels_out[ ch ]; if( ch >= in->output_channels ) in_data = 0;
			    STYPE *out_data = item->channels_in[ ch ];
			    int in_ch = ch;
			    if( in_data == 0 ) { in_data = prev_in_channel; in_ch = prev_ch; }
			    prev_in_channel = in_data;
			    prev_ch = in_ch;
			    if( in_data && out_data )
				if( in->out_empty[ in_ch ] == 0 )
				{
				    for( i = 0; i < buf_size; i++ )
				    {
					out_data[ i ] += in_data[ i ];
				    }
				    //out_data is not empty anymore:
				    item->in_empty[ ch ] = 0;
				}
			}
		    }
		}
	    }
	    //Set volume:
	    if( pnet->global_volume != 256 )
	    {
		for( ch = 0; ch < item->output_channels; ch++ )
		{
		    STYPE *out_data = item->channels_in[ ch ];
		    if( out_data && item->in_empty[ ch ] == 0 )
			for( i = 0; i < buf_size; i++ )
			{
			    STYPE_CALC v = out_data[ i ];
			    v *= pnet->global_volume;
			    v /= 256;
			    out_data[ i ] = (STYPE)v;
			}
		}
	    }
	}
	else
	{
	    int no_inputs = 1;
	    for( inp = 0; inp < item->input_num; inp++ )
	    {
		//For each input:
		if( item->input_links[ inp ] >= 0 && item->input_links[ inp ] < pnet->items_num )
		{
		    no_inputs = 0;
		    in = &pnet->items[ item->input_links[ inp ] ];
		    if( !( in->flags & PSYNTH_FLAG_RENDERED ) )
		    {
			//This input is not rendered yet. Do it:
			psynth_render( item->input_links[ inp ], buf_size, pnet );
		    }
		    if( in->flags & PSYNTH_FLAG_RENDERED )
		    {
			//It's external instrument:
			STYPE *prev_in_channel = 0;
			int prev_ch;
			if( !( item->flags & PSYNTH_FLAG_RENDERED ) )
			    for( ch = 0; ch < item->output_channels; ch++ )
			    {
				STYPE *in_data = in->channels_out[ ch ]; if( ch >= in->output_channels ) in_data = 0;
				STYPE *out_data = item->channels_in[ ch ];
				int in_ch = ch;
				if( in_data == 0 ) { in_data = prev_in_channel; in_ch = prev_ch; }
				prev_in_channel = in_data;
				prev_ch = in_ch;
				if( in_data && out_data )
				{
				    int offset = in->out_empty[ in_ch ];
				    if( item->in_empty[ ch ] < offset )
					offset = item->in_empty[ ch ];
				    for( i = offset; i < buf_size; i++ )
				    {
					out_data[ i ] = in_data[ i ];
				    }
				    item->in_empty[ ch ] = in->out_empty[ in_ch ];
				}
			    }
			else
			    for( ch = 0; ch < item->output_channels; ch++ )
			    {
				STYPE *in_data = in->channels_out[ ch ]; if( ch >= in->output_channels ) in_data = 0;
				STYPE *out_data = item->channels_in[ ch ];
				int in_ch = ch;
				if( in_data == 0 ) { in_data = prev_in_channel; in_ch = prev_ch; }
				prev_in_channel = in_data;
				prev_ch = in_ch;
				if( in_data && out_data )
				    if( in->out_empty[ in_ch ] == 0 )
				    {
					for( i = 0; i < buf_size; i++ )
					{
					    out_data[ i ] += in_data[ i ];
					}
					//out_data is not empty anymore:
					item->in_empty[ ch ] = 0;
				    }
			    }
			item->flags |= PSYNTH_FLAG_RENDERED;
		    }
		}
	    }

	    if( no_inputs )
	    {
		//No inputs:
		if( item->flags & PSYNTH_FLAG_EFFECT )
		{
		    //Effect (filter):
		    for( int c = 0; c < item->input_channels; c++ )
		    {
			//Clear all inputs:
			STYPE *ch = item->channels_in[ c ];
			if( ch ) 
			{
			    for( int ss = item->in_empty[ c ]; ss < buf_size; ss++ )
				ch[ ss ] = 0;
			    item->in_empty[ c ] = buf_size;
			}
		    }
		}
	    }
	    //Render outputs:
	    int result = 0;
	    if( item->flags & PSYNTH_FLAG_INITIALIZED )
	    {
#ifdef HIRES_TIMER
                ticks_t synth_start_time;
                if( pnet->cpu_usage_enable )
                    synth_start_time = time_ticks_hires(); 
                else
                    synth_start_time = 0;
#endif
		int mute = 0;
		if( item->flags & PSYNTH_FLAG_MUTE )
		    mute = 1;
		if( pnet->all_synths_muted )
		{
		    if( item->flags & PSYNTH_FLAG_SOLO )
		    {
			mute = 0;
		    }
		    else
		    {
			if( item->flags & PSYNTH_FLAG_GENERATOR )
			{
			    mute = 1;
			}
		    }
		}

		result =
		    item->synth( item->data_ptr, 
			start_item,
			item->channels_in,
			item->channels_out,
			buf_size,
			COMMAND_RENDER_REPLACE,
			(void*)pnet );

		if( mute && result )
		{
		    int outputs_num = psynth_get_number_of_outputs( start_item, pnet );
		    for( int ch = 0; ch < outputs_num; ch++ )
		    {
			STYPE *out = item->channels_out[ ch ];
			for( int snum = 0; snum < buf_size; snum++ )
			{
			    out[ snum ] = 0;
			}
		    }
		}
#ifdef HIRES_TIMER
                ticks_t synth_end_time;
                if( pnet->cpu_usage_enable )
                    synth_end_time = time_ticks_hires();
                else
                    synth_end_time = 0;
		item->cpu_usage_ticks += synth_end_time - synth_start_time;
		item->cpu_usage_samples += buf_size;
#endif
	    }
	    if( result == 0 )
	    {
		//Clean outputs:
		for( int c = 0; c < item->output_channels; c++ )
		{
		    //Clear all inputs:
		    STYPE *ch = item->channels_out[ c ];
		    if( ch )
		    {
			for( int ss = item->out_empty[ c ]; ss < buf_size; ss++ )
			    ch[ ss ] = 0;
			item->out_empty[ c ] = buf_size;
		    }
		}
	    }
	    else
	    {
		//Output channels are not empty now:
		for( int c = 0; c < item->output_channels; c++ )
		{
		    STYPE *ch = item->channels_out[ c ];
		    if( ch ) item->out_empty[ c ] = 0;
		}
	    }
	    item->flags |= PSYNTH_FLAG_RENDERED;
	}

	//All synths was rendered. Result in the OUTPUT item
    }
#endif
}

void psynth_register_ctl( 
    int	synth_id, 
    const UTF8_CHAR *ctl_name, //For example: "Delay", "Feedback"
    const UTF8_CHAR *ctl_label, //For example: "dB", "samples"
    CTYPE ctl_min,
    CTYPE ctl_max,
    CTYPE ctl_def,
    int	type,
    CTYPE *value,
    void *net )
{
#ifndef NOPSYNTH
    psynth_net *pnet = (psynth_net*)net;
    if( pnet->items_num )
    if( synth_id >= 0 && synth_id < pnet->items_num )
    {
	psynth_control *ctl = &pnet->items[ synth_id ].ctls[ pnet->items[ synth_id ].ctls_num ];
	ctl->ctl_name = ctl_name;
	ctl->ctl_label = ctl_label;
	ctl->ctl_min = ctl_min;
	ctl->ctl_max = ctl_max;
	ctl->ctl_def = ctl_def;
	ctl->ctl_val = value;
	ctl->type = type;
	*value = ctl_def;
	pnet->items[ synth_id ].ctls_num++;
    }
#endif
}

void psynth_new_chunk( int synth_id, int num, int size, int flags, void *net )
{
#ifndef NOPSYNTH
    psynth_net *pnet = (psynth_net*)net;
    if( pnet->items_num )
    if( synth_id >= 0 && synth_id < pnet->items_num )
    {
	psynth_net_item *ss = &pnet->items[ synth_id ];
	if( ss->chunks == 0 )
	{
	    //No any chunks yet:
	    int init_chunks = 16;
	    if( num >= init_chunks ) init_chunks = num + 1;
	    ss->chunks = (char**)MEM_NEW( HEAP_DYNAMIC, init_chunks * sizeof( char* ) );
	    ss->chunk_flags = (int*)MEM_NEW( HEAP_DYNAMIC, init_chunks * sizeof( int ) );
	    mem_set( ss->chunks, init_chunks * sizeof( char* ), 0 );
	    mem_set( ss->chunk_flags, init_chunks * sizeof( int ), 0 );
	}
	//Need to resize?
	int real_chunks = mem_get_size( ss->chunks ) / sizeof( char* );
	if( num >= real_chunks )
	{
	    real_chunks = num + 1;
	    ss->chunks = (char**)mem_resize( ss->chunks, real_chunks * sizeof( char* ) );
	    ss->chunk_flags = (int*)mem_resize( ss->chunk_flags, real_chunks * sizeof( int ) );
	}
	//ok. Lets create new chunk:
	if( ss->chunks[ num ] != 0 ) 
	    mem_free( ss->chunks[ num ] );
	ss->chunks[ num ] = (char*)MEM_NEW( HEAP_DYNAMIC, size );
	ss->chunk_flags[ num ] = flags;
	mem_set( ss->chunks[ num ], size, 0 );
    }
#endif
}

void *psynth_get_chunk( int synth_id, int num, void *net )
{
    void *retval = 0;
#ifndef NOPSYNTH
    psynth_net *pnet = (psynth_net*)net;
    if( pnet->items_num )
    if( synth_id >= 0 && synth_id < pnet->items_num )
    {
	psynth_net_item *ss = &pnet->items[ synth_id ];
	if( ss->chunks )
	{
	    int amount = mem_get_size( ss->chunks ) / sizeof( char* );
	    if( num < amount )
		retval = ss->chunks[ num ];
	}
    }
#endif
    return retval;
}

int psynth_get_chunk_info( int synth_id, int num, void *net, ulong *size, int *flags )
{
    int retval = 1;
#ifndef NOPSYNTH
    psynth_net *pnet = (psynth_net*)net;
    if( pnet->items_num )
    if( synth_id >= 0 && synth_id < pnet->items_num )
    {
	psynth_net_item *ss = &pnet->items[ synth_id ];
	if( ss->chunks )
	{
	    int amount = mem_get_size( ss->chunks ) / sizeof( char* );
	    if( num < amount )
	    {
		retval = 0;
		*size = mem_get_size( ss->chunks[ num ] );
		if( ss->chunk_flags )
		    *flags = ss->chunk_flags[ num ];
	    }
	}
    }
#endif
    return retval;
}

void *psynth_resize_chunk( int synth_id, int num, ulong new_size, void *net )
{
    void *retval = 0;
#ifndef NOPSYNTH
    psynth_net *pnet = (psynth_net*)net;
    if( pnet->items_num )
    if( synth_id >= 0 && synth_id < pnet->items_num )
    {
	psynth_net_item *ss = &pnet->items[ synth_id ];
	if( ss->chunks )
	{
	    int amount = mem_get_size( ss->chunks ) / sizeof( char* );
	    if( num < amount )
	    {
		if( ss->chunks[ num ] ) ss->chunks[ num ] = (char*)mem_resize( ss->chunks[ num ], new_size );
		retval = ss->chunks[ num ];
	    }
	}
    }
#endif
    return retval;
}

void psynth_clear_chunk( int synth_id, int num, void *net )
{
#ifndef NOPSYNTH
    psynth_net *pnet = (psynth_net*)net;
    if( pnet->items_num )
    if( synth_id >= 0 && synth_id < pnet->items_num )
    {
	psynth_net_item *ss = &pnet->items[ synth_id ];
	if( ss->chunks )
	{
	    int amount = mem_get_size( ss->chunks ) / sizeof( char* );
	    if( num < amount )
	    {
		if( ss->chunks[ num ] ) mem_free( ss->chunks[ num ] );
		ss->chunks[ num ] = 0;
	    }
	}
    }
#endif
}

void psynth_clear_chunks( int synth_id, void *net )
{
#ifndef NOPSYNTH
    psynth_net *pnet = (psynth_net*)net;
    if( pnet->items_num )
    if( synth_id >= 0 && synth_id < pnet->items_num )
    {
	psynth_net_item *ss = &pnet->items[ synth_id ];
	if( ss->chunks )
	{
	    for( unsigned int a = 0; a < mem_get_size( ss->chunks ) / sizeof( char* ); a++ )
	    {
		if( ss->chunks[ a ] ) mem_free( ss->chunks[ a ] );
	    }
	    mem_free( ss->chunks );
	}
        ss->chunks = 0;
	if( ss->chunk_flags )
	{
	    mem_free( ss->chunk_flags );
	}
	ss->chunk_flags = 0;
    }
#endif
}

int psynth_get_number_of_outputs( int synth_id, void *net )
{
    int retval = 0;
#ifndef NOPSYNTH
    psynth_net *pnet = (psynth_net*)net;
    if( pnet->items_num )
    if( synth_id >= 0 && synth_id < pnet->items_num )
    {
	psynth_net_item *ss = &pnet->items[ synth_id ];
	retval = ss->output_channels;
    }
#endif
    return retval;
}

int psynth_get_number_of_inputs( int synth_id, void *net )
{
    int retval = 0;
#ifndef NOPSYNTH
    psynth_net *pnet = (psynth_net*)net;
    if( pnet->items_num )
    if( synth_id >= 0 && synth_id < pnet->items_num )
    {
	psynth_net_item *ss = &pnet->items[ synth_id ];
	retval = ss->input_channels;
    }
#endif
    return retval;
}

void psynth_set_number_of_outputs( int num, int synth_id, void *net )
{
#ifndef NOPSYNTH
    psynth_net *pnet = (psynth_net*)net;
    if( pnet->items_num )
    if( synth_id >= 0 && synth_id < pnet->items_num )
    {
	psynth_net_item *ss = &pnet->items[ synth_id ];
	ss->output_channels = num;
    }
#endif
}

void psynth_set_number_of_inputs( int num, int synth_id, void *net )
{
#ifndef NOPSYNTH
    psynth_net *pnet = (psynth_net*)net;
    if( pnet->items_num )
    if( synth_id >= 0 && synth_id < pnet->items_num )
    {
	psynth_net_item *ss = &pnet->items[ synth_id ];
	ss->input_channels = num;
    }
#endif
}
