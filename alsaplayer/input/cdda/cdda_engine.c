/*
 *  cdda_engine.c (C) 1999-2003 by Andy Lo A Foe <andy@alsaplayer.org>
 *  CDDB lookup code by Anders Rune Jensen
 *	
 *	Based on code from dagrab 0.3 by Marcello Urbani <murbani@numerica.it>
 *
 *  This plugin reads music CD's in digital form, allowing for exceptional
 *  quality playback of CD audio. It is used in the alsaplayer project
 *  by Andy Lo A Foe. If you use use this please be so kind as to put a
 *  pointer to the web page at http://www.alsa-project.org/~andy
 *
 *  This file is part of AlsaPlayer.
 *
 *  AlsaPlayer is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  AlsaPlayer is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"
#include "cdda.h"
#include "prefs.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <dirent.h>
#include <pwd.h>
#include <endian.h>
#include <inttypes.h>
#ifdef HAVE_LINUX_CDROM_H
#include <linux/cdrom.h>
#endif
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/vfs.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h>
#include <pthread.h>
#include "input_plugin.h"
#include "alsaplayer_error.h"
#include "AlsaPlayer.h"
#include "control.h"
#include "prefs.h"
#include "utilities.h"

#define MAX_TRACKS	128
#define BUFFER_SIZE     4096

#define DEFAULT_DEVICE	"/dev/cdrom"
#define FRAME_LEN	4
#define N_BUF 8
#define OVERLAY 3
#define KEYLEN 12
#define OFS 12
#define RETRYS 20
#define IFRAMESIZE (CD_FRAMESIZE_RAW/sizeof(int))
#define BLEN 255

/* global variables */
static char *REAL_PATH = NULL;

typedef struct
{
	char *artist;
	char *album;
	char *track;

} track;


struct cd_trk_list {
	int min;
	int max;
	int *l_min;
	int *l_sec;
	int *l_frame;
	int *starts;
	char *types;
};

struct cdda_local_data {
	track tracks[MAX_TRACKS];
	char device_path[1024];
	struct cd_trk_list tl;
	int cdrom_fd;
	int samplerate;
	int track_length;
	int track_start;
	int rel_pos;
	int track_nr;
};

//static cd_trk_list tl, old_tl;

typedef unsigned short Word;
typedef unsigned char  unchar;

int cddb_sum (int n);


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
		alsaplayer_error("CDDA: read raw ioctl failed at lba %d length %d",
				lba, num);
		perror("CDDA");
		return 1;
	}
	return 0;
}

/*
static char *resttime(int sec)
{
	static char buf[BLEN+1];
	snprintf(buf, BLEN, "%02d:%02d:%02d", sec/3600, (sec/60)%60, sec%60);
	return buf;
}
*/

void toc_fail(struct cd_trk_list *tl)
{
	free(tl->starts);
	free(tl->types);
	free(tl->l_min);
	free(tl->l_sec);
	free(tl->l_frame);
	tl->starts = NULL;
	tl->types = NULL;
	tl->l_min = NULL;
	tl->l_sec = NULL;
	tl->l_frame = NULL;
}

