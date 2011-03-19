/*   http.c
 *   Copyright (C) 2002 Evgeny Chukreev <codedj@echo.ru>
 *   Copyright (C) 2002 Andy Lo A Foe <andy@alsaplayer.org>
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

#include <stdlib.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include "reader.h"
#include "alsaplayer_error.h"
#include "utilities.h"
#include "prefs.h"
#include "ap_string.h"


typedef struct http_desc_t_ {
    /* Info about stream */
    char *host, *path;
    int port;
    int sock;
    long size;			/* Total size of stream (0 if unknown ) */
    int seekable;

    /* Info about reading */
    int direction;		/* Reading direction. */
    int played_parts;		/* How much parts (of 16) use for played data */
    long pos;			/* Current position where reader is */

    /* Buffer stuff */
    pthread_t buffer_thread;
    pthread_mutex_t buffer_lock;
    void *buffer;
    int buffer_pos;		/* Pos of the byte which will be readed from server */
    int begin;			/* Pos of first byte in the buffer. */
    int len;			/* Length of the buffered data. */

    /* Metadata stuff */
    pthread_mutex_t meta_lock;
    char *metadata;
    int icy_metaint;

    /* Variables to notify buffer_thread about one pass of non-blocking mode */
    pthread_cond_t dont_wait_signal;
    int dont_wait;

    /* Variables to notify http_read when we have new data block for this function */
    pthread_cond_t new_datablock_signal;
    int new_datablock;

    /* Status of this stream */
    int error;			/* 0 if none */
    int going;			/* True if buffer is filling. */

    /* Callback info */
    reader_status_type status;
    void *data;
} http_desc_t;


/* How much data we should read at once? (bytes)*/
#define  HTTP_BLOCK_SIZE  (32*1024)

/* How long should be buffer? (bytes) */
#define  DEFAULT_HTTP_BUFFER_SIZE  (1*1024*1024)
int http_buffer_size;

/* Maximum waiting time in thread (secs, float) */
#define  MAX_WAIT_TIME  1000000.0

/* Debug mode on/off */
/*#define DEBUG_HTTP_BUFFERING  0*/


/* --------------------------------------------------------------- */
/* ----------------------------- MISC FUNCTIONS ------------------ */
static int cond_timedwait_relative (pthread_cond_t *cond, pthread_mutex_t *mut, unsigned long delay)
{
    struct timeval now;
    struct timespec timeout;

    gettimeofday (&now, NULL);

    timeout.tv_sec = now.tv_sec + delay / 1000000;
    timeout.tv_nsec = (now.tv_usec + delay % 1000000) * 1000;

    return pthread_cond_timedwait (cond, mut, &timeout);
}

/* ------------------------------------------------ */
static int calc_time_to_wait (http_desc_t *desc)
{
    int timetowait;
    int suggested_len = ((long int)http_buffer_size * (long int)(16-desc->played_parts)) / 16;
    int useless_buffer_len = desc->pos - desc->begin;
    int useful_buffer_len = desc->len - useless_buffer_len;

    /* if size of stream is known we could use it to limit suggested len */
    if (desc->size) {
	int rest_of_stream = desc->size - desc->buffer_pos;

	if (rest_of_stream < suggested_len)
	    suggested_len = rest_of_stream;
    };

    /* check for possible overtime */
    if (useful_buffer_len > suggested_len)
	return (int)MAX_WAIT_TIME;

    /* calculate waiting time */
    timetowait = (int)((float)useful_buffer_len / (float)suggested_len * MAX_WAIT_TIME);

    return timetowait;
}

/* ------------------------------------------------ */
void shrink_buffer (http_desc_t *desc)
{
    int low_pos = desc->begin + ((long int)http_buffer_size * (long int)desc->played_parts) / 16;

    /* trying to shrink buffer */
    if (desc->len + HTTP_BLOCK_SIZE > http_buffer_size && desc->pos > low_pos) {
	void *newbuf;
	int shrinking_len = desc->pos - low_pos;

	desc->len -= shrinking_len;
	desc->begin += shrinking_len;

	/* allocate new buffer with the part of old one */
	newbuf = malloc (desc->len);
	memcpy (newbuf, desc->buffer + shrinking_len, desc->len);

	/* replace old buffer */
	free (desc->buffer);
	desc->buffer = newbuf;
    }
}

