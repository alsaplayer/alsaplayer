/*
 *  ffmpeg_engine.cpp (C) 2007 by Peter Lemenkov <lemenkov@gmail.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <string.h> /* memset */
#include <stdlib.h>

#include <alsaplayer/alsaplayer_error.h>
#include <alsaplayer/input_plugin.h>
#include <alsaplayer/reader.h>

#include <avcodec.h>
#include <avformat.h>

#ifdef __BIG_ENDIAN
#include <asm/byteorder.h> /* BE to LE */
#include <unistd.h> /* swab */
#endif

#define BLOCK_SIZE AVCODEC_MAX_AUDIO_FRAME_SIZE	/* We can use any block size we like */
//#define BLOCK_SIZE 4096	/* We can use any block size we like */

typedef struct tag_ffmpeg_data {
	AVFormatContext* format;
	AVCodecContext*  codec_context;
	AVCodec*  codec;
	int audio_stream_num;
} ffmpeg_data ;

static int ffmpeg_init(void) 
{
	av_register_all ();
	return 1;
}

static void ffmpeg_shutdown(void)
{
	return;
}

static float ffmpeg_can_handle(const char *path)
{
	AVFormatContext *pFormatCtx;
	if(av_open_input_file(&pFormatCtx, path, NULL, 0, NULL)!=0)
		return 0.0;

	if(av_find_stream_info(pFormatCtx)<0)
		return 0.0;

	av_close_input_file(pFormatCtx);	
	return 1.0;
}

static int ffmpeg_open(input_object *obj, const char *path)
{
	int i;
	int audioStream = -1;
	void* datasource = NULL;
	
	/*	
	AVFormatContext* format;
	AVCodecContext*  codec_context;
	AVCodec*  codec;
	*/
	
	if (!obj)
		return 0;
	
	if ((datasource = reader_open(path, NULL, NULL)) == NULL) 
		return 0;

	obj->flags = 0;
	if (reader_seekable ((reader_type*)datasource)) {
		obj->flags |= P_SEEK;
		obj->flags |= P_PERFECTSEEK;
		obj->flags |= P_FILEBASED;
	} else 
		obj->flags |= P_STREAMBASED;
	
	obj->local_data = (ffmpeg_data*) calloc (1, sizeof (ffmpeg_data));

	if(av_open_input_file(&(((ffmpeg_data*)obj->local_data)->format), path, NULL, 0, NULL)!=0)
		return 0;

	if(av_find_stream_info((((ffmpeg_data*)obj->local_data)->format))<0)
		return 0;

	// TODO remove it later
	dump_format((((ffmpeg_data*)obj->local_data)->format), 0, path, 0);

	for(i=0; i<(((ffmpeg_data*)obj->local_data)->format)->nb_streams; i++)
		if((((ffmpeg_data*)obj->local_data)->format)->streams[i]->codec->codec_type==CODEC_TYPE_AUDIO){
			audioStream=i;
			printf ("Audiostream: %u\n", i);
			break;
		}

	if(audioStream==-1)
		return 0; // Didn't find a audio stream

	((ffmpeg_data*)obj->local_data)->audio_stream_num = audioStream;
	// Get a pointer to the codec context for the video stream
	((ffmpeg_data*)obj->local_data)->codec_context=(((ffmpeg_data*)obj->local_data)->format)->streams[audioStream]->codec;

	// Find the decoder for the stream
	((ffmpeg_data*)obj->local_data)->codec=avcodec_find_decoder((((ffmpeg_data*)obj->local_data)->codec_context)->codec_id);

	if (!((ffmpeg_data*)obj->local_data)->codec)
		return 0;

	if(avcodec_open(((ffmpeg_data*)obj->local_data)->codec_context, ((ffmpeg_data*)obj->local_data)->codec)<0)
		return 0;

	obj->nr_channels = (((ffmpeg_data*)obj->local_data)->format)->streams[audioStream]->codec->channels;
	obj->nr_tracks   = 1;
	obj->frame_size = BLOCK_SIZE;

	printf ("ffmpeg_open: return\n \
			\t\tnr_channels[%u]\n \
			\t\tnr_tracks[%u]\n \
			\t\tframe_size[%u]\n \
			\t\tpath[%s]\n \
			\t\tsamplerate[%u]\n \
			\t\tbit_rate[%u]\n \
			\t\tbits_per_sample[%d]\n \
			", 
			obj->nr_channels, 
			obj->nr_tracks, 
			obj->frame_size, 
			obj->path,
			((ffmpeg_data*)obj->local_data)->codec_context->sample_rate,
			((ffmpeg_data*)obj->local_data)->codec_context->bit_rate,
			((ffmpeg_data*)obj->local_data)->codec_context->bits_per_sample);

	return 1;
}

