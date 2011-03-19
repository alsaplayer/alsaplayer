/*
 *		reverb_e.c
 *		experimental channel effects processing
 *		provided under GNU General Public License
 *	contents : reverb ( controller 91 ) effect processing 
 *    Nicolas Witczak juillet 1998
 *	  witczak@geocities.com
 *  This file is part of the MIDI input plugin for AlsaPlayer.
 *
 *  The MIDI input plugin for AlsaPlayer is free software;
 *  you can redistribute it and/or modify it under the terms of the
 *  GNU General Public License as published by the Free Software
 *  Foundation; either version 3 of the License, or (at your option)
 *  any later version.
 *
 *  The MIDI input plugin for AlsaPlayer is distributed in the hope that
 *  it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 *  warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "gtim.h"

#ifdef CHANNEL_EFFECT

#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <stdio.h>
#ifndef __WIN32__
#include <unistd.h>
#endif
#include <stdlib.h>
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "effects.h"
#include "md.h"
#include "output.h"


/**************************************************************************/
/**	 reverb_effect structure definition
 *	formula : 
 *
 *	early reflections
 *
 *	left	
 *		yel(n) = G1 * xl( n - d1 ) + G2 * xr( n - d2 ) + G3 * xl( n - d3 )
 *
 *	right
 *		yer(n) = G1 * xr( n - d1 ) + G2 * xl( n - d2 ) + G3 * xr( n - d3 )
 *
 *	ambient reverb with embedded filter 
 *	left	
 *		yal =  xl( n ) + G4 * xl( n - d4 ) + G5 * xl( n - d5 ) + G6 * xl( n - d6 ) + G7 * ( yal( n - d7 ) - m ) 
 *
 *	right
 *		yar =  xr( n ) + G4 * xl( n - d4 ) + G6 * xr( n - d6 ) + G7 * ( yal( n - d7 ) - m ) 
 *
 *		m : average	 : used for hight pass filter	
 *
 *	outup signal
 *		s(n) = x(n) + Ge * ye(n) + Ga * ya(n) 
 */

/* >> choice of the day >> */

/** G_BIT and G : denominator size */
#define G_BITS 10
#define G	((int32)( 1 << G_BITS ))

/** max die reverb time in ms */
#define DIE_TIME 2300

/** delays in ms */
#define D1 15
#define D2 30
#define D3 54

#define D4 70
#define D5 154  
#define D6 250 

#define D7 220

/** gains*/
#define G1	0.78
#define G2	(-0.79)
#define G3	0.4

#define G4	0.87
#define G5	0.67
#define G6	0.48

#define G7	0.7

/** gain variation between left and right channels in stereo mode */
#define DG  0.05 

/** relative min gain for echos */
#define G_MIN	0.1

/** for early and ambient echo gain tuning */
#define GE_MAX	0.31  
#define GA_MAX  0.056 

/** averaging hight filter cutoff frequency in Hz */
#define FILTER_FREQU 300	

/** averaging hight filter impulsionnal response size in samples*/
static int delay_flt = 0 ;

/** time param normalized to sampling rate*/
static int32 d1 , d2 , d3 , d4 , d5 , d6 , d7;
static uint32 dieTime;

/** gain param normalized to G */
static int32 g1 = (int32)(G * G1);
static int32 g2 = (int32)(G * G2);
static int32 g3 = (int32)(G * G3);
static int32 g4 = (int32)(G * G4);
static int32 g5 = (int32)(G * G5);
static int32 g6 = (int32)(G * G6);
static int32 g7 = (int32)(G * G7);


static int32 gm ;

typedef struct 
{
/*---------------------------------------------------*/	
/* Effect base implementation */
	void *m_pfnActionMono ;
	void *m_pfnActionStereo ;
	void *m_pfnCtrlChange ;
	void *m_pfnCtrlReset ;
	void *m_pfnName ;
	void *m_pfnDestruct ;	

/*---------------------------------------------------*/	
/* additionnal parameters */

	/** m_uiNullCount : number of last null samples or 0 
	 */
	uint32 m_uiNullCount ;

	/**	l/rX/Y past samples circular buffer for x and ya left(or mono) and right */
	cirbuff leftX , leftYa , rightX , rightYa  ;

	/** gain param normalized to G : if ge == 0 inactivated */
	int32 ge , ga;

	/** average value of last 128 x samples */
	int32 m_left , m_right ;

} reverb_effect ;


/**************************************************************************/
/**	 reverb_effect function overriding
 */