/* ------------------------------------------------ */
void status_notify (http_desc_t *desc)
{
    char sbuf[1024];

    if (!desc->dont_wait && desc->status) {
	int bck_len = desc->pos - desc->begin;
	int fwd_len = desc->len - bck_len;

	if (desc->seekable)
	    snprintf (sbuf, 1023, "Buf %dK | %dK", fwd_len/1024, bck_len/1024);
	else
	    snprintf (sbuf, 1023, "Buf %dK", fwd_len/1024);

	desc->status (desc->data, sbuf);
    }
}

/* ------------------------------------------------ */
#ifdef DEBUG_HTTP_BUFFERING
static void print_debug_info (http_desc_t *desc)
{
    struct timeval now;

    gettimeofday (&now, NULL);

    printf ("%lld %ld %d %d %ld %d\n", (long long int)now.tv_sec * 1000 + (long long int)now.tv_usec/1000,
	    desc->size, desc->begin, desc->begin + desc->len,
	    desc->pos, desc->buffer_pos);
}
#endif

/* --------------------------------------------------------------- */
/* ---------------------- NETWORK RELATED FUNCTIONS -------------- */

/* ******************************************************************* */
/* Parse URI.                                                           */
static int parse_uri (const char *uri, char **host, int *port, char **path)
{
    char *slash, *colon;
    int l;

    *port = 80;

     /* Trying to find end of a host part */
    slash = strchr (uri+7, '/');
    colon = strchr (uri+7, ':');

    if ((slash && colon && slash > colon) || (!slash && colon)) {
	/* As I see, there is port specified */
	char *s;

	*port = (int)strtol (colon+1, &s, 10);

	/* Test, port should be digit */
	if ((slash && s!=slash) || (!slash && *s!='\0')) {
	    alsaplayer_error ("\nHTTP: Couldn't open %s: Port -- parse error.", uri);
	    return -1;
	}

	/* Calculate host part length */
	l = colon - uri - 7;
    } else {
	/* Calculate host part length */
	l = slash  ?  slash - uri - 7  :  (int) strlen (uri+7);
    }

    /* Reset port if URI looks like 'foo.bar:/aaa.mp3' */
    if (colon && slash && slash==colon+1)
	*port = 80;

    /* Split URI */
    //if (*host)
    //	    free(*host);
    *host = malloc (l+1);
    ap_strlcpy (*host, uri+7, l+1);

    //if (*path)
    //	    free(*path);
    *path = strdup (slash ? slash : "/");
    return 0;
} /* end of: parse_uri */

/* ******************************************************************* */
/* Sleep for data.                                                     */
static int sleep_for_data (int sock)
{
    fd_set set;
    struct timeval tv;

    tv.tv_sec = 5;
    tv.tv_usec = 0;
    FD_ZERO (&set);
    FD_SET (sock, &set);

    if (select (sock+1, &set, NULL, NULL, &tv) < 1) {
	alsaplayer_error ("HTTP: Connection is too slow.");
	return 1;
    }

    return 0;
} /* end of: sleep_for_data */

/* ******************************************************************* */
/* Receive response head.					       */
static int get_response_head (int sock, char *response, int max)
{
    int len = 0;

    while (len < 4 || memcmp (response + len - 4, "\r\n\r\n", 4)) {
	/* check for overflow */
	if (len >= max) {
	    alsaplayer_error ("HTTP: Response is too long.");
	    return 1;
	}

	/* wait for data */
	if (sleep_for_data (sock))  return 1;

	/* read */
	if (read (sock, response + len, 1) <=0 )
	    break;

	len += 1;
    }

    /* terminate string */
    response [len] = '\0';

    return 0;
} /* end of: get_response_head */

