/*
 *		celeste_e.c
 *		experimental channel effects processing
 *		provided under GNU General Public License
 *	contents : celeste ( controller 94 ) effect processing 
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
#include "md.h"
#include "output.h"
#include "tables.h"

/**************************************************************************/
/**	 celeste_effect structure definition
 *	formula : 
 *
 *	output signal
 *
 *		phi = d + sw * triangle( 2Pi/T  * t )
 *
 *		s = x( n - phi ) 
 */

/** abs macro */
#define abs(x) ( ( (x) >= 0 ) ? (x) : (-(x)) )

/* >> choice of the day >> */

/** G_BIT and G : denominator size and fractionnal time part */
#define G_BITS 8
#define G	((int32)( 1 << G_BITS ))

/** relative min and max ampl for phi signal modulation */
#define D_MIN	0.004
#define D_MAX	0.01

/** max die time in ms */
#define DIE_TIME ( 1000 / FREQU )

/** celeste detune frequency Hz */
#define FREQU 20.0

/** time param normalized to sampling rate*/
static uint32 dieTime = 0 ;


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
	cirbuff leftX , rightX ;

	/** d_max : depth and average delay relative to sampling rate and G_BITS fractionnal part for 
	 *	linear interpolation
	 */
	uint32 d_max ;
	
	/** current state for triangle phase ramp relative to fractionnal part G_BITS */
	uint32 d ;
	
	/** incremental step for chorus phase eval , calculated according to FREQU and depth */
	int32 incr ;

} celeste_effect ;


/**************************************************************************/
/**	 reverb_effect function overriding
 */
static void ActionMono( celeste_effect* pThis , int32* pMonoBuffer, int32 count , int* pbSignal )
{
	if( pThis->d_max == 0 )
		return ;
	if( *pbSignal )
		pThis->m_uiNullCount = 0 ;
	else
		pThis->m_uiNullCount += count ;
	if( pThis->m_uiNullCount < dieTime )
	{
		int32* pCur = pMonoBuffer;	
		int32* pEnd = pMonoBuffer + count ;				
		int32  x , xd , v1 , v2 , tmp ;		
		for( ; pCur != pEnd ; ++ pCur )
		{			
			x = *pCur / G ;
			
			v1 = (pThis->leftX.m_pCur)[ - ( pThis->d >> FRACTION_BITS ) ] ;
			v2 = (pThis->leftX.m_pCur)[ 1 - ( pThis->d >> FRACTION_BITS ) ] ;
			tmp = ( FRACTION_MASK - ( pThis->d & FRACTION_MASK ) ) ;		
			xd = v1 + ( ( (v2-v1) * tmp )  / FRACTION ) ;

			*pCur = ( G * xd )  ;
			pushval_cirbuff( &(pThis->leftX) , x ) ;

			if( pThis->d > pThis->d_max )
			{
				pThis->incr = -abs( pThis->incr ) ;
				pThis->d  = pThis->d_max;
			}
			else if( pThis->d <= FRACTION )
			{
				pThis->incr = +abs( pThis->incr ) ;
				pThis->d = FRACTION + 1 ;
			}
			else
				pThis->d += pThis->incr ;
		}
		*pbSignal = 1 ;
	}
	else
	{
		shift_cirbuff( &( pThis->leftX ) , pThis->m_uiNullCount ) ;	
		pThis->d = 1 ;
	}
}

