/*
 *		chorus_e.c
 *		experimental channel effects processing
 *		provided under GNU General Public License
 *	contents : chorus ( controller 93 ) effect processing 
 *    Nicolas Witczak juillet 1998
 *	  witczak@geocities.com
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
#include "output.h"
#include "tables.h"

/**************************************************************************/
/**	 chorus_effect structure definition
 *	formula : 
 *
 *	output signal
 *
 *		phi = d + sw * triangle( 2Pi/T  * t )
 *
 *		sl(n) = xl(n) + A * x( n - phi(t) ) 
 *
 *		sr(n) = xr(n) + A * x( n - phi(t - T / 4 ) ) 
 *
 */


/* >> choice of the day >> */

/** G_BIT and G : denominator size  */
#define G_BITS 9
#define G	((int32)( 1 << G_BITS ))

/** average delays in ms */
#define D 25

/** max die time in ms */
#define DIE_TIME ( D * 2 )

/** sweep ratio in percent of D */
#define SWEEP 0.08

/** max chorus amplitude */
#define A_MAX 1.0
#define A_MIN 0.3

/** time param normalized to sampling rate*/
static uint32 dieTime = 0 ;

/** approximate chorus frequency Hz */
#define FREQU	1.5

/** d_min , d_max : chorus depth and average delay relative to sampling rate and G_BITS fractionnal part for 
 *	linear interpolation
 */
static uint32 d_min = 0 ;
static uint32 d_max = 0 ;

typedef struct 
{
/*---------------------------------------------------*/	
/* Effect base implementation */
	void *m_pfnActionMono;
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
	cirbuff leftX , rightX  ;

	/** gain param , if != 0 this effect is active*/
	int32 a ;
	
	/** current state for triangle phase ramp relative to fractionnal part G_BITS */
	uint32 d_left , d_right ;
	
	/** incremental step for chorus phase eval , calculated according to FREQU and depth */
	int32 incr_left , incr_right ;

} chorus_effect ;

/**************************************************************************/
/**	 reverb_effect function overriding
 */
static void ActionMono( chorus_effect* pThis , int32* pMonoBuffer, int32 count , int* pbSignal )
{
	if( pThis->a == 0 )
		return ;
	if( *pbSignal )
		pThis->m_uiNullCount = 0 ;
	else
		pThis->m_uiNullCount += count ;
	if( pThis->m_uiNullCount < dieTime )
	{
		int32* pCur = pMonoBuffer;	
		int32* pEnd = pMonoBuffer + count ;				
		int32 x , v1 , v2 , delta , tmp ;
		
		for( ; pCur != pEnd ; ++ pCur )
		{			
			x = *pCur / G;
			v1 = (pThis->leftX.m_pCur)[ - ( pThis->d_left >> FRACTION_BITS ) ] ;
			v2 = (pThis->leftX.m_pCur)[ 1 - ( pThis->d_left >> FRACTION_BITS ) ] ;
			tmp = ( FRACTION_MASK - ( pThis->d_left & FRACTION_MASK ) ) ;
			delta = ( (v2-v1) * tmp )  / FRACTION ;
			
			*pCur += pThis->a * ( v1 + delta ) ;			
			pushval_cirbuff( &(pThis->leftX) , x ) ;
			if( pThis->d_left > d_max )
			{
				pThis->incr_left = -abs( pThis->incr_left ) ;
				pThis->d_left  = d_max ;
			}
			else if( pThis->d_left < d_min )
			{
				pThis->incr_left = +abs( pThis->incr_left ) ;
				pThis->d_left = d_min ;
			}
			else
				pThis->d_left += pThis->incr_left ;
		}
		*pbSignal = 1 ;
	}
	else
	{
		shift_cirbuff( &( pThis->leftX ) , pThis->m_uiNullCount ) ;	
		pThis->d_left = ( d_max + d_min ) / 2 ;
	}
}

