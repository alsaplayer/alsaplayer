/*
 *		phaser_e.c
 *		experimental channel effects processing
 *		based on timidity (Unix ) from Tuukka Toivonen tt@cgs.fi	
 *		and provided under GNU General Public License
 *	contents : phaser ( controller 95 ) effect processing 
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
/**	 phaser_effect structure definition
 *	formula : 
 *
 *	output signal
 *
 *		phi = d + sw * triangle( 2Pi/T  * t )
 *
 *		y(n) =  - GP * x(n) + x( n - phi ) + GP * y( n - d )  (phase filter)
 *
 *		s = gx * x + gy * y(n) 
 */


/* >> choice of the day >> */

/** G_BIT and G : denominator size and fractionnal time part */
#define G_BITS 9
#define G	((int32)( 1 << G_BITS ))

/** average delays in ms */
#define D 1.0

/** phaser loopback gain*/
#define GP 0.5

/** relative min gain for phased signal */
#define G_MIN	0.3

/** max die time in ms */
#define DIE_TIME ( 1000 / FREQU )

/** approximate phaser frequency Hz */
#define FREQU 1.0

/** sweep ratio in percent of D */
#define SWEEP ( D / 1.01 )

/** time param normalized to sampling rate*/
static uint32 dieTime = 0 ;

/** phaser loopback gain normalized to G */
static int32 gp = ( (int32)( GP * G) ) ;

/** d_min , d_max : phaser depth and average delay relative to sampling rate and G_BITS fractionnal part for 
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
	cirbuff leftX , rightX , leftY , rightY ;
	
	/** current state for triangle phase ramp relative to fractionnal part G_BITS */
	uint32 d ;
	
	/** incremental step for phaser phase eval , calculated according to FREQU and depth */
	int32 incr;

	/** dry and phased gain normalized to G */
	int32 gx , gy ;

} phaser_effect ;

/**************************************************************************/
/**	 reverb_effect function overriding
 */
static void ActionMono( phaser_effect* pThis , int32* pMonoBuffer, int32 count , int* pbSignal )
{
	if( pThis->gy == 0 )
		return ;
	if( *pbSignal )
		pThis->m_uiNullCount = 0 ;
	else
		pThis->m_uiNullCount += count ;
	if( pThis->m_uiNullCount < dieTime )
	{
		int32* pCur = pMonoBuffer;	
		int32* pEnd = pMonoBuffer + count ;				
		int32  x , y , xd , yd , v1 , v2 , tmp ;		
		for( ; pCur != pEnd ; ++ pCur )
		{			
			x = *pCur / G ;
			
			v1 = (pThis->leftX.m_pCur)[ - ( pThis->d >> FRACTION_BITS ) ] ;
			v2 = (pThis->leftX.m_pCur)[ 1 - ( pThis->d >> FRACTION_BITS ) ] ;
			tmp = ( FRACTION_MASK - ( pThis->d & FRACTION_MASK ) ) ;		
			xd = v1 + ( ( (v2-v1) * tmp )  / FRACTION ) ;

			v1 = (pThis->leftY.m_pCur)[ - ( pThis->d >> FRACTION_BITS ) ] ;
			v2 = (pThis->leftY.m_pCur)[ 1 - ( pThis->d >> FRACTION_BITS ) ] ;
			tmp = ( FRACTION_MASK - ( pThis->d & FRACTION_MASK ) ) ;	
			yd = v1 + ( ( (v2-v1) * tmp )  / FRACTION ) ; ;

			y = ( ( - gp * x ) + ( gp * yd ) + ( G * xd ) ) / G ;

			*pCur = ( G * x ) + ( pThis->gy * y ) ;
			pushval_cirbuff( &(pThis->leftX) , x ) ;
			pushval_cirbuff( &(pThis->leftY) , y ) ;

			if( pThis->d > d_max )
			{
				pThis->incr = -abs( pThis->incr ) ;
				pThis->d  = d_max ;
			}
			else if( pThis->d < d_min )
			{
				pThis->incr = +abs( pThis->incr ) ;
				pThis->d = d_min ;
			}
			else
				pThis->d += pThis->incr ;
		}
		*pbSignal = 1 ;
	}
	else
	{
		shift_cirbuff( &( pThis->leftX ) , pThis->m_uiNullCount ) ;	
		shift_cirbuff( &( pThis->leftY ) , pThis->m_uiNullCount ) ;	
		pThis->d = d_min ;
	}
}