static void ffmpeg_close(input_object *obj)
{
	printf("ffmpeg_close: begin\n");
	if (!obj || !(obj->local_data))
		return;

	if (((ffmpeg_data*)obj->local_data)->codec_context){
		printf("ffmpeg_close: destroying codec_context\n");
		avcodec_close(((ffmpeg_data*)obj->local_data)->codec_context);
		((ffmpeg_data*)obj->local_data)->codec_context = NULL;
	}

	if (((ffmpeg_data*)obj->local_data)->format){
		printf("ffmpeg_close: destroying format\n");
		av_close_input_file (((ffmpeg_data*)obj->local_data)->format);
		((ffmpeg_data*)obj->local_data)->format = NULL;
	}

		
	printf("ffmpeg_close: destroying local_data\n");
	free (obj->local_data);
	obj->local_data = NULL;
	
	if(obj->path){
		printf("ffmpeg_close: destroying path\n");
		free(obj->path);
		obj->path = NULL;
	}
	printf("ffmpeg_close: return\n");
}

static long ffmpeg_frame_to_sec (input_object *obj, int frame)
{
	printf("ffmpeg_frame_to_sec %d\n", frame);
	AVCodecContext*  codec;

	if (!obj || !(obj->local_data) || !(((ffmpeg_data*)obj->local_data)->codec_context))
		return 0;

	codec  = ((ffmpeg_data*)obj->local_data)->codec_context;
	printf ("ffmpeg_frame_to_sec: return %u\n", (frame * obj->frame_size) / (codec->sample_rate * 2 * 2 * obj->nr_channels) * 100);
	return (frame * obj->frame_size) / (codec->sample_rate * 2 * 2 /* TODO codec->bits_per_sample */ * obj->nr_channels) * 100;
}

static int ffmpeg_sample_rate(input_object *obj)
{
	AVCodecContext*  codec;

	if (!obj || !(obj->local_data) || !(((ffmpeg_data*)obj->local_data)->codec_context))
		return 0;

	codec  = ((ffmpeg_data*)obj->local_data)->codec_context;
	printf ("ffmpeg_sample_rate: return %u\n", codec->sample_rate);
	return codec->sample_rate;
}

static int ffmpeg_channels(input_object *obj)
{
	if (!obj || !(obj->local_data) || !(((ffmpeg_data*)obj->local_data)->codec_context))
		return 0;

	printf ("ffmpeg_channels: return %u\n", obj->nr_channels);
	return obj->nr_channels;
}

static int ffmpeg_stream_info (input_object *obj, stream_info *info)
{
	printf("ffmpeg_stream_info\n");
	AVCodecContext*  codec;

	if (!obj || !(obj->local_data) || !(((ffmpeg_data*)obj->local_data)->codec_context))
		return 0;

//	codec  = ((ffmpeg_data*)obj->local_data)->pCodecCtx;
	codec  = ((ffmpeg_data*)obj->local_data)->codec_context;
	sprintf(info->stream_type, "FFmpeg\n");
	
	strcpy(info->status, "playing...");
	strcpy(info->path, obj->path);

	info->channels = obj->nr_channels;
	info->tracks = 1; // number of tracks
	info->current_track = 1; // number of current track
	info->sample_rate = codec->sample_rate;
	info->bitrate = codec->bit_rate;

	return 1;
}

