/*  This file is part of AlsaPlayer.
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
 */
#include "mpg123.h"

real equalizer[2][32];
real equalizer_sum[2][32];
int equalizer_cnt;

real equalizerband[2][SBLIMIT*SSLIMIT];

void do_equalizer(real *bandPtr,int channel) 
{
	int i;

	if(equalfile) {
		for(i=0;i<32;i++)
			bandPtr[i] *= equalizer[channel][i];
	}

/*	if(param.equalizer & 0x2) {
		
		for(i=0;i<32;i++)
			equalizer_sum[channel][i] += bandPtr[i];
	}
*/
}

void do_equalizerband(real *bandPtr,int channel)
{
  int i;
  for(i=0;i<576;i++) {
    bandPtr[i] *= equalizerband[channel][i];
  }
}

