/*
 *  wv_engine.cpp (C) 2006-2007 by Peter Lemenkov <lemenkov@gmail.com>
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

#include <wavpack/wavpack.h>

#ifdef __BIG_ENDIAN
#include <asm/byteorder.h> /* BE to LE */
#endif

#define BLOCK_SIZE 4096	/* We can use any block size we like */

static int wv_init(void) 
{
	return 1;
}

static void wv_shutdown(void)
{
	return;
}

static float wv_can_handle(const char *path)
{
	char *ext;	
	ext = strrchr(path, '.');
	
	if (!ext)
		return 0.0;
	ext++;
	if (!strcasecmp(ext, "wv")) 
			return 1.0;
	 
	return 0.0;
}

static int wv_open(input_object *obj, const char *path)
{
	char error [128];

	void* datasource = NULL;
	
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
	
	obj->local_data = WavpackOpenFileInput (path, error, OPEN_TAGS | OPEN_2CH_MAX | OPEN_NORMALIZE, 23);

	if (!obj->local_data)
		return 0;
	else
		printf ("WPC: RC[%u], NC[%u], NS[%u], SR[%u], ByPS[%u], V[%u]\n", 
				WavpackGetReducedChannels (obj->local_data), 
				WavpackGetNumChannels (obj->local_data),
				WavpackGetNumSamples (obj->local_data),
				WavpackGetSampleRate (obj->local_data),
				WavpackGetBytesPerSample (obj->local_data),
				WavpackGetVersion(obj->local_data));
	
	obj->nr_channels = WavpackGetReducedChannels (obj->local_data);
	obj->nr_tracks   = 1;
	obj->frame_size = BLOCK_SIZE;

	return 1;
}

static void wv_close(input_object *obj)
{
	if (!obj)
		return;
	if (obj->local_data){
		WavpackCloseFile (obj->local_data);
		obj->local_data = NULL;
	}
}

static long wv_frame_to_sec (input_object *obj, int frame)
{
	if (!obj || !(obj->local_data))
		return 0;

	return (frame * BLOCK_SIZE) / (WavpackGetSampleRate (obj->local_data) * WavpackGetBytesPerSample (obj->local_data) ) * 100;
}

static int wv_sample_rate(input_object *obj)
{
	if (!obj || !(obj->local_data))
		return 0;

	return WavpackGetSampleRate (obj->local_data);
}

static int wv_channels(input_object *obj)
{
	if (!obj || !(obj->local_data))
		return 0;

	return obj->nr_channels;
}

static int wv_stream_info (input_object *obj, stream_info *info)
{
	if (!info || !obj || !(obj->local_data))
		return 0;

	sprintf(info->stream_type, "%d channels, %dHz, version %.2f",
		WavpackGetNumChannels (obj->local_data),
		WavpackGetSampleRate (obj->local_data),
		WavpackGetVersion(obj->local_data));
	
	strcpy(info->status, "");
	strcpy(info->artist, "");
	strcpy(info->title, "");

	return 1;
}

static int wv_nr_frames(input_object *obj)
{
	if (!obj || !(obj->local_data))
		return 0;

	return (WavpackGetNumSamples (obj->local_data) * WavpackGetBytesPerSample (obj->local_data) / BLOCK_SIZE);	
}

static int wv_frame_size(input_object *obj)
{
	if (!obj || !(obj->local_data))
		return 0;

	return obj->frame_size;
}

static int wv_frame_seek(input_object *obj, int frame)
{
	if (!obj || !(obj->local_data) || obj->flags & P_STREAMBASED)
		return 0;

	WavpackSeekSample (obj->local_data, frame * BLOCK_SIZE / (WavpackGetBytesPerSample (obj->local_data)));
	return frame;
}

static int wv_play_frame (input_object *obj, char *buf)
{
	int i;
	uint32_t ret;

	if (!obj || !(obj->local_data))
		return 0;

	int32_t* samples = (int32_t*) calloc (BLOCK_SIZE, sizeof (int32_t));

//	ret = WavpackUnpackSamples (obj->local_data, samples, BLOCK_SIZE / WavpackGetBytesPerSample (obj->local_data));
	ret = WavpackUnpackSamples (obj->local_data, samples, BLOCK_SIZE / (WavpackGetBytesPerSample (obj->local_data) * 2));
//	ret = WavpackUnpackSamples (obj->local_data, samples, BLOCK_SIZE);
	
	if (WavpackGetBytesPerSample (obj->local_data) == 2){
		for (i = 0; i < BLOCK_SIZE / WavpackGetBytesPerSample (obj->local_data); i++){
//		for (i = 0; i < BLOCK_SIZE; i++){
#ifdef __BIG_ENDIAN
			/*
	tile.tileIndex   = __le16_to_cpu(tile.tileIndex);
	tile.indexAddon  = __le16_to_cpu(tile.indexAddon);
	tile.uniqNumber1 = __le32_to_cpu(tile.uniqNumber1);
	tile.uniqNumber2 = __le32_to_cpu(tile.uniqNumber2);
	*/
			buf[2*i]   = samples[i] >> 8;
			buf[2*i+1] = samples[i];
#else
			buf[2*i]   = samples[i];
			buf[2*i+1] = samples[i] >> 8;
#endif
		}
	}

//	printf ("WV: ret[%d]\n", ret);
	free (samples);

	if (ret == 0)
		return 0;

	return 1;
}

static input_plugin wv_plugin;

#ifdef __cplusplus
extern "C" {
#endif

input_plugin *input_plugin_info (void)
{
	memset(&wv_plugin, 0, sizeof(input_plugin));

	wv_plugin.version 	= INPUT_PLUGIN_VERSION;
	wv_plugin.name 		= "WavPack plugin ver. 0.0.0.2";
	wv_plugin.author 	= "Peter Lemenkov";
	wv_plugin.init 		= wv_init;  // DONE
	wv_plugin.shutdown 	= wv_shutdown; //DONE
	wv_plugin.can_handle 	= wv_can_handle; // DONE
	wv_plugin.open 		= wv_open;  // DONE
	wv_plugin.close 	= wv_close; // DONE
	wv_plugin.play_frame 	= wv_play_frame; // TODO 
	wv_plugin.frame_seek 	= wv_frame_seek; // DONE
	wv_plugin.frame_size 	= wv_frame_size; // DONE
	wv_plugin.nr_frames 	= wv_nr_frames; // DONE
	wv_plugin.frame_to_sec  = wv_frame_to_sec; // DONE
	wv_plugin.sample_rate 	= wv_sample_rate; // DONE
	wv_plugin.channels 	= wv_channels;  // DONE
	wv_plugin.stream_info 	= wv_stream_info; /* TODO */ 

	return &wv_plugin;
}

#ifdef __cplusplus
}
#endif