/* ******************************************************************* */
/* Read data from stream.                                              */
/* Returns amount of data readed.				       */
static int read_data (int sock, void *ptr, size_t size)
{
    int len;

    /* wait for data */
    if (sleep_for_data (sock)) {
	    return -1;
    }
    len = recv (sock, ptr, size, 0);

    if (len == -1 && errno == EAGAIN) {
	return 0;
    }
    return len;
} /* end of: read_data */


/* ******************************************************************* */
/* Buffer filling thread.                                              */
static void buffer_thread (http_desc_t *desc)
{
    pthread_mutex_t mut;			/* Temporary mutex. */
    int BLOCK_SIZE = HTTP_BLOCK_SIZE;
    void *ibuffer;
    int rest = 0;
    int metasize = 0, metapos = 0, extra_read = 0;
    char *p;

    /* Init */
    pthread_mutex_init (&mut, NULL);

    if (desc->icy_metaint) {
	BLOCK_SIZE = (HTTP_BLOCK_SIZE > desc->icy_metaint) ? desc->icy_metaint : HTTP_BLOCK_SIZE;
    }

    ibuffer = malloc (BLOCK_SIZE << 1);

    /* Process while main thread allow it. */
    while (desc->going) {
	void *newbuf;
	int readed;

#ifdef DEBUG_HTTP_BUFFERING
	print_debug_info (desc);
#endif

	rest = metasize = 0;

	/* trying to shrink buffer */
	pthread_mutex_lock (&desc->buffer_lock);
	shrink_buffer (desc);
	pthread_mutex_unlock (&desc->buffer_lock);

	/* let them know about our state, heh */
	status_notify (desc);

	/* check for overflow */
	if (desc->len > http_buffer_size) {
	    /* Notice waiting function that the new block of data has arrived */
	    desc->new_datablock = 1;
	    pthread_cond_signal (&desc->new_datablock_signal);

	    /* Make pause */
	    if (!desc->dont_wait) {
		pthread_mutex_lock (&mut);
		cond_timedwait_relative (&desc->dont_wait_signal, &mut, calc_time_to_wait (desc));
		pthread_mutex_unlock (&mut);
	    } else {
		desc->dont_wait--;
	    }

	    continue;
	}

	/* read to internal buffer */
        readed = read_data (desc->sock, ibuffer, BLOCK_SIZE);

	/* reasons to stop */
	if (readed == 0) {
	    desc->going = 0;
	} else if (readed <0) {
	    desc->error = 1;
	    desc->going = 0;
	} else {
	    /* Something readed */

	    /* Metadata stuff */
	    if (desc->icy_metaint > 0 &&
		(desc->buffer_pos+readed) >  desc->icy_metaint) {
		/* Metadata block is next! */
		rest = (desc->buffer_pos+readed) - desc->icy_metaint;

		p = ((char *)ibuffer);
		p += (readed-rest);
		metapos = (readed-rest);
		if (rest) {
		    metasize = *(int8_t *)p;
		    metasize <<= 4;
		    if (rest < metasize) {
			/* Uh oh, big trouble ahead, or maybe not? */
			extra_read = read_data (desc->sock, ibuffer+readed, metasize);
			readed += extra_read;
			rest += extra_read;
		    }
		    if (metasize > 4080) {
			alsaplayer_error("Invalid metasize (%d)", metasize);
		    } else if (metasize > 0) {
			p++;
			p[metasize] = '\0';
			pthread_mutex_lock (&desc->meta_lock);
			if (desc->metadata) {
			    free(desc->metadata);
			}
			desc->metadata = malloc(strlen(p)+1);
			memcpy(desc->metadata, p, strlen(p));
			pthread_mutex_unlock (&desc->meta_lock);
		    } else {
			/* Metadata is zero length */
		    }
		} else {
		    alsaplayer_error("Rest = 0???");
		}
		metasize++; /* Length byte */
	    } else {
		desc->buffer_pos += readed;
	    }

            /* These operations are fast. -> doesn't break reader_read */
	    /* ---------------- lock buffer ( */
	    pthread_mutex_lock (&desc->buffer_lock);

	    /* enlarge buffer */
	    newbuf = malloc (desc->len + (BLOCK_SIZE * 2)); /* HTTP_BLOCK_SIZE */
	    memcpy (newbuf, desc->buffer, desc->len);
	    if (metasize) {
		    memcpy(newbuf + desc->len, ibuffer, metapos);
		    memcpy(newbuf + desc->len + metapos, ibuffer+metapos+metasize, rest - metasize);
		    readed -= metasize;
		    desc->buffer_pos = rest - metasize;
	    } else {
		    memcpy (newbuf + desc->len, ibuffer, readed);
	    }
	    /* switch buffers */
	    free (desc->buffer);
	    desc->buffer = newbuf;
	    desc->len += readed;

	    /* unlock buffer ) */
	    pthread_mutex_unlock (&desc->buffer_lock);
	}

	/* Notice waiting function that the new block of data has arrived */
	desc->new_datablock = 1;
	pthread_cond_signal (&desc->new_datablock_signal);

	/* Do wait */
	if (desc->going && !desc->dont_wait) {
	    pthread_mutex_lock (&mut);
	    cond_timedwait_relative (&desc->dont_wait_signal, &mut, calc_time_to_wait (desc));
	    pthread_mutex_unlock (&mut);
	}

	if (desc->dont_wait)
	    desc->dont_wait--;
    }

    free (ibuffer);
    pthread_exit (NULL);
} /* end of: buffer_thread */

