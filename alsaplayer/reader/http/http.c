/*   http.c
 *   Copyright (C) 2002 Evgeny Chukreev <codedj@echo.ru>
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
 *   $Id$
 *
 */

#include "config.h"

#include <malloc.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include "reader.h"
#include "alsaplayer_error.h"
#include "utilities.h"
#include "prefs.h"

typedef struct http_desc_t_ {
    char *host, *path;
    int port;
    int sock;
    long size, pos;
    void *buffer;
    int begin;			    /* Pos of first byte in the buffer. */
    int len;			    /* Length of the buffered data. */
    int direction;		    /* Reading direction. */
    int going;			    /* True if buffer is filling. */
    pthread_t thread;		    /* Thread which fill the buffer. */
    pthread_mutex_t buffer_lock;    /* Lock to share buffer in threads */
    pthread_cond_t read_condition;  /* Notice reader_read about new block. */
    pthread_cond_t fast_condition;  /* Give me data as soon as possible. */
    int error;			    /* Error status (0 - none). */
    int fastmode;		    /* No delay while reading from socket. */
} http_desc_t;

/* How much data we should read at once? (bytes)*/
#define  HTTP_BLOCK_SIZE  (32*1024)

/* How long should be buffer? (bytes) */
#define  DEFAULT_HTTP_BUFFER_SIZE  (1*1024*1024)
int http_buffer_size;

/* --------------------------------------------------------------- */
/* ----------------------------- MISC FUNCTIONS ------------------ */
static int cond_timedwait_relative (pthread_cond_t *cond, pthread_mutex_t *mut, unsigned int delay)
{
    struct timeval now;
    struct timespec timeout;

    gettimeofday (&now, NULL);
	    
    timeout.tv_sec = now.tv_sec;
    timeout.tv_nsec = (now.tv_usec + delay) * 1000;
	    
    return pthread_cond_timedwait (cond, mut, &timeout);
}

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
	
	/* Test, port should be digital */
	if ((slash && s!=slash) || (!slash && *s!='\0')) {
	    alsaplayer_error ("HTTP: Couldn't open %s: Port parse error.", uri);
	    return -1;
	}

	/* Calculate host part length */
	l = colon - uri - 7;
    } else {
	/* Calculate host part length */
	l = slash  ?  slash - uri - 7  :  strlen (uri+7);
    }
   
    /* Reset port if URI looks like 'foo.bar:/aaa.mp3' */
    if (colon && slash && slash==colon+1)
	*port = 80;
    
    /* Split URI */
    *host = malloc ((l+1) * sizeof(char));
    strncpy (*host, uri+7, l);
    (*host) [l] = '\0';

    if (slash)
	*path = strdup (slash);
    else
	*path = strdup ("/index.html");

    return 0;
} /* end of: parse_uri */

/* ******************************************************************* */
/* Sleep for data.                                                     */
static int sleep_for_data (int sock)
{
    fd_set set;
    struct timeval tv;
    
    tv.tv_sec = 2;
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
    if (sleep_for_data (sock))  return -1;

    len = recv (sock, ptr, size, 0);

    if (len == -1 && errno == EAGAIN)
	return 0;

    return len;
} /* end of: read_data */


/* ******************************************************************* */
/* Buffer filling thread.                                              */
static void buffer_thread (http_desc_t *desc)
{
    pthread_mutex_t mut;			/* Temporary mutex. */
    int going = desc->going;			/* We should be careful. */
    void *ibuffer = malloc (HTTP_BLOCK_SIZE);	/* Internal thread buffer. */

    /* Init */
    pthread_mutex_init (&mut, NULL);
    
    /* Process while main thread allow it. */
    while (going) {
	void *newbuf;
	int readed;
	
	/* check for overflow */
	going = desc->going;
	if (desc->len > http_buffer_size) {
	    /* Notice waiting function that the new block of data has arrived */
	    pthread_cond_signal (&desc->read_condition);

	    /* Make pause */
	    pthread_mutex_lock (&mut);
	    cond_timedwait_relative (&desc->fast_condition, &mut, 300000);
	    pthread_mutex_unlock (&mut);

	    continue;
	}
	
	/* read to internal buffer */
        readed = read_data (desc->sock, ibuffer, HTTP_BLOCK_SIZE);
	
	/* reasons to stop */
	if (readed == 0) {
	    desc->going = 0;
	    going = 0;
	} else if (readed <0) {
	    desc->error = 1;
	    desc->going = 0;
	    going = 0;
	}

	/* These operations are fast. -> doesn't break reader_read */
	if (readed > 0) {
	   /* ---------------- lock buffer ( */
	    pthread_mutex_lock (&desc->buffer_lock);
	
	    /* enlarge buffer */
	    newbuf = malloc (desc->len + HTTP_BLOCK_SIZE);
	    memcpy (newbuf, desc->buffer, desc->len);
	    memcpy (newbuf + desc->len, ibuffer, readed);
	
	    /* switch buffers */
	    free (desc->buffer);
	    desc->buffer = newbuf;
	    desc->len += readed;

	    /* unlock buffer ) */
	    pthread_mutex_unlock (&desc->buffer_lock);
	}
	
	/* Notice waiting function that the new block of data has arrived */
	pthread_cond_signal (&desc->read_condition);

	/* Do wait */
	if (going && !desc->fastmode) {
	    pthread_mutex_lock (&mut);
	    cond_timedwait_relative (&desc->fast_condition, &mut, 300000);
	    pthread_mutex_unlock (&mut);
	}

	/* Decrement fast mode TTL ;) */
	if (desc->fastmode)
	    desc->fastmode--;
    }
    
    free (ibuffer);
    pthread_exit (NULL);
} /* end of: buffer_thread */

