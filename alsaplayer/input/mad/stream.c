/*
 * mad - MPEG audio decoder
 * Copyright (C) 2000-2001 Robert Leslie
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id$
 */

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

# include "global.h"

# include <stdlib.h>

# include "bit.h"
# include "stream.h"

/*
 * NAME:	stream->init()
 * DESCRIPTION:	initialize stream struct
 */
void mad_stream_init(struct mad_stream *stream)
{
  stream->buffer     = 0;
  stream->bufend     = 0;
  stream->skiplen    = 0;

  stream->sync       = 0;
  stream->freerate   = 0;

  stream->this_frame = 0;
  stream->next_frame = 0;
  mad_bit_init(&stream->ptr, 0);

  mad_bit_init(&stream->anc_ptr, 0);
  stream->anc_bitlen = 0;

  stream->main_data  = 0;
  stream->md_len     = 0;

  stream->options    = 0;
  stream->error      = 0;
}

/*
 * NAME:	stream->finish()
 * DESCRIPTION:	deallocate any dynamic memory associated with stream
 */
void mad_stream_finish(struct mad_stream *stream)
{
  if (stream->main_data) {
    free(stream->main_data);
    stream->main_data = 0;
  }

  mad_bit_finish(&stream->anc_ptr);
  mad_bit_finish(&stream->ptr);
}

/*
 * NAME:	stream->buffer()
 * DESCRIPTION:	set stream buffer pointers
 */
void mad_stream_buffer(struct mad_stream *stream,
		       unsigned char const *buffer, unsigned long length)
{
  stream->buffer = buffer;
  stream->bufend = buffer + length;

  stream->this_frame = buffer;
  stream->next_frame = buffer;

  stream->sync = 1;

  mad_bit_init(&stream->ptr, buffer);
}

/*
 * NAME:	stream->skip()
 * DESCRIPTION:	arrange to skip bytes before the next frame
 */
void mad_stream_skip(struct mad_stream *stream, unsigned long length)
{
  stream->skiplen += length;
}

/*
 * NAME:	stream->sync()
 * DESCRIPTION:	locate the next stream sync word
 */
int mad_stream_sync(struct mad_stream *stream)
{
  register unsigned char const *ptr, *end;

  ptr = mad_bit_nextbyte(&stream->ptr);
  end = stream->bufend;

  while (ptr < end - 1 &&
	 !(ptr[0] == 0xff && (ptr[1] & 0xe0) == 0xe0))
    ++ptr;

  if (end - ptr < MAD_BUFFER_GUARD)
    return -1;

  mad_bit_init(&stream->ptr, ptr);

  return 0;
}