/* ******************************************************************* */
/* close exist connection, and open new one                            */
/* follow redirect URLs if encountered                                 */
static int reconnect (http_desc_t *desc, char *redirect)
{
    char request [2048];
    char response [10240];
    char *s;
    int error;
    socklen_t error_len;
    struct hostent *hp;
    struct sockaddr_in address;
    fd_set set;
    struct timeval tv;
    int flags;
    int rc = 0;

    /* Clear error status */
    desc->error = 0;

    /* Stop filling thread */
    if (desc->going) {
	desc->going = 0;
	desc->dont_wait = 10;
	pthread_cond_signal (&desc->dont_wait_signal);
	pthread_join (desc->buffer_thread, NULL);
    }

    /* Close connection */
    if (desc->sock) {
	close (desc->sock);
	desc->sock = 0;
    }

    /* Free buffer */
    if (desc->buffer) {
	free (desc->buffer);
	desc->buffer = NULL;
    }
    desc->begin = 0;
    desc->len = 0;

    /* Look up for host IP */
    if (!(hp = gethostbyname (desc->host))) {
	alsaplayer_error ("HTTP: Couldn't look up host %s.", desc->host);
	return 1;
    }

    /* Open socket */
    desc->sock = socket (AF_INET, SOCK_STREAM, 0);
    if (desc->sock==-1) {
	alsaplayer_error ("HTTP: Couldn't open socket.");
	return 1;
    }

    flags = fcntl (desc->sock, F_GETFL, 0);
    fcntl (desc->sock, F_SETFL, flags | O_NONBLOCK);

    /* Fill address struct */
    address.sin_family = AF_INET;
    address.sin_port = htons (desc->port);
    memcpy (&address.sin_addr.s_addr, *(hp->h_addr_list), sizeof (address.sin_addr.s_addr));

    /* Start connection */
    if (connect (desc->sock, (struct sockaddr *) &address, sizeof (struct sockaddr_in)) == -1) {
	if (errno != EINPROGRESS) {
	    alsaplayer_error ("HTTP: Couldn't connect to host %s:%u", desc->host, desc->port);
	    return 1;
	}
    }

    /* Wait for connection */
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    FD_ZERO (&set);
    FD_SET (desc->sock, &set);

    if (select (desc->sock+1, NULL, &set, NULL, &tv) < 1) {
	alsaplayer_error ("HTTP: Connection is too slow.");
	return 1;
    }

    /* Test for errors while connected */
    error_len = sizeof (error);
    getsockopt (desc->sock, SOL_SOCKET, SO_ERROR, &error, &error_len);
    if (error) {
	alsaplayer_error ("HTTP: Couldn't connect to host %s:%u", desc->host, desc->port);
	return 1;
    }

    /* Send request for remote file */
    snprintf (request, 2048, "GET %s HTTP/1.1\r\n"
			     "Host: %s\r\n"
			     "Connection: close\r\n"
			     "User-Agent: %s/%s\r\n"
			     "Range: bytes=%ld-\r\n"
			     "Icy-Metadata: 1\r\n"
			     "\r\n",
			     desc->path, desc->host, PACKAGE, VERSION,
			     desc->pos);
    //alsaplayer_error("%s", request);
    write (desc->sock, request, strlen (request));
    desc->begin = desc->buffer_pos = desc->pos;

    /* Get response */
    if (get_response_head (desc->sock, response, 10240))
	return 1;

    /* Check protocol */
    if (!strncmp (response, "HTTP/1.0 ", 9)) {
	desc->seekable = 0;
    } else if (!strncmp (response, "HTTP/1.1 ", 9)) {
        desc->seekable = 1;
    } else if (!strncmp (response, "ICY 200 OK", 10)) {
    	desc->seekable = 0;
	rc = 200;
	//alsaplayer_error("%s", response);
    } else if (!strncmp (response, "ICY 400 Server Full", 19)) {
	rc = 400;
    } else if (!strncmp (response, "ICY 404", 7)) {
	 rc = 404;
    } else {
	if (strlen(response)) {
		alsaplayer_error ("HTTP: Wrong server protocol for http://%s:%u%s",
			desc->host, desc->port, desc->path);
		alsaplayer_error("ERROR:\n\"%s\"", response);
	}
	return 1;
    }

    /* Check return code */
    if (strstr(response, "HTTP"))
    	rc = atoi (response + 9);

    if (rc != 200 && rc != 206) {
	/* Wrong code */
	if (rc == 404) {
	    /* 404 */
	    alsaplayer_error ("HTTP: File not found: http://%s:%u%s",
		desc->host, desc->port, desc->path);
	    return 1;
	} else if (rc == 302) {
            s = strstr(response, "302");
	    if (s) {
		    //alsaplayer_error("%s", s);
		    s = strstr(response, "Location: ");
		    if (s && redirect) {
			    /* Parse redirect */
			    if (sscanf(s, "Location: %[^\r]", redirect)) {
				    /* alsaplayer_error("Redirection: %s", redirect); */
			    }
		    }
		    return 1;
	    }
	} else if (rc == 400) {
		if (desc->status) {
			desc->status(desc->data, "Server is full");
		}
		if (redirect)
			redirect[0] = 0;
		return 1;
	} else if (rc == 401) {
		if (desc->status) {
			desc->status(desc->data, "Unauthorized access");
		}
		if (redirect)
			redirect[0] = 0;
		return 1;
	} else if (rc == 404) {
		if (desc->status) {
			desc->status(desc->data, "Resource not found");
		}
		if (redirect)
			redirect[0] = 0;
		return 1;
	} else {
		/* unknown */
		alsaplayer_error ("HTTP: We don't support %d response code: http://%s:%u%s",
				rc, desc->host, desc->port, desc->path);
		if (redirect)
			redirect[0] = 0;
		return 1;
	}
    }
    /* Looking for size */
    s = strstr (response, "\r\nContent-Length: ");
    if (s) {
	/* Set size only once */
	if (!desc->size)
	    desc->size = atol (s+18);
    } else {
	desc->seekable = 0;
    }
    /* Look for icy-metaint */
    s = strstr (response, "\r\nicy-metaint:");
    if (s) {
	desc->icy_metaint = atoi(s+14);
	//alsaplayer_error("Found metaint: %d", desc->icy_metaint);
    } else {
    	desc->icy_metaint = 0;
    }

    /* Setup played parts */
    desc->played_parts = desc->seekable ? 5 : 0;

    /* Attach thread to fill a buffer */
    desc->going = 1;
    pthread_create (&desc->buffer_thread, NULL, (void* (*)(void *)) buffer_thread, desc);

    return 0;
} /* end of: reconnect */

