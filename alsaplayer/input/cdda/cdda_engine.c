/*
 *  cdda_engine.c (C) 1999-2002 by Andy Lo A Foe
 *	
 *	Based on code from dagrab 0.3 by Marcello Urbani <murbani@numerica.it>
 *
 *  This plugin reads music CD's in digital form, allowing for exceptional
 *  quality playback of CD audio. It is used in the alsaplayer project
 *  by Andy Lo A Foe. If you use use this please be so kind as to put a
 *  pointer to the web page at http://www.alsa-project.org/~andy
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

#include "config.h"
#include "cdda.h"
#include "prefs.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <dirent.h>
#include <pwd.h>
#ifdef HAVE_LINUX_CDROM_H
#include <linux/cdrom.h>
#endif
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/vfs.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include "input_plugin.h"

#define DEFAULT_DEVICE	"/dev/cdrom"
#define FRAME_LEN	4
#define N_BUF 8
#define OVERLAY 3
#define KEYLEN 12
#define OFS 12
#define RETRYS 20
#define IFRAMESIZE (CD_FRAMESIZE_RAW/sizeof(int))
#define BLEN 255

struct cdda_local_data {
	char device_path[1024];
	int cdrom_fd;
	int samplerate;
	int track_length;
	int track_start;
	int rel_pos;
	int track_nr;
};

struct cd_trk_list {
	int min;
	int max;
	int *starts;
	char *types;
};


static struct cd_trk_list tl;

typedef unsigned short Word;

static int cd_get_tochdr(int cdrom_fd, struct cdrom_tochdr *Th)
{
	return ioctl(cdrom_fd, CDROMREADTOCHDR,Th);
}

static int cd_get_tocentry(int cdrom_fd, int trk, struct cdrom_tocentry *Te, int mode)
{
	Te->cdte_track=trk;
	Te->cdte_format=mode;
	return ioctl(cdrom_fd,CDROMREADTOCENTRY,Te);
}


static int cd_read_audio(int cdrom_fd, int lba, int num, unsigned char *buf)
{
	struct cdrom_read_audio ra;

	ra.addr.lba = lba;
	ra.addr_format = CDROM_LBA;
	ra.nframes = num;
	ra.buf = buf;
	if (ioctl(cdrom_fd, CDROMREADAUDIO, &ra)) {
		fprintf(stderr, "\nCDDA: read raw ioctl failed at lba %d length %d\n",
				lba, num);
		perror("CDDA");
		return 1;
	}
	return 0;
}


static char *resttime(int sec)
{
	static char buf[BLEN+1];
	snprintf(buf, BLEN, "%02d:%02d:%02d", sec/3600, (sec/60)%60, sec%60);
	return buf;
}


static int cd_getinfo(int *cdrom_fd, char *cd_dev,struct cd_trk_list *tl)
{
	int i;
	struct cdrom_tochdr Th;
	struct cdrom_tocentry Te;

	if ((*cdrom_fd=open(cd_dev,O_RDONLY | O_NONBLOCK))==-1){
		fprintf(stderr,"CDDA: error opening device %s\n",cd_dev);
		return 1;
	}
	if(cd_get_tochdr(*cdrom_fd, &Th)){
		fprintf(stderr,"CDDA: read TOC ioctl failed\n");
		return 1;
	}
	tl->min=Th.cdth_trk0;tl->max=Th.cdth_trk1;
	if((tl->starts=(int *)malloc((tl->max-tl->min+2)*sizeof(int)))==NULL){
		fprintf(stderr,"CDDA: list data allocation failed\n");
		return 1;
	}
	if((tl->types=(char *)malloc(tl->max-tl->min+2))==NULL){
		fprintf(stderr,"CDDA: list data allocation failed\n");
		return 1;
	}

	for (i=tl->min;i<=tl->max;i++)
	{
		if(cd_get_tocentry(*cdrom_fd, i,&Te,CDROM_LBA)){
			fprintf(stderr,"CDDA: read TOC entry ioctl failed\n");
			free(tl->starts);
			free(tl->types);
			tl->starts = NULL;
			tl->types = NULL;
			return 1;
		}
		tl->starts[i-tl->min]=Te.cdte_addr.lba;
		tl->types[i-tl->min]=Te.cdte_ctrl&CDROM_DATA_TRACK;
	}
	i=CDROM_LEADOUT;
	if(cd_get_tocentry(*cdrom_fd, i,&Te,CDROM_LBA)){
		fprintf(stderr,"CDDA: read TOC entry ioctl failed\n");
		free(tl->starts);
		free(tl->types);
		tl->starts = NULL;
		tl->types = NULL;
		return 1;
	}
	tl->starts[tl->max-tl->min+1]=Te.cdte_addr.lba;
	tl->types[tl->max-tl->min+1]=Te.cdte_ctrl&CDROM_DATA_TRACK;
	return 0;
}

#if 0
static void cd_disp_TOC(struct cd_trk_list *tl)
{
	int i, len;
	printf("%5s %8s %8s %5s %9s %4s","track","start","length","type",
			"duration", "MB");
	printf("\n");
	for (i=tl->min;i<=tl->max;i++)
	{
		len = tl->starts[i + 1 - tl->min] - tl->starts[i - tl->min];
		printf("%5d %8d %8d %5s %9s %4d",i,
				tl->starts[i-tl->min]+CD_MSF_OFFSET, len,
				tl->types[i-tl->min]?"data":"audio",resttime(len / 75),
				(len * CD_FRAMESIZE_RAW) >> (20)); // + opt_mono));
				printf("\n");
	}
	printf("%5d %8d %8s %s\n",CDROM_LEADOUT,
			tl->starts[i-tl->min]+CD_MSF_OFFSET,"-","leadout");
}


static int cd_jc1(int *p1,int *p2) //nuova
	/* looks for offset in p1 where can find a subset of p2 */
{
	int *p,n;

	p=p1+opt_ibufsize-IFRAMESIZE-1;n=0;
	while(n<IFRAMESIZE*opt_overlap && *p==*--p)n++;
	if (n>=IFRAMESIZE*opt_overlap)	/* jitter correction is useless on silence */ 
	{
		n=(opt_bufstep)*CD_FRAMESIZE_RAW;
	}
	else			/* jitter correction */
	{
		n=0;p=p1+opt_ibufsize-opt_keylen/sizeof(int)-1;
		while((n<IFRAMESIZE*(1+opt_overlap)) && memcmp(p,p2,opt_keylen))
		{p--;n++;};
		/*		  {p-=6;n+=6;}; //should be more accurate, but doesn't work well*/
		if(n>=IFRAMESIZE*(1+opt_overlap)){		/* no match */
			return -1;
		};
		n=sizeof(int)*(p-p1);
	}
	return n;
}

