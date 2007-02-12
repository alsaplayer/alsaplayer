/*
 *  mac_engine.cpp (C) 2005 by Peter Lemenkov <lemenkov@newmail.ru>
 *  version: 0.0.0.6
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
#include <asm/byteorder.h> /* BE to LE */

#include <alsaplayer/alsaplayer_error.h>
#include <alsaplayer/input_plugin.h>
#include <alsaplayer/reader.h>

#include <mac/All.h>
#include <mac/MACLib.h>
#include <mac/CharacterHelper.h>

#define BLOCK_SIZE 4096	/* We can use any block size we like */

typedef struct tag_ape_local_data
{
	int total_ms;
	int total_blocks;
	IAPEDecompress* ape_file;
} ape_local_data;

static int ape_init(void) 
{
	return 1;
}

static void ape_shutdown(void)
{
	return;
}

static float ape_can_handle(const char *path)
{
	int ret = 0;
	wchar_t* pUTF16 = GetUTF16FromANSI (path);
	IAPEDecompress* temp = CreateIAPEDecompress(pUTF16, &ret);
	delete temp;
	free (pUTF16);
	if(ret == ERROR_SUCCESS)
		return 1.0;

	return 0.0;
}


static int ape_open(input_object *obj, const char *path)
{
	int ret = 0;

	ape_local_data *data;
	void* datasource = NULL;
	
	if (!obj)
		return 0;
	
	obj->local_data = malloc(sizeof(ape_local_data));

	if (!obj->local_data) {
		return 0;
	}

	data = (ape_local_data *)obj->local_data;

	if ((datasource = reader_open(path, NULL, NULL)) == NULL) {
		return 0;
	}

	obj->flags = 0;
	if (reader_seekable ((reader_type*)datasource)) {
		obj->flags |= P_SEEK;
		obj->flags |= P_PERFECTSEEK;
		obj->flags |= P_FILEBASED;
	} else {
		obj->flags |= P_STREAMBASED;
	}	
	
	wchar_t* pUTF16 = GetUTF16FromANSI (path);
	data->ape_file = CreateIAPEDecompress(pUTF16, &ret);
	free (pUTF16);
	
	obj->nr_channels = (data->ape_file)->GetInfo(APE_INFO_CHANNELS);
	obj->nr_tracks   = 1;
	
	data->total_ms		= (data->ape_file)->GetInfo(APE_DECOMPRESS_LENGTH_MS);
	data->total_blocks 	= (data->ape_file)->GetInfo(APE_DECOMPRESS_TOTAL_BLOCKS);
	
	obj->frame_size = BLOCK_SIZE;

	return 1;
}

static void ape_close(input_object *obj)
{
	ape_local_data *data;

	if (!obj)
		return;
	if (obj->local_data) {
		data = (ape_local_data *)obj->local_data;
		free (data->ape_file);
		free(obj->local_data);
		obj->local_data = NULL;
	}
}

static long ape_frame_to_sec (input_object *obj, int frame)
{
	int ret = 0;
	long	result = 0;
	ape_local_data	*data;

	if (!obj)
		return result;

	data = (ape_local_data *) obj->local_data;
	if(ret == ERROR_SUCCESS){
		result = (frame * BLOCK_SIZE) / \
		      ((data->ape_file)->GetInfo(APE_INFO_SAMPLE_RATE) * \
		       (data->ape_file)->GetInfo(APE_INFO_CHANNELS) * \
		       (data->ape_file)->GetInfo(APE_INFO_BYTES_PER_SAMPLE) / 100);
	}
	return result;
}

static int ape_sample_rate(input_object *obj)
{
	int ret = 0;
	ape_local_data *data;

	data = (ape_local_data *) obj->local_data;
	if(ret == ERROR_SUCCESS)
		ret = (data->ape_file)->GetInfo(APE_INFO_SAMPLE_RATE);

	return ret;
}

