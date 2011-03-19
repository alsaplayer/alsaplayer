/* alsaplayer_fft.h: Header for iterative implementation of a FFT
 * Copyright (C) 1999 Richard Boulton <richard@tartarus.org>
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

#ifndef _FFT_H_
#define _FFT_H_

#define FFT_BUFFER_SIZE_LOG 9

#define FFT_BUFFER_SIZE (1 << FFT_BUFFER_SIZE_LOG)

/* sound sample - should be an signed 16 bit value */
typedef short int sound_sample;

#ifdef __cplusplus
extern "C" {
#endif

/* FFT library */
typedef struct _struct_fft_state fft_state;
fft_state *fft_init (void);
extern void fft_perform (const sound_sample *input, double *output, fft_state *state);
extern void fft_close (fft_state *state);

/* Convolution stuff */
typedef struct _struct_convolve_state convolve_state;
extern convolve_state *convolve_init (void);
extern int convolve_match (const int * lastchoice,
		    const sound_sample * input,
		    convolve_state * state);
extern void convolve_close (convolve_state * state);

#ifdef __cplusplus
}
#endif

#endif /* _FFT_H_ */
