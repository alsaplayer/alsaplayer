/*   curlplugin.c
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

#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <malloc.h>
#include <curl/curl.h>

#include "alsaplayer_error.h"
#include "reader.h"
#include "string.h"

/* Internal info. */
typedef struct _curlplugin_info {
    CURL	    *easy;
    CURLM	    *multi;
    void	    *buf;
    unsigned int    size;
    unsigned int    pos;
    char	    *uri;
} curlplugin_info;

/*
 * Handler for 'data arrived' callback.
*/
static size_t write_response (void *ptr, size_t size, size_t nmemb, curlplugin_info *info)
{
    /* Grow up temporary buffer. */
    info->buf = realloc (info->buf, info->size + size*nmemb);

    /* Append new data at the end. */
    memcpy (info->buf + info->size, ptr, size*nmemb);
    info->size += size*nmemb;
    
    return size*nmemb;
}


/*
 * Helper function to skip first n bytes from the internal buffer.
*/
static void skip_first (curlplugin_info *info, size_t n)
{
    void *newbuf;
    int tail;
      
    /* Get the length of data which we will keep. */
    tail = info->size - n;

    /* Allocate new internal buffer for this tail. */
    newbuf = malloc (tail + 1);
    memcpy (newbuf, info->buf + n, tail);
    
    /* Swap new buffer with the current. */
    free (info->buf);
    info->buf = newbuf;
    
    /* Set new buffer's info. */
    info->pos += n;
    info->size = tail;
}

/*
 * Intialize stream to open.
*/
static void *curlplugin_open(const char *uri)
{
    CURL *easy;
    CURLM *multi;
    curlplugin_info *info;
    CURLMcode mrc;
   
    easy = curl_easy_init ();
    if (!easy) {
	alsaplayer_error ("curl reader plugin: Couldn't init easy handler.\n");
	return NULL;
    }
    
    multi = curl_multi_init ();
    if (!multi) {
	alsaplayer_error ("curl reader plugin: Couldn't init multi handler.\n");
	curl_easy_cleanup (easy);
	return NULL;
    }

    /* Remember some info. */
    info = (curlplugin_info*)malloc (sizeof (curlplugin_info));
    info->easy = easy;
    info->uri = strdup (uri);
    info->multi = multi;
    info->size = 0;
    info->buf = malloc (1);
    info->pos = 0;
    
    /* Setup easy handle. */
    curl_easy_setopt (easy, CURLOPT_WRITEFUNCTION, write_response);
    curl_easy_setopt (easy, CURLOPT_FILE, info);
    curl_easy_setopt (easy, CURLOPT_URL, uri);
 
    /* Links easy and multi handles. */
    mrc = curl_multi_add_handle (multi, easy);
    if (mrc != CURLM_OK && mrc != CURLM_CALL_MULTI_PERFORM) {
	alsaplayer_error ("curl reader plugin: Couldn't add easy handler in multi handler (rc=%d).\n", mrc);

	curl_easy_cleanup (easy);
	curl_multi_cleanup (multi);
	free (info->buf);
	free (info);

	return NULL;
    }
    
    return info;
} /* end of: static void *curlplugin_open(const char *uri) */

/*
 * Close stream.
*/
static void curlplugin_close(void *d)
{
    curlplugin_info *info = (curlplugin_info*)d;
 
    if (info->multi)
	curl_multi_cleanup (info->multi);

    if (info->easy)
	curl_easy_cleanup (info->easy);

    free (info->buf);
    free (info->uri);
    free (info);
   
    return;
}

/*
 * Can this reader plugin handle this URI?
 * Return processing quiality.
*/
static float curlplugin_can_handle(const char *uri)
{
    /* Check for prefix */
    if (strncmp (uri, "ftp://", 6) && strncmp (uri, "http://", 7))  return 0.0;
    
    return 1.0;
}

/*
 * Init curl plugin.
*/
static int curlplugin_init()
{
    curl_global_init (CURL_GLOBAL_DEFAULT);

    return 1;
}

/*
 * Cleanup curl plugin.
*/
static void curlplugin_shutdown()
{
    curl_global_cleanup ();

    return;
}