static void ActionStereo( celeste_effect* pThis , int32* pStereoBuffer , int32 count , int* pbSignal )
{
	if( pThis->d_max == 0 )
		return ;
	if( *pbSignal )
		pThis->m_uiNullCount = 0 ;
	else
		pThis->m_uiNullCount += count ;
	if( pThis->m_uiNullCount < dieTime )
	{
		int32* pCur = pStereoBuffer;	
		int32* pEnd = pStereoBuffer + 2 * count ;				
		int32  x , xd , v1 , v2 , tmp ;
		for( ; pCur != pEnd ; ++ pCur )
		{			
			x = *pCur / G ;			
			v1 = (pThis->leftX.m_pCur)[ - ( pThis->d >> FRACTION_BITS ) ] ;
			v2 = (pThis->leftX.m_pCur)[ 1 - ( pThis->d >> FRACTION_BITS ) ] ;
			tmp = ( FRACTION_MASK - ( pThis->d & FRACTION_MASK ) ) ;		
			xd = v1 + ( ( (v2-v1) * tmp )  / FRACTION ) ;
			*pCur = ( G * xd )  ;

			pushval_cirbuff( &(pThis->leftX) , x ) ;

			++pCur ;

			x = *pCur / G ;			
			v1 = (pThis->rightX.m_pCur)[ - ( pThis->d >> FRACTION_BITS ) ] ;
			v2 = (pThis->rightX.m_pCur)[ 1 - ( pThis->d >> FRACTION_BITS ) ] ;
			tmp = ( FRACTION_MASK - ( pThis->d & FRACTION_MASK ) ) ;		
			xd = v1 + ( ( (v2-v1) * tmp )  / FRACTION ) ;
			*pCur = ( G * xd )  ;
			
			pushval_cirbuff( &(pThis->rightX) , x ) ;


			if( pThis->d > pThis->d_max )
			{
				pThis->incr = -abs( pThis->incr ) ;
				pThis->d  = pThis->d_max ;
			}
			else if( pThis->d <= FRACTION )
			{
				pThis->incr = +abs( pThis->incr ) ;
				pThis->d = FRACTION + 1 ;
			}
			else
				pThis->d += pThis->incr ;
		}
		*pbSignal = 1 ;
	}
	else
	{
		shift_cirbuff( &( pThis->leftX ) , pThis->m_uiNullCount ) ;	
		shift_cirbuff( &( pThis->rightX ) , pThis->m_uiNullCount ) ;			
		pThis->d = FRACTION_BITS ;
	}
}

static void CtrlReset( celeste_effect* pThis )
{
	pThis->m_uiNullCount = 0 ;
	redim_cirbuff( &( pThis->leftX ) , 0 ) ;
	redim_cirbuff( &( pThis->rightX ) , 0 ) ;		
	pThis->d_max = 0 ;
	pThis->incr = 0 ;
	pThis->d = 0 ;
	dieTime = (uint32)( ( DIE_TIME  * play_mode->rate ) / 1000 );
}

static void CtrlChange( celeste_effect* pThis , MidiEvent* pCurrentEvent )
{
	int amount = pCurrentEvent->a;
	if (amount < global_chorus) amount = global_chorus;

	if( pCurrentEvent->b ==  ME_CELESTE ||
	    (pCurrentEvent->b ==  ME_CHORUSDEPTH && XG_effect_chorus_is_celeste_flag) )
	{
		if( amount != 0 )
		{
			pThis->d_max = (uint32)( ( ( D_MAX * play_mode->rate *  ( D_MIN  + ( ( 1.0 - D_MIN ) / 126.0 ) * ( amount - 1 ) ) ) / FREQU ) * FRACTION );
			pThis->incr = (int32)( ( 2 * pThis->d_max * FREQU ) / play_mode->rate );
			redim_cirbuff( &( pThis->leftX ) , ( pThis->d_max >> FRACTION_BITS ) + 2 ) ;
			if( ! ( play_mode->encoding & PE_MONO ) )
			{
				redim_cirbuff( &( pThis->rightX ) , ( pThis->d_max >> FRACTION_BITS ) + 2 ) ;
			}
		}
		else
			CtrlReset( pThis ) ;
	}
}

static void Name( char* pszBuff )
{
	strcpy( pszBuff , "celeste" );
}

static void Destruct( celeste_effect* pThis  )
{
	delete_cirbuff( &( pThis->leftX ) ) ;
	delete_cirbuff( &( pThis->rightX ) ) ;

	memset( pThis , 0 , sizeof( celeste_effect ) ) ;
	free( pThis ) ;
}


/**************************************************************************/
/**	 chorus_effect construction function prototype
 */
Effect* CelesteCtor(void) 
{
	celeste_effect* pReturn = 0 ;
	pReturn = ( celeste_effect* )malloc( sizeof( celeste_effect) ) ;
	memset( pReturn , 0 , sizeof( celeste_effect ) ) ;
	
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