static int cd_getinfo(int *cdrom_fd, char *cd_dev, struct cd_trk_list *tl)
{
	int i;
	struct cdrom_tochdr Th;
	struct cdrom_tocentry Te;

	if ((*cdrom_fd=open(cd_dev,O_RDONLY | O_NONBLOCK))==-1){
		alsaplayer_error("CDDA: error opening device %s",cd_dev);
		return 1;
	}

	if(cd_get_tochdr(*cdrom_fd, &Th)){
		alsaplayer_error("CDDA: read TOC ioctl failed");
		return 1;
	}

	tl->min=Th.cdth_trk0; /* first track */
	tl->max=Th.cdth_trk1; /* last track */

	if((tl->starts=(int *)malloc((tl->max-tl->min+2)*sizeof(int)))==NULL){
		alsaplayer_error("CDDA: list data allocation failed");
		return 1;
	}
	if((tl->types=(char *)malloc(tl->max-tl->min+2))==NULL){
		alsaplayer_error("CDDA: list data allocation failed");
		return 1;
	}

	/* length */
	if((tl->l_min=(int *)malloc((tl->max-tl->min+2)*sizeof(int)))==NULL){
		alsaplayer_error("CDDA: list data allocation failed");
		return 1;
	}
	if((tl->l_sec=(int *)malloc((tl->max-tl->min+2)*sizeof(int)))==NULL){
		alsaplayer_error("CDDA: list data allocation failed");
		return 1;
	}
	if((tl->l_frame=(int *)malloc((tl->max-tl->min+2)*sizeof(int)))==NULL){
		alsaplayer_error("CDDA: list data allocation failed");
		return 1;
	}

	i=CDROM_LEADOUT;
	if(cd_get_tocentry(*cdrom_fd, i,&Te,CDROM_LBA)){
		alsaplayer_error("CDDA: read TOC entry ioctl failed");
		toc_fail(tl);
		return 1;
	}
	tl->starts[tl->max]=Te.cdte_addr.lba;
	tl->types[tl->max]=Te.cdte_ctrl&CDROM_DATA_TRACK;

	if(cd_get_tocentry(*cdrom_fd, i,&Te,CDROM_MSF)){
		alsaplayer_error("CDDA: read TOC entry ioctl failed");
		toc_fail(tl);
		return 1;
	}
	/* length info */
	tl->l_min[tl->max] = Te.cdte_addr.msf.minute;
	tl->l_sec[tl->max] = Te.cdte_addr.msf.second;
	tl->l_frame[tl->max] = Te.cdte_addr.msf.frame;

	for (i=tl->max;i>=tl->min;i--)
	{
		if(cd_get_tocentry(*cdrom_fd, i,&Te,CDROM_LBA)){
			alsaplayer_error("CDDA: read TOC entry ioctl failed");
			toc_fail(tl);
			return 1;
		}
		tl->starts[i-1]=Te.cdte_addr.lba;
		tl->types[i-1]=Te.cdte_ctrl&CDROM_DATA_TRACK;


		if(cd_get_tocentry(*cdrom_fd, i,&Te,CDROM_MSF)){
			alsaplayer_error("CDDA: read TOC entry ioctl failed");
			toc_fail(tl);
			return 1;
		}

		/* length info */
		tl->l_min[i-1] = Te.cdte_addr.msf.minute;
		tl->l_sec[i-1] = Te.cdte_addr.msf.second;
		tl->l_frame[i-1] = Te.cdte_addr.msf.frame;
	}

	return 0;
}

/*
 * create_socket - create a socket to communicate with the remote server
 * return the fd' int on success, or -1 on error.
 */
