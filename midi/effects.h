/*
 *		effects.h
 *		experimental channel effects processing
 *		provided under GNU General Public License
 *	contents : channel pre-mix , effect struct definition
 *    Nicolas Witczak juillet 1998
 *	  witczak@geocities.com
 */

/*
	remarks about occured changes

	+ each note from a voice was directly mixed in the common_buffer 
	in order to add channel effect we need to insert an extra and inefficient mixing
	so implementing the scheme :
			voice -> channel_buffer -> common_buffer -> output_buffer

	the only optimisation I see from now is to keep track of empty
	channel_buffer in order to skip them from processing and mixing
	
	+ I tried to keep the C but yet modular style of prog , hope it'll be usefull
	
	+ this experimental add on can be switched on and off with the define CHANNEL_EFFECT
*/

#ifndef EFFECTS_H
	#define EFFECTS_H


extern int XG_effect_chorus_is_celeste_flag;
extern int XG_effect_chorus_is_flanger_flag;
extern int XG_effect_chorus_is_phaser_flag;

/**************************************************************************/
/**	 exported from playmidi.c
 */
extern MidiEvent *event_list, *current_event;
extern uint32 sample_count, current_sample;
extern int32 *buffer_pointer;

/**************************************************************************/
/**	helpers functions : circular buffer impl */

/** used in several effect that need phase modulation */
#define FRACTION ( 1 << FRACTION_BITS ) 

/**************************************************************************/
/**	cirbuff structure : manage a circular buffer : given x it retreives x * z-1 , x * z-2 ...    
 *	accesible values are (ptr->m_pCur)[0] , (ptr->m_pCur)[-1] ... (ptr->m_pCur)[ -m_count + 1 ]
 */
typedef struct 
{
	/** m_count : active content sample count */
	uint32 m_count ;

	/** m_pCur : represent the past sample content ] pLast , m_pCur ] 
	  * transient should not be stored
	  */
	int32* m_pCur ;

	/** m_pBufCur , m_pBufLast represent the actual buffer ] m_pBufLast , m_pBufCur ] such that 
	 *		m_pBufLast <= m_pLast < m_pCur <= m_pBufCur
	 */
	int32* m_pBufCur ;
	int32* m_pBufLast ;

} cirbuff ;

/**	create_cirbuff : initialize a circular buffer given as pThis with an active size of count */
void create_cirbuff( cirbuff* pThis , uint32 count ) ;

/**	delete_cirbuff : delete a circular buffer given as pThis */
void delete_cirbuff( cirbuff* pThis ) ;

/**	redim_cirbuff : resize a circular buffer given as pThis with an new size of count 
 *	preserve content 
 */
void redim_cirbuff( cirbuff* pThis , uint32 count ) ;

/**	pushval_cirbuff : insert newSample inside pThis */
void pushval_cirbuff( cirbuff* pThis , int32 newSample ) ;

/**	shift_cirbuff : shift past samples inside pThis of uiShift complete with 0 sample val */
void shift_cirbuff( cirbuff* pThis , uint32 uiShift ) ;

/* dump_cirbuff : output a ascii dump of this buffer ( for debugging) */
void dump_cirbuff( cirbuff* pThis , FILE* pOutFile ) ;

/**************************************************************************/
/**	 Effect structure 
 *		this structure represent a particuliar kind of effect
 *	ie rev , chorus, acting on a single channel buffer 
 */
typedef struct 
{
	/** called each time a new mono data chunk is to be processed 
	 *	PARAM : this Effect derived structure
	 *	PARAM : the channel buffer to process
	 *	PARAM : number of samples to process
	 *	PARAM : 1 if this buffer is not null signal, may be set to 1 by this function
	 *  RELATED GLOBAL VAR : sample_count, current_sample (current sample counter)
	 */
	void (*m_pfnActionMono)( void* , int32* , uint32 , int* ) ;

	/** called each time a new stereo data chunk is to be processed 
	 *	PARAM : this Effect derived structure
	 *	PARAM : the channel buffer to process
	 *	PARAM : number of samples to process
	 *	PARAM : 1 if this buffer is not null signal, may be set to 1 by this function
	 *  RELATED GLOBAL VAR : sample_count, current_sample (current sample counter)
	 *	RQ : sample buffer contains 2 * PARAM2 samples
	 */
	void (*m_pfnActionStereo)( void* , int32* , uint32 , int* ) ;

	/** hook function : called when a controller have changed 
	 * PARAM : this Effect derived structure
	 */
	void (*m_pfnCtrlChange)( void* , MidiEvent* pCurrentEvent );

	/** hook function : called when a controller must be reseted in a inactive state 
	 *	PARAM : this Effect derived structure
	 */
	void (*m_pfnCtrlReset)( void* );

	/** accessor : give the name of this effect
	 *	fill the buffer given as argument ( max size 16 char including \0 )
	 */
	void (*m_pfnName)( char* );

	/** destructor function
	 *	PARAM : this Effect derived structure
	 */
	void (*m_pfnDestruct)( void* );	

} Effect;

/**************************************************************************/
/**	 Effect construction function prototype
 *	this function is called once for each effect on each channel upon
 *	it will be used
 *	PARAM : channel attached to the newly constructed Effect obj
 *		may be used to access the channel array
 *	RELATED GLOBAL VAR : channel array
 *	RETURN : shall return an allocated effect object whose structure is compatible 
 *	whith the Effect one
 */
typedef Effect* (*EFFECT_CTOR)(void) ;

/**************************************************************************/
/**	null terminated list effects types contructors
 */
extern EFFECT_CTOR effect_type_list[] ;

/* effect_list[effect type][channel] , list of 
 * active effects for each channel in the same order than former list
 * RQ1 : may contains 0 if not activated 
 * RQ2 : placed in the same than for the effect_type_list array
 */
extern Effect* effect_list[][MAXCHAN] ; 

/** effect name list 
 */
extern char effect_name[][MAXCHAN] ;

/**************************************************************************/
/**	effect_ctrl_change
 *	this function is called in order to give a chance to ctrl effect 
 *	object to update their parameters
 *	PARAM pCurrentEvent : midi event reflecting changes 
 */
void effect_ctrl_change( MidiEvent* pCurrentEvent );

/**************************************************************************/
/**	effect_ctrl_reset
 *	this function is called in order to reset all ctrl effect attached to a channel
 *	object to update their parameters
 *	PARAM idChannel : midi channel number
 */
void effect_ctrl_reset( int idChannel );

/**************************************************************************/
/**	effect_activate
 *	this function turns on or off effect processing according to iSwitch 
 */
void effect_activate( int iSwitch ) ;

#endif /*EFFECTS_H*/
/****************************************************************************************/
extern int opt_effect;
extern Effect* ChorusCtor(void) ;
extern Effect* PhaserCtor(void) ;
extern Effect* CelesteCtor(void) ;
extern Effect* ReverbCtor(void) ;
extern int init_effect(void) ;
