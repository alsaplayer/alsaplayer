/*
 *		effects.c
 *		experimental channel effects processing
 *		provided under GNU General Public License
 *	contents : channel pre-mix , effect struct definition
 *    Nicolas Witczak juillet 1998
 *	  witczak@geocities.com
 */
#include "gtim.h"

#ifdef CHANNEL_EFFECT

#include <stdio.h>
#ifndef __WIN32__
#include <unistd.h>
#endif
#include <stdlib.h>

#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "effects.h"
#include "output.h"
#include "mix.h"
#include "controls.h"

int XG_effect_chorus_is_celeste_flag = 0;
int XG_effect_chorus_is_flanger_flag = 0;
int XG_effect_chorus_is_phaser_flag = 0;

/**************************************************************************/
/**	null terminated list of effects types
extern Effect* ChorusCtor( int ) ;
extern Effect* PhaserCtor( int ) ;
extern Effect* CelesteCtor( int ) ;
extern Effect* ReverbCtor( int ) ;
 */

EFFECT_CTOR effect_type_list[]={
  ChorusCtor , PhaserCtor , CelesteCtor , ReverbCtor , 0
};

/* number of effects*/
#define NUM_EFFECTS (int)(( sizeof(effect_type_list) / sizeof(EFFECT_CTOR) ) - 1)

Effect* effect_list[ NUM_EFFECTS ][MAXCHAN] ; 

char effect_name[NUM_EFFECTS][MAXCHAN] ;

/**************************************************************************/
/**	 channel buffers and empty flags
 */
static int32 channel_buffer[MAXCHAN][AUDIO_BUFFER_SIZE*2] ; /* stereo samples */
static int channel_buffer_state[MAXCHAN] ; /* 0 means null signal , 1 non null */

void do_compute_data_effect(uint32 count);

/**************************************************************************/
/**	c_buff structure helpers functions */

#define CIRCBUFF_PARAM	8 /* define factor between active content part and actual buffer */

void create_cirbuff( cirbuff* pThis , uint32 count ) 
{
	memset( pThis , 0 , sizeof( cirbuff ) ) ;
	if( count != 0 )
	{
		pThis->m_pBufLast = (int32 *) safe_malloc( count * CIRCBUFF_PARAM * 4 ) ;
		memset( pThis->m_pBufLast , 0 , count * CIRCBUFF_PARAM * 4 ) ;
		-- pThis->m_pBufLast ;
		pThis->m_pBufCur = pThis->m_pBufLast + count * CIRCBUFF_PARAM ;
		pThis->m_pCur = pThis->m_pBufLast + count ;
		pThis->m_count = count ;
	}
}

void delete_cirbuff( cirbuff* pThis ) 
{
	if( pThis->m_pBufLast != 0 )
	{
		++ pThis->m_pBufLast ;
		free( pThis->m_pBufLast ) ;
	}
	memset( pThis , 0 , sizeof( cirbuff ) ) ;
}

void redim_cirbuff( cirbuff* pThis , uint32 count ) 
{
	if( count == 0 )
		delete_cirbuff( pThis ) ;
	else if( count <= pThis->m_count )
		pThis->m_count = count ;
	else
	{
		cirbuff CircTmp ;
		create_cirbuff( &CircTmp , count ) ;
		memcpy( CircTmp.m_pCur - pThis->m_count + 1 , pThis->m_pCur - pThis->m_count + 1, pThis->m_count * 4 ) ;
		delete_cirbuff( pThis ) ;
		memcpy( pThis , &CircTmp , sizeof( cirbuff ) ) ;
	}
}

void pushval_cirbuff( cirbuff* pThis , int32 newSample ) 
{
	if( pThis->m_pCur >= pThis->m_pBufCur )
	{ 
		memcpy( pThis->m_pBufLast + 1 , pThis->m_pCur - pThis->m_count + 2 , ( pThis->m_count - 1 ) * 4 ) ;
		pThis->m_pCur = pThis->m_pBufLast + pThis->m_count - 1 ;
	}
	++ pThis->m_pCur ;
	*(pThis->m_pCur) = newSample ;
}