int create_socket (const char *unchar_address, int int_port)
{
	int sock, len;
	struct	hostent		*remote;
	struct	sockaddr_in	server;
	ushort	port = (ushort) (int_port);
	ulong	address, temp;


	/* get the "remote" server information */
	remote = gethostbyname (unchar_address);
	if (! remote)
	{
		alsaplayer_error("%s\n", strerror (errno));
		return (-1);
	}
	bcopy ((char *) remote->h_addr, (char *) &temp, remote->h_length);

	/* convert the 32bit long value from *network byte order* to the *host byte order* */
	address = ntohl (temp);

	/* create the address of the CDDB server, filling the server's mem_area with 0 values */
	len = sizeof (struct sockaddr_in);
	memset (&server, 0, len);
	server.sin_family = AF_INET; /* set the address as being in the internet domain */
	server.sin_port = htons (port); /* set the port address of the server	*/
	server.sin_addr.s_addr = htonl (address);

	/* create a socket in the INET domain */
	sock = socket (AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{
		alsaplayer_error("socket error\n");
		return (-1);
	}

	/* connect to the server */
	if (connect (sock, (struct sockaddr *) &server, sizeof (server)) < 0) 
	{
		alsaplayer_error("%s\n", strerror (errno));
		return (-1);
	}

	return (sock);
}


/*
 * sent_to_server - send a message to the server, and return the server response
 * on success, or NULL on error
 */
char * send_to_server (int server_fd, char *message)
{
	ssize_t	total, i;
	int 	len = BUFFER_SIZE;
	char	*response, *temp;

	temp = (char *) malloc(BUFFER_SIZE);

	/* write 'message' to the server */
	if (send (server_fd, message, strlen (message), MSG_DONTWAIT) < 0) 
	{
		alsaplayer_error("%s: %s\n", message, strerror (errno));
		free (temp);
		return (NULL);
	}

	if (global_verbose) {
	/* print the message sent to the server */
	alsaplayer_error("-> %s", message);
	}

	/* read the response from the server */
	total = 0;
	do
	{
		i = read (server_fd, temp + total, BUFFER_SIZE);
		if (i < 0)
		{
			alsaplayer_error("%s\n", strerror (errno));
			free (temp);
			return (NULL);
		}
		total += i;
		if (total + BUFFER_SIZE > len)
		{
			temp = (char *) realloc(temp, len + BUFFER_SIZE);
			len += BUFFER_SIZE;
		}
	}
	while (total > 2 && temp[total - 2] != '\r' && i != 0);

	if (total < 2)
	{
		free (temp);
		return (NULL);
	}

	temp[total-2] = '\0';		/* temp[total-1] == \r; temp[total] == \n	*/
	response = strdup (temp);	/* duplicate the response from the server	*/
	free(temp);

	if (global_verbose) {
	/* print the message sent to the server */
	alsaplayer_error("<- %s", response);
	}

	return (response);
}

/*
 * cddb_disc_id - generate the disc ID from the total music time
 * (routine token from the cddb.howto)
 */
unsigned int cddb_disc_id (struct cd_trk_list *tl)
{
	int i, t = 0, n = 0;

	/* n == total music time in seconds */
	i = 0;
	while (i < tl->max) 
	{
		n += cddb_sum((tl->l_min[i] * 60) + tl->l_sec[i]);
		i++;
	}

	/* t == lead-out offset seconds - 1st music total time, in seconds */
	t = ((tl->l_min[tl->max] * 60) + tl->l_sec[tl->max]) 
		- ((tl->l_min[0] * 60) + tl->l_sec[0]);

	/*
	 * mod 'n' with FFh and left-shift it by 24. 't' is left-shifted by 8.
	 * the disc_id is then returned as these operations + total tracks
	 * 'OR'ed together.
	 */
	return ((n % 0xff) << 24 | t << 8 | tl->max);
}


/*
 * cddb_sum - adds the value of each digit in the decimal string representation
 * of the number. (routine token from the cddb.howto)
 */
int cddb_sum (int n)
{
	int ret = 0;

	while (n > 0) 
	{
		ret = ret + (n % 10);
		n = n/10;
	}

	return (ret);
}

/*
 * save_to_disk - receive the subdir, cdID and the message, and save the
 * information into the cddb directory. This function returns the filename on
 * success, or NULL on error.
 */
char * cddb_save_to_disk(char *subdir, int cdID, char *message)
{
	FILE *destination;
	DIR *thedir; 
	char *path, *retval;
	char new[strlen (message)], *filename;
	int i = 0, j = 0;

	/* check if we already have the subdir created */
	path = (char *) malloc ((strlen (subdir) + strlen (REAL_PATH) + 2) * sizeof (char));

	/* print the message sent to the server */
	sprintf(path, "%s", REAL_PATH);
	if (! (thedir=opendir(path))) { /* No cddb directory yet! */
		if ((mkdir(path, 0744)) < 0) {
			perror("mkdir");
			free(path);
			return (NULL);
		}
	} else {
		closedir(thedir);
	}  
	/* cddb directory should be there at this point */	 

	sprintf (path, "%s/%s", REAL_PATH, subdir);
	if (global_verbose)
		alsaplayer_error("path = %s", path);	
	/* check if we have the directory in the disk */
	if (! (thedir = opendir (path))) {
		/* given directory doesn't exist */
		if (global_verbose)
			printf ("directory %s doesn't exist, trying to create it.\n", path);

		/* try to create it.. */
		if ((mkdir (path, 0744)) < 0) {
			perror ("mkdir");
			free(path);
			return (NULL);
		} else {
			if (global_verbose)
				printf ("directory created successfully\n");
		}
	} else {
		closedir(thedir);
	} 

	while (message[i] != '\n')
		i++;
	i++;

	for (; i < (int)strlen (message); i++, j++)
		new[j] = message[i];

	/* save it into the disc */
	filename = (char *) malloc ((strlen (subdir) + strlen (REAL_PATH) + 11) * sizeof (char));
	sprintf (filename, "%s/%s/%08x", REAL_PATH, subdir, cdID);
	retval = strdup (filename);
	if (global_verbose)
		alsaplayer_error("filename = %s", filename);
	/* create the file */
	destination = fopen (filename, "w");
	free(filename);
	if (! destination)
	{
		alsaplayer_error("error creating file");
		free(path);
		return (NULL);
	}

	/* copy the new string content into the file */
	for (i = 0; i < (int)strlen (new); i++)
		fputc (new[i], destination);

	/* free path's memory */
	free (path);

	/* close the file */
	fclose (destination);

	return (retval);
}

/*
 * search for the CD info in the hard disk CDDB, returning NULL on error or the
 * filename on success.
 */
char * cddb_local_lookup (char *path, unsigned int cd_id)
{
	int i, number, fd;
	char *name;
	char cdrom_id[9];
	struct dirent	**directory;

	if (global_verbose)
		alsaplayer_error ("Searching for CDDB entries on %s ... ", path);

	/* try to open the given directory */
	if (! (opendir (path))) 
	{
		return (NULL);
	}

	/* get the number of subdirectories in the 'path' dir */
	number = scandir (path, &directory, 0, alphasort);
	if (number < 0)
	{
		alsaplayer_error("scandir\n");
		return (NULL);
	}

	/* set the cdrom_id */
	sprintf (cdrom_id, "%08x", cd_id);
	cdrom_id[8] = '\0';

	for (i = 0; i < number; i++) 
	{
		/* ignore '.' and '..' directories */
		if ((strcmp (directory[i]->d_name, ".")) && (strcmp (directory[i]->d_name, ".."))) 
		{
			name = malloc((strlen (path) + strlen (directory[i]->d_name) + 15) * sizeof(char));
			sprintf (name, "%s", path);
			strcat (name, "/");
			strncat (name, directory[i]->d_name, strlen (directory[i]->d_name));
			strcat (name, "/");
			strncat (name, cdrom_id, 8);
			if ((fd = open (name, O_RDONLY)) >= 0) 
			{
				if (global_verbose)
					printf ("OK\n");
				close (fd);
				return (name);
			}
			free (name);
		}
	}

	if (global_verbose)
		printf ("not found\n");
	return (NULL);
}

char*
cut_html_head(char *answer)
{
	if(!answer)
		return NULL;
		
	char *new_answer;
	int counter = 0, i = 0;

	for (i = 0; i < strlen(answer); i++) {
		if (*(answer+i) == '\n') {
			if (counter < 3) {
				new_answer = strdup(answer+i+1);
				free(answer);
				return new_answer;
			}
			counter = 0;
		}
		
		counter++;
	}

	free(answer);
	return NULL; 
}

/*
 * search for the song in the CDDB given address/port, returning it's name, or
 * NULL if not found.
 */
char * cddb_lookup (const char *address, const char *char_port, int discID, struct cd_trk_list *tl)
{
	int port = atoi (char_port);
	int server_fd, i, j;
	int total_secs = 0, counter = 0;
	char *answer = NULL, *username, *filename, categ[20], newID[9];
	char msg[BUFFER_SIZE], offsets[BUFFER_SIZE], tmpbuf[BUFFER_SIZE];
	char hostname[MAXHOSTNAMELEN], separator='+';

	/* try to create a socket to the server */
	if (global_verbose)
		alsaplayer_error ("Opening Connection to %s:%d ... ", address, port);

	/* get the server fd from the create_socket function */
	server_fd = create_socket (address, port);
	if (server_fd < 0)
		return (NULL);
	else
		if (global_verbose)
			printf ("OK\n");

	/* get the initial message from the server */
	if (port > 80) {
		answer = send_to_server (server_fd, "");
		if (global_verbose) {
			printf ("Saying HELLO to CDDB server ...\n");
		}
		free (answer);
	}
	/* set some settings before saying HELLO to the CDDB server */
	username = getlogin ();
	if ((gethostname (hostname, sizeof (hostname))) < 0)
		snprintf (hostname, sizeof (hostname), "unknown");

	if (port > 80) {
		snprintf (msg, sizeof (msg), "cddb hello %s %s %s %s\r\n\r\n", username, hostname,PACKAGE, VERSION);
			
		answer = send_to_server (server_fd, msg);
		if (! answer)
		{
			alsaplayer_error("bad response from the server\n");
			close (server_fd);
			return (NULL);
		}
		separator=' ';
	}
	/* set another settings before querying the CDDB database */
	tmpbuf[0] = '\0';
	for (i = 0; i < tl->max; i++) 
	{
		/* put the frame offset of the starting location of each track in a string */
		//snprintf (offsets, sizeof (offsets), "%s %d ", tmpbuf,
		snprintf (offsets, sizeof (offsets), "%s%c%d", tmpbuf, separator,
				tl->l_frame[i] + (75 * (tl->l_sec[i] + (60 * tl->l_min[i]))));
		strcpy (tmpbuf, offsets);
		counter += tl->l_frame[i] + (75 * tl->l_sec[i] + (60 * tl->l_min[i]));
	}

	total_secs = tl->l_sec[tl->max] + (tl->l_min[tl->max] * 60);

	/* send it */
	if (port > 80) 
		snprintf (msg, sizeof (msg), "cddb query %08x %d %s %d\r\n", discID, tl->max, offsets, total_secs);
	else
		snprintf (msg, sizeof (msg), "GET /~cddb/cddb.cgi?cmd=cddb+query+%08x+%d%s+%d&hello=%s+%s+%s+%s&proto=6 HTTP/1.0\r\n\r\n", discID, tl->max, offsets, total_secs, username, hostname,PACKAGE, VERSION);
	
	if (answer)
		free(answer);
	answer = send_to_server (server_fd, msg);
	if (! answer)
	{
		alsaplayer_error("bad response from the server\n");
		close (server_fd);
		return (NULL);
	}

	/*
	 * if answer == "200...", found exact match
	 * if answer == "211...", found too many matches
	 * if answer == "202...", found no matches
	 */
	
	if (port <= 80)  
		answer = cut_html_head(answer);
	
	if (!answer)
		return NULL;
			 
	i = 0;
	if (! (strncmp (answer, "211", 3)))
	{
		/* seek the 2nd line */
		while (answer[i] != '\n') {
			if (! answer[i])
				goto error;
			++i;
		}

		/* copy the 1st match to the category */
		j = 0; 
		i++;
		while (j < 19 && answer[i] != ' ') {
			if (! answer[i])
				goto error;
			categ[j++] = answer[i++];
		}
		categ[j++] = '\0';
		while (answer[i] != ' ') {
			if (! answer[i])
				goto error;
			i++;
		}

		/* get the new cdID given from the CDDB */
		j = 0; 
		i++;
		while (j < 8 && answer[i] != ' ') {
			if (! answer[i])
				goto error;
			newID[j++] = answer[i++];
		}
		newID[j++] = '\0';
		while (answer[i] != ' ') {
			if (! answer[i])
				goto error;
			i++;
		}

	} 
	else if (! (strncmp (answer, "200", 3)))
	{
		/* get it from the 1st line */
		while (answer[i] != ' ') {
			if (! answer[i])
				goto error;
			i++;
		}
		i++;

		/* copy the match to the category */
		j = 0;
		while (j < 19 && answer[i] != ' ') {
			if (! answer[i])
				goto error;
			categ[j++] = answer[i++];
		}
		categ[j++] = '\0';
		while (answer[i] != ' ') {
			if (! answer[i])
				goto error;
			i++;
		}

		/* copy the new cdID */
		j = 0; 
		i++;
		while (j < 8 && answer[i] != ' ') {
			if (! answer[i])
				goto error;
			newID[j++] = answer[i++];
		}
		newID[j++] = '\0';
		while (answer[i] != ' ') {
			if (! answer[i])
				goto error;
			i++;
		}
	} 
	else 
	{
	error:
		alsaplayer_error("Could not find any matches for %08x\n\n", discID);
		close (server_fd);
		free(answer);
		return (NULL);
	}

	/* read from the server */

	if (port > 80)
		sprintf (msg, "cddb read %s %s\r\n", categ, newID);
	else {
		server_fd = create_socket (address, port);
		snprintf (msg, sizeof (msg), "GET /~cddb/cddb.cgi?cmd=cddb+read+%s+%s&hello=%s+%s+%s+%s&proto=6 HTTP/1.0\r\n\r\n", categ, newID, username, hostname,PACKAGE, VERSION);
	}
	
	free(answer);
	answer = send_to_server(server_fd, msg);

	if (! answer)
	{
		alsaplayer_error("could not receive the informations from %s\n", address);
		close (server_fd);
		return (NULL);
	}

	/* save the output into the disc */
	if (global_verbose)
	{
		printf ("Saving CDDB information into %s/%s ...\n", REAL_PATH, newID);
		printf ("save_to_disk(%s)\n", answer);
	}

	filename = cddb_save_to_disk(categ, discID, answer); 
	if (! filename)
	{
		alsaplayer_error("could not create the file %s/%s, check permission\n", categ, newID);
		close (server_fd);
		return (NULL);
	}

	if (global_verbose) 
		puts ("");

	/* close the fd */
	close (server_fd);
	free(answer);	
	return (filename);
}

/*
 * open the filename and put music title's into the global variable
 */
void cddb_read_file (char *file, struct cdda_local_data *data)
{
	char line[BUFFER_SIZE], name[BUFFER_SIZE];
	char *token = NULL, *tmp, *divider, *s, *post;
	int i, index = 1;
	FILE *f;

	/* try to open the file */
	f = fopen (file, "r");
	if (! f) {
		alsaplayer_error("couldn't open file");
		return;	  
	}
	/* read it line by line */
	while (!feof (f)) {
		if ((fgets (line, sizeof (line), f))) {
			if (! (strstr (line, "DTITLE="))) {
				/* check if is the music name.. */
				if ((strstr (line, "TTITLE"))) {
					token = strtok (line, "=");
					if (!token) { 
						alsaplayer_error("error: TTITLE has no arguments");
						continue;
					}
					token = strtok (NULL, "=");
					if (!token) {
						alsaplayer_error("TTITLE has no arguments");
						continue;
					}	  
					/* seek for the \r character */
					for (i = 0; i < (int)strlen (token); i++) {
						if ((token[i] == '\n') || (token[i] == '\r'))
							break;
					}
					if (sscanf(line, "TTITLE%d=", &index)) {
						//alsaplayer_error("Found index %d", index);
						index++;
					} else {
						index = 1;
						alsaplayer_error("Error reading index number!");
					}
					token[i] = '\0';
					sprintf (name, "%s", token);
					if (data->tracks[index].track) {
						post = (char *)malloc(strlen(data->tracks[index].track) + strlen(name) + 1);
						*post = '\0';
						strcat(post, data->tracks[index].track);
						strcat(post, name);
						free(data->tracks[index].track);
						data->tracks[index].track = strdup(post);
						free(post);
					} else {	
						data->tracks[index].track = strdup (name);
					}	
				}
				continue;
			} else {
				// workaround album name on multiple lines, need improvment
				if (data->tracks[1].album)
					continue;
				  
				/* print the album name */
				tmp = strtok (line, "=");
				if (!tmp) { 
					alsaplayer_error ("error: no arguments given on %s", line);
					continue;
				}
				tmp = strtok (NULL, "=");
				if (!tmp) {
					alsaplayer_error ("error: no arguments given on %s", line);
					continue;
				}
				divider = strstr (tmp, " / ");
				if (!divider) { 
					alsaplayer_error("No divider found in DTITLE");
					
					    	data->tracks[1].artist = strdup(tmp);
						data->tracks[1].album = strdup(tmp);
				} else {
						data->tracks[1].album = strdup (divider+3);
						tmp[strlen(tmp)-strlen(data->tracks[1].album)-3] = '\0';
						data->tracks[1].artist = strdup (tmp);
				}	
				if ((s = strstr(data->tracks[1].artist, "\r"))) {
					*s = '\0';
				}
				if ((s = strstr(data->tracks[1].artist, "\n"))) {
					*s = '\0';
				} 
				if ((s = strstr(data->tracks[1].album, "\r"))) {
					*s = '\0';
				}
				if ((s = strstr(data->tracks[1].album, "\n"))) {
					*s = '\0';
				}
				if (data->tracks[1].album[strlen(data->tracks[1].album)-1] == ' ') {
					data->tracks[1].album[strlen(data->tracks[1].album)-1] = '\0';
				}
				if (data->tracks[1].artist[strlen(data->tracks[1].artist)-1] == ' ') {
					data->tracks[1].artist[strlen(data->tracks[1].artist)-1] = '\0';
				}
				if (global_verbose) {
					alsaplayer_error ("Artist: %s", data->tracks[1].artist);
					alsaplayer_error ("Album name: %s", data->tracks[1].album);
				}
			}
		}	/* if */
	}	/* while */
}


void cddb_update_info(struct cdda_local_data *data)
{
	char *file_name = NULL;
	const char *cddb_servername = NULL;
	const char *cddb_serverport = NULL;
	struct cd_trk_list *tl;
	int index;
	unsigned int cd_id;

	if (!data)
		return;
	tl = &data->tl;
	cd_id = cddb_disc_id(tl);

	/* try to get the audio info from the hard disk.. */
	file_name = cddb_local_lookup(REAL_PATH, cd_id);
	if (file_name)
	{
		/* open the 'file_name' and search for album information */
		cddb_read_file (file_name, data);
	}	
	else 
	{
		/* if could not, try to get it from the internet connection.. */
		cddb_servername = prefs_get_string(ap_prefs, "cdda", "cddb_servername", "freedb.freedb.org");
		cddb_serverport = prefs_get_string(ap_prefs, "cdda", "cddb_serverport", "80"); 
		if (global_verbose)
			alsaplayer_error("CDDB server: %s:%s", cddb_servername, cddb_serverport);	    
		file_name = cddb_lookup(cddb_servername, cddb_serverport, cd_id, tl);
		if (file_name) {

			/* fine, now we have the hard disk access! so, let's use it's information */
			free(file_name);
			file_name = cddb_local_lookup(REAL_PATH, cd_id);
			if (file_name)
				cddb_read_file (file_name, data);
			else {
				for (index = 1; index <= tl->max; index++)
					data->tracks[index].track = strdup ("unrecognized song");
			}

		} else {
			for (index = 1; index <= tl->max; index++) 
				data->tracks[index].track = strdup ("unrecognized song");
		}
	}	
}


static int cdda_init(void)
{
	char *prefsdir;

	prefsdir = get_prefsdir();
	REAL_PATH = (char *)malloc((strlen(prefsdir) + 8) * sizeof(char));
	if (REAL_PATH) {
		sprintf(REAL_PATH, "%s/cddb", prefsdir);
		return 1;
	}
	return 0;
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



void cd_adder(void *data) {
	int i;
	intptr_t nr_tracks;
	char track_name[1024];

	if (!data)
		return;

	nr_tracks = (intptr_t)data;

	for (i=1;i <= nr_tracks;i++) {
		sprintf(track_name, "Track %02d.cdda", i);
		ap_add_path(global_session_id, track_name);
	}
	pthread_exit(NULL);
}


static int cdda_open(input_object *obj, const char *name)
{
	struct cdda_local_data *data;	
	const char *fname;
	char device_name[1024];

	if (!obj)
		return 0;

	fname = strrchr(name, '/');
	if (!fname)
		fname = name;
	else fname++; // Ignore '/'

	if (ap_prefs) {
		strcpy(device_name, prefs_get_string(ap_prefs, "cdda", "device", DEFAULT_DEVICE));
	} else {
		strcpy(device_name, DEFAULT_DEVICE);
	}	
#ifdef DEBUG
	alsaplayer_error("device = %s, name = %s\n", device_name, fname);
#endif

	obj->local_data = malloc(sizeof(struct cdda_local_data));
	if (!obj->local_data) {
		return 0;
	}		
	data = (struct cdda_local_data *)obj->local_data;
	memset(data->tracks, 0, sizeof(data->tracks));
	if(cd_getinfo(&data->cdrom_fd, device_name, &data->tl)) {
		free(obj->local_data);
		obj->local_data = NULL;
		return 0;
	}


#ifdef DEBUG	
	cd_disp_TOC(&data->tl);	
	alsaplayer_error("IFRAMESIZE = %d\n", IFRAMESIZE);
#endif

	obj->nr_channels = 2;
	data->samplerate = 44100;
	data->track_length = 0;
	data->track_start  = 0;
	data->rel_pos = 0;
	data->track_nr = 0;
	strcpy(data->device_path, device_name);

	if (prefs_get_bool(ap_prefs, "cdda", "do_cddb_lookup", 1)) {
		/* look up cd in cddb */
		cddb_update_info(data);
	}
	if (strcmp(fname, "CD.cdda") == 0) {
		pthread_t cd_add;

		pthread_create(&cd_add, NULL, (void *(*)(void *))cd_adder, (void *)(intptr_t)data->tl.max);

		pthread_detach(cd_add);
		return 1;
	} else if (sscanf(fname, "Track %02d.cdda", &data->track_nr) != 1 ||
			sscanf(fname, "Track%02d.cdda", &data->track_nr) != 1) {
		alsaplayer_error("Failed to read track number (%s)", fname);
		free(obj->local_data);
		obj->local_data = NULL;
		return 0;
	} else {
#ifdef DEBUG		
		alsaplayer_error("Found track number %d (%s)\n", data->track_nr, fname);
#endif
		data->track_start = data->tl.starts[data->track_nr-1];
		data->track_length = data->tl.starts[data->track_nr] - 
			data->tl.starts[data->track_nr-1];
		data->rel_pos = 0;
	}
	obj->flags = P_SEEK;

	return 1;
}


static void cdda_close(input_object *obj)
{
	struct cdda_local_data *data;
	int i = 0;
#ifdef DEBUG
	alsaplayer_error("In cdda_close()\n");
#endif
	if (!obj)
		return;
	data = (struct cdda_local_data *)obj->local_data;
	if (!data) {
		return;
	}
	for (i = 0; i < MAX_TRACKS;i++) {
		if (data->tracks[i].album) {
			free(data->tracks[i].album);
		}	
		if (data->tracks[i].artist) {
			free(data->tracks[i].artist);
		}	
		if (data->tracks[i].track) {
			free(data->tracks[i].track);
		}	
	}	
	close(data->cdrom_fd);
	if (data->tl.starts) free(data->tl.starts);
	data->tl.starts = NULL;
	if (data->tl.types) free(data->tl.types);
	data->tl.types = NULL;

	/* length */
	if (data->tl.l_min) free(data->tl.l_min);
	data->tl.l_min = NULL;
	if (data->tl.l_sec) free(data->tl.l_sec);
	data->tl.l_sec = NULL;
	if (data->tl.l_frame) free(data->tl.l_frame);
	data->tl.l_frame = NULL;

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
#ifdef DEBUG		
		alsaplayer_error("rel_pos = %d, start = %d, end = %d\n",
				data->rel_pos, data->track_start, 
				data->track_start + data->track_length);
#endif		
		return 0;
	}
	memset(bla, 0, sizeof(bla));
	if (cd_read_audio(data->cdrom_fd, data->track_start + data->rel_pos, FRAME_LEN, bla)) {
		return 0;
	}
	data->rel_pos += FRAME_LEN;
	if (buf) {
		memcpy(buf, bla, (CD_FRAMESIZE_RAW * FRAME_LEN));
#ifdef __BYTE_ORDER
#if __BYTE_ORDER == __BIG_ENDIAN
		swab (buf, buf, (CD_FRAMESIZE_RAW * FRAME_LEN));
#endif	
#endif		
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
	struct cd_trk_list *tl;

	assert(obj);
	assert(info);

	data = (struct cdda_local_data *)obj->local_data;

	tl = &data->tl;

	sprintf(info->stream_type, "CD Audio, 44KHz, stereo");
	if (data->tracks[1].artist)
	  sprintf(info->artist, "%s", data->tracks[1].artist);
	if (data->tracks[1].album)
	  sprintf(info->album, "%s", data->tracks[1].album);
	info->status[0] = 0;
	if (data->track_nr < 0)
		info->title[0] = 0;
	else if (data->track_nr == 0)
		sprintf(info->title, "Full CD length playback");
	else if (data->tracks[data->track_nr].track)
			sprintf(info->title, "%s", data->tracks[data->track_nr].track);
	
	//alsaplayer_error("title = %s\nalbum = %s\nartist = %s",
	//		info->title, info->album, info->artist);
	return 1;
}


static int cdda_nr_tracks(input_object *obj)
{
	struct cdda_local_data *data;

	if (!obj)
		return 0;
	data = (struct cdda_local_data *)obj->local_data;
	if (!data)
		return 0;
	return data->tl.max;
}

static void cdda_shutdown(void)
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

input_plugin *input_plugin_info(void)
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