static void ActionStereo( chorus_effect* pThis , int32* pStereoBuffer , int32 count , int* pbSignal )
{
	if( pThis->a == 0 )
		return ;
	if( *pbSignal )
		pThis->m_uiNullCount = 0 ;
	else
		pThis->m_uiNullCount += count ;
	if( pThis->m_uiNullCount < dieTime )
	{
		int32* pCur = pStereoBuffer;	
		int32* pEnd = pStereoBuffer + 2 * count ;				
		int32 x , v1 , v2 , delta , tmp ;
		
		for( ; pCur != pEnd ; ++ pCur )
		{			
			x = *pCur / G ;
			v1 = (pThis->leftX.m_pCur)[ - ( pThis->d_left >> FRACTION_BITS ) ] ;
			v2 = (pThis->leftX.m_pCur)[ 1 - ( pThis->d_left >> FRACTION_BITS ) ] ;
			tmp = ( FRACTION_MASK - ( pThis->d_left & FRACTION_MASK ) ) ;
			delta = ( (v2-v1) * tmp )  / FRACTION ;
			
			*pCur += pThis->a * ( v1 + delta ) ;			
			pushval_cirbuff( &(pThis->leftX) , x ) ;
			if( pThis->d_left > d_max )
			{
				pThis->incr_left = -abs( pThis->incr_left ) ;
				pThis->d_left  = d_max ;
			}
			else if( pThis->d_left < d_min )
			{
				pThis->incr_left = +abs( pThis->incr_left ) ;
				pThis->d_left = d_min ;
			}
			else
				pThis->d_left += pThis->incr_left ;
			
			++pCur ;

			x = *pCur ;
			v1 = (pThis->rightX.m_pCur)[ - ( pThis->d_right >> FRACTION_BITS ) ] / G ;
			v2 = (pThis->rightX.m_pCur)[ 1 - ( pThis->d_right >> FRACTION_BITS ) ] / G ;
			tmp = ( FRACTION_MASK - ( pThis->d_right & FRACTION_MASK ) ) ;
			delta = ( (v2-v1) * tmp )  / FRACTION ;
			
			*pCur += pThis->a * ( v1 + delta ) ;			
			pushval_cirbuff( &(pThis->rightX) , x ) ;
			if( pThis->d_right > d_max )
			{
				pThis->incr_right = -abs( pThis->incr_right ) ;
				pThis->d_right  = d_max ;
			}
			else if( pThis->d_right < d_min )
			{
				pThis->incr_right = +abs( pThis->incr_right ) ;
				pThis->d_right = d_min ;
			}
			else
				pThis->d_right += pThis->incr_right ;
			
		}
		*pbSignal = 1 ;
	}
	else
	{
		shift_cirbuff( &( pThis->leftX ) , pThis->m_uiNullCount ) ;	
		shift_cirbuff( &( pThis->rightX ) , pThis->m_uiNullCount ) ;	
		pThis->d_left = ( d_max + d_min ) / 2 ;
		pThis->d_right = d_min ;
	}
}

static void CtrlReset( chorus_effect* pThis )
{
	pThis->m_uiNullCount = 0 ;
	redim_cirbuff( &( pThis->leftX ) , 0 ) ;
	redim_cirbuff( &( pThis->rightX ) , 0 ) ;	
	
	d_max = (uint32)( ( ( ( D * ( 1.0 + ( SWEEP / 2 ) ) * play_mode->rate ) / 1000 ) - 1 ) * FRACTION );
	d_min = (uint32)( ( ( ( D * ( 1.0 - ( SWEEP / 2 ) ) * play_mode->rate ) / 1000 ) - 1 ) * FRACTION );

	pThis->incr_left = (int32)( ( 2 * ( d_max - d_min ) * FREQU ) / play_mode->rate );
	pThis->incr_right = pThis->incr_left ;
	pThis->d_left = ( d_max + d_min ) / 2 ;
	pThis->d_right = d_min ;
	dieTime = ( DIE_TIME  * play_mode->rate ) / 1000 ;
	pThis->a = 0 ;
}

static void CtrlChange( chorus_effect* pThis , MidiEvent* pCurrentEvent )
{
	int amount = pCurrentEvent->a;
	if (amount < global_chorus) amount = global_chorus;

	if( pCurrentEvent->b ==  ME_CELESTE ||
	    (pCurrentEvent->b ==  ME_CHORUSDEPTH && XG_effect_chorus_is_celeste_flag) )
	if( pCurrentEvent->b == ME_CHORUSDEPTH && !XG_effect_chorus_is_celeste_flag &&
		!XG_effect_chorus_is_phaser_flag)
	{
		if( amount != 0 )
		{
			redim_cirbuff( &( pThis->leftX ) , ( d_max >> FRACTION_BITS ) + 1 ) ;
			if( ! ( play_mode->encoding & PE_MONO ) )
			{
				redim_cirbuff( &( pThis->rightX ) , ( d_max >> FRACTION_BITS ) + 1 ) ;
			}
			pThis->a = (int32)( G * ( A_MIN + ( A_MAX - A_MIN ) * ( amount - 1 ) / 126.0 ) );	
		}
		else
			CtrlReset( pThis ) ;
	}
}

static void Name( char* pszBuff )
{
	strcpy( pszBuff , "chorus" );
}

static void Destruct( chorus_effect* pThis  )
{
	delete_cirbuff( &( pThis->leftX ) ) ;
	delete_cirbuff( &( pThis->rightX ) ) ;

	memset( pThis , 0 , sizeof( chorus_effect ) ) ;
	free( pThis ) ;
}


/**************************************************************************/
/**	 chorus_effect construction function prototype
 */
Effect* ChorusCtor() 
{
	chorus_effect* pReturn = 0 ;
	pReturn = (chorus_effect *) malloc( sizeof( chorus_effect ) ) ;
	memset( pReturn , 0 , sizeof( chorus_effect ) ) ;
	
	pReturn->m_pfnActionMono = (void*)&ActionMono ;
	pReturn->m_pfnActionStereo = (void*)&ActionStereo ;
	pReturn->m_pfnCtrlChange = (void*)&CtrlChange ;
	pReturn->m_pfnCtrlReset = (void*)&CtrlReset ;
	pReturn->m_pfnName = (void*)&Name ;
	pReturn->m_pfnDestruct = (void*)&Destruct ;

	create_cirbuff( &( pReturn->leftX ) , 0 ) ;
	create_cirbuff( &( pReturn->rightX ) , 0 ) ;

	CtrlReset( pReturn ) ;
	return ( Effect* )pReturn ;
}






#endif /* CHANNEL_EFFECT */