/*
  Read data from the stream.
*/
static size_t curlplugin_read (void *ptr, size_t size, void *d)
{
    curlplugin_info *info = (curlplugin_info*)d;
    int running_handles = 1;
    CURLMcode rc = CURLM_OK;
    fd_set r;
    int downloaded;

    /* Init vars. */
    FD_ZERO (&r);
 
    /* Fill internal buffer. */
    while ((rc == CURLM_CALL_MULTI_PERFORM || info->size < size) && running_handles) {
	int m;

	/* Wait for data */
	rc = curl_multi_fdset (info->multi, &r, NULL, NULL, &m);
	if (m>0 && rc == CURLM_OK) {
	    struct timeval tv;
	    int retval;

	    tv.tv_sec = 1;
	    tv.tv_usec = 0;
	    
	    retval = select (m+1, &r, NULL, NULL, &tv);
	    
	    if (!retval)
		alsaplayer_error ("Warning: Connection to '%s' is too slow.\n", info->uri);
	    else if (retval == -1) {
		alsaplayer_error ("curl reader plugin: Internal error occuered while downloading.\n");
		return 0;
	    }
	}
	
	/* Read data */
	rc = curl_multi_perform (info->multi, &running_handles);
	if (rc != CURLM_CALL_MULTI_PERFORM && rc != CURLM_OK) {
	    alsaplayer_error ("curl reader plugin: Couldn't perform downloading (rc=%d).\n", rc);
	    return 0;
	}
    }

    /* Find a minimum between requested and downloaded size. */
    if (info->size > size)
	downloaded = size;
    else
	downloaded = info->size;
    
    /* Copy to an external buffer. */
    memcpy (ptr, info->buf, downloaded);

    /* Remove returning data from the internal buffer */
    skip_first (info, downloaded);

    return downloaded;
}

/*
 * Seek in the stream.
*/
static int curlplugin_seek (void *d, long offset, int whence)
{
    curlplugin_info *info = (curlplugin_info*)d;
    CURLMcode mrc;

    /* TODO: Implement it, if it is necessary. */
    if (whence == SEEK_END) {
	alsaplayer_error ("curl reader plugin: Seek function of the curl plugin doesn't support SEEK_END whence.\n");
	return -1;
    }
    
    /* Cases when the real seek is not needed. */
    if (info->pos == offset && whence == SEEK_SET)
	return 0;
    else if (offset == 0 && whence == SEEK_CUR)
	return 0;
    else if (offset > 0 && offset <= info->size && whence == SEEK_CUR) {
	skip_first (info, offset);
	return 0;
    } else if (offset >= info->pos && offset <= info->pos+info->size && whence == SEEK_SET) {
	skip_first (info, offset - info->pos);
	return 0;
    }
    
    /* Really seek. */

    /* Since libcurl has no function whih can terminate connection,
     * we should reopen connection. TODO: Follow libcurl features.
    */
 
    /* Close connection. */
    curl_multi_cleanup (info->multi);
    curl_easy_cleanup (info->easy);
    info->multi = info->easy = NULL;
    
    /* Init new connection. */
    info->easy = curl_easy_init ();
    if (!info->easy) {
	alsaplayer_error ("curl reader plugin: Couldn't init easy handler.\n");
	return -1;
    }
    
    info->multi = curl_multi_init ();
    if (!info->multi) {
	alsaplayer_error ("curl reader plugin: Couldn't init multi handler.\n");
	return -1;
    }
    
    /* Setup easy handle. */
    curl_easy_setopt (info->easy, CURLOPT_WRITEFUNCTION, write_response);
    curl_easy_setopt (info->easy, CURLOPT_FILE, info);
    curl_easy_setopt (info->easy, CURLOPT_URL, info->uri);
 
    if (whence == SEEK_SET) {
	info->pos = offset;
    } else if (whence == SEEK_CUR) {
	info->pos += offset;
    }
    curl_easy_setopt (info->easy, CURLOPT_RESUME_FROM, info->pos);
   
    /* Links easy and multi handles. */
    mrc = curl_multi_add_handle (info->multi, info->easy);
    if (mrc != CURLM_OK && mrc != CURLM_CALL_MULTI_PERFORM) {
	alsaplayer_error ("curl reader plugin: Couldn't add easy handler in multi handler (rc=%d).\n", mrc);
	return -1;
    }
       
    /* buffer is empty now */
    free (info->buf);
    info->buf = malloc (1);
    info->size = 0;
   
    return -1;
}

/*
 * Can we expand this uri?
*/
static float curlplugin_can_expand (const char *uri)
{
    return 0.0;
}

/*
 * Get the list of URIs which this URI could provide.
*/
static char **curlplugin_expand (const char *uri)
{
    return NULL;
}

/* Info about this plugin. */
reader_plugin curlplugin_plugin = {
	READER_PLUGIN_VERSION,
	"curl reader v1.0",
	"Evgeny Chukreev",
	NULL,
	curlplugin_init,
	curlplugin_shutdown,
	curlplugin_can_handle,
	curlplugin_open,
	curlplugin_close,
	curlplugin_read,
	curlplugin_seek,
	curlplugin_can_expand,
	curlplugin_expand
};

/* Return info about this plugin. */
reader_plugin *reader_plugin_info(void)
{
	return &curlplugin_plugin;
}
