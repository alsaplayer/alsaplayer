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

#include "reader.h"
#include "alsaplayer_error.h"

typedef struct http_desc_t_ {
    char *host, *path;
    int port;
    int sock;
    long size, pos;
} http_desc_t;

/* --------------------------------------------------------------- */
/* ---------------------- NETWORK RELATED FUNCTIONS -------------- */

/* Parse URI */
static int parse_uri (const char *uri, char **host, int *port, char **path) 
{
    char *slash, *colon;
    int l;

    *port = 80;
    
     /* Trying to find the end of a host part */
    slash = strchr (uri+7, '/');
    colon = strchr (uri+7, ':');
    
    if ((slash && colon && slash > colon) || (!slash && colon)) {
	/* As I see, there is port specified */
	char *s;
	
	*port = (int)strtol (colon+1, &s, 10);
	
	/* Test, port should be digital */
	if ((slash && s!=slash) || (!slash && *s!='\0')) {
	    alsaplayer_error ("HTTP: Couldn't open %s: Port parse error.\n", uri);
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

    alsaplayer_error ("HTTP: Parsed URI: host='%s', port=%u, path='%s'\n", *host, *port, *path);

    return 0;
}

/* sleep for data */
static int sleep_for_data (int sock)
{
    fd_set set;
    struct timeval tv;
    
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    FD_ZERO (&set);
    FD_SET (sock, &set);
    
    if (select (sock+1, &set, NULL, NULL, &tv) < 0) {
	alsaplayer_error ("HTTP: Connection is too slow.\n");
	return 1;
    }

    return 0;
}

/* get response head */
static int get_response_head (int sock, char *response, int max)
{
    int len = 0;

    while (len < 4 || memcmp (response + len - 4, "\r\n\r\n", 4)) {
	/* check for overflow */
	if (len >= max) {
	    alsaplayer_error ("HTTP: Response is too long.\n");
	    return 1;
	}

	/* wait for data */
	if (sleep_for_data (sock))  return 1;
	
	/* read */
	if (read (sock, response + len, 1) <=0 ) {
	    alsaplayer_error ("HTTP: Error reading from socket.\n");
	    return 1;
	}
	
	len += 1;
    }
    
    /* terminate string */
    response [len] = '\0';
    
    return 0;
}


/* read data from stream */
static int read_data (int sock, void *ptr, size_t size)
{
    int len = 0;

    while (len < size) {
	/* wait for data */
	if (sleep_for_data (sock))  return 1;
	
	/* read */
	if (read (sock, ptr + len, 1) <=0 ) {
	    alsaplayer_error ("HTTP: Error reading from socket.\n");
	    return 1;
	}
	
	len += 1;
    }

    return 0;
}

/* close exist connection, and open new one */
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

    /* Close connection */
    if (desc->sock) {
	close (desc->sock);
	desc->sock = 0;
    }
    
    /* Look up for host IP */
    if (!(hp = gethostbyname (desc->host))) {
	alsaplayer_error ("HTTP: Couldn't look up host %s.\n", desc->host);
	return 1;
    }

    /* Open socket */
    desc->sock = socket (AF_INET, SOCK_STREAM, 0);
    if (desc->sock==-1) {
	alsaplayer_error ("HTTP: Couldn't open socket.\n");
	return 1;
    }
    
    fcntl (desc->sock, F_SETFL, O_NONBLOCK);

    /* Fill address struct */
    address.sin_family = AF_INET;
    address.sin_port = htons (desc->port);
    memcpy (&address.sin_addr.s_addr, *(hp->h_addr_list), sizeof (address.sin_addr.s_addr));

    /* Start connection */
    if (connect (desc->sock, (struct sockaddr *) &address, sizeof (struct sockaddr_in)) == -1) {
	if (errno != EINPROGRESS) {
	    alsaplayer_error ("HTTP: Couldn't connect to host %s:%u\n", desc->host, desc->port);
	    return 1;
	}
    }

    /* Wait for connection */
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    FD_ZERO (&set);
    FD_SET (desc->sock, &set);
    
    if (select (desc->sock+1, NULL, &set, NULL, &tv) < 0) {
	alsaplayer_error ("HTTP: Connection is too slow.\n");
	return 1;
    }

    /* Test for errors while connection */
    error_len = sizeof (error);
    getsockopt (desc->sock, SOL_SOCKET, SO_ERROR, &error, &error_len);
    if (error) {
	alsaplayer_error ("HTTP: Couldn't connect to host %s:%u\n", desc->host, desc->port);
	return 1;
    }

    /* Get info about remote file */
    snprintf (request, 2048, "HEAD %s HTTP/1.1\r\n"
			     "Host: %s\r\n"
			     "User-Agent: %s/%s\r\n"
			     "\r\n",
			     desc->path, desc->host, PACKAGE, VERSION);

    write (desc->sock, request, strlen (request));
 
    if (get_response_head (desc->sock, response, 10240))
	return 1;

    /* Looking for size */
    s = strstr (response, "\r\nContent-Length: ");
    if (s) {
	desc->size = atol (s+18);
    } else {
	alsaplayer_error ("HTTP: Content-Length header is absent!\n");
    }

    return 0;
}

/* --------------------------------------------------------------- */
/* ------------------------------------ PLUGIN FUNCTIONS --------- */

/* close stream */
static void http_close(void *d)
{
    http_desc_t *desc = (http_desc_t*)d;
    
    if (desc->host)  free (desc->host);
    if (desc->path)  free (desc->path);
    if (desc->sock)  close (desc->sock);
    
    free (desc);
}


/* open stream */
static void *http_open(const char *uri)
{
    http_desc_t *desc;
    
    /* Alloc descripor */
    desc = malloc (sizeof (http_desc_t));
    desc->sock = 0;
    desc->size = 0;
    desc->pos = 0;
   
    /* Parse URI */
    if (parse_uri (uri, &desc->host, &desc->port, &desc->path))
	return NULL;

    /* Connect */
    if (reconnect (desc)) {
	http_close (desc);
	return NULL;
    }
    
    return desc;
}


/* test function */
static float http_can_handle(const char *uri)
{
    /* Check for prefix */
    if (strncmp (uri, "http://", 7))  return 0.0;

    return 1.0;
}


/* init plugin */
static int http_init()
{
    return 1;
}


/* shutdown plugin */
static void http_shutdown()
{
    return;
}


/* read from stream */
static size_t http_read (void *ptr, size_t size, void *d)
{
    http_desc_t *desc = (http_desc_t*)d;
    char request [2048];
    char response [10240];
    char *s;
    
    printf ("HTTP: Read ptr=%p, size=%d\n", ptr, size);
    
    /* Send request */
    snprintf (request, 2048, "GET %s HTTP/1.1\r\n"
			     "Host: %s\r\n"
			     "User-Agent: %s/%s\r\n"
			     "Range: bytes=%ld-%ld\r\n"
			     "\r\n",
			     desc->path, desc->host,
			     PACKAGE, VERSION,
			     desc->pos, desc->pos + size-1);

    write (desc->sock, request, strlen (request));

    /* Get head of response */
    if (get_response_head (desc->sock, response, 10240))
	return 0;

    /* Looking for size */
    s = strstr (response, "\r\nContent-Length: ");
    if (s) {
	size = atol (s+18);
	printf ("%d\n", size);
    } else {
	alsaplayer_error ("HTTP: Content-Length header is absent!\n");
	return 0;
    }
    
    /* read */
    read_data (desc->sock, ptr, size);
    
    /* Increment position in stream */
    desc->pos += size;
       
    return size;
}


/* seek in stream */
static int http_seek (void *d, long offset, int whence)
{
    http_desc_t *desc = (http_desc_t*)d;
  
    alsaplayer_error ("HTTP: seek offset=%ld, whence=%d\n", offset, whence);

    if (whence == SEEK_SET)
	desc->pos = offset;
    else if (whence == SEEK_CUR)
	desc->pos += offset;
    else
	desc->pos = desc->size - offset;
    
    return 0;
}


/*
 * Return current position in stream.
*/
static long http_tell (void *d)
{
    http_desc_t *desc = (http_desc_t*)d;

    return desc->pos;
}


/* directory test */
static float http_can_expand (const char *uri)
{
    return 0.0;
}


/* expand directory */
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


/* return info about this plugin */
reader_plugin *reader_plugin_info(void)
{
	return &http_plugin;
}