void shift_cirbuff( cirbuff* pThis , uint32 uiShift )
{
	if( uiShift == 0 )
		return ;
	if( uiShift >= pThis->m_count )
	{
		memset( pThis->m_pBufLast + 1 , 0 , pThis->m_count  * 4 ) ;
		pThis->m_pCur = pThis->m_pBufLast + pThis->m_count ;
	}
	else
	{
		uint32 offset ; 
		if( pThis->m_pCur + uiShift <= pThis->m_pBufCur )
			pThis->m_pCur += uiShift ;
		else
		{
			offset = pThis->m_count - uiShift ;
			memcpy( pThis->m_pBufLast + 1 
				, pThis->m_pCur - offset + 1, offset * 4 );
			pThis->m_pCur = pThis->m_pBufLast + pThis->m_count ;
		}
		memset( pThis->m_pCur - uiShift + 1 , 0 , uiShift * 4 );
	}
}

void dump_cirbuff( cirbuff* pThis , FILE* pOutFile ) 
{
	int32* pTest ;
	fprintf( pOutFile , "{ " ) ;
	for( pTest = pThis->m_pBufLast + 1 ; pTest <= pThis->m_pBufCur ; ++ pTest )
	{
		fprintf( pOutFile , " %lu " , *pTest ) ;
		if( pTest == pThis->m_pCur - pThis->m_count )
			fprintf( pOutFile , "[ " ) ;
		if( pTest == pThis->m_pCur )
			fprintf( pOutFile , " ]" ) ;
	}
	fprintf( pOutFile , " }\n\n" ) ;
}

/** performs various buffer manipulations */
#if 0
static void DebugCircBuffer()
{
	cirbuff		test ;
	FILE* pfTest ;
	pfTest = fopen("test.txt","w");
	create_cirbuff( &test , 4 ) ;
	pushval_cirbuff( &test , 1 ) ;
	dump_cirbuff( &test , pfTest ) ;

	pushval_cirbuff( &test , 2 ) ;
	dump_cirbuff( &test , pfTest ) ;

	pushval_cirbuff( &test , 3 ) ;
	dump_cirbuff( &test , pfTest ) ;
	
	pushval_cirbuff( &test , 4 ) ;
	dump_cirbuff( &test , pfTest ) ;
	
	shift_cirbuff( &test , 2 ) ;
	dump_cirbuff( &test , pfTest ) ;
	
	pushval_cirbuff( &test , 5 ) ;
	dump_cirbuff( &test , pfTest ) ;

	redim_cirbuff( &test , 6 ) ;
	dump_cirbuff( &test , pfTest ) ;

	pushval_cirbuff( &test , 6 ) ;
	dump_cirbuff( &test , pfTest ) ;
	
	shift_cirbuff( &test , 2 ) ;
	dump_cirbuff( &test , pfTest ) ;
	
	pushval_cirbuff( &test , 7 ) ;
	dump_cirbuff( &test , pfTest ) ;	
	
	pushval_cirbuff( &test , 8 ) ;
	dump_cirbuff( &test , pfTest ) ;
	
	shift_cirbuff( &test , 3 ) ;
	dump_cirbuff( &test , pfTest ) ;
	
	delete_cirbuff( &test ) ;
	fclose( pfTest ) ;
}
#endif
/**************************************************************************/
/** do_compute_data redefined from playmidi
 */ 