static int ape_channels(input_object *obj)
{
	int ret = 0;
	ape_local_data *data;

	data = (ape_local_data *) obj->local_data;
	if(ret == ERROR_SUCCESS)
		ret = (data->ape_file)->GetInfo(APE_INFO_CHANNELS);

	return ret;
}

static int ape_stream_info (input_object *obj, stream_info *info)
{
	int ret = 0;
	double			sampleRate;
	char			*fileType;
	ape_local_data	*data;

	if (!obj || !info)
		return 0;

	data = (ape_local_data *) obj->local_data;

	sprintf(info->stream_type, "%d channels, %dHz %s, version %.2f",
		(data->ape_file)->GetInfo(APE_INFO_CHANNELS),
		(data->ape_file)->GetInfo(APE_INFO_SAMPLE_RATE),
		"stereo", /* TODO */
		float ((data->ape_file)->GetInfo(APE_INFO_FILE_VERSION)) / 1000);
	
	strcpy(info->status, "");
	strcpy(info->artist, "");
	strcpy(info->title, "");

	return 1;
}

static int ape_nr_frames(input_object *obj)
{
	int ret = 0;
	ape_local_data *data;
	int frames	= 0;
	int64_t	l	= 0;

	if (!obj || !obj->local_data)
		return 0;
	
	data = (ape_local_data *)obj->local_data;
	
        l = (data->ape_file)->GetInfo(APE_INFO_WAV_DATA_BYTES);
	frames = (l / BLOCK_SIZE);	

	return frames;
}

static int ape_frame_size(input_object *obj)
{
	if (!obj) {
		puts("No frame size!!!!");
		return 0;
	}
	return obj->frame_size;
}

static int ape_frame_seek(input_object *obj, int frame)
{
	ape_local_data *data;
	int64_t pos;

	if (!obj)
		return 0;
	data = (ape_local_data *)obj->local_data;
	if (data) {
		if (obj->flags & P_STREAMBASED)
			return 0;
		if (data->ape_file == NULL)
			return 0;
	
		(data->ape_file)->Seek(frame * BLOCK_SIZE / 4); 
		return frame;
	}		
	return 0;
}

static int ape_play_frame (input_object *obj, char *buf)
{
	int nRead = 0;
	int i;
	char tmp;

	ape_local_data	*data;

	if (!obj)
		return 0;

	data = (ape_local_data *) obj->local_data;

	if (!data)
		return 0;

#ifdef __powerpc__
	char buf1[4096];
	(data->ape_file)->GetData (buf1, 1024, &nRead);
	for (i = 0; i < (nRead * 2); i++)
		*((__u16*)(buf+2*i)) = __le16_to_cpu(*((__u16*)(buf1+2*i)));
#else
	(data->ape_file)->GetData (buf, 1024, &nRead);
#endif
	
	if (nRead != 0)
		return 1;

	return 0;
}

static input_plugin ape_plugin;

#ifdef __cplusplus
extern "C" {
#endif

input_plugin *input_plugin_info (void)
{
	memset(&ape_plugin, 0, sizeof(input_plugin));

	ape_plugin.version 	= INPUT_PLUGIN_VERSION;
	ape_plugin.name 	= "Monkey's Audio plugin ver. 0.0.0.6";
	ape_plugin.author 	= "Peter Lemenkov";
	ape_plugin.init 	= ape_init; 
	ape_plugin.shutdown 	= ape_shutdown;
	ape_plugin.can_handle 	= ape_can_handle;
	ape_plugin.open 	= ape_open; 
	ape_plugin.close 	= ape_close;
	ape_plugin.play_frame 	= ape_play_frame; 
	ape_plugin.frame_seek 	= ape_frame_seek;
	ape_plugin.frame_size 	= ape_frame_size;
	ape_plugin.nr_frames 	= ape_nr_frames; 
	ape_plugin.frame_to_sec = ape_frame_to_sec;
	ape_plugin.sample_rate 	= ape_sample_rate;
	ape_plugin.channels 	= ape_channels; 
	ape_plugin.stream_info 	= ape_stream_info; /* TODO */ 

	return &ape_plugin;
}

#ifdef __cplusplus
}
#endif

