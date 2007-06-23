/*
 *  mac_engine.cpp (C) 2005-2007 by Peter Lemenkov <lemenkov@gmail.com>
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

#ifdef __BIG_ENDIAN
#include <unistd.h> /* swab */
#endif

#include <alsaplayer/alsaplayer_error.h>
#include <alsaplayer/input_plugin.h>
#include <alsaplayer/reader.h>

#include <mac/All.h>
#include <mac/MACLib.h>
//#include <mac/APETag.h>
#include <mac/CharacterHelper.h>

//#include <taglib/tag_c.h>

#define BLOCK_SIZE 4096	/* We can use any block size we like */

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

	void* datasource = NULL;
	
	if (!obj)
		return 0;
	
	if ((datasource = reader_open(path, NULL, NULL)) == NULL) {
		return 0;
	}

	obj->flags = 0;
	if (reader_seekable ((reader_type*)datasource)) {
		obj->flags |= P_SEEK;
		obj->flags |= P_PERFECTSEEK;
		obj->flags |= P_FILEBASED;
	} 
	else 
		obj->flags |= P_STREAMBASED;
	
	wchar_t* pUTF16 = GetUTF16FromANSI (path);
	obj->local_data = CreateIAPEDecompress(pUTF16, &ret);
	free (pUTF16);
	
	obj->nr_channels = ((IAPEDecompress*) obj->local_data)->GetInfo(APE_INFO_CHANNELS);
	obj->nr_tracks   = 1;
	obj->frame_size = BLOCK_SIZE;
	obj->path = (char*)malloc (strlen(path));
	strcpy (obj->path, path);

	return 1;
}

static void ape_close(input_object *obj)
{
	if (!obj || !(obj->local_data))
		return;
	
	free (obj->local_data);
	obj->local_data = NULL;
	
	if(obj->path){
		free(obj->path);
		obj->path = NULL;
	}
}

static long ape_frame_to_sec (input_object *obj, int frame)
{
	if (!obj || !(obj->local_data))
		return 0;

	return (frame * obj->frame_size) / \
	      (((IAPEDecompress*) obj->local_data)->GetInfo(APE_INFO_SAMPLE_RATE) * \
	       obj->nr_channels * \
	       ((IAPEDecompress*) obj->local_data)->GetInfo(APE_INFO_BYTES_PER_SAMPLE) / 100);
}

static int ape_sample_rate(input_object *obj)
{
	if (!obj || !(obj->local_data))
		return 0;

	return ((IAPEDecompress*) obj->local_data)->GetInfo(APE_INFO_SAMPLE_RATE);
}

static int ape_channels(input_object *obj)
{
	if (!obj || !(obj->local_data))
		return 0;

	return obj->nr_channels;
}

static int ape_stream_info (input_object *obj, stream_info *info)
{
	if (!obj || !(obj->local_data) || !info)
		return 0;

//	CAPETag *tag = (CAPETag *)((IAPEDecompress*) obj->local_data)->GetInfo(APE_INFO_TAG);
//	TagLib_File* tag_file = taglib_file_new(const char *filename);

	sprintf(info->stream_type, "APE version %.2f", float (((IAPEDecompress*) obj->local_data)->GetInfo(APE_INFO_FILE_VERSION)) / 1000);
	
	strcpy(info->status, "playing...");
	strcpy(info->artist, "Artist");
	strcpy(info->title, "Title");
	strcpy(info->album, "Album");
	strcpy(info->genre, "Genre");
	strcpy(info->year, "Year");
	strcpy(info->track, "Tracknum");
	strcpy(info->comment, "Comment");
	strcpy(info->path, obj->path);

	info->channels = obj->nr_channels;
	info->tracks = 1; // number of tracks
	info->current_track = 1; // number of current track
	info->sample_rate = ((IAPEDecompress*) obj->local_data)->GetInfo(APE_INFO_SAMPLE_RATE);
	info->bitrate = ((IAPEDecompress*) obj->local_data)->GetInfo(APE_INFO_AVERAGE_BITRATE);

//	printf ("ape: artist[%s], title[%s], path[%s]\n", info->artist, info->title, info->path);

	return 1;
}

static int ape_nr_frames(input_object *obj)
{
	if (!obj || !(obj->local_data))
		return 0;
	
	return (((IAPEDecompress*) obj->local_data)->GetInfo(APE_INFO_WAV_DATA_BYTES) / obj->frame_size);
}

static int ape_frame_size(input_object *obj)
{
	if (!obj || !(obj->local_data))
		return 0;
	
	return obj->frame_size;
}

static int ape_frame_seek(input_object *obj, int frame)
{
	int64_t pos;

	if (!obj || !(obj->local_data))
		return 0;

	if (obj->flags & P_STREAMBASED)
		return 0;
	
	((IAPEDecompress*) obj->local_data)->Seek(frame * obj->frame_size / 4); 
	return frame;
}

static int ape_play_frame (input_object *obj, char *buf)
{
	int nRead = 0;
	int i;

	if (!obj || !(obj->local_data))
		return 0;

	((IAPEDecompress*) obj->local_data)->GetData (buf, 1024, &nRead);
#ifdef __BIG_ENDIAN__
	swab(buf, buf, (nRead * 4));
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
	ape_plugin.name 	= "Monkey's Audio plugin ver. 0.0.0.7";
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