static void ActionMono( reverb_effect* pThis , int32* pMonoBuffer, int32 count , int* pbSignal )
{
	if( pThis->ge == 0 )
		return ;
	if( *pbSignal )
		pThis->m_uiNullCount = 0 ;
	else
		pThis->m_uiNullCount += count ;
	if( pThis->m_uiNullCount < dieTime )
	{
		int32* pCur = pMonoBuffer;	
		int32* pEnd = pMonoBuffer + count ;				
		int x ,ye, ya ;
		for( ; pCur != pEnd ; ++ pCur )
		{			
			x = *pCur / G ;
			
			ye = (  ( (pThis->leftX.m_pCur)[d1] * g1 ) 
				+ ( (pThis->leftX.m_pCur)[d2] * g2 ) 
				+ ( (pThis->leftX.m_pCur)[d3] * g3 ) ) / G ;
			
			ya = (  ( (pThis->leftX.m_pCur)[d4] * g4 ) 
					+ ( (pThis->leftX.m_pCur)[d5] * g5 ) 
					+ ( (pThis->leftX.m_pCur)[d6] * g6 ) 
					+ ( (pThis->leftYa.m_pCur)[d7] * g7 ) 
					- ( pThis->m_left * gm )
					) / G ;			
		
			*pCur =  x * G + ( ye * pThis->ge  ) + ( ya * pThis->ga )  ;
			
			pThis->m_left += (pThis->leftYa.m_pCur)[ d7 ]  ;
			pThis->m_left -= (pThis->leftYa.m_pCur)[ d7 - delay_flt ] ;
			pushval_cirbuff( &(pThis->leftX) , x ) ;
			pushval_cirbuff( &(pThis->leftYa) , ya ) ;			
		}
		*pbSignal = 1 ;
	}
	else
	{
		shift_cirbuff( &( pThis->leftX ) , pThis->m_uiNullCount ) ;
		shift_cirbuff( &( pThis->leftYa ) , pThis->m_uiNullCount ) ;
		
		pThis->m_left = 0 ;		
	}
}

static void ActionStereo( reverb_effect* pThis , int32* pStereoBuffer , int32 count , int* pbSignal )
{
	/* int32 test = G ; */
	if( pThis->ge == 0 )
		return ;
	if( *pbSignal )
		pThis->m_uiNullCount = 0 ;
	else
		pThis->m_uiNullCount += count ;
	if( pThis->m_uiNullCount < dieTime )
	{
		int32* pCur = pStereoBuffer;	
		int32* pEnd = pStereoBuffer + 2 * count ;				
		int x , ye , ya ;

		for( ; pCur != pEnd ; ++ pCur )
		{			
			x = *pCur / G ;		
			ye = ( ( (pThis->leftX.m_pCur)[d1] * g1 ) 
				+ ( (pThis->rightX.m_pCur)[d2] * g2 ) 
				+ ( (pThis->leftX.m_pCur)[d3] * g3 ) ) / G ;
			
			ya = (  
					( (pThis->leftX.m_pCur)[d4] * g4 )
					+ ( (pThis->leftX.m_pCur)[d5] * g5 ) 
					+ ( (pThis->leftX.m_pCur)[d6] * g6 ) 
					+ ( (pThis->leftYa.m_pCur)[d7] * g7 ) 
					- ( pThis->m_left * gm )
					) / G ;		
		
			*pCur =  x * G + ( ye * pThis->ge  ) + ( ya * pThis->ga )  ;

			pThis->m_left += (pThis->leftYa.m_pCur)[ d7 ]  ;
			pThis->m_left -= (pThis->leftYa.m_pCur)[ d7 - delay_flt ] ;
			pushval_cirbuff( &(pThis->leftX) , x ) ;
			pushval_cirbuff( &(pThis->leftYa) , ya ) ;
		
			++pCur ;

			x = *pCur / G ;		
			ye = ( ( (pThis->rightX.m_pCur)[d1] * g1 ) 
				+ ( (pThis->leftX.m_pCur)[d2] * g2 ) 
				+ ( (pThis->rightX.m_pCur)[d3] * g3 ) ) / G ;	
			
			ya = (  
					( (pThis->rightX.m_pCur)[d4] * g4 )
					+ ( (pThis->rightX.m_pCur)[d5] * g5 ) 
					+ ( (pThis->rightX.m_pCur)[d6] * g6 ) 
					+ ( (pThis->rightYa.m_pCur)[d7] * g7 ) 
					- ( pThis->m_right * gm )
					) / G ;				
		
			*pCur =  x * G + ( ye * pThis->ge  ) + ( ya * pThis->ga )  ;

			pThis->m_right += (pThis->rightYa.m_pCur)[ d7 ]  ;
			pThis->m_right -= (pThis->rightYa.m_pCur)[ d7 - delay_flt ] ;
			pushval_cirbuff( &(pThis->rightX) , x ) ;
			pushval_cirbuff( &(pThis->rightYa) , ya ) ;		
		}

		*pbSignal = 1 ;
	}
	else
	{
		shift_cirbuff( &( pThis->leftX ) , pThis->m_uiNullCount ) ;
		shift_cirbuff( &( pThis->leftYa ) , pThis->m_uiNullCount ) ;
		shift_cirbuff( &( pThis->rightX ) , pThis->m_uiNullCount ) ;
		shift_cirbuff( &( pThis->rightYa ) , pThis->m_uiNullCount ) ;		
		pThis->m_left = 0 ;
		pThis->m_right = 0 ;
	}
}