static void ActionStereo( phaser_effect* pThis , int32* pStereoBuffer , int32 count , int* pbSignal )
{
	if( pThis->gy == 0 )
		return ;
	if( *pbSignal )
		pThis->m_uiNullCount = 0 ;
	else
		pThis->m_uiNullCount += count ;
	if( pThis->m_uiNullCount < dieTime )
	{
		int32* pCur = pStereoBuffer;	
		int32* pEnd = pStereoBuffer + 2 * count ;				
		int32  x , y , xd , yd , v1 , v2 , tmp ;
		for( ; pCur != pEnd ; ++ pCur )
		{			
			x = *pCur / G ;
			
			v1 = (pThis->leftX.m_pCur)[ - ( pThis->d >> FRACTION_BITS ) ] ;
			v2 = (pThis->leftX.m_pCur)[ 1 - ( pThis->d >> FRACTION_BITS ) ] ;
			tmp = ( FRACTION_MASK - ( pThis->d & FRACTION_MASK ) ) ;		
			xd = v1 + ( ( (v2-v1) * tmp )  / FRACTION ) ;

			v1 = (pThis->leftY.m_pCur)[ - ( pThis->d >> FRACTION_BITS ) ] ;
			v2 = (pThis->leftY.m_pCur)[ 1 - ( pThis->d >> FRACTION_BITS ) ] ;
			tmp = ( FRACTION_MASK - ( pThis->d & FRACTION_MASK ) ) ;	
			yd = v1 + ( ( (v2-v1) * tmp )  / FRACTION ) ;

			y = ( ( - gp * x ) + ( gp * yd ) + ( G * xd ) ) / G ;

			*pCur = ( G * x ) + ( pThis->gy * y ) ;
			pushval_cirbuff( &(pThis->leftX) , x ) ;
			pushval_cirbuff( &(pThis->leftY) , y ) ;

			++pCur;

			x = *pCur / G ;
			
			v1 = (pThis->rightX.m_pCur)[ - ( pThis->d >> FRACTION_BITS ) ] ;
			v2 = (pThis->rightX.m_pCur)[ 1 - ( pThis->d >> FRACTION_BITS ) ] ;
			tmp = ( FRACTION_MASK - ( pThis->d & FRACTION_MASK ) ) ;		
			xd = v1 + ( ( (v2-v1) * tmp )  / FRACTION ) ;

			v1 = (pThis->rightY.m_pCur)[ - ( pThis->d >> FRACTION_BITS ) ] ;
			v2 = (pThis->rightY.m_pCur)[ 1 - ( pThis->d >> FRACTION_BITS ) ] ;
			tmp = ( FRACTION_MASK - ( pThis->d & FRACTION_MASK ) ) ;	
			yd = v1 + ( ( (v2-v1) * tmp )  / FRACTION ) ;

			y = ( ( - gp * x ) + ( gp * yd ) + ( G * xd ) ) / G ;

			*pCur = ( G * x ) + ( pThis->gy * y ) ;
			pushval_cirbuff( &(pThis->rightX) , x ) ;
			pushval_cirbuff( &(pThis->rightY) , y ) ;

			if( pThis->d > d_max )
			{
				pThis->incr = -abs( pThis->incr ) ;
				pThis->d  = d_max ;
			}
			else if( pThis->d < d_min )
			{
				pThis->incr = +abs( pThis->incr ) ;
				pThis->d = d_min ;
			}
			else
				pThis->d += pThis->incr ;
		}
		*pbSignal = 1 ;
	}
	else
	{
		shift_cirbuff( &( pThis->leftX ) , pThis->m_uiNullCount ) ;	
		shift_cirbuff( &( pThis->leftY ) , pThis->m_uiNullCount ) ;			
		shift_cirbuff( &( pThis->rightX ) , pThis->m_uiNullCount ) ;
		shift_cirbuff( &( pThis->rightY ) , pThis->m_uiNullCount ) ;	
		pThis->d = d_min ;
	}
}