static int cd_jc(int *p1,int *p2)
{
	int n,d;
	n=0;
	do
		d=cd_jc1(p1,p2+n);
	while((d==-1)&&(n++<opt_ofs));n--;
	if (d==-1) return (d);
	else return (d-n*sizeof(int));
}
#endif


static int cdda_init()
{
	return 1;
}


static float cdda_can_handle(const char *name)
{
	char *ext;
	ext = strrchr(name, '.');

	if (ext)
		if ((strcasecmp(ext, ".cdda") == 0)) {
			return 1.0;
		}				
	return 0.0;			
}


static int cdda_open(input_object *obj, char *name)
{
	struct cdda_local_data *data;	
	char *fname;
	char device_name[1024];
	int cdrom_fd;

	if (!obj)
		return 0;

	fname = strrchr(name, '/');
	if (!fname)
		fname = name;

	if (ap_prefs) {
		strcpy(device_name, prefs_get_string(ap_prefs, "cdda", "device", DEFAULT_DEVICE));
	} else {
		strcpy(device_name, DEFAULT_DEVICE);
	}	
#ifdef DEBUG
	printf("device = %s, name = %s\n", device_name, fname);
#endif
	if(cd_getinfo(&cdrom_fd, device_name, &tl)) {
		return 0;
	}

#ifdef DEBUG	
	cd_disp_TOC(&tl);	
	printf("IFRAMESIZE = %d\n", IFRAMESIZE);
#endif

	obj->local_data = malloc(sizeof(struct cdda_local_data));
	if (!obj->local_data) {
		return 0;
	}		
	data = (struct cdda_local_data *)obj->local_data;

	obj->nr_channels = 2;
	data->samplerate = 44100;
	data->track_length = 0;
	data->track_start  = 0;
	data->rel_pos = 0;
	data->track_nr = 0;
	data->cdrom_fd = cdrom_fd;
	strcpy(data->device_path, device_name);

	if (strcmp(fname, "CD.cdda") == 0) {
		data->track_start = tl.starts[tl.min-1]; /* + CD_MSF_OFFSET; */
		data->track_length = tl.starts[tl.max] - data->track_start;
		data->rel_pos = 0;
		data->track_nr = 1;
#ifdef DEBUG
		printf("Start: %d. Length: %d\n", data->track_start,
				data->track_length);
#endif
	}			
	else
		if (sscanf(fname, "Track %02d.cdda\n", &data->track_nr) != 1) {
			printf("Hmm failed to read track number\n");
			free(obj->local_data);
			obj->local_data = NULL;
			return 0;
		} else  {
#ifdef DEBUG
			printf("Found track number %d (%s)\n", data->track_nr, fname);
#endif
			data->track_start = tl.starts[data->track_nr-1];
			data->track_length = tl.starts[data->track_nr] - tl.starts[data->track_nr-1];
			data->rel_pos = 0;
		}

	obj->flags = P_SEEK;

	return 1;
}


