/*	$Id$   Greg Lee */

#ifdef PLAYLOCK
#include <semaphore.h>
#endif
struct md
{
	char midi_name[FILENAME_MAX+1];
	char midi_path_name[FILENAME_MAX+1];
	char author[128];
	char title[128];
	char midi_type[3];
	int is_playing;
	int is_open;
	unsigned char *bbuf;
	int bboffset;
	int bbcount;
	int outchunk;
	int starting_up;
	int flushing;
	int out_count;
	int total_bytes;
	int output_buffer_full;
	int output_device_open;
	int flushing_output_device;
	int super_buffer_count;
#ifdef PLAYLOCK
	sem_t play_lock;
#endif
	int xmp_epoch;
	unsigned xxmp_epoch;
	unsigned time_expired;
	unsigned last_time_expired;
	unsigned last_req_time;
	unsigned calc_window;
	unsigned last_calc;
	unsigned last_slack;
	unsigned trouble_ahead;
	MidiEvent *event;
	MidiEvent *current_event;
	MidiEventList *evlist;
	int32 event_count;
	FILE *fp;
	uint32 at;
	int32 sample_increment, sample_correction;
	int track_info;
	int curr_track;
	int curr_title_track;
	int midi_port_number;
	FLOAT_T *vol_table;
	Channel channel[MAXCHAN];
	Voice voice[MAX_VOICES];
	int voices;
	int voice_reserve;
	int32 amplification;
	FLOAT_T master_volume;
	unsigned lost_notes;
	unsigned current_polyphony;
	unsigned current_free_voices;
	unsigned current_dying_voices;
#ifdef POLYPHONY_COUNT
	int future_polyphony;
#endif
	int dont_chorus;
	int dont_reverb;
	int dont_cspline;
	int current_interpolation;
	int dont_keep_looping;

	int GM_System_On;
	int XG_System_On;
	int GS_System_On;

	int XG_System_reverb_type;
	int XG_System_chorus_type;
	int XG_System_variation_type;

	signed char drumvolume[MAXCHAN][MAXNOTE];
	signed char drumpanpot[MAXCHAN][MAXNOTE];
	signed char drumreverberation[MAXCHAN][MAXNOTE];
	signed char drumchorusdepth[MAXCHAN][MAXNOTE];
	int32 drumchannels;
	int adjust_panning_immediately;

	int32 common_buffer[AUDIO_BUFFER_SIZE*2], /* stereo samples */
             *buffer_pointer;
	uint32 buffered_count;
	uint32 sample_count;
	uint32 current_sample;
	sample_t resample_buffer[AUDIO_BUFFER_SIZE+100];
	uint32 resample_buffer_offset;
	int32 channel_buffer[MAXCHAN][AUDIO_BUFFER_SIZE*2] ; /* stereo samples */
	int channel_buffer_state[MAXCHAN] ; /* 0 means null signal , 1 non null */
	Effect* effect_list[ NUM_EFFECTS ][MAXCHAN] ; 
};
extern int play_midi_file(struct md *d);
extern int play_some_midi(struct md *d);
extern void play_midi_finish(struct md *d);
extern int skip_to(uint32 until_time, struct md *d);
extern void recompute_freq(int v, struct md *d);
extern int init_effect(struct md *d) ;
extern void effect_ctrl_change( MidiEvent* pCurrentEvent, struct md *d );
extern void effect_ctrl_reset( int idChannel, struct md *d );
extern void effect_ctrl_kill( int idChannel, struct md *d );
