/*
 * common.h
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

/* max = 1728 */
#define MAXFRAMESIZE 1792
#define HDRCMPMASK 0xfffffd00

extern void print_id3_tag(unsigned char *buf);
extern unsigned long firsthead;
extern int tabsel_123[2][3][16];
extern double compute_tpf(struct frame *fr);
extern double compute_bpf(struct frame *fr);
extern long compute_buffer_offset(struct frame *fr);

struct bitstream_info {
  int bitindex;
  unsigned char *wordpointer;
};

extern struct bitstream_info bsi;