static void CtrlReset( reverb_effect* pThis )
{
	if (!opt_effect_reverb) return;

	pThis->m_uiNullCount = 0 ;
	redim_cirbuff( &( pThis->leftX ) , 0 ) ;
	redim_cirbuff( &( pThis->leftYa ) , 0 ) ;	
	redim_cirbuff( &( pThis->rightX ) , 0 ) ;
	redim_cirbuff( &( pThis->rightYa ) , 0 ) ;	
	
	d1 =  1 - ( ( D1 * play_mode->rate ) / 1000 ) ;
	d2 =  1 - ( ( D2 * play_mode->rate ) / 1000 ) ;
	d3 =  1 - ( ( D3 * play_mode->rate ) / 1000 ) ;
	d4 =  1 - ( ( D4 * play_mode->rate ) / 1000 ) ;
	d5 =  1 - ( ( D5 * play_mode->rate ) / 1000 ) ;
	d6 =  1 - ( ( D6 * play_mode->rate ) / 1000 ) ;
	d7 =  1 - ( ( D7 * play_mode->rate ) / 1000 ) ;
	dieTime = ( DIE_TIME  * play_mode->rate ) / 1000 ;

	delay_flt = play_mode->rate / FILTER_FREQU ;
	gm = g7 / delay_flt ;

	pThis->ge = 0 ;
	pThis->ga = 0 ; 

	pThis->m_left = 0 ;
	pThis->m_right = 0 ;
}

static void CtrlChange( reverb_effect* pThis , MidiEvent* pCurrentEvent )
{
	int amount = pCurrentEvent->a;
	if (!opt_effect_reverb) return;

	if (amount < global_reverb) amount = global_reverb;

	if (!opt_effect_reverb) return;

	if( pCurrentEvent->type ==  ME_REVERBERATION )
	{
		if( amount != 0 )
		{
if ( 1-d6 < 0 ) fprintf(stderr,"Check reverb_e.c!\n");
			redim_cirbuff( &( pThis->leftX ) , (uint32)(1 - d6) ) ;
if ( 1+delay_flt-d6 < 0 ) fprintf(stderr,"Check reverb_e.c!\n");
			redim_cirbuff( &( pThis->leftYa ) ,  (uint32)(1 + delay_flt - d6) ) ;
			if( ! ( play_mode->encoding & PE_MONO ) )
			{
if ( 1-d6 < 0 ) fprintf(stderr,"Check reverb_e.c!\n");
				redim_cirbuff( &( pThis->rightX ) , (uint32)(1 - d6) ) ;
if ( 1+delay_flt-d6 < 0 ) fprintf(stderr,"Check reverb_e.c!\n");
				redim_cirbuff( &( pThis->rightYa) , (uint32)(1 + delay_flt - d6) ) ;
			}

			pThis->ge = (int32)( G * ( ( GE_MAX * G_MIN ) + ( GE_MAX * ( 1.0 - G_MIN ) / 126.0 ) * ( amount - 1 ) ) );			
			pThis->ga = (int32)( G * ( ( GA_MAX * G_MIN ) + ( GA_MAX * ( 1.0 - G_MIN ) / 126.0 ) * ( amount - 1 ) ) );

		}
		else
			CtrlReset( pThis ) ;
	}
}

static void Name( char* pszBuff )
{
	strcpy( pszBuff , "reverb" );
}

static void Destruct( reverb_effect* pThis  )
{
	if (!opt_effect_reverb) return;

	delete_cirbuff( &( pThis->leftX ) ) ;
	delete_cirbuff( &( pThis->leftYa ) ) ;	
	delete_cirbuff( &( pThis->rightX ) ) ;
	delete_cirbuff( &( pThis->rightYa )  ) ;	

	memset( pThis , 0 , sizeof( reverb_effect ) ) ;
	free( pThis ) ;
}


/**************************************************************************/
/**	 reverb_effect construction function prototype
 */
Effect* ReverbCtor() 
{
	reverb_effect* pReturn = 0 ;

	if (!opt_effect_reverb) return ( Effect* )pReturn ;

	pReturn =(reverb_effect*) malloc( sizeof( reverb_effect ) ) ;
	memset( pReturn , 0 , sizeof( reverb_effect ) ) ;
	
	pReturn->m_pfnActionMono = (void*)&ActionMono ;
	pReturn->m_pfnActionStereo = (void*)&ActionStereo ;
	pReturn->m_pfnCtrlChange = (void*)&CtrlChange ;
	pReturn->m_pfnCtrlReset = (void*)&CtrlReset ;
	pReturn->m_pfnName = (void*)&Name ;
	pReturn->m_pfnDestruct = (void*)&Destruct ;

	create_cirbuff( &( pReturn->leftX ) , 0 ) ;
	create_cirbuff( &( pReturn->leftYa ) , 0 ) ;	
	create_cirbuff( &( pReturn->rightX ) , 0 ) ;
	create_cirbuff( &( pReturn->rightYa ) , 0 ) ;	

	CtrlReset( pReturn ) ;
	return ( Effect* )pReturn ;
}




#endif /* CHANNEL_EFFECT */