static void cdda_close(input_object *obj)
{
	struct cdda_local_data *data;	
#ifdef DEBUG
	printf("In cdda_close()\n");
#endif
	if (!obj)
		return;
	data = (struct cdda_local_data *)obj->local_data;
	if (!data) {
		return;
	}
#ifdef DEBUG	
	printf("closing\n");
#endif
	close(data->cdrom_fd);
	if (tl.starts) free(tl.starts);
	tl.starts = NULL;
	if (tl.types) free(tl.types);
	tl.types = NULL;
	free(obj->local_data);
	obj->local_data = NULL;
}


static int cdda_play_frame(input_object *obj, char *buf)
{
	struct cdda_local_data *data;
	unsigned char bla[CD_FRAMESIZE_RAW*FRAME_LEN];
	if (!obj)
		return 0;
	data = (struct cdda_local_data *)obj->local_data;	

	if (!data) {
		return 0;
	}		
	if (!data->track_length || 
			(data->rel_pos > data->track_length)) {
		printf("rel_pos = %d, start = %d, end = %d\n",
				data->rel_pos, data->track_start, 
				data->track_start + data->track_length);
		return 0;
	}
	memset(bla, 0, sizeof(bla));
	if (cd_read_audio(data->cdrom_fd, data->track_start + data->rel_pos, FRAME_LEN, bla)) {
		return 0;
	}
	data->rel_pos += FRAME_LEN;
	if (buf) {
		memcpy(buf, bla, (CD_FRAMESIZE_RAW * FRAME_LEN));
	}
	return 1;
}


static int cdda_frame_seek(input_object *obj, int index)
{
	struct cdda_local_data *data;	
	if (!obj)
		return 0;
	data = (struct cdda_local_data *)obj->local_data;	
	if (data)
		data->rel_pos = (index * FRAME_LEN);
	return 1;
}


static int cdda_frame_size(input_object *obj)
{
	int size = (CD_FRAMESIZE_RAW * FRAME_LEN);
	return size;
}