/* --------------------------------------------------------------- */
/* ------------------------------------ PLUGIN FUNCTIONS --------- */

/* ******************************************************************* */
/* close stream                                                        */
static void http_close(void *d)
{
    http_desc_t *desc = (http_desc_t*)d;

    /* stop buffering thread */
    if (desc->going) {
	desc->going = 0;
	desc->dont_wait = 10000;
	pthread_cond_signal (&desc->dont_wait_signal);
	pthread_join (desc->buffer_thread, NULL);
    }

    /* free resources */
    if (desc->host)  free (desc->host);
    if (desc->path)  free (desc->path);
    if (desc->metadata) free (desc->metadata);
    if (desc->sock)  close (desc->sock);
    if (desc->buffer)  free (desc->buffer);

    free (desc);
} /* end of http_close */

/* ******************************************************************* */
/* open stream                                                         */
static void *http_open(const char *uri, reader_status_type status, void *data)
{
    http_desc_t *desc;
    char redirect[10240];
    int tries = 0;

    /* Alloc descripor and init members. */
    desc = malloc (sizeof (http_desc_t));
    desc->going = 0;
    desc->new_datablock = 0;
    desc->dont_wait = 0;
    desc->sock = 0;
    desc->size = 0;
    desc->pos = 0;
    desc->buffer = NULL;
    desc->buffer_pos = 0;
    desc->begin = 0;
    desc->len = 0;
    desc->direction = 0;
    desc->metadata = NULL;
    desc->status = status;
    desc->data = data;
    pthread_mutex_init (&desc->buffer_lock, NULL);
    pthread_mutex_init (&desc->meta_lock, NULL);
    pthread_cond_init (&desc->new_datablock_signal, NULL);
    pthread_cond_init (&desc->dont_wait_signal, NULL);

    /* Parse URI */
    if (parse_uri (uri, &desc->host, &desc->port, &desc->path)) {
	http_close (desc);
	return NULL;
    }

    /* Connect */
    if (desc->status) {
	    desc->status(desc->data, "Connecting");
    }
    while (tries++ < 5) {
	redirect[0] = 0;
    	if (reconnect (desc, redirect)) {
		if (*redirect) {
			if (parse_uri (redirect, &desc->host, &desc->port, &desc->path)) {
				http_close(desc);
				return NULL;
			}
		}
		continue;
	} else { /* Success! */
		return desc;
	}
    }
    http_close (desc);
    return NULL;
}

