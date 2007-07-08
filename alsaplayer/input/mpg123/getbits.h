/* that's the same file as getits.c but with defines to
  force inlining
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

static unsigned long rval;
static unsigned char rval_uc;

#define backbits(nob) ((void)( \
  bsi.bitindex    -= nob, \
  bsi.wordpointer += (bsi.bitindex>>3), \
  bsi.bitindex    &= 0x7 ))

#define getbitoffset() ((-bsi.bitindex)&0x7)
#define getbyte()      (*bsi.wordpointer++)

#define getbits(nob) ( \
  rval = bsi.wordpointer[0], rval <<= 8, rval |= bsi.wordpointer[1], \
  rval <<= 8, rval |= bsi.wordpointer[2], rval <<= bsi.bitindex, \
  rval &= 0xffffff, bsi.bitindex += nob, \
  rval >>= (24-nob), bsi.wordpointer += (bsi.bitindex>>3), \
  bsi.bitindex &= 7,rval)

#define getbits_fast(nob) ( \
  rval = (unsigned char) (bsi.wordpointer[0] << bsi.bitindex), \
  rval |= ((unsigned long) bsi.wordpointer[1]<<bsi.bitindex)>>8, \
  rval <<= nob, rval >>= 8, \
  bsi.bitindex += nob, bsi.wordpointer += (bsi.bitindex>>3), \
  bsi.bitindex &= 7, rval )

#define get1bit() ( \
  rval_uc = *bsi.wordpointer << bsi.bitindex, bsi.bitindex++, \
  bsi.wordpointer += (bsi.bitindex>>3), bsi.bitindex &= 7, rval_uc>>7 )

