/*
 * libmad - MPEG audio decoder library
 * Copyright (C) 2000-2004 Underbit Technologies, Inc.
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
 * $Id$
 */

# ifndef LIBMAD_BIT_H
# define LIBMAD_BIT_H

struct mad_bitptr {
  unsigned char const *byte;
  unsigned short cache;
  unsigned short left;
};

void mad_bit_init(struct mad_bitptr *, unsigned char const *);

# define mad_bit_finish(bitptr)		/* nothing */

unsigned int mad_bit_length(struct mad_bitptr const *,
			    struct mad_bitptr const *);

# define mad_bit_bitsleft(bitptr)  ((bitptr)->left)
unsigned char const *mad_bit_nextbyte(struct mad_bitptr const *);

void mad_bit_skip(struct mad_bitptr *, unsigned int);
unsigned long mad_bit_read(struct mad_bitptr *, unsigned int);
void mad_bit_write(struct mad_bitptr *, unsigned int, unsigned long);

unsigned short mad_bit_crc(struct mad_bitptr, unsigned int, unsigned short);

# endif