static int cdda_nr_frames(input_object *obj)
{
	struct cdda_local_data *data;	
	int nr_frames = 0;

	if (!obj)
		return 0;
	data = (struct cdda_local_data *)obj->local_data;

	if (data)
		nr_frames = data->track_length >> 2;	
	return nr_frames;
}


static  long cdda_frame_to_sec(input_object *obj, int frame)
{
	unsigned long byte_count = FRAME_LEN * frame * CD_FRAMESIZE_RAW;

	return (byte_count / 1764); /* 44Khz stereo 16 bit, fixed */
	/* 1764 = 176400 / 100 voor de 100ste seconden representatie */
}


static int cdda_sample_rate(input_object *obj)
{
	struct cdda_local_data *data;	
	if (!obj)
		return 0;
	data = (struct cdda_local_data *)obj->local_data;	
	return (data ? data->samplerate : 0);
}


static int cdda_channels(input_object *obj)
{
	if (!obj)
		return 0;
	return obj->nr_channels;
}

static int cdda_stream_info(input_object *obj, stream_info *info)
{
	struct cdda_local_data *data;	
	if (!obj)
		return 0;
	data = (struct cdda_local_data *)obj->local_data;
	if (!data || !info)
		return 0;
	sprintf(info->stream_type, "16-bit 44KHz stereo CDDA");
	info->author[0] = 0;
	info->status[0] = 0;
	if (data->track_nr < 0)
		info->title[0] = 0;
	else if (data->track_nr == 0)
		sprintf(info->title, "Full CD length playback");
	else
		sprintf(info->title, "Track %d", data->track_nr);

	return 1;
}


static int cdda_nr_tracks(input_object *obj)
{
	return 0;
}

static void cdda_shutdown()
{
	return;
}


static int cdda_track_seek(input_object *obj, int track)
{
	return 1;
}


static input_plugin cdda_plugin;
/*
= {
	INPUT_PLUGIN_VERSION,
	0,
	"CDDA player v1.1",
	"Andy Lo A Foe <andy@alsaplayer.org>",
	NULL,
	cdda_init,
	cdda_shutdown,
	NULL,
	cdda_can_handle,
	cdda_open,
	cdda_close,
	cdda_play_frame,
	cdda_frame_seek,
	cdda_frame_size,
	cdda_nr_frames,
	cdda_frame_to_sec,
	cdda_sample_rate,
	cdda_channels,
	cdda_stream_info,
	cdda_nr_tracks,
	cdda_track_seek
};
*/
#ifdef __cplusplus
extern "C" {
#endif

input_plugin *input_plugin_info()
{
	memset(&cdda_plugin, 0, sizeof(input_plugin));
	cdda_plugin.version = INPUT_PLUGIN_VERSION;
	cdda_plugin.name = "CDDA player v1.2";
	cdda_plugin.author = "Andy Lo A Foe <andy@alsaplayer.org>";
	cdda_plugin.init = cdda_init;
	cdda_plugin.shutdown = cdda_shutdown;
	cdda_plugin.can_handle = cdda_can_handle;
	cdda_plugin.open = cdda_open;
	cdda_plugin.close = cdda_close;
	cdda_plugin.play_frame = cdda_play_frame;
	cdda_plugin.frame_seek = cdda_frame_seek;
	cdda_plugin.frame_size = cdda_frame_size;
	cdda_plugin.nr_frames = cdda_nr_frames;
	cdda_plugin.frame_to_sec = cdda_frame_to_sec;
	cdda_plugin.sample_rate = cdda_sample_rate;
	cdda_plugin.channels = cdda_channels;
	cdda_plugin.stream_info = cdda_stream_info;
	cdda_plugin.nr_tracks = cdda_nr_tracks;
	cdda_plugin.track_seek = cdda_track_seek;
	return &cdda_plugin;
}

#ifdef __cplusplus
}
#endif