static int ffmpeg_nr_frames(input_object *obj)
{
	printf("ffmpeg_nr_frames\n");
	if (!obj || !(obj->local_data) || !(((ffmpeg_data*)obj->local_data)->codec_context))
		return 0;

//	return (WavpackGetNumSamples (obj->local_data) * WavpackGetBytesPerSample (obj->local_data) * obj->nr_channels / obj->frame_size);	
	return 0;
}

static int ffmpeg_frame_size(input_object *obj)
{
	if (!obj || !(obj->local_data) || !(((ffmpeg_data*)obj->local_data)->codec_context))
		return 0;

	printf("ffmpeg_frame_size return %d\n", obj->frame_size);
	return obj->frame_size;
}

static int ffmpeg_frame_seek(input_object *obj, int frame)
{
	if (!obj || !(obj->local_data) || !(((ffmpeg_data*)obj->local_data)->codec_context) || obj->flags & P_STREAMBASED)
		return 0;

	printf("ffmpeg_frame_seek return %d\n", frame);
	return frame;
}

static int ffmpeg_play_frame (input_object *obj, char *buf)
{
	printf("ffmpeg_play_frame\n");
	AVPacket pkt;
	int i = AVCODEC_MAX_AUDIO_FRAME_SIZE;
	if (!obj || !(obj->local_data) || !(((ffmpeg_data*)obj->local_data)->codec_context))
		return 0;

	av_read_packet (((ffmpeg_data*)obj->local_data)->format, &pkt);

	avcodec_decode_audio2 (	((ffmpeg_data*)obj->local_data)->codec_context, 
					buf, 
					&i,
					pkt.data,
					pkt.size);

	swab(buf, buf, i/2);
 	av_free_packet (&pkt);
	printf("ffmpeg_play_frame return %d\n", i);
	return i;
}

static input_plugin ffmpeg_plugin;

#ifdef __cplusplus
extern "C" {
#endif

input_plugin *input_plugin_info (void)
{
	memset(&ffmpeg_plugin, 0, sizeof(input_plugin));

	ffmpeg_plugin.version		= INPUT_PLUGIN_VERSION;
	ffmpeg_plugin.name 		= "FFmpeg plugin ver. 0.0.0.0";
	ffmpeg_plugin.author		= "Peter Lemenkov";
	ffmpeg_plugin.init 		= ffmpeg_init;  // TODO
	ffmpeg_plugin.shutdown		= ffmpeg_shutdown; // TODO
	ffmpeg_plugin.can_handle 	= ffmpeg_can_handle; // TODO
	ffmpeg_plugin.open 		= ffmpeg_open;  // TODO
	ffmpeg_plugin.close		= ffmpeg_close; // TODO
	ffmpeg_plugin.play_frame 	= ffmpeg_play_frame; // TODO 
	ffmpeg_plugin.frame_seek 	= ffmpeg_frame_seek; // TODO
	ffmpeg_plugin.frame_size 	= ffmpeg_frame_size; // TODO
	ffmpeg_plugin.nr_frames 	= ffmpeg_nr_frames; // TODO
	ffmpeg_plugin.frame_to_sec	= ffmpeg_frame_to_sec; // TODO
	ffmpeg_plugin.sample_rate	= ffmpeg_sample_rate; // TODO
	ffmpeg_plugin.channels		= ffmpeg_channels;  // TODO
	ffmpeg_plugin.stream_info 	= ffmpeg_stream_info; // TODO

	return &ffmpeg_plugin;
}

#ifdef __cplusplus
}
#endif