/* ******************************************************************* */
/* close exist connection, and open new one                            */
static int reconnect (http_desc_t *desc)
{
    char request [2048];
    char response [10240];
    char *s;
    int error, error_len;
    struct hostent *hp;
    struct sockaddr_in address;
    fd_set set;
    struct timeval tv;
    int flags;
    int rc;

    /* Clear error status */
    desc->error = 0;
    
    /* Stop filling thread */
    if (desc->going) {
	desc->going = 0;
	pthread_join (desc->thread, NULL);
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
    tv.tv_sec = 2;
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
			     "\r\n",
			     desc->path, desc->host, PACKAGE, VERSION,
			     desc->pos);

    write (desc->sock, request, strlen (request));
    desc->begin = desc->pos;
 
    /* Get response */
    if (get_response_head (desc->sock, response, 10240))
	return 1;

    /* Check protocol */
    if (strncmp (response, "HTTP/1.1 ", 9)) {
	alsaplayer_error ("HTTP: Wrong server protocol for http://%s:%u%s",
		desc->host, desc->port, desc->path);
	return 1;
    }

    /* Check return code */
    rc = atoi (response + 9);
    if (rc != 200) {
	/* Wrong code */
	if (rc == 404) {
	    /* 404 */
	    alsaplayer_error ("HTTP: File not found: http://%s:%u%s",
		desc->host, desc->port, desc->path);
	    return 1;
	} else {
	    /* unknown */
	    alsaplayer_error ("HTTP: We doesn't support %d response code: http://%s:%u%s",
		rc, desc->host, desc->port, desc->path);
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
	alsaplayer_error ("HTTP: Content-Length header is absent!");
	return 0;
    }

    /* Attach thread to fill a buffer */
    desc->going = 1;
    desc->fastmode = 0;
    pthread_create (&desc->thread, NULL, (void* (*)(void *)) buffer_thread, desc);
    
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
	pthread_join (desc->thread, NULL);
    }
    
    /* free resources */
    if (desc->host)  free (desc->host);
    if (desc->path)  free (desc->path);
    if (desc->sock)  close (desc->sock);
    if (desc->buffer)  free (desc->buffer);
    
    free (desc);
} /* end of http_close */

/* ******************************************************************* */
/* open stream                                                         */
static void *http_open(const char *uri)
{
    http_desc_t *desc;
    
    /* Alloc descripor and init members. */
    desc = malloc (sizeof (http_desc_t));
    desc->sock = 0;
    desc->size = 0;
    desc->pos = 0;
    desc->buffer = NULL;
    desc->begin = 0;
    desc->len = 0;
    desc->direction = 0;  
    pthread_mutex_init (&desc->buffer_lock, NULL);
    pthread_cond_init (&desc->read_condition, NULL);
    pthread_cond_init (&desc->fast_condition, NULL);

    /* Parse URI */
    if (parse_uri (uri, &desc->host, &desc->port, &desc->path)) {
	http_close (desc);
	return NULL;
    }

    /* Connect */
    if (reconnect (desc)) {
	http_close (desc);
	return NULL;
    }
    
    return desc;
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
	reconnect (desc);
 
    /* wait while the buffer will has entire block */
    while (1) {
	int readed;
	
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
	
	/* turn on fast mode */
	desc->fastmode = 2;
	pthread_cond_signal (&desc->fast_condition);
	
	/* Allow buffer_thread to use buffer */
        pthread_mutex_unlock (&desc->buffer_lock);
    
	/* Wait for next portion of data */
	pthread_mutex_lock (&mut);
	pthread_cond_wait (&desc->read_condition, &mut);
	pthread_mutex_unlock (&mut);
    }

    /* If there are data to copy */
    if (tocopy) {
	/* copy result */
	memcpy (ptr, desc->buffer + desc->pos - desc->begin, tocopy);
	desc->pos += tocopy;

	/* trying to shrink buffer */
	if (desc->len + HTTP_BLOCK_SIZE > http_buffer_size && desc->pos - desc->begin > http_buffer_size/2) {
	    void *newbuf;
	    
	    desc->len -= tocopy;
	    desc->begin += tocopy;
	    
	    /* allocate new buffer with the part of old one */
	    newbuf = malloc (desc->len);    
	    memcpy (newbuf, desc->buffer + tocopy, desc->len);

	    /* replace old buffer */
	    free (desc->buffer);
	    desc->buffer = newbuf;
	}
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
  
    if (whence == SEEK_SET)
	desc->pos = offset;
    else if (whence == SEEK_CUR)
	desc->pos += offset;
    else
	desc->pos = desc->size - offset;
 
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


/* info about this plugin */
reader_plugin http_plugin = {
	READER_PLUGIN_VERSION,
	"HTTP reader v1.0",
	"Evgeny Chukreev",
	NULL,
	http_init,
	http_shutdown,
	http_can_handle,
	http_open,
	http_close,
	http_read,
	http_seek,
	http_tell,
	http_can_expand,
	http_expand
};

/* ******************************************************************* */
/* return info about this plugin                                       */
reader_plugin *reader_plugin_info(void)
{
	return &http_plugin;
}
