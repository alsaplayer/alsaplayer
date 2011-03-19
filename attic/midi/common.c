/*

    TiMidity -- Experimental MIDI to WAVE converter
    Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>

 *  This file is part of the MIDI input plugin for AlsaPlayer.
 *
 *  The MIDI input plugin for AlsaPlayer is free software;
 *  you can redistribute it and/or modify it under the terms of the
 *  GNU General Public License as published by the Free Software
 *  Foundation; either version 3 of the License, or (at your option)
 *  any later version.
 *
 *  The MIDI input plugin for AlsaPlayer is distributed in the hope that
 *  it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 *  warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
   common.c

   */

#include <stdio.h>
#include <stdlib.h>

#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#ifndef OLD_FOPEN_METHOD
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

#include <errno.h>
#include "gtim.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "effects.h"
#include "md.h"
#include "readmidi.h"
#include "output.h"
#include "controls.h"

/* I guess "rb" should be right for any libc */
#define OPEN_MODE "rb"

char *program_name, current_filename[1024];
int current_filedescriptor;

static PathList *pathlist=0;

#ifdef __WIN32__
#define R_OPEN_MODE O_RDONLY | O_BINARY
#else
#define R_OPEN_MODE O_RDONLY
#endif

/* Try to open a file for reading. If the filename ends in one of the 
   defined compressor extensions, pipe the file through the decompressor */
static FILE *try_to_open(char *name, int decompress)
{
  FILE *fp;

#ifdef OLD_FOPEN_METHOD
  fp=fopen(name, OPEN_MODE); /* First just check that the file exists */
#else
  if ( (current_filedescriptor = open(name, R_OPEN_MODE)) == -1) return 0;
  fp = fdopen(current_filedescriptor, OPEN_MODE);
#endif

  if (!fp)
    return 0;

#ifdef DECOMPRESSOR_LIST
  if (decompress)
    {
      int l,el;
      static const char *decompressor_list[] = DECOMPRESSOR_LIST, **dec;
      char tmp[1024], tmp2[1024], *cp, *cp2;
      /* Check if it's a compressed file */ 
      l=strlen(name);
      for (dec=decompressor_list; *dec; dec+=2)
	{
	  el=strlen(*dec);
	  if ((el>=l) || (strcmp(name+l-el, *dec)))
	    continue;

	  /* Yes. Close the file, open a pipe instead. */
	  fclose(fp);

	  /* Quote some special characters in the file name */
	  cp=name;
	  cp2=tmp2;
	  while (*cp)
	    {
	      switch(*cp)
		{
		case '\'':
		case '\\':
		case ' ':
		case '`':
		case '!':
		case '"':
		case '&':
		case ';':
		  *cp2++='\\';
		}
	      *cp2++=*cp++;
	    }
	  *cp2=0;

	  sprintf(tmp, *(dec+1), tmp2);
	  fp=popen(tmp, "r");
	  break;
	}
    }
#endif
  
  return fp;
}

/* This is meant to find and open files for reading, possibly piping
   them through a decompressor. */
FILE *open_file(const char *name, int decompress, int noise_mode, int level)
{
  FILE *fp;
  PathList *plp=pathlist;
  int l;

  if (!name || !(*name))
    {
      ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Attempted to open nameless file.");
      return 0;
    }

  /* First try the given name */

  strncpy(current_filename, name, 1023);
  current_filename[1023]='\0';

  /* when called from read_config_file, do not look in current dir */
  if (level==0 || name[0]==PATH_SEP)
    {
      ctl->cmsg(CMSG_INFO, VERB_DEBUG, "Trying to open %s", current_filename);

      if ( (fp=try_to_open(current_filename, decompress)) )
        return fp;

      if (noise_mode && (errno != ENOENT))
        {
          ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s", 
	       current_filename, sys_errlist[errno]);
          return 0;
        }
    }
  
  if (name[0] != PATH_SEP)
    while (plp)  /* Try along the path then */
      {
	*current_filename=0;
	l=strlen(plp->path);
	if(l)
	  {
	    strcpy(current_filename, plp->path);
	    if(current_filename[l-1]!=PATH_SEP)
	      strcat(current_filename, PATH_STRING);
	  }
	strcat(current_filename, name);
	ctl->cmsg(CMSG_INFO, VERB_DEBUG, "Trying to open %s", current_filename);
	if ((fp=try_to_open(current_filename, decompress)))
	  return fp;
	if (noise_mode && (errno != ENOENT))
	  {
	    ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s", 
		 current_filename, sys_errlist[errno]);
	    return 0;
	  }
	plp=(PathList *)plp->next;
      }
  
  /* Nothing could be opened. */

  *current_filename=0;
  
  if (noise_mode>=2)
    ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s", name, sys_errlist[errno]);
  
  return 0;
}

/* This closes files opened with open_file */
void close_file(FILE *fp)
{
#ifdef DECOMPRESSOR_LIST
  if (pclose(fp)) /* Any better ideas? */
#endif
    fclose(fp);
}

/* This is meant for skipping a few bytes in a file or fifo. */
void skip(FILE *fp, size_t len)
{
  size_t c;
  char tmp[1024];
  while (len>0)
    {
      c=len;
      if (c>1024) c=1024;
      len-=c;
      if (c!=fread(tmp, 1, c, fp))
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: skip: %s",
	     current_filename, sys_errlist[errno]);
    }
}

/* This'll allocate memory or die. */
void *safe_malloc(size_t count)
{
  void *p;
  if (count > (1<<21))
    {
      ctl->cmsg(CMSG_FATAL, VERB_NORMAL, 
	   "Strange, I feel like allocating %d bytes. This must be a bug.",
	   count);
    }
  else if ((p=malloc(count)))
    return p;
  else
    ctl->cmsg(CMSG_FATAL, VERB_NORMAL, "Sorry. Couldn't malloc %d bytes.", count);

  ctl->close();
  /*play_mode->purge_output();*/
  /*play_mode->close_output();*/
    #ifdef ADAGIO
    X_EXIT
    #else
    exit(10);
    #endif
  return 0;
}

/* This adds a directory to the path list */
void add_to_pathlist(char *s, int level)
{
  int l;
  PathList *plp=(PathList *)safe_malloc(sizeof(PathList));

  if (s[0] == PATH_SEP)
	strcpy((plp->path=(char *)safe_malloc(strlen(s)+1)),s);
  else {
	PathList *pnp = pathlist;
	while (pnp && pnp->next && ((PathList *)(pnp->next))->level == level)
		pnp = (PathList *)pnp->next;
	if (!pnp || !pnp->path) return;
	l=strlen(pnp->path);
	plp->path = (char *)safe_malloc(l + strlen(s) + 2);
	if(l)
	  {
	    strcpy(plp->path, pnp->path);
	    if(plp->path[l-1]!=PATH_SEP)
	      strcat(plp->path, PATH_STRING);
	  }
	else plp->path[0] = '\0';
	strcat( plp->path, s);
  }
  plp->next=pathlist;
  plp->level=level;
  pathlist=plp;
}
/* This clears the path list of paths added from read_config_file. */
void clear_pathlist(int level)
{
  PathList *nlp, *plp=pathlist, *prev=0;
  while (plp) {
    nlp = (PathList *)plp->next;
    if (plp->level > level)
      {
	if (plp == pathlist) pathlist = nlp;
	free(plp->path);
	free(plp);
	if (prev) prev->next = nlp;
      }
    else prev = plp;
    plp = nlp;
  }
}