/* ******************************************************************* */
/* test function                                                       */
static float http_can_handle(const char *uri)
{
    /* Check for prefix */
    if (strncmp (uri, "http://", 7))  return 0.0;

    return 1.0;
}

/* ******************************************************************* */
/* init plugin                                                         */
static int http_init()
{
    http_buffer_size = prefs_get_int (ap_prefs, "http", "buffer_size", DEFAULT_HTTP_BUFFER_SIZE);

    /* Be sure http.buffer_size is available in config */
    prefs_set_int (ap_prefs, "http", "buffer_size", http_buffer_size);

    return 1;
}

/* ******************************************************************* */
/* shutdown plugin                                                     */
static void http_shutdown()
{
    return;
}


/* ***************************************************************** */
/* read metadata from stream					     */
static size_t http_metadata (void *d, size_t size, void *data)
{
	http_desc_t *desc = (http_desc_t*)d;
	size_t msize;

	pthread_mutex_lock (&desc->meta_lock);

	if (!desc->metadata) {
		pthread_mutex_unlock (&desc->meta_lock);
		return 0;
	}
	msize = strlen(desc->metadata);
	if (msize > size)
		msize = size;
	memcpy(data, desc->metadata, msize);
	pthread_mutex_unlock (&desc->meta_lock);
	return msize;
}