void do_compute_data_effect(uint32 count)
{
	int idChannel , idVoice , idEffect;
	uint32 byteCount;
	int32 *pBuffDestEnd ;

	if( play_mode->encoding & PE_MONO )
		byteCount = count * 4 ;
	else
		byteCount = count * 8 ;

/* mix voices into channel buffers*/
	for ( idVoice = 0; idVoice < voices; ++idVoice )
	{
		if( voice[ idVoice ].status != VOICE_FREE )
		{
		  idChannel = voice[ idVoice ].channel ;
		  if (!voice[ idVoice ].sample_offset && voice[ idVoice ].echo_delay_count)
		    {
			if (voice[ idVoice ].echo_delay_count >= count) voice[ idVoice ].echo_delay_count -= count;
			else
			  {
		            mix_voice( channel_buffer[ idChannel ] + voice[ idVoice ].echo_delay_count, idVoice,
						count - voice[ idVoice ].echo_delay_count);
			    voice[ idVoice ].echo_delay_count = 0;
			  }
		    }
		  else mix_voice( channel_buffer[ idChannel ] , idVoice , count );
		  channel_buffer_state[ idChannel ] = 1 ;

		}
	}

/* apply effects*/
	if( play_mode->encoding & PE_MONO )
	for( idEffect = 0 ; idEffect < NUM_EFFECTS ; ++ idEffect )
	{		
		for( idChannel = 0 ; idChannel < MAXCHAN ; ++ idChannel )		
		{
		if( effect_list[idEffect][idChannel] != 0 )
			(( effect_list[idEffect][idChannel] )->m_pfnActionMono)
				( effect_list[idEffect][idChannel] , channel_buffer[ idChannel ] 
					, count , &(channel_buffer_state[ idChannel ]) ) ;
		}
	}
	else
	for( idEffect = 0 ; idEffect < NUM_EFFECTS ; ++ idEffect )
	{
		for( idChannel = 0 ; idChannel < MAXCHAN ; ++ idChannel )		
		{
		if( effect_list[idEffect][idChannel] != 0 )
			(( effect_list[idEffect][idChannel] )->m_pfnActionStereo)
				( effect_list[idEffect][idChannel] , channel_buffer[ idChannel ] 
					, count , &(channel_buffer_state[ idChannel ]) ) ;
		}
	}

/* clear common buffer */
	  memset(buffer_pointer, 0, byteCount );

/* mix channel buffers into common_buffer */
	if( play_mode->encoding & PE_MONO )
		pBuffDestEnd = buffer_pointer + count  ;
	else
		pBuffDestEnd = buffer_pointer + ( count * 2 ) ;

	for( idChannel = 0 ; idChannel < MAXCHAN ; ++ idChannel )
	{
		int32 *pBuffSrcCur , *pBuffDestCur ;
		if( channel_buffer_state[ idChannel ] )
		{	/* mix this channel if non empty */
			pBuffSrcCur = channel_buffer[ idChannel ] ;
			pBuffDestCur = buffer_pointer ;
			for( ; pBuffDestCur != pBuffDestEnd ; ++ pBuffDestCur , ++ pBuffSrcCur )
				*pBuffDestCur += *pBuffSrcCur ;
		}
	}

/* clear channel buffer */
	for( idChannel = 0 ; idChannel < MAXCHAN ; ++ idChannel )
	{
		if( channel_buffer_state[ idChannel ] )
		{
			memset( channel_buffer[ idChannel ] , 0, byteCount ) ;
			channel_buffer_state[ idChannel ] = 0 ;
		}
	}

	current_sample += count;
}

/** cut and paste from playmidi*/

static void do_compute_data_default(uint32 count)
{
  int i;
  if (!count) return; /* (gl) */
  memset(buffer_pointer, 0, 
	 (play_mode->encoding & PE_MONO) ? (count * 4) : (count * 8));
  for (i=0; i<voices; i++)
    {
      if(voice[i].status != VOICE_FREE)
	{
	  if (!voice[i].sample_offset && voice[i].echo_delay_count)
	    {
		if (voice[i].echo_delay_count >= count) voice[i].echo_delay_count -= count;
		else
		  {
	            mix_voice(buffer_pointer+voice[i].echo_delay_count, i, count - voice[i].echo_delay_count);
		    voice[i].echo_delay_count = 0;
		  }
	    }
	  else mix_voice(buffer_pointer, i, count);
	}
    }
  current_sample += count;
}
/**************************************************************************/
/**	switch beetween effect / no_effect mixing mode */
void (*do_compute_data)(uint32) = &do_compute_data_default ;


/**************************************************************************/
/**	fct : effect_activate
 */