static void CtrlReset( phaser_effect* pThis )
{
	pThis->m_uiNullCount = 0 ;
	redim_cirbuff( &( pThis->leftX ) , 0 ) ;
	redim_cirbuff( &( pThis->rightX ) , 0 ) ;	
	
	pThis->gy = 0 ;
	
	d_max = (uint32)( ( ( ( D * ( 1.0 + ( SWEEP / 2 ) ) * play_mode->rate ) / 1000 ) - 1 ) * FRACTION );
	d_min = (uint32)( ( ( ( D * ( 1.0 - ( SWEEP / 2 ) ) * play_mode->rate ) / 1000 ) - 1 ) * FRACTION );

	pThis->incr = (int32)( ( 2 * ( d_max - d_min ) * FREQU ) / play_mode->rate  );
	pThis->d =  d_min ;
	dieTime = (uint32)( ( DIE_TIME  * play_mode->rate ) / 1000 );
}

static void CtrlChange( phaser_effect* pThis , MidiEvent* pCurrentEvent )
{
	if( pCurrentEvent->b ==  ME_PHASER ||
	    (pCurrentEvent->b ==  ME_CHORUSDEPTH && XG_effect_chorus_is_phaser_flag) )
	{
		if( pCurrentEvent->a != 0 )
		{
			redim_cirbuff( &( pThis->leftX ) , ( d_max >> FRACTION_BITS ) + 1 ) ;
			redim_cirbuff( &( pThis->leftY ) , ( d_max >> FRACTION_BITS ) + 1 ) ;
			if( ! ( play_mode->encoding & PE_MONO ) )
			{
				redim_cirbuff( &( pThis->rightX ) , ( d_max >> FRACTION_BITS ) + 1 ) ;
				redim_cirbuff( &( pThis->rightY ) , ( d_max >> FRACTION_BITS ) + 1 ) ;
			}
			pThis->gy = (int32)( G * ( G_MIN  + ( ( 1.0 - G_MIN ) / 126.0 ) * ( pCurrentEvent->a - 1 ) ) );
		}
		else
			CtrlReset( pThis ) ;
	}
}

static void Name( char* pszBuff )
{
	strcpy( pszBuff , "phaser" );
}

static void Destruct( phaser_effect* pThis  )
{
	delete_cirbuff( &( pThis->leftX ) ) ;
	delete_cirbuff( &( pThis->rightX ) ) ;
	delete_cirbuff( &( pThis->leftY ) ) ;
	delete_cirbuff( &( pThis->rightY ) ) ;

	memset( pThis , 0 , sizeof( phaser_effect ) ) ;
	free( pThis ) ;
}


/**************************************************************************/
/**	 phaser_effect construction function prototype
 */
Effect* PhaserCtor() 
{
	phaser_effect* pReturn = 0 ;
	pReturn = ( phaser_effect* )malloc( sizeof( phaser_effect) ) ;
	memset( pReturn , 0 , sizeof( phaser_effect ) ) ;
	
	pReturn->m_pfnActionMono = (void*)&ActionMono ;
	pReturn->m_pfnActionStereo = (void*)&ActionStereo ;
	pReturn->m_pfnCtrlChange = (void*)&CtrlChange ;
	pReturn->m_pfnCtrlReset = (void*)&CtrlReset ;
	pReturn->m_pfnName = (void*)&Name ;
	pReturn->m_pfnDestruct = (void*)&Destruct ;

	create_cirbuff( &( pReturn->leftX ) , 0 ) ;
	create_cirbuff( &( pReturn->rightX ) , 0 ) ;
	create_cirbuff( &( pReturn->leftY ) , 0 ) ;
	create_cirbuff( &( pReturn->rightY ) , 0 ) ;

	CtrlReset( pReturn ) ;
	return ( Effect* )pReturn ;
}






#endif /* CHANNEL_EFFECT */