/* ***************************************************************** */
/* read from stream                                                  */
static size_t http_read (void *ptr, size_t size, void *d)
{
    http_desc_t *desc = (http_desc_t*)d;
    pthread_mutex_t mut;    /* temporary mutex. What is for? ;) */
    int tocopy;		    /* how much bytes we got? */

    /* Init temp mutex */
    pthread_mutex_init (&mut, NULL);

    /* check for reopen */
    if (desc->begin > desc->pos || desc->begin + desc->len + 3*HTTP_BLOCK_SIZE < desc->pos)
	reconnect (desc, NULL);

    /* wait while the buffer will has entire block */
    while (1) {
	size_t readed;

	/* check for error */
	if (desc->error) {
	    return 0;
	}

	/* We will work with buffer... we have to lock it! */
	pthread_mutex_lock (&desc->buffer_lock);
	readed = desc->begin + desc->len - desc->pos;

	/* done? */
	if (readed > 0 && readed >= size) {
	    tocopy = size;
	    break;
	}

	/* EOF reached and there is some data */
	if (!desc->going && readed > 0) {
	    tocopy = readed;
	    break;
	}

	/* EOF reached and there is no data readed*/
	if (!desc->going) {
	    tocopy = 0;
	    break;
	}

	/* break waiting */
	desc->dont_wait = 1;
	pthread_cond_signal (&desc->dont_wait_signal);

	/* Allow buffer_thread to use buffer */
        pthread_mutex_unlock (&desc->buffer_lock);

	/* Wait for next portion of data */
	if (!desc->new_datablock) {
	    pthread_mutex_lock (&mut);
	    pthread_cond_wait (&desc->new_datablock_signal, &mut);
	    pthread_mutex_unlock (&mut);
	} else {
	    desc->new_datablock --;
	}
    }

    /* copy result If there are data to copy */
    if (tocopy) {
	memcpy (ptr, desc->buffer + desc->pos - desc->begin, tocopy);
	desc->pos += tocopy;

	/* let them know about new state, heh */
	status_notify (desc);
    }

    /* Allow buffer_thread to use buffer. */
    pthread_mutex_unlock (&desc->buffer_lock);

    return tocopy;
} /* http_read */


/* ******************************************************************* */
/* seek in stream                                                      */
static int http_seek (void *d, long offset, int whence)
{
    http_desc_t *desc = (http_desc_t*)d;

    if (!desc->seekable)  return -1;

    if (whence == SEEK_SET)
	desc->pos = offset;
    else if (whence == SEEK_END)
	desc->pos = desc->size + offset;
    else
	desc->pos += offset;

    return 0;
}


/* ******************************************************************* */
/* Return current position in stream.                                  */
static long http_tell (void *d)
{
    http_desc_t *desc = (http_desc_t*)d;

    return desc->pos;
}

/* ******************************************************************* */
/* directory test                                                      */
static float http_can_expand (const char *uri)
{
    return 0.0;
}

/* ******************************************************************* */
/* expand directory                                                    */
static char **http_expand (const char *uri)
{
    return NULL;
}

/* ******************************************************************* */
/* seekable                                                            */
static int http_seekable (void *d)
{
    return ((http_desc_t*)d)->seekable;
}

/* ******************************************************************* */
/* amount of data in stream                                            */
static long http_length (void *d)
{
    http_desc_t *desc = (http_desc_t*)d;

    if (!desc->seekable)
	return -1;

    return desc->size;
}

/* ******************************************************************* */
static int http_eof (void *d)
{
    http_desc_t *desc = (http_desc_t*)d;

    if (!desc->seekable)
	return 0;

    return !(desc->pos < desc->size);
}

/* #################################################################### */
/* info about this plugin */
reader_plugin http_plugin = {
	READER_PLUGIN_VERSION,
	"HTTP reader v1.3",
	"Evgeny Chukreev",
	NULL,
	http_init,
	http_shutdown,
	http_can_handle,
	http_open,
	http_close,
	http_read,
	http_metadata,
	http_seek,
	http_tell,
	http_can_expand,
	http_expand,
	http_length,
	http_eof,
	http_seekable
};

/* ******************************************************************* */
/* return info about this plugin                                       */
reader_plugin *reader_plugin_info(void)
{
	return &http_plugin;
}