void effect_activate( int iSwitch ) 
{
	if( iSwitch )
	{
		do_compute_data = &do_compute_data_effect ;
		opt_effect = 1;
	}
	else
	{
		do_compute_data = &do_compute_data_default ;
		opt_effect = 0;
	}
} 

/**************************************************************************/
/** initialize this file scope variables according to (TODO) command line
 *	switches , to be called prior any other processing
 *	return 1 if success 
 */
int init_effect()
{
	int idChannel ;
	for( idChannel = 0 ; idChannel < MAXCHAN ; ++ idChannel )
	{
		int idEffect ;
		memset( channel_buffer[ idChannel ] , 0, AUDIO_BUFFER_SIZE*8 ) ;
		channel_buffer_state[ idChannel ] = 0 ;
		for( idEffect = 0 ; idEffect < NUM_EFFECTS ; ++ idEffect ) 
		{
			effect_list[ idEffect ][ idChannel ] = effect_type_list[idEffect]() ;
			if( effect_list[ idEffect ][ idChannel ] == 0 )
				return 0 ;			
			if( idChannel == 0 )
				((effect_list[ idEffect ][ idChannel ])->m_pfnName)( effect_name[idEffect] );
		}
		effect_list[ idEffect ][ idChannel ] = 0 ;
	}
	return 1 ;
}


/**************************************************************************/
/**	fct : effect_ctrl_change
 */
void effect_ctrl_change( MidiEvent* pCurrentEvent )
{
	int idEffect ;
	for( idEffect = 0 ; idEffect < NUM_EFFECTS ; ++ idEffect )
	{
		if( effect_list[idEffect][pCurrentEvent->channel] != 0 )
			( (effect_list[idEffect][pCurrentEvent->channel])->m_pfnCtrlChange )
				( effect_list[idEffect][pCurrentEvent->channel] , pCurrentEvent ) ;
	}
}

/**************************************************************************/
/**	get info about XG effects
 */
static void reset_XG_effect_info() {

	XG_effect_chorus_is_celeste_flag =
	 XG_effect_chorus_is_flanger_flag =
	 XG_effect_chorus_is_phaser_flag = 0;

	if (XG_System_chorus_type >= 0) {
	    /* int subtype = XG_System_chorus_type & 0x07; */
	    int chtype = 0x0f & (XG_System_chorus_type >> 3);
	    switch (chtype) {
		case 0: /* no effect */
		  break;
		case 1: /* chorus */
		  break;
		case 2: /* celeste */
		  XG_effect_chorus_is_celeste_flag = 1;
		  break;
		case 3: /* flanger */
		  XG_effect_chorus_is_flanger_flag = 1;
		  break;
		case 4: /* symphonic : cf Children of the Night /128 bad, /1024 ok */
		  break;
		case 8: /* phaser */
		  XG_effect_chorus_is_phaser_flag = 1;
		  break;
	      default:
		  break;
	    }
	}
	if (XG_System_reverb_type >= 0) {
	    /* int subtype = XG_System_reverb_type & 0x07; */
	    int rtype = XG_System_reverb_type >>3;
	    switch (rtype) {
		case 0: /* no effect */
		  break;
		case 1: /* hall */
		  break;
		case 2: /* room */
		  break;
		case 3: /* stage */
		  break;
		case 4: /* plate */
		  break;
		case 16: /* white room */
		  break;
		case 17: /* tunnel */
		  break;
		case 18: /* canyon */
		  break;
		case 19: /* basement */
		  break;
	        default: break;
	    }
	}
}

/**************************************************************************/
/**	fct : effect_ctrl_reset
 */
void effect_ctrl_reset( int idChannel )
{
	int idEffect ; 

	if (!idChannel) reset_XG_effect_info();

	for( idEffect = 0 ; idEffect < NUM_EFFECTS ; ++ idEffect )
	{
		if( effect_list[idEffect][idChannel] != 0 )
			( (effect_list[idEffect][idChannel])->m_pfnCtrlReset )( effect_list[idEffect][idChannel] ) ;
	}
}

#endif /*CHANNEL_EFFECT*/
